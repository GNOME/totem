/* 
 * Copyright (C) 2006 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifndef HAVE_GTK_ONLY
#include <gnome.h>
#endif

#include <bacon-video-widget.h>
#include <glib/gthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-init.h>

#include "totem-mime-types.h"

static gboolean show_mimetype = FALSE;
static char **filenames = NULL;

static void
print_mimetypes (void)
{
	guint i;

	for (i =0; i < G_N_ELEMENTS (mime_types); i++) {
		g_print ("%s\n", mime_types[i]);
	}
}

static const char *
boolean_to_string (gboolean data)
{
	return (data ? "True" : "False");
}

static void
totem_print_string (BaconVideoWidget *bvw, const char *key, BaconVideoWidgetMetadataType id)
{
	GValue value = { 0, };
	const char *str;

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			id, &value);
	str = g_value_get_string (&value);
	if (str != NULL) {
		g_print ("%s=%s\n", key, str);
	}
}

static void
totem_print_int (BaconVideoWidget *bvw, const char *key, BaconVideoWidgetMetadataType id)
{
	GValue value = { 0, };
	int num;

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			id, &value);
	num = g_value_get_int (&value);
	if (num != 0) {
		g_print ("%s=%d\n", key, num);
	}
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, gpointer data)
{
	GValue value = { 0, };
	gboolean has_type;

	totem_print_string (bvw, "TOTEM_INFO_TITLE", BVW_INFO_TITLE);
	totem_print_string (bvw, "TOTEM_INFO_ARTIST", BVW_INFO_ARTIST);
	totem_print_string (bvw, "TOTEM_INFO_YEAR", BVW_INFO_YEAR);
	totem_print_string (bvw, "TOTEM_INFO_ALBUM", BVW_INFO_ALBUM);

	totem_print_int (bvw, "TOTEM_INFO_DURATION", BVW_INFO_DURATION);
	totem_print_int (bvw, "TOTEM_INFO_TRACK_NUMBER", BVW_INFO_TRACK_NUMBER);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_VIDEO, &value);
	has_type = g_value_get_boolean (&value);
	g_print ("TOTEM_INFO_HAS_VIDEO=%s\n", boolean_to_string (has_type));
	g_value_unset (&value);

	if (has_type) {
		totem_print_int (bvw, "TOTEM_INFO_VIDEO_WIDTH", BVW_INFO_DIMENSION_X);
		totem_print_int (bvw, "TOTEM_INFO_VIDEO_HEIGHT", BVW_INFO_DIMENSION_Y);
		totem_print_string (bvw, "TOTEM_INFO_VIDEO_CODEC", BVW_INFO_VIDEO_CODEC);
		totem_print_int (bvw, "TOTEM_INFO_FPS", BVW_INFO_FPS);
		totem_print_int (bvw, "TOTEM_INFO_VIDEO_BITRATE", BVW_INFO_VIDEO_BITRATE);
	}

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_AUDIO, &value);
	has_type = g_value_get_boolean (&value);
	g_print ("TOTEM_INFO_HAS_AUDIO=%s\n", boolean_to_string (has_type));
	g_value_unset (&value);

	if (has_type) {
		totem_print_int (bvw, "TOTEM_INFO_AUDIO_BITRATE", BVW_INFO_AUDIO_BITRATE);
		totem_print_string (bvw, "TOTEM_INFO_AUDIO_CODEC", BVW_INFO_AUDIO_CODEC);
		totem_print_int (bvw, "TOTEM_INFO_AUDIO_SAMPLE_RATE", BVW_INFO_AUDIO_SAMPLE_RATE);
		totem_print_string (bvw, "TOTEM_INFO_AUDIO_CHANNELS", BVW_INFO_AUDIO_CHANNELS);
	}
	bacon_video_widget_close (bvw);
	exit (0);
}

static const GOptionEntry entries[] = {
	{"mimetype", 'm', 0, G_OPTION_ARG_NONE, &show_mimetype, "List the supported mime-types", NULL},
	{G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, "Movies to index", NULL},
	{NULL}
};

int main (int argc, char **argv)
{
	GOptionGroup *options;
	GOptionContext *context;
	GtkWidget *widget;
	BaconVideoWidget *bvw;
	GError *error = NULL;
	const char *path;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_thread_init (NULL);
	gdk_threads_init ();
	context = g_option_context_new ("Index movies or songs");
	options = bacon_video_widget_get_option_group ();
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, options);
	g_type_init ();

#ifndef HAVE_GTK_ONLY
	gnome_authentication_manager_init ();
#endif

	gnome_vfs_init ();
	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_print ("couldn't parse command-line options: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

	if (show_mimetype == TRUE) {
		print_mimetypes ();
		return 0;
	} else if (filenames == NULL || g_strv_length (filenames) != 1) {
		g_print ("Expects exactly one URI\n");
		return 1;
	}

	widget = bacon_video_widget_new (-1, -1, BVW_USE_TYPE_METADATA, &error);
	if (widget == NULL) {
		g_print ("error creating the video widget: %s\n", error->message);
		g_error_free (error);
		return 1;
	}
	bvw = BACON_VIDEO_WIDGET (widget);
	g_signal_connect (G_OBJECT (bvw), "got-metadata",
			G_CALLBACK (on_got_metadata_event),
			NULL);

	path = filenames[0];
	if (bacon_video_widget_open (bvw, path, &error) == FALSE) {
		g_print ("Can't open %s: %s\n", path, error->message);
		return 1;
	}
	if (bacon_video_widget_play (bvw, &error) == FALSE) {
		g_print ("Can't play %s: %s\n", path, error->message);
		return 1;
	}

	gtk_main ();

	return 0;
}

