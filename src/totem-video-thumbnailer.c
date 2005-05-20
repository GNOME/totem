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
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 */

#include "config.h"

#ifndef HAVE_GTK_ONLY
#include <gnome.h>
#else
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#endif

#include <popt.h>

#include <string.h>
#include <stdlib.h>
#include "bacon-video-widget.h"

/* #define THUMB_DEBUG */

#define MIN_LEN_FOR_SEEK 25000
#define HALF_SECOND G_USEC_PER_SEC * .5

gboolean finished = FALSE;

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

	if (holes == NULL)
	{
		gdk_pixbuf_ref (pixbuf);
		return pixbuf;
	}

	g_assert (gdk_pixbuf_get_has_alpha (pixbuf) == FALSE);
	g_assert (gdk_pixbuf_get_has_alpha (holes) != FALSE);
	target = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

	for (i = 0; i < height; i += gdk_pixbuf_get_height (holes))
	{
		gdk_pixbuf_composite (holes, target, 0, i,
				MIN (width, gdk_pixbuf_get_width (holes)),
				MIN (height-i, gdk_pixbuf_get_height (holes)),
				0, i, 1, 1, GDK_INTERP_NEAREST, 255);
	}

	tmp = gdk_pixbuf_flip (holes, FALSE);
	gdk_pixbuf_unref (holes);
	holes = tmp;

	for (i = 0; i < height; i += gdk_pixbuf_get_height (holes))
	{
		gdk_pixbuf_composite (holes, target,
				width - gdk_pixbuf_get_width (holes), i,
				MIN (width, gdk_pixbuf_get_width (holes)),
				MIN (height-i, gdk_pixbuf_get_height (holes)),
				width - gdk_pixbuf_get_width (holes), i,
				1, 1, GDK_INTERP_NEAREST, 255);
	}

	gdk_pixbuf_unref (holes);

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
		gdk_pixbuf_ref (pixbuf);
		return pixbuf;
	}

	filename = g_build_filename (DATADIR, "totem",
			"filmholes-big-right.png", NULL);
	right = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	if (right == NULL)
	{
		gdk_pixbuf_unref (left);
		gdk_pixbuf_ref (pixbuf);
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

	small = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
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

static void
save_pixbuf (GdkPixbuf *pixbuf, const char *path,
	     const char *video_path, int size)
{
	GdkPixbuf *small, *with_holes;
	GError *err = NULL;

	if (size <= 128)
	{
		int width, height, d_width, d_height;

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

		with_holes = add_holes_to_pixbuf_small (small,
				d_width, d_height);
		g_return_if_fail (with_holes != NULL);
		gdk_pixbuf_unref (small);
	} else {
		with_holes = add_holes_to_pixbuf_large (pixbuf, size);
		g_return_if_fail (with_holes != NULL);
	}

	if (gdk_pixbuf_save (with_holes, path, "png", &err, NULL) == FALSE)
	{
		if (err != NULL)
		{
			g_print ("totem-video-thumbnailer couln't write the thumbnail '%s' for video '%s': %s\n", path, video_path, err->message);
			g_error_free (err);
		} else {
			g_print ("totem-video-thumbnailer couln't write the thumbnail '%s' for video '%s'\n", path, video_path);
		}

		gdk_pixbuf_unref (with_holes);
		return;
	}

#ifdef THUMB_DEBUG
	show_pixbuf (with_holes);
#endif

	gdk_pixbuf_unref (with_holes);
}

static void
save_still_pixbuf (GdkPixbuf *pixbuf, const char *path,
		const char *video_path, int size)
{
	GdkPixbuf *small;
	int width, height, d_width, d_height;
	GError *err = NULL;

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

	if (gdk_pixbuf_save (small, path, "png", &err, NULL) == FALSE)
	{
		if (err != NULL)
		{
			g_print ("totem-video-thumbnailer couln't write the thumbnail '%s' for video '%s': %s\n", path, video_path, err->message);
			g_error_free (err);
		} else {
			g_print ("totem-video-thumbnailer couln't write the thumbnail '%s' for video '%s'\n", path, video_path);
		}

		gdk_pixbuf_unref (small);
		return;
	}

#ifdef THUMB_DEBUG
	show_pixbuf (small);
#endif

	gdk_pixbuf_unref (small);
}

static gpointer
time_monitor (gpointer data)
{
	g_usleep (30 * G_USEC_PER_SEC);

	if (finished != FALSE)
		g_thread_exit (NULL);

	g_print ("totem-video-thumbnailer couln't thumbnail file: '%s'\n"
			"Reason: Took too much time to thumbnail.\n",
			(const char *) data);
	exit (0);
}

int main (int argc, char *argv[])
{
	GError *err = NULL;
	BaconVideoWidget *bvw;
	GdkPixbuf *pixbuf;
	int i, length, size;
	char *input, *output;

	if (argc <= 2 || strcmp (argv[1], "-h") == 0 ||
	    strcmp (argv[1], "--help") == 0) {
		g_print ("Usage: %s [-s <size>] <input> <output> [backend options]\n", argv[0]);
		return -1;
	}

	nice (20);

	g_thread_init (NULL);
#ifndef THUMB_DEBUG
	g_type_init ();
#else
	gtk_init (&argc, &argv);
#endif
	bacon_video_widget_init_backend (&argc, &argv);

	if (!strcmp (argv[1], "-s")) {
		input = argv[3];
		output = argv[4];
		size = g_strtod (argv[2], NULL);
	} else {
		input = argv[1];
		output = argv[2];
		size = 128;
	}

	bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new (-1, -1, BVW_USE_TYPE_CAPTURE, &err));
	if (err != NULL)
	{
		g_print ("totem-video-thumbnailer couln't create the video "
				"widget.\nReason: %s.\n", err->message);
		g_error_free (err);
		exit (1);
	}

	g_thread_create (time_monitor, (gpointer) input, FALSE, NULL);

	if (bacon_video_widget_open (bvw, input, &err) == FALSE)
	{
		if (err->code == BVW_ERROR_STILL_IMAGE)
		{
			g_error_free (err);
			err = NULL;
			pixbuf = gdk_pixbuf_new_from_file (input, &err);
			if (pixbuf != NULL)
			{
				save_still_pixbuf (pixbuf,
						output, input, size);
				gdk_pixbuf_unref (pixbuf);
				exit (0);
			}
			g_print ("totem-video-thumbnailer couln't open file '%s'\n"
					"Reason: %s.\n", input, err->message);
			g_error_free (err);
			exit (1);
		}

		g_print ("totem-video-thumbnailer couln't open file '%s'\n"
				"Reason: %s.\n",
				input, err->message);
		g_error_free (err);
		exit (1);
	}

	bacon_video_widget_play (bvw, &err);
	if (err != NULL)
	{
		g_print ("totem-video-thumbnailer couln't play file: '%s'\n"
				"Reason: %s.\n",
				input, err->message);
		g_error_free (err);
		exit (1);
	}

	/* A 3rd into the file - multi-times = HACK for gst backend... */
	i = 0;
	do {
		if (i != 0) {
			g_usleep (HALF_SECOND);
			g_main_iteration (FALSE);
		}
		length = bacon_video_widget_get_stream_length (bvw);
		i++;
	} while (length == 0 && i < 20);

	if (length > MIN_LEN_FOR_SEEK)
	{
		if (bacon_video_widget_seek
				(bvw, ((float) 1 / 3), NULL) == FALSE)
		{
			bacon_video_widget_play (bvw, NULL);
		}
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

	/* 10 seconds! */
	i = 0;
	do {
		/* This is a hack for the gst backend because it resyncs
		 * after a seekj asynchronously, and so a direct get_frame()
		 * would still return the old frame. */
		g_usleep (HALF_SECOND);
		pixbuf = bacon_video_widget_get_current_frame (bvw);
		i++;
	} while (pixbuf == NULL && i < 20);

	/* Cleanup */
	bacon_video_widget_close (bvw);
	finished = TRUE;
	gtk_widget_destroy (GTK_WIDGET (bvw));

	if (pixbuf == NULL)
	{
		g_print ("totem-video-thumbnailer couln't get a picture from "
					"'%s'\n", input);
		exit (1);
	}

	save_pixbuf (pixbuf, output, input, size);
	gdk_pixbuf_unref (pixbuf);

	return 0;
}

