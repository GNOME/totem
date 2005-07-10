#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_GTK_ONLY
#include <gnome.h>
#else
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#endif

#include <bacon-video-widget.h>
#include <glib/gthread.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-init.h>

static char*
totem_create_full_path (const char *path)
{
	char *retval, *curdir, *curdir_withslash, *escaped;

	g_return_val_if_fail (path != NULL, NULL);

	if (g_str_has_prefix (path, "cdda:") != FALSE)
		return g_strdup (path);

	if (strstr (path, "://") != NULL)
		return g_strdup (path);

	if (path[0] == '/')
	{
		escaped = gnome_vfs_escape_path_string (path);

		retval = g_strdup_printf ("file://%s", escaped);
		g_free (escaped);
		return retval;
	}

	curdir = g_get_current_dir ();
	escaped = gnome_vfs_escape_path_string (curdir);
	curdir_withslash = g_strdup_printf ("file://%s%s",
			escaped, G_DIR_SEPARATOR_S);
	g_free (escaped);
	g_free (curdir);

	escaped = gnome_vfs_escape_path_string (path);
	retval = gnome_vfs_uri_make_full_from_relative
		(curdir_withslash, escaped);
	g_free (curdir_withslash);
	g_free (escaped);

	return retval;
}

static void
print_usage (const char *arg)
{
	g_print ("incorrect number of arguments\n");
	g_print ("Usage: %s <file>\n", arg);
}

static const char *
boolean_to_string (gboolean data)
{
	return (data ? "True" : "False");
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, gpointer data)
{
	GValue value = { 0, };
	int x, y;
	gboolean has_type;

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_TITLE, &value);
	g_print ("Title: %s\n", g_value_get_string (&value));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_ARTIST, &value);
	g_print ("Artist: %s\n", g_value_get_string (&value));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_YEAR, &value);
	g_print ("Year: %s\n", g_value_get_string (&value));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_ALBUM, &value);
	g_print ("Album: %s\n", g_value_get_string (&value));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_DURATION, &value);
	g_print ("Duration: %d\n", g_value_get_int (&value));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_CDINDEX, &value);
	g_print ("CD Index: %s\n", g_value_get_string (&value));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_TRACK_NUMBER, &value);
	g_print ("Track Number: %d\n", g_value_get_int (&value));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_VIDEO, &value);
	has_type = g_value_get_boolean (&value);
	g_print ("Has video: %s\n", boolean_to_string (has_type));
	g_value_unset (&value);

	if (has_type) {
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_DIMENSION_X, &value);
		x = g_value_get_int (&value);
		g_value_unset (&value);
		bacon_video_widget_get_metadata (bvw, BVW_INFO_DIMENSION_Y,
				&value);
		y = g_value_get_int (&value);
		g_value_unset (&value);
		g_print ("Dimensions: %d x %d\n", x, y);

		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_VIDEO_CODEC, &value);
		g_print ("Video Codec: %s\n", g_value_get_string (&value));
		g_value_unset (&value);

		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_FPS, &value);
		g_print ("Frames/sec: %d\n", g_value_get_int (&value));
		g_value_unset (&value);
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_VIDEO_BITRATE, &value);
		g_print ("Video Bitrate: %d kbps\n", g_value_get_int (&value));
		g_value_unset (&value);
	}
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_AUDIO, &value);
	has_type = g_value_get_boolean (&value);
	g_print ("Has audio: %s\n", boolean_to_string (has_type));
	g_value_unset (&value);

	if (has_type) {
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_AUDIO_BITRATE, &value);
		g_print ("Audio Bitrate: %d kbps\n", g_value_get_int (&value));
		g_value_unset (&value);

		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_AUDIO_CODEC, &value);
		g_print ("Audio Codec: %s\n", g_value_get_string (&value));
		g_value_unset (&value);
	}
	bacon_video_widget_close (bvw);
	exit (0);
}

int main (int argc, char **argv)
{
	static struct poptOption options[] = {
		{NULL, '\0', POPT_ARG_INCLUDE_TABLE, NULL, 0,
			"Backend options", NULL},
		{NULL, '\0', 0, NULL, 0} /* end the list */
	};
	GtkWidget *widget;
	BaconVideoWidget *bvw;
	GError *error = NULL;
	char *path;

	if (argc != 2) {
		print_usage (argv[0]);
		return 1;
	}

	g_thread_init (NULL);
	gdk_threads_init ();
	options[0].arg = bacon_video_widget_get_popt_table ();
	g_type_init ();
	gnome_vfs_init ();
	bacon_video_widget_init_backend (&argc, &argv);

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

	path = totem_create_full_path (argv[1]);
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

