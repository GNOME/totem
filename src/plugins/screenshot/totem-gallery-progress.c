/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2008 Philip Withnall <philip@tecnocode.co.uk>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "totem-gallery-progress.h"

static void totem_gallery_progress_finalize (GObject *object);

struct _TotemGalleryProgress {
	GObject parent;
	GPid child_pid;
	GString *line;
	gchar *output_filename;
};

enum {
	PROGRESS,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (TotemGalleryProgress, totem_gallery_progress, G_TYPE_OBJECT)

static void
totem_gallery_progress_class_init (TotemGalleryProgressClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = totem_gallery_progress_finalize;

	signals[PROGRESS] = g_signal_new ("progress",
	                                  TOTEM_TYPE_GALLERY_PROGRESS,
	                                  G_SIGNAL_RUN_LAST,
	                                  0, NULL, NULL, NULL,
									  G_TYPE_NONE,
	                                  1, G_TYPE_DOUBLE);
}

static void
totem_gallery_progress_init (TotemGalleryProgress *self)
{
}

static void
totem_gallery_progress_finalize (GObject *object)
{
	TotemGalleryProgress *progress = TOTEM_GALLERY_PROGRESS (object);

	g_spawn_close_pid (progress->child_pid);
	g_free (progress->output_filename);

	if (progress->line != NULL)
		g_string_free (progress->line, TRUE);

	/* Unlink the output file, just in case (race condition) it's already been created */
	g_unlink (progress->output_filename);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (totem_gallery_progress_parent_class)->finalize (object);
}

TotemGalleryProgress *
totem_gallery_progress_new (GPid child_pid, const gchar *output_filename)
{
	TotemGalleryProgress *self;

	/* Create the gallery */
	self = g_object_new (TOTEM_TYPE_GALLERY_PROGRESS, NULL);

	/* Initialize class variables */
	self->child_pid = child_pid;
	self->output_filename = g_strdup (output_filename);

	return self;
}

static gboolean
process_line (TotemGalleryProgress *self, const gchar *line)
{
	gfloat percent_complete;

	if (sscanf (line, "%f%% complete", &percent_complete) == 1) {
		g_signal_emit (self, signals[PROGRESS], 0, percent_complete / 100.0);
		return TRUE;
	}

	/* Error! */
	return FALSE;
}

static gboolean
stdout_watch_cb (GIOChannel *source, GIOCondition condition, TotemGalleryProgress *self)
{
	gboolean retval = TRUE;

	if (condition & G_IO_IN) {
		gchar *line;
		gchar buf[1];
		GIOStatus status;

		status = g_io_channel_read_line (source, &line, NULL, NULL, NULL);

		if (status == G_IO_STATUS_NORMAL) {
			if (self->line != NULL) {
				g_string_append (self->line, line);
				g_free (line);
				line = g_string_free (self->line, FALSE);
				self->line = NULL;
			}

			retval = process_line (self, line);
			g_free (line);
		} else if (status == G_IO_STATUS_AGAIN) {
			/* A non-terminated line was read, read the data into the buffer. */
			status = g_io_channel_read_chars (source, buf, 1, NULL, NULL);

			if (status == G_IO_STATUS_NORMAL) {
				gchar *line2;

				if (self->line == NULL)
					self->line = g_string_new (NULL);
				g_string_append_c (self->line, buf[0]);

				switch (buf[0]) {
				case '\n':
				case '\r':
				case '\xe2':
				case '\0':
					line2 = g_string_free (self->line, FALSE);
					self->line = NULL;

					retval = process_line (self, line2);
					g_free (line2);
					break;
				default:
					break;
				}
			}
		} else if (status == G_IO_STATUS_EOF) {
			/* Emit as complete and stop processing */
			g_signal_emit (self, signals[PROGRESS], 0, 1.0);
			retval = FALSE;
		}
	} else if (condition & G_IO_HUP) {
		retval = FALSE;
	}

	return retval;
}

void
totem_gallery_progress_run (TotemGalleryProgress *self, gint stdout_fd)
{
	GIOChannel *channel;

	fcntl (stdout_fd, F_SETFL, O_NONBLOCK);

	/* Watch the output from totem-video-thumbnailer */
	channel = g_io_channel_unix_new (stdout_fd);
	g_io_channel_set_flags (channel, g_io_channel_get_flags (channel) | G_IO_FLAG_NONBLOCK, NULL);

	g_io_add_watch (channel, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL, (GIOFunc) stdout_watch_cb, self);

	g_io_channel_unref (channel);
}

