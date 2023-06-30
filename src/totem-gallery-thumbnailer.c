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
#include <cairo.h>
#include <gst/gst.h>
#include <gdk/gdk.h>
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
#include "gst/totem-time-helpers.h"
#include "gst/totem-gst-pixbuf-helpers.h"

/* The main() function controls progress in the first and last 10% */
#define PRINT_PROGRESS(p) { g_printf ("%f%% complete\n", p); }
#define MIN_PROGRESS 10.0
#define MAX_PROGRESS 90.0

#define GALLERY_MIN 3				/* minimum number of screenshots in a gallery */
#define GALLERY_MAX 30				/* maximum number of screenshots in a gallery */
#define GALLERY_HEADER_HEIGHT 66		/* header height (in pixels) for the gallery */
#define DEFAULT_OUTPUT_SIZE 256

static gboolean raw_output = FALSE;
static int output_size = -1;
static gint gallery = -1;
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

	g_debug("setting URI %s", uri);

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
	GdkPixbuf *with_holes;
	GError *err = NULL;
	gboolean ret;

	/* If we're outputting a gallery or a raw image without a size,
	 * don't scale the pixbuf or add borders */
	if (gallery != -1 || (raw_output != FALSE && size == -1))
		with_holes = g_object_ref (pixbuf);
	else if (raw_output != FALSE)
		with_holes = scale_pixbuf (pixbuf, size, TRUE);
	else
		with_holes = scale_pixbuf (pixbuf, size, is_still);


	ret = gdk_pixbuf_save (with_holes, path, "jpeg", &err, NULL);

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
cairo_surface_to_pixbuf (cairo_surface_t *surface)
{
	gint stride, width, height, x, y;
	guchar *data, *output, *output_pixel;

	/* This doesn't deal with alpha --- it simply converts the 4-byte Cairo ARGB
	 * format to the 3-byte GdkPixbuf packed RGB format. */
	g_assert (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_RGB24);

	stride = cairo_image_surface_get_stride (surface);
	width = cairo_image_surface_get_width (surface);
	height = cairo_image_surface_get_height (surface);
	data = cairo_image_surface_get_data (surface);

	output = g_malloc (stride * height);
	output_pixel = output;

	for (y = 0; y < height; y++) {
		guint32 *row = (guint32*) (data + y * stride);

		for (x = 0; x < width; x++) {
			output_pixel[0] = (row[x] & 0x00ff0000) >> 16;
			output_pixel[1] = (row[x] & 0x0000ff00) >> 8;
			output_pixel[2] = (row[x] & 0x000000ff);

			output_pixel += 3;
		}
	}

	return gdk_pixbuf_new_from_data (output, GDK_COLORSPACE_RGB, FALSE, 8,
					 width, height, width * 3,
					 (GdkPixbufDestroyNotify) g_free, NULL);
}


static GdkPixbuf *
create_gallery (ThumbApp *app)
{
	GdkPixbuf *screenshot, *pixbuf = NULL;
	cairo_t *cr;
	cairo_surface_t *surface;
	PangoLayout *layout;
	PangoFontDescription *font_desc;
	gint64 stream_length, screenshot_interval, pos;
	guint columns = 3, rows, current_column, current_row, x, y;
	gint screenshot_width = 0, screenshot_height = 0, x_padding = 0, y_padding = 0;
	gfloat scale = 1.0;
	gchar *header_text, *duration_text, *filename;
	GFile *file;

	/* Calculate how many screenshots we're going to take */
	stream_length = app->duration;

	/* As a default, we have one screenshot per minute of stream,
	 * but adjusted so we don't have any gaps in the resulting gallery. */
	if (gallery == 0) {
		gallery = stream_length / 60000;

		while (gallery % 3 != 0 &&
		       gallery % 4 != 0 &&
		       gallery % 5 != 0) {
			gallery++;
		}
	}

	if (gallery < GALLERY_MIN)
		gallery = GALLERY_MIN;
	if (gallery > GALLERY_MAX)
		gallery = GALLERY_MAX;
	screenshot_interval = stream_length / gallery;

	/* Put a lower bound on the screenshot interval so we can't enter an infinite loop below */
	if (screenshot_interval == 0)
		screenshot_interval = 1;

	g_debug ("Producing gallery of %u screenshots, taken at %" G_GINT64_FORMAT " millisecond intervals throughout a %" G_GINT64_FORMAT " millisecond-long stream.",
			gallery, screenshot_interval, stream_length);

	/* Calculate how to arrange the screenshots so we don't get ones orphaned on the last row.
	 * At this point, only deal with arrangements of 3, 4 or 5 columns. */
	y = G_MAXUINT;
	for (x = 3; x <= 5; x++) {
		if (gallery % x == 0 || x - gallery % x < y) {
			y = x - gallery % x;
			columns = x;

			/* Have we found an optimal solution already? */
			if (y == x)
				break;
		}
	}

	rows = ceil ((gfloat) gallery / (gfloat) columns);

	g_debug ("Outputting as %u rows and %u columns.", rows, columns);

	/* Take the screenshots and composite them into a pixbuf */
	current_column = current_row = x = y = 0;
	for (pos = screenshot_interval; pos <= stream_length; pos += screenshot_interval) {
		if (pos == stream_length)
			screenshot = capture_frame_at_time (app, pos - 1);
		else
			screenshot = capture_frame_at_time (app, pos);

		if (pixbuf == NULL) {
			screenshot_width = gdk_pixbuf_get_width (screenshot);
			screenshot_height = gdk_pixbuf_get_height (screenshot);

			/* Calculate a scaling factor so that screenshot_width -> output_size */
			scale = (float) output_size / (float) screenshot_width;

			x_padding = x = MAX (output_size * 0.05, 1);
			y_padding = y = MAX (scale * screenshot_height * 0.05, 1);

			g_debug ("Scaling each screenshot by %f.", scale);

			/* Create our massive pixbuf */
			pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
						 columns * output_size + (columns + 1) * x_padding,
						 (guint) (rows * scale * screenshot_height + (rows + 1) * y_padding));
			gdk_pixbuf_fill (pixbuf, 0x000000ff);

			g_debug ("Created output pixbuf (%ux%u).", gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf));
		}

		/* Composite the screenshot into our gallery */
		gdk_pixbuf_composite (screenshot, pixbuf,
				      x, y, output_size, scale * screenshot_height,
				      (gdouble) x, (gdouble) y, scale, scale,
				      GDK_INTERP_BILINEAR, 255);
		g_object_unref (screenshot);

		g_debug ("Composited screenshot from %" G_GINT64_FORMAT " milliseconds (address %u) at (%u,%u).",
				pos, GPOINTER_TO_UINT (screenshot), x, y);

		/* We print progress in the range 10% (MIN_PROGRESS) to 50% (MAX_PROGRESS - MIN_PROGRESS) / 2.0 */
		PRINT_PROGRESS (MIN_PROGRESS + (current_row * columns + current_column) * (((MAX_PROGRESS - MIN_PROGRESS) / gallery) / 2.0));

		current_column = (current_column + 1) % columns;
		x += output_size + x_padding;
		if (current_column == 0) {
			x = x_padding;
			y += scale * screenshot_height + y_padding;
			current_row++;
		}
	}

	g_debug ("Converting pixbuf to a Cairo surface.");

	/* Load the pixbuf into a Cairo surface and overlay the text. The height is the height of
	 * the gallery plus the necessary height for 3 lines of header (at ~18px each), plus some
	 * extra padding. */
	surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, gdk_pixbuf_get_width (pixbuf),
					      gdk_pixbuf_get_height (pixbuf) + GALLERY_HEADER_HEIGHT + y_padding);
	cr = cairo_create (surface);
	cairo_surface_destroy (surface);

	/* First, copy across the gallery pixbuf */
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0.0, GALLERY_HEADER_HEIGHT + y_padding);
	cairo_rectangle (cr, 0.0, GALLERY_HEADER_HEIGHT + y_padding, gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf));
	cairo_fill (cr);
	g_object_unref (pixbuf);

	/* Build the header information */
	duration_text = totem_time_to_string (stream_length, TOTEM_TIME_FLAG_NONE);
	file = g_file_new_for_commandline_arg (app->input);
	filename = g_file_get_basename (file);
	g_object_unref (file);

	/* Translators: The first string is "Filename" (as translated); the second is an actual filename.
			The third string is "Resolution" (as translated); the fourth and fifth are screenshot height and width, respectively.
			The sixth string is "Duration" (as translated); the seventh is the movie duration in words. */
	header_text = g_markup_printf_escaped (_("<b>%s</b>: %s\n<b>%s</b>: %d\303\227%d\n<b>%s</b>: %s"),
					       _("Filename"),
					       filename,
					       _("Resolution"),
					       screenshot_width,
					       screenshot_height,
					       _("Duration"),
					       duration_text);
	g_free (duration_text);
	g_free (filename);

	g_debug ("Writing header text with Pango.");

	/* Write out some header information */
	layout = pango_cairo_create_layout (cr);
	font_desc = pango_font_description_from_string ("Sans 18px");
	pango_layout_set_font_description (layout, font_desc);
	pango_font_description_free (font_desc);

	pango_layout_set_markup (layout, header_text, -1);
	g_free (header_text);

	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0); /* white */
	cairo_move_to (cr, (gdouble) x_padding, (gdouble) y_padding);
	pango_cairo_show_layout (cr, layout);

	/* Go through each screenshot and write its timestamp */
	current_column = current_row = 0;
	x = x_padding + output_size;
	y = y_padding * 2 + GALLERY_HEADER_HEIGHT + scale * screenshot_height;

	font_desc = pango_font_description_from_string ("Sans 10px");
	pango_layout_set_font_description (layout, font_desc);
	pango_font_description_free (font_desc);

	g_debug ("Writing screenshot timestamps with Pango.");

	for (pos = screenshot_interval; pos <= stream_length; pos += screenshot_interval) {
		gchar *timestamp_text;
		gint layout_width, layout_height;

		timestamp_text = totem_time_to_string (pos, TOTEM_TIME_FLAG_NONE);

		pango_layout_set_text (layout, timestamp_text, -1);
		pango_layout_get_pixel_size (layout, &layout_width, &layout_height);

		/* Display the timestamp in the bottom-right corner of the current screenshot */
		cairo_move_to (cr, x - layout_width - 0.02 * output_size, y - layout_height - 0.02 * scale * screenshot_height);

		/* We have to stroke the text so it's visible against screenshots of the same
		 * foreground color. */
		pango_cairo_layout_path (cr, layout);
		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); /* black */
		cairo_stroke_preserve (cr);
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0); /* white */
		cairo_fill (cr);

		g_debug ("Writing timestamp \"%s\" at (%f,%f).", timestamp_text,
				x - layout_width - 0.02 * output_size,
				y - layout_height - 0.02 * scale * screenshot_height);

		/* We print progress in the range 50% (MAX_PROGRESS - MIN_PROGRESS) / 2.0) to 90% (MAX_PROGRESS) */
		PRINT_PROGRESS (MIN_PROGRESS + (MAX_PROGRESS - MIN_PROGRESS) / 2.0 + (current_row * columns + current_column) * (((MAX_PROGRESS - MIN_PROGRESS) / gallery) / 2.0));

		g_free (timestamp_text);

		current_column = (current_column + 1) % columns;
		x += output_size + x_padding;
		if (current_column == 0) {
			x = x_padding + output_size;
			y += scale * screenshot_height + y_padding;
			current_row++;
		}
	}

	g_object_unref (layout);

	g_debug ("Converting Cairo surface back to pixbuf.");

	/* Create a new pixbuf from the Cairo context */
	pixbuf = cairo_surface_to_pixbuf (cairo_get_target (cr));
	cairo_destroy (cr);

	return pixbuf;
}

static const GOptionEntry entries[] = {
	{ "size", 's', 0, G_OPTION_ARG_INT, &output_size, "Size of the thumbnail in pixels (with --gallery sets the size of individual screenshots)", NULL },
	{ "raw", 'r', 0, G_OPTION_ARG_NONE, &raw_output, "Output the raw picture of the video without scaling or adding borders", NULL },
	{ "time", 't', 0, G_OPTION_ARG_INT64, &second_index, "Choose this time (in seconds) as the thumbnail (can't be used with --gallery)", NULL },
	{ "gallery", 'g', 0, G_OPTION_ARG_INT, &gallery, "Output a gallery of the given number (0 is default) of screenshots (can't be used with --time)", NULL },
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

	context = g_option_context_new ("Thumbnail movies");
	options = gst_init_get_option_group ();
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, options);

	if (g_option_context_parse (context, &argc, &argv, &err) == FALSE) {
		g_print ("couldn't parse command-line options: %s\n", err->message);
		g_error_free (err);
		return 1;
	}

	fcntl (fileno (stdout), F_SETFL, O_NONBLOCK);
	setbuf (stdout, NULL);

	if (raw_output == FALSE && output_size == -1)
		output_size = DEFAULT_OUTPUT_SIZE;

	if (filenames == NULL || g_strv_length (filenames) != 2 ||
	    (second_index != -1 && gallery != -1)) {
		char *help;
		help = g_option_context_get_help (context, FALSE, NULL);
		g_print ("%s", help);
		g_free (help);
		return 1;
	}
	input = filenames[0];
	output = filenames[1];

	g_debug("Initialised libraries, about to create video widget");
	PRINT_PROGRESS (2.0);

	app.input = input;
	app.output = output;

	thumb_app_setup_play (&app);
	thumb_app_set_filename (&app);

	g_debug("Video widget created");
	PRINT_PROGRESS (6.0);

	g_debug("About to open video file");

	if (thumb_app_start (&app) == FALSE) {
		g_print ("totem-video-thumbnailer couldn't open file '%s'\n", input);
		exit (1);
	}
	thumb_app_set_error_handler (&app);

	if (thumb_app_get_has_video (&app) == FALSE) {
		g_debug ("totem-video-thumbnailer couldn't find a video track in '%s'\n", input);
		exit (1);
	}
	thumb_app_set_duration (&app);

	g_debug("Opened video file: '%s'", input);
	PRINT_PROGRESS (10.0);

	assert_duration (&app);
	/* We're producing a gallery of screenshots from throughout the file */
	pixbuf = create_gallery (&app);

	/* Cleanup */
	thumb_app_cleanup (&app);
	PRINT_PROGRESS (92.0);

	if (pixbuf == NULL) {
		g_print ("totem-video-thumbnailer couldn't get a picture from '%s'\n", input);
		exit (1);
	}

	g_debug("Saving captured screenshot to %s", output);
	save_pixbuf (pixbuf, output, input, output_size, FALSE);
	g_object_unref (pixbuf);
	PRINT_PROGRESS (100.0);

	return 0;
}

