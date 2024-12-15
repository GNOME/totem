/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Philip Withnall <philip@tecnocode.co.uk>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "totem-dirs.h"
#include "totem-gallery.h"
#include "totem-gallery-progress.h"
#include "totem-screenshot-plugin.h"

static void filechooser_response_callback (GtkNativeDialog *file_chooser, gint response_id, TotemGallery *self);

static void create_gallery_cb (GtkButton *button, TotemGallery *self);

struct _TotemGallery {
	HdyWindow parent;
	Totem *totem;
	GtkCheckButton *default_screenshot_count;
	GtkSpinButton *screenshot_count;
	GtkSpinButton *screenshot_width;
	GtkProgressBar *progress_bar;

	TotemGalleryProgress *gallery_progress;
	GFile *saved_tmp_file;
};

G_DEFINE_TYPE (TotemGallery, totem_gallery, HDY_TYPE_WINDOW)

static void
totem_gallery_finalize (GObject *object)
{
	TotemGallery *self = TOTEM_GALLERY (object);

	g_clear_object (&self->progress_bar);
	g_clear_object(&self->saved_tmp_file);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (totem_gallery_parent_class)->finalize (object);
}

static void
totem_gallery_class_init (TotemGalleryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = totem_gallery_finalize;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/plugins/screenshot/gallery.ui");

	gtk_widget_class_bind_template_child (widget_class, TotemGallery, default_screenshot_count);
	gtk_widget_class_bind_template_child (widget_class, TotemGallery, screenshot_count);
	gtk_widget_class_bind_template_child (widget_class, TotemGallery, screenshot_width);
	gtk_widget_class_bind_template_child (widget_class, TotemGallery, progress_bar);

	gtk_widget_class_bind_template_callback (widget_class, create_gallery_cb);
}

static void
totem_gallery_init (TotemGallery *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	self->saved_tmp_file = NULL;
}

TotemGallery *
totem_gallery_new (Totem *totem)
{
	TotemGallery *gallery;

	/* Create the gallery and its interface */
	gallery = g_object_new (TOTEM_TYPE_GALLERY, NULL);

	gallery->totem = totem;

	return gallery;
}

static void
save_gallery_file (TotemGallery *self)
{
	GtkFileChooserNative *file_chooser;
	g_autofree gchar *suggested_name = NULL;
	g_autofree gchar *movie_title = NULL;

	gtk_widget_set_visible (GTK_WIDGET (self), FALSE);

	movie_title = totem_object_get_short_title (self->totem);

	/* Translators: The first argument is the movie title. The second
	 * argument is a number which is used to prevent overwriting files.
	 * Just translate "Gallery", and not the ".jpg". Example:
	 * "Galerie-%s-%d.jpg". */
	suggested_name = totem_screenshot_plugin_filename_for_current_video (self->totem, N_("Gallery-%s-%d.jpg"));

	file_chooser = gtk_file_chooser_native_new (_("Save Gallery"),
						    GTK_WINDOW (totem_object_get_main_window(self->totem)),
						    GTK_FILE_CHOOSER_ACTION_SAVE,
						    _("_Save"), _("_Cancel"));
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_chooser), suggested_name);
	totem_screenshot_plugin_set_file_chooser_folder (GTK_FILE_CHOOSER (file_chooser));
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (file_chooser), TRUE);

	g_signal_connect (file_chooser, "response",
			  G_CALLBACK (filechooser_response_callback), self);

	gtk_native_dialog_show (GTK_NATIVE_DIALOG (file_chooser));
}

static void
gallery_progress_cb (TotemGalleryProgress *gallery_progress, double progress, TotemGallery *self)
{
	gtk_progress_bar_set_fraction (self->progress_bar, progress);

	if (1.0 == progress)
		save_gallery_file (self);
}

static void
create_gallery_cb (GtkButton *button, TotemGallery *self)
{
	g_autofree char *tmp_filename = NULL;
	gchar *video_mrl, *argv[6];
	guint screenshot_count, i;
	gint stdout_fd;
	GPid child_pid;
	gboolean ret;
	GError *error = NULL;
	int fd;

	if (hdy_expander_row_get_expanded (HDY_EXPANDER_ROW (self->default_screenshot_count)) == FALSE)
		screenshot_count = 0;
	else
		screenshot_count = gtk_spin_button_get_value_as_int (self->screenshot_count);

	video_mrl = totem_object_get_current_mrl (self->totem);

	tmp_filename = g_build_filename (g_get_tmp_dir (), "totem-gallery-XXXXXX.jpg", NULL);
	fd = g_mkstemp(tmp_filename);

	if (fd == -1) {
		g_warning ("Could not create a temporary file");
		gtk_window_close (GTK_WINDOW (self));
		return;
	}

	self->saved_tmp_file = g_file_new_for_path (tmp_filename);

	/* Build the command and arguments to pass it */
	argv[0] = (gchar*) LIBEXECDIR "/totem-gallery-thumbnailer"; /* a little hacky, but only the allocated stuff is freed below */
	argv[1] = g_strdup_printf ("--gallery=%u", screenshot_count); /* number of screenshots to output */
	argv[2] = g_strdup_printf ("--size=%u", gtk_spin_button_get_value_as_int (self->screenshot_width)); /* screenshot width */
	argv[3] = video_mrl; /* video to thumbnail */
	argv[4] = tmp_filename; /* output filename */
	argv[5] = NULL;

	/* Run the command */
	ret = g_spawn_async_with_pipes (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
					&child_pid, NULL, &stdout_fd, NULL, &error);

	/* Free argv, minus the filename */
	for (i = 1; i < G_N_ELEMENTS (argv) - 2; i++)
		g_free (argv[i]);

	if (ret == FALSE) {
		g_warning ("Error spawning totem-video-thumbnailer: %s", error->message);
		g_error_free (error);
		return;
	}

	/* Create the progress dialogue */
	gtk_widget_set_visible (GTK_WIDGET (self->progress_bar), TRUE);

	self->gallery_progress = totem_gallery_progress_new (child_pid, tmp_filename);
	g_signal_connect (self->gallery_progress, "progress", G_CALLBACK (gallery_progress_cb), self);
	totem_gallery_progress_run (TOTEM_GALLERY_PROGRESS (self->gallery_progress), stdout_fd);
}

static void
filechooser_response_callback (GtkNativeDialog *file_chooser, gint response_id, TotemGallery *self)
{
	g_autoptr(GFile) filename = NULL;

	if (response_id == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (file_chooser));

		totem_screenshot_plugin_update_file_chooser (gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (file_chooser)));

		g_file_move (self->saved_tmp_file,
		             filename,
		             G_FILE_COPY_OVERWRITE,
		             NULL, NULL, NULL, NULL);
	}

	gtk_native_dialog_destroy (file_chooser);

	gtk_window_close (GTK_WINDOW (self));
}

