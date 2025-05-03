/*
 * Copyright (C) 2003,2004 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#define GST_USE_UNSTABLE_API 1

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gst/gst.h>
#include <totem-pl-parser.h>

#include <locale.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "gst/totem-gst-helpers.h"
#include "gst/totem-gst-pixbuf-helpers.h"
#include "totem-resources.h"

#ifdef G_HAVE_ISO_VARARGS
#define PROGRESS_DEBUG(...) { if (verbose != FALSE) g_message (__VA_ARGS__); }
#elif defined(G_HAVE_GNUC_VARARGS)
#define PROGRESS_DEBUG(format...) { if (verbose != FALSE) g_message (format); }
#endif

/* The main() function controls progress in the first and last 10% */
#define PRINT_PROGRESS(p) { if (print_progress) g_printf ("%f%% complete\n", p); }
#define MIN_PROGRESS 10.0
#define MAX_PROGRESS 90.0

#define BORING_IMAGE_VARIANCE 256.0		/* Tweak this if necessary */
#define DEFAULT_OUTPUT_SIZE 256

static gboolean raw_output = FALSE;
static int output_size = -1;
static gboolean time_limit = TRUE;
static gboolean verbose = FALSE;
static gboolean print_progress = FALSE;
static gint64 second_index = -1;
static char **filenames = NULL;

typedef struct {
	const char *output;
	const char *input;
	GstElement *play;
	gint64      duration;
} ThumbApp;

static void save_pixbuf (GdkPixbuf *pixbuf, const char *path,
			 const char *video_path, int size, gboolean is_still);

static void
entry_parsed_cb (TotemPlParser *parser,
		 const char    *uri,
		 GHashTable    *metadata,
		 char         **new_url)
{
	*new_url = g_strdup (uri);
}

static char *
get_special_url (GFile *file)
{
	char *path, *orig_uri, *uri, *mime_type;
	TotemPlParser *parser;
	TotemPlParserResult res;

	path = g_file_get_path (file);

	mime_type = g_content_type_guess (path, NULL, 0, NULL);
	g_free (path);
	if (g_strcmp0 (mime_type, "application/x-cd-image") != 0) {
		g_free (mime_type);
		return NULL;
	}
	g_free (mime_type);

	uri = NULL;
	orig_uri = g_file_get_uri (file);

	parser = totem_pl_parser_new ();
	g_signal_connect (parser, "entry-parsed",
			  G_CALLBACK (entry_parsed_cb), &uri);

	res = totem_pl_parser_parse (parser, orig_uri, FALSE);

	g_free (orig_uri);
	g_object_unref (parser);

	if (res == TOTEM_PL_PARSER_RESULT_SUCCESS)
		return uri;

	g_free (uri);

	return NULL;
}

static gboolean
is_special_uri (const char *uri)
{
	if (g_str_has_prefix (uri, "dvd://") ||
	    g_str_has_prefix (uri, "vcd://"))
		return TRUE;

	return FALSE;
}

static void
thumb_app_set_filename (ThumbApp *app)
{
	GFile *file;
	char *uri;

	if (is_special_uri (app->input)) {
		g_object_set (app->play, "uri", app->input, NULL);
		return;
	}

	file = g_file_new_for_commandline_arg (app->input);
	uri = get_special_url (file);
	if (uri == NULL)
		uri = g_file_get_uri (file);
	g_object_unref (file);

	PROGRESS_DEBUG("setting URI %s", uri);

	g_object_set (app->play, "uri", uri, NULL);
	g_free (uri);
}

static GstBusSyncReply
error_handler (GstBus *bus,
	       GstMessage *message,
	       GstElement *play)
{
	GstMessageType msg_type;

	msg_type = GST_MESSAGE_TYPE (message);
	switch (msg_type) {
	case GST_MESSAGE_ERROR:
		totem_gst_message_print (message, play, "totem-video-thumbnailer-error");
		exit (1);
	case GST_MESSAGE_EOS:
		exit (0);

	case GST_MESSAGE_ASYNC_DONE:
	case GST_MESSAGE_UNKNOWN:
	case GST_MESSAGE_WARNING:
	case GST_MESSAGE_INFO:
	case GST_MESSAGE_TAG:
	case GST_MESSAGE_BUFFERING:
	case GST_MESSAGE_STATE_CHANGED:
	case GST_MESSAGE_STATE_DIRTY:
	case GST_MESSAGE_STEP_DONE:
	case GST_MESSAGE_CLOCK_PROVIDE:
	case GST_MESSAGE_CLOCK_LOST:
	case GST_MESSAGE_NEW_CLOCK:
	case GST_MESSAGE_STRUCTURE_CHANGE:
	case GST_MESSAGE_STREAM_STATUS:
	case GST_MESSAGE_APPLICATION:
	case GST_MESSAGE_ELEMENT:
	case GST_MESSAGE_SEGMENT_START:
	case GST_MESSAGE_SEGMENT_DONE:
	case GST_MESSAGE_DURATION_CHANGED:
	case GST_MESSAGE_LATENCY:
	case GST_MESSAGE_ASYNC_START:
	case GST_MESSAGE_REQUEST_STATE:
	case GST_MESSAGE_STEP_START:
	case GST_MESSAGE_QOS:
	case GST_MESSAGE_PROGRESS:
	case GST_MESSAGE_TOC:
	case GST_MESSAGE_RESET_TIME:
	case GST_MESSAGE_STREAM_START:
	case GST_MESSAGE_ANY:
	case GST_MESSAGE_NEED_CONTEXT:
	case GST_MESSAGE_HAVE_CONTEXT:
	default:
		/* Ignored */
		;;
	}

	return GST_BUS_PASS;
}

static void
thumb_app_cleanup (ThumbApp *app)
{
	gst_element_set_state (app->play, GST_STATE_NULL);
	g_clear_object (&app->play);
}

static void
thumb_app_set_error_handler (ThumbApp *app)
{
	GstBus *bus;

	bus = gst_element_get_bus (app->play);
	gst_bus_set_sync_handler (bus, (GstBusSyncHandler) error_handler, app->play, NULL);
	g_object_unref (bus);
}

static void
check_cover_for_stream (ThumbApp   *app,
			const char *signal_name)
{
	GdkPixbuf *pixbuf;
	GstTagList *tags = NULL;

	g_signal_emit_by_name (G_OBJECT (app->play), signal_name, 0, &tags);

	if (!tags)
		return;

	pixbuf = totem_gst_tag_list_get_cover (tags);
	if (!pixbuf) {
		gst_tag_list_unref (tags);
		return;
	}

	PROGRESS_DEBUG("Saving cover image to %s", app->output);
	thumb_app_cleanup (app);
	save_pixbuf (pixbuf, app->output, app->input, output_size, TRUE);
	g_object_unref (pixbuf);

	exit (0);
}

static void
thumb_app_check_for_cover (ThumbApp *app)
{
	PROGRESS_DEBUG ("Checking whether file has cover");
	check_cover_for_stream (app, "get-audio-tags");
	check_cover_for_stream (app, "get-video-tags");
}

static gboolean
thumb_app_set_duration (ThumbApp *app)
{
	gint64 len = -1;

	if (gst_element_query_duration (app->play, GST_FORMAT_TIME, &len) && len != -1) {
		app->duration = len / GST_MSECOND;
		return TRUE;
	}
	app->duration = -1;
	return FALSE;
}

static void
assert_duration (ThumbApp *app)
{
	if (app->duration != -1)
		return;
	g_print ("totem-video-thumbnailer couldn't get the duration of file '%s'\n", app->input);
	exit (1);
}

static gboolean
thumb_app_get_has_video (ThumbApp *app)
{
	guint n_video;
	g_object_get (app->play, "n-video", &n_video, NULL);
	return n_video > 0;
}

static gboolean
thumb_app_start (ThumbApp *app)
{
	GstBus *bus;
	GstMessageType events;
	gboolean terminate = FALSE;
	gboolean async_received = FALSE;

	gst_element_set_state (app->play, GST_STATE_PAUSED);
	bus = gst_element_get_bus (app->play);
	events = GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR;

	while (terminate == FALSE) {
		GstMessage *message;
		GstElement *src;

		message = gst_bus_timed_pop_filtered (bus,
		                                      GST_CLOCK_TIME_NONE,
		                                      events);

		src = (GstElement*)GST_MESSAGE_SRC (message);

		switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_ASYNC_DONE:
			if (src == app->play) {
				async_received = TRUE;
				terminate = TRUE;
			}
			break;
		case GST_MESSAGE_ERROR:
			totem_gst_message_print (message, app->play, "totem-video-thumbnailer-error");
			terminate = TRUE;
			break;

		case GST_MESSAGE_UNKNOWN:
		case GST_MESSAGE_EOS:
		case GST_MESSAGE_WARNING:
		case GST_MESSAGE_INFO:
		case GST_MESSAGE_TAG:
		case GST_MESSAGE_BUFFERING:
		case GST_MESSAGE_STATE_CHANGED:
		case GST_MESSAGE_STATE_DIRTY:
		case GST_MESSAGE_STEP_DONE:
		case GST_MESSAGE_CLOCK_PROVIDE:
		case GST_MESSAGE_CLOCK_LOST:
		case GST_MESSAGE_NEW_CLOCK:
		case GST_MESSAGE_STRUCTURE_CHANGE:
		case GST_MESSAGE_STREAM_STATUS:
		case GST_MESSAGE_APPLICATION:
		case GST_MESSAGE_ELEMENT:
		case GST_MESSAGE_SEGMENT_START:
		case GST_MESSAGE_SEGMENT_DONE:
		case GST_MESSAGE_DURATION_CHANGED:
		case GST_MESSAGE_LATENCY:
		case GST_MESSAGE_ASYNC_START:
		case GST_MESSAGE_REQUEST_STATE:
		case GST_MESSAGE_STEP_START:
		case GST_MESSAGE_QOS:
		case GST_MESSAGE_PROGRESS:
		case GST_MESSAGE_TOC:
		case GST_MESSAGE_RESET_TIME:
		case GST_MESSAGE_STREAM_START:
		case GST_MESSAGE_ANY:
		case GST_MESSAGE_NEED_CONTEXT:
		case GST_MESSAGE_HAVE_CONTEXT:
		default:
			/* Ignore */
			;;
		}

		gst_message_unref (message);
	}

	gst_object_unref (bus);

	if (async_received) {
		/* state change succeeded */
		GST_DEBUG ("state change to %s succeeded", gst_element_state_get_name (GST_STATE_PAUSED));
	}

	return async_received;
}

/* Manually set number of worker threads for decoders in order to reduce memory
 * usage on setups with many cores, see
 * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/4423 */
static void
element_setup_cb (GstElement * playbin, GstElement * element, gpointer udata)
{
	gchar *name;

	name = gst_element_get_name (element);
	if (g_str_has_prefix (name, "avdec_")) {
		GObjectClass *gobject_class;
		GParamSpec *pspec;

		gobject_class = G_OBJECT_GET_CLASS (element);
		pspec = g_object_class_find_property (gobject_class, "max-threads");

		if (pspec) {
			PROGRESS_DEBUG("Setting max-threads to 1 for %s", name);
			g_object_set (element, "max-threads", 1, NULL);
		}
	} else if (g_str_has_prefix (name, "dav1ddec")) {
		PROGRESS_DEBUG("Setting n-threads to 1 for %s", name);
		g_object_set (element, "n-threads", 1, NULL);
	} else if (g_str_has_prefix (name, "vp8dec") ||
		   g_str_has_prefix (name, "vp9dec")) {
		PROGRESS_DEBUG("Setting threads to 1 for %s", name);
		g_object_set (element, "threads", 1, NULL);
	}

	g_free (name);
}

static void
thumb_app_setup_play (ThumbApp *app)
{
	GstElement *play;
	GstElement *audio_sink, *video_sink;

	play = gst_element_factory_make ("playbin", "play");
	audio_sink = gst_element_factory_make ("fakesink", "audio-fake-sink");
	video_sink = gst_element_factory_make ("fakesink", "video-fake-sink");
	g_object_set (video_sink, "sync", TRUE, NULL);

	g_object_set (play,
		      "audio-sink", audio_sink,
		      "video-sink", video_sink,
		      "flags", GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO,
		      NULL);

	g_signal_connect (play, "element-setup", G_CALLBACK (element_setup_cb), NULL);

	app->play = play;

	totem_gst_disable_hardware_decoders ();
}

static void
thumb_app_seek (ThumbApp *app,
		gint64    _time)
{
	gst_element_seek (app->play, 1.0,
			  GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
			  GST_SEEK_TYPE_SET, _time * GST_MSECOND,
			  GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
	/* And wait for this seek to complete */
	gst_element_get_state (app->play, NULL, NULL, GST_CLOCK_TIME_NONE);
}

/* This function attempts to detect images that are mostly solid images
 * It does this by calculating the statistical variance of the
 * black-and-white image */
static gboolean
is_image_interesting (GdkPixbuf *pixbuf)
{
	/* We're gonna assume 8-bit samples. If anyone uses anything different,
	 * it doesn't really matter cause it's gonna be ugly anyways */
	int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	int height = gdk_pixbuf_get_height(pixbuf);
	guchar* buffer = gdk_pixbuf_get_pixels(pixbuf);
	int num_samples = (rowstride * height);
	int i;
	float x_bar = 0.0f;
	float variance = 0.0f;

	/* FIXME: If this proves to be a performance issue, this function
	 * can be modified to perhaps only check 3 rows. I doubt this'll
	 * be a problem though. */

	/* Iterate through the image to calculate x-bar */
	for (i = 0; i < num_samples; i++) {
		x_bar += (float) buffer[i];
	}
	x_bar /= ((float) num_samples);

	/* Calculate the variance */
	for (i = 0; i < num_samples; i++) {
		float tmp = ((float) buffer[i] - x_bar);
		variance += tmp * tmp;
	}
	variance /= ((float) (num_samples - 1));

	return (variance > BORING_IMAGE_VARIANCE);
}

static GdkPixbuf *
scale_pixbuf (GdkPixbuf *pixbuf, int size, gboolean is_still)
{
	GdkPixbuf *result;
	int width, height;
	int d_width, d_height;

	if (size != -1) {
		height = gdk_pixbuf_get_height (pixbuf);
		width = gdk_pixbuf_get_width (pixbuf);

		if (width > height) {
			d_width = size;
			d_height = size * height / width;
		} else {
			d_height = size;
			d_width = size * width / height;
		}
	} else {
		d_width = d_height = -1;
	}

	if (size <= 256) {
		GdkPixbuf *small;

		small = gdk_pixbuf_scale_simple (pixbuf, d_width, d_height, GDK_INTERP_BILINEAR);
		result = small;
	} else {
		if (size > 0)
			result = gdk_pixbuf_scale_simple (pixbuf, d_width, d_height, GDK_INTERP_BILINEAR);
		else
			result = g_object_ref (pixbuf);
	}

	return result;
}

static void
save_pixbuf (GdkPixbuf *pixbuf, const char *path,
	     const char *video_path, int size, gboolean is_still)
{
	int width, height;
	char *a_width, *a_height;
	GdkPixbuf *with_holes;
	GError *err = NULL;
	gboolean ret;

	height = gdk_pixbuf_get_height (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);

	/* If we're outputting a raw image without a size,
	 * don't scale the pixbuf or add borders */
	if (raw_output != FALSE && size == -1)
		with_holes = g_object_ref (pixbuf);
	else if (raw_output != FALSE)
		with_holes = scale_pixbuf (pixbuf, size, TRUE);
	else
		with_holes = scale_pixbuf (pixbuf, size, is_still);

	a_width = g_strdup_printf ("%d", width);
	a_height = g_strdup_printf ("%d", height);

	ret = gdk_pixbuf_save (with_holes, path, "png", &err,
			       "tEXt::Thumb::Image::Width", a_width,
			       "tEXt::Thumb::Image::Height", a_height,
			       NULL);

	if (ret == FALSE) {
		if (err != NULL) {
			g_print ("totem-video-thumbnailer couldn't write the thumbnail '%s' for video '%s': %s\n", path, video_path, err->message);
			g_error_free (err);
		} else {
			g_print ("totem-video-thumbnailer couldn't write the thumbnail '%s' for video '%s'\n", path, video_path);
		}

		g_object_unref (with_holes);
		return;
	}

	g_object_unref (with_holes);
}

static GdkPixbuf *
capture_frame_at_time (ThumbApp   *app,
		       gint64 milliseconds)
{
	if (milliseconds != 0)
		thumb_app_seek (app, milliseconds);

	return totem_gst_playbin_get_frame (app->play, NULL);
}

static GdkPixbuf *
capture_interesting_frame (ThumbApp *app)
{
	GdkPixbuf* pixbuf;
	guint current;
	const double frame_locations[] = {
		1.0 / 3.0,
		2.0 / 3.0,
		0.1,
		0.9,
		0.5
	};

	if (app->duration == -1) {
		PROGRESS_DEBUG("Video has no duration, so capture 1st frame");
		return capture_frame_at_time (app, 0);
	}

	/* Test at multiple points in the file to see if we can get an
	 * interesting frame */
	for (current = 0; current < G_N_ELEMENTS(frame_locations); current++)
	{
		PROGRESS_DEBUG("About to seek to %f", frame_locations[current]);
		thumb_app_seek (app, frame_locations[current] * app->duration);

		/* Pull the frame, if it's interesting we bail early */
		PROGRESS_DEBUG("About to get frame for iter %d", current);
		pixbuf = totem_gst_playbin_get_frame (app->play, NULL);
		if (pixbuf != NULL && is_image_interesting (pixbuf) != FALSE) {
			PROGRESS_DEBUG("Frame for iter %d is interesting", current);
			break;
		}

		/* If we get to the end of this loop, we'll end up using
		 * the last image we pulled */
		if (current + 1 < G_N_ELEMENTS(frame_locations))
			g_clear_object (&pixbuf);
		PROGRESS_DEBUG("Frame for iter %d was not interesting", current);
	}
	return pixbuf;
}

static const GOptionEntry entries[] = {
	{ "size", 's', 0, G_OPTION_ARG_INT, &output_size, "Size of the thumbnail in pixels", NULL },
	{ "raw", 'r', 0, G_OPTION_ARG_NONE, &raw_output, "Output the raw picture of the video without scaling or adding borders", NULL },
	{ "no-limit", 'l', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &time_limit, "Don't limit the thumbnailing time to 30 seconds", NULL },
	{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Output debug information", NULL },
	{ "time", 't', 0, G_OPTION_ARG_INT64, &second_index, "Choose this time (in seconds) as the thumbnail", NULL },
	{ "print-progress", 'p', 0, G_OPTION_ARG_NONE, &print_progress, "Only print progress updates (can't be used with --verbose)", NULL },
	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, "[INPUT FILE] [OUTPUT FILE]" },
	{ NULL }
};

int main (int argc, char *argv[])
{
	GOptionGroup *options;
	GOptionContext *context;
	GError *err = NULL;
	GdkPixbuf *pixbuf;
	const char *input, *output;
	ThumbApp app;

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* Limit the number of OpenBLAS threads to avoid reaching our RLIMIT_DATA
	 * address space max size safeguard for the thumbnailer. */
	g_setenv("OMP_NUM_THREADS", "1", TRUE);

	/* Call before the global thread pool is setup */
	errno = 0;
	if (nice (20) != 20 && errno != 0)
		g_warning ("Couldn't change nice value of process.");

	context = g_option_context_new ("Thumbnail movies");
	options = gst_init_get_option_group ();
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, options);

	if (g_option_context_parse (context, &argc, &argv, &err) == FALSE) {
		g_print ("couldn't parse command-line options: %s\n", err->message);
		g_error_free (err);
		return 1;
	}

	if (print_progress) {
		fcntl (fileno (stdout), F_SETFL, O_NONBLOCK);
		setbuf (stdout, NULL);
	}

	if (raw_output == FALSE && output_size == -1)
		output_size = DEFAULT_OUTPUT_SIZE;

	if (filenames == NULL || g_strv_length (filenames) != 2 ||
	    (print_progress == TRUE && verbose == TRUE)) {
		char *help;
		help = g_option_context_get_help (context, FALSE, NULL);
		g_print ("%s", help);
		g_free (help);
		return 1;
	}
	input = filenames[0];
	output = filenames[1];

	PROGRESS_DEBUG("Initialized libraries, about to create video widget");
	PRINT_PROGRESS (2.0);

	app.input = input;
	app.output = output;

	thumb_app_setup_play (&app);
	thumb_app_set_filename (&app);

	PROGRESS_DEBUG("Video widget created");
	PRINT_PROGRESS (6.0);

	if (time_limit != FALSE)
		totem_resources_monitor_start (input, 0, verbose);

	PROGRESS_DEBUG("About to open video file");

	if (thumb_app_start (&app) == FALSE) {
		g_print ("totem-video-thumbnailer couldn't open file '%s'\n", input);
		exit (1);
	}
	thumb_app_set_error_handler (&app);

	thumb_app_check_for_cover (&app);
	if (thumb_app_get_has_video (&app) == FALSE) {
		PROGRESS_DEBUG ("totem-video-thumbnailer couldn't find a video track in '%s'\n", input);
		exit (1);
	}
	thumb_app_set_duration (&app);

	PROGRESS_DEBUG("Opened video file: '%s'", input);
	PRINT_PROGRESS (10.0);

	/* If the user has told us to use a frame at a specific second
	 * into the video, just use that frame no matter how boring it
	 * is */
	if (second_index != -1) {
		assert_duration (&app);
		pixbuf = capture_frame_at_time (&app, second_index * 1000);
	} else {
		pixbuf = capture_interesting_frame (&app);
	}
	PRINT_PROGRESS (90.0);

	/* Cleanup */
	totem_resources_monitor_stop ();
	thumb_app_cleanup (&app);
	PRINT_PROGRESS (92.0);

	if (pixbuf == NULL) {
		g_print ("totem-video-thumbnailer couldn't get a picture from '%s'\n", input);
		exit (1);
	}

	PROGRESS_DEBUG("Saving captured screenshot to %s", output);
	save_pixbuf (pixbuf, output, input, output_size, FALSE);
	g_object_unref (pixbuf);
	PRINT_PROGRESS (100.0);

	return 0;
}

