
#include <bacon-video-widget.h>
#include <gtk/gtk.h>
#include <glib/gthread.h>
#include <unistd.h>
#include <stdlib.h>

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
	}
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_AUDIO, &value);
	has_type = g_value_get_boolean (&value);
	g_print ("Has audio: %s\n", boolean_to_string (has_type));
	g_value_unset (&value);

	if (has_type) {
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_BITRATE, &value);
		g_print ("Bitrate: %d kbps\n", g_value_get_int (&value));
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
	GtkWidget *widget;
	BaconVideoWidget *bvw;
	GError *error = NULL;

	if (argc != 2) {
		print_usage (argv[0]);
		return 1;
	}

	g_thread_init (NULL);
	gdk_threads_init ();
	gtk_init (&argc, &argv);

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

	if (bacon_video_widget_open (bvw, argv[1], NULL) == FALSE) {
		g_print ("Can't open %s\n", argv[1]);
		return 1;
	}
	if (bacon_video_widget_play (bvw, NULL) == FALSE) {
		g_print ("Can't play %s\n", argv[1]);
		return 1;
	}

	gtk_main ();

	return 0;
}

