/* 
 * Copyright (C) 2003,2004 Bastien Nocera <hadess@hadess.net>
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
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include "config.h"

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef HAVE_GTK_ONLY
#include <libgnomeui/gnome-authentication-manager.h>
#endif
#include <libgnomevfs/gnome-vfs-init.h>
#include "bacon-video-widget.h"
#include "totem-resources.h"

/* #define THUMB_DEBUG */

#ifdef G_HAVE_ISO_VARARGS
#define PROGRESS_DEBUG(...) { if (verbose != FALSE) g_message (__VA_ARGS__); }
#elif defined(G_HAVE_GNUC_VARARGS)
#define PROGRESS_DEBUG(format...) { if (verbose != FALSE) g_message (format); }
#endif

#define BORING_IMAGE_VARIANCE 256.0		/* Tweak this if necessary */

static gboolean jpeg_output = FALSE;
static gboolean output_size = 128;
static gboolean time_limit = TRUE;
static gboolean verbose = FALSE;
static gboolean g_fatal_warnings = FALSE;
static gint64 second_index = -1;
static char **filenames = NULL;

#ifdef THUMB_DEBUG
static void
show_pixbuf (GdkPixbuf *pix)
{
	GtkWidget *win, *img;

	img = gtk_image_new_from_pixbuf (pix);
	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_container_add (GTK_CONTAINER (win), img);
	gtk_widget_show_all (win);

	/* Display and crash baby crash */
	gtk_main ();
}
#endif

static GdkPixbuf *
add_holes_to_pixbuf_small (GdkPixbuf *pixbuf, int width, int height)
{
	GdkPixbuf *holes, *tmp, *target;
	char *filename;
	int i;

	filename = g_build_filename (DATADIR, "totem", "filmholes.png", NULL);
	holes = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	if (holes == NULL) {
		g_object_ref (pixbuf);
		return pixbuf;
	}

	g_assert (gdk_pixbuf_get_has_alpha (pixbuf) == FALSE);
	g_assert (gdk_pixbuf_get_has_alpha (holes) != FALSE);
	target = g_object_ref (pixbuf);

	for (i = 0; i < height; i += gdk_pixbuf_get_height (holes))
	{
		gdk_pixbuf_composite (holes, target, 0, i,
				      MIN (width, gdk_pixbuf_get_width (holes)),
				      MIN (height - i, gdk_pixbuf_get_height (holes)),
				      0, i, 1, 1, GDK_INTERP_NEAREST, 255);
	}

	tmp = gdk_pixbuf_flip (holes, FALSE);
	g_object_unref (holes);
	holes = tmp;

	for (i = 0; i < height; i += gdk_pixbuf_get_height (holes))
	{
		gdk_pixbuf_composite (holes, target,
				      width - gdk_pixbuf_get_width (holes), i,
				      MIN (width, gdk_pixbuf_get_width (holes)),
				      MIN (height - i, gdk_pixbuf_get_height (holes)),
				      width - gdk_pixbuf_get_width (holes), i,
				      1, 1, GDK_INTERP_NEAREST, 255);
	}

	g_object_unref (holes);

	return target;
}

static GdkPixbuf *
add_holes_to_pixbuf_large (GdkPixbuf *pixbuf, int size)
{
	char *filename;
	int lh, lw, rh, rw, i;
	GdkPixbuf *left, *right, *small;
	int canvas_w, canvas_h;
	int d_height, d_width;
	double ratio;

	filename = g_build_filename (DATADIR, "totem",
			"filmholes-big-left.png", NULL);
	left = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	if (left == NULL)
	{
		g_object_ref (pixbuf);
		return pixbuf;
	}

	filename = g_build_filename (DATADIR, "totem",
			"filmholes-big-right.png", NULL);
	right = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	if (right == NULL)
	{
		g_object_unref (left);
		g_object_ref (pixbuf);
		return pixbuf;
	}

	lh = gdk_pixbuf_get_height (left);
	lw = gdk_pixbuf_get_width (left);
	rh = gdk_pixbuf_get_height (right);
	rw = gdk_pixbuf_get_width (right);
	g_assert (lh == rh);
	g_assert (lw == rw);

	{
		int height, width;

		height = gdk_pixbuf_get_height (pixbuf);
		width = gdk_pixbuf_get_width (pixbuf);

		if (width > height)
		{
			d_width = size - lw - lw;
			d_height = d_width * height / width;
		} else {
			d_height = size - lw -lw;
			d_width = d_height * width / height;
		}

		canvas_h = d_height;
		canvas_w = d_width + 2 * lw;
	}

	small = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
			canvas_w, canvas_h);
	gdk_pixbuf_fill (small, 0x000000ff);
	ratio = ((double)d_width / (double) gdk_pixbuf_get_width (pixbuf));

	gdk_pixbuf_scale (pixbuf, small, lw, 0,
			d_width, d_height,
			lw, 0, ratio, ratio, GDK_INTERP_NEAREST);

	/* Left side holes */
	for (i = 0; i < canvas_h; i += lh)
	{
		gdk_pixbuf_composite (left, small, 0, i,
				MIN (canvas_w, lw),
				MIN (canvas_h - i, lh),
				0, i, 1, 1, GDK_INTERP_NEAREST, 255);
	}

	/* Right side holes */
	for (i = 0; i < canvas_h; i += rh)
	{
		gdk_pixbuf_composite (right, small,
				canvas_w - rw, i,
				MIN (canvas_w, rw),
				MIN (canvas_h - i, rh),
				canvas_w - rw, i,
				1, 1, GDK_INTERP_NEAREST, 255);
	}

	/* TODO Add a one pixel border of 0x33333300 all around */

	return small;
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

static void
save_pixbuf (GdkPixbuf *pixbuf, const char *path,
	     const char *video_path, int size, gboolean is_still)
{
	int width, height;
	GdkPixbuf *small, *with_holes;
	GError *err = NULL;
	char *a_width, *a_height;

	height = gdk_pixbuf_get_height (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);

	if (size <= 256)
	{
		int d_width, d_height;

		height = gdk_pixbuf_get_height (pixbuf);
		width = gdk_pixbuf_get_width (pixbuf);

		if (width > height)
		{
			d_width = size;
			d_height = size * height / width;
		} else {
			d_height = size;
			d_width = size * width / height;
		}

		small = gdk_pixbuf_scale_simple (pixbuf, d_width, d_height,
				GDK_INTERP_TILES);

		if (is_still == FALSE) {
			with_holes = add_holes_to_pixbuf_small (small,
					d_width, d_height);
			g_return_if_fail (with_holes != NULL);
			g_object_unref (small);
		} else {
			with_holes = small;
		}
	} else {
		with_holes = add_holes_to_pixbuf_large (pixbuf, size);
		g_return_if_fail (with_holes != NULL);
	}

	a_width = g_strdup_printf ("%d", width);
	a_height = g_strdup_printf ("%d", height);

	if (gdk_pixbuf_save (with_holes, path,
				jpeg_output ? "jpeg" : "png", &err,
				"tEXt::Thumb::Image::Width", a_width,
				"tEXt::Thumb::Image::Height", a_height,
				NULL) == FALSE)
	{
		g_free (a_width);
		g_free (a_height);

		if (err != NULL)
		{
			g_print ("totem-video-thumbnailer couln't write the thumbnail '%s' for video '%s': %s\n", path, video_path, err->message);
			g_error_free (err);
		} else {
			g_print ("totem-video-thumbnailer couln't write the thumbnail '%s' for video '%s'\n", path, video_path);
		}

		g_object_unref (with_holes);
		return;
	}

#ifdef THUMB_DEBUG
	show_pixbuf (with_holes);
#endif

	g_object_unref (with_holes);
}

static GdkPixbuf *
capture_interesting_frame (BaconVideoWidget * bvw, char *input, char *output) 
{
	GdkPixbuf* pixbuf;
	guint current;
	GError *err = NULL;
	const float frame_locations[] = {
		1.0 / 3.0,
		2.0 / 3.0,
		0.1,
		0.9,
		0.5
	};

	/* Test at multiple points in the file to see if we can get an 
	 * interesting frame */
	for (current = 0; current < G_N_ELEMENTS(frame_locations); current++)
	{
		PROGRESS_DEBUG("About to seek to %f\n", frame_locations[current]);
		if (bacon_video_widget_seek (bvw, frame_locations[current], NULL) == FALSE) {
			bacon_video_widget_play (bvw, NULL);
		}

		if (bacon_video_widget_can_get_frames (bvw, &err) == FALSE)
		{
			g_print ("totem-video-thumbnailer: '%s' isn't thumbnailable\n"
				 "Reason: %s\n",
				 input, err ? err->message : "programming error");
			bacon_video_widget_close (bvw);
			gtk_widget_destroy (GTK_WIDGET (bvw));
			g_error_free (err);

			exit (1);
		}

		/* Pull the frame, if it's interesting we bail early */
		PROGRESS_DEBUG("About to get frame for iter %d\n", current);
		pixbuf = bacon_video_widget_get_current_frame (bvw);
		if (pixbuf != NULL && is_image_interesting (pixbuf) != FALSE) {
			PROGRESS_DEBUG("Frame for iter %d is interesting\n", current);
			break;
		}

		/* If we get to the end of this loop, we'll end up using
		 * the last image we pulled */
		if (current + 1 < G_N_ELEMENTS(frame_locations)) {
			if (pixbuf != NULL) {
				g_object_unref (pixbuf);
				pixbuf = NULL;
			}
		}
		PROGRESS_DEBUG("Frame for iter %d was not interesting\n", current);
	}
	return pixbuf;
}

static GdkPixbuf *
capture_frame_at_time(BaconVideoWidget *bvw, char *input, char *output,  gint64 seconds) 
{
	GError *err = NULL;

	if (bacon_video_widget_seek_time (bvw, seconds * 1000, &err) == FALSE) {
		g_print ("totem-video-thumbnailer: could not seek to %d seconds in '%s'\n"
			 "Reason: %s\n",
			 (int) seconds, input, err ? err->message : "programming error");
		bacon_video_widget_close (bvw);
		gtk_widget_destroy (GTK_WIDGET (bvw));
		g_error_free (err);

		exit (1);
	}
	if (bacon_video_widget_can_get_frames (bvw, &err) == FALSE)
	{
		g_print ("totem-video-thumbnailer: '%s' isn't thumbnailable\n"
			 "Reason: %s\n",
			 input, err ? err->message : "programming error");
		bacon_video_widget_close (bvw);
		gtk_widget_destroy (GTK_WIDGET (bvw));
		g_error_free (err);

		exit (1);
	}

	return bacon_video_widget_get_current_frame (bvw);
}

static const GOptionEntry entries[] = {
	{ "jpeg", 'j',  0, G_OPTION_ARG_NONE, &jpeg_output, "Output the thumbnail as a JPEG instead of PNG", NULL },
	{ "size", 's', 0, G_OPTION_ARG_INT, &output_size, "Size of the thumbnail in pixels", NULL },
	{ "no-limit", 'l', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &time_limit, "Don't limit the thumbnailing time to 30 seconds", NULL },
	{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Output debug information", NULL },
	{ "time", 't', 0, G_OPTION_ARG_INT64, &second_index, "Choose this time (in seconds) as the thumbnail", NULL },
#ifndef THUMB_DEBUG
	{"g-fatal-warnings", 0, 0, G_OPTION_ARG_NONE, &g_fatal_warnings, "Make all warnings fatal", NULL},
#endif /* THUMB_DEBUG */
 	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, "[FILE...]" },
	{ NULL }
};

int main (int argc, char *argv[])
{
	GOptionGroup *options;
	GOptionContext *context;
	GError *err = NULL;
	BaconVideoWidget *bvw;
	GdkPixbuf *pixbuf;
	char *input, *output;

#ifdef G_OS_UNIX
	nice (20);
#endif

	g_thread_init (NULL);

	context = g_option_context_new ("Thumbnail movies");
	options = bacon_video_widget_get_option_group ();
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, options);
#ifndef THUMB_DEBUG
	g_type_init ();
#else
 	g_option_context_add_group (context, gtk_get_option_group (FALSE));
#endif

	if (g_option_context_parse (context, &argc, &argv, &err) == FALSE) {
		g_print ("couldn't parse command-line options: %s\n", err->message);
		g_error_free (err);
		return 1;
	}

#ifndef THUMB_DEBUG
	if (g_fatal_warnings) {
		GLogLevelFlags fatal_mask;

		fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
		fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
		g_log_set_always_fatal (fatal_mask);
	}
#endif /* THUMB_DEBUG */

	gnome_vfs_init ();

#ifndef HAVE_GTK_ONLY
	gnome_authentication_manager_init ();
#endif

	if (filenames == NULL || g_strv_length (filenames) != 2) {
		g_print ("Expects an input and an output file\n");
		return 1;
	}
	input = filenames[0];
	output = filenames[1];

	PROGRESS_DEBUG("Initialised libraries, about to create video widget\n");

	bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new (-1, -1, BVW_USE_TYPE_CAPTURE, &err));
	if (err != NULL)
	{
		g_print ("totem-video-thumbnailer couln't create the video "
				"widget.\nReason: %s.\n", err->message);
		g_error_free (err);
		exit (1);
	}

	PROGRESS_DEBUG("Video widget created\n");

	if (time_limit != FALSE)
		totem_resources_monitor_start (input, 0);

	PROGRESS_DEBUG("About to open video file\n");

	if (bacon_video_widget_open (bvw, input, &err) == FALSE)
	{
		g_print ("totem-video-thumbnailer couln't open file '%s'\n"
				"Reason: %s.\n",
				input, err->message);
		g_error_free (err);
		exit (1);
	}

	PROGRESS_DEBUG("Opened video file: '%s'\n", input);
	PROGRESS_DEBUG("About to play file\n");

	bacon_video_widget_play (bvw, &err);
	if (err != NULL)
	{
		g_print ("totem-video-thumbnailer couln't play file: '%s'\n"
				"Reason: %s.\n", input, err->message);
		g_error_free (err);
		exit (1);
	}

	PROGRESS_DEBUG("Started playing file\n");

	/* If the user has told us to use a frame at a specific second 
	 * into the video, just use that frame no matter how boring it
	 * is */
	if(-1 != second_index) {
	  pixbuf = capture_frame_at_time(bvw, input, output, second_index);
	} else {
	  pixbuf = capture_interesting_frame(bvw, input, output);
	}

	/* Cleanup */
	bacon_video_widget_close (bvw);
	totem_resources_monitor_stop ();
	gtk_widget_destroy (GTK_WIDGET (bvw));

	if (pixbuf == NULL)
	{
		g_print ("totem-video-thumbnailer couln't get a picture from "
					"'%s'\n", input);
		exit (1);
	}

	PROGRESS_DEBUG("Saving captured screenshot\n");
	save_pixbuf (pixbuf, output, input, output_size, FALSE);
	g_object_unref (pixbuf);

	return 0;
}

