
#include "config.h"

#include <gnome.h>
#include <string.h>
#include "bacon-video-widget.h"

/* #define THUMB_DEBUG */

static void
print_usage (void)
{
	g_print ("usage: totem-video-thumbnailer <infile> <outfile>\n");
	exit (1);
}

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

static void
gdk_pixbuf_mirror (GdkPixbuf *pixbuf)
{
	int i, j, rowstride, offset, right;
	guchar *pixels;
	int width, height, size;
	guint32 tmp;

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	g_return_if_fail (pixels != NULL);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	size = height * width * sizeof (guint32);

	for (i = 0; i < size; i += rowstride)
	{
		for (j = 0; j < rowstride; j += sizeof(guint32))
		{
			offset = i + j;
			right = i + (((width - 1) * sizeof(guint32)) - j);

			if (right <= offset)
				break;

			memcpy (&tmp, pixels + offset, sizeof(guint32));
			memcpy (pixels + offset, pixels + right,
					sizeof(guint32));
			memcpy (pixels + right, &tmp, sizeof(guint32));
		}
	}
}

static GdkPixbuf *
add_holes_to_pixbuf (GdkPixbuf *pixbuf, int width, int height)
{
	GdkPixbuf *holes, *tmp;
	char *filename;
	int i;

	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "filmholes.png", NULL);
	holes = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	if (holes == NULL)
	{
		gdk_pixbuf_ref (pixbuf);
		return pixbuf;
	}

	g_assert (gdk_pixbuf_get_has_alpha (pixbuf) == FALSE);
	g_assert (gdk_pixbuf_get_has_alpha (holes) == TRUE);
	tmp = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

	for (i = 0; i < height; i += gdk_pixbuf_get_height (holes))
	{
		gdk_pixbuf_composite (holes, tmp, 0, i,
				MIN (width, gdk_pixbuf_get_width (holes)),
				MIN (height-i, gdk_pixbuf_get_height (holes)),
				0, i, 1, 1, GDK_INTERP_NEAREST, 255);
	}

	gdk_pixbuf_mirror (holes);

	for (i = 0; i < height; i += gdk_pixbuf_get_height (holes))
	{
		gdk_pixbuf_composite (holes, tmp,
				width - gdk_pixbuf_get_width (holes), i,
				MIN (width, gdk_pixbuf_get_width (holes)),
				MIN (height-i, gdk_pixbuf_get_height (holes)),
				width - gdk_pixbuf_get_width (holes), i,
				1, 1, GDK_INTERP_NEAREST, 255);
	}

	gdk_pixbuf_unref (holes);

	return tmp;
}

static void
save_pixbuf (GdkPixbuf *pixbuf, const char *path, const char *video_path)
{
	GdkPixbuf *small, *with_holes;
	int width, height, d_width, d_height;
	GError *err = NULL;

	height = gdk_pixbuf_get_height (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);

	if (width > height)
	{
		d_width = 128;
		d_height = 128 * height / width;
	} else {
		d_height = 128;
		d_width = 128 * width / height;
	}

	small = gdk_pixbuf_scale_simple (pixbuf, d_width, d_height,
			GDK_INTERP_TILES);

	with_holes = add_holes_to_pixbuf (small, d_width, d_height);
	g_return_if_fail (with_holes != NULL);
	gdk_pixbuf_unref (small);

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

int main (int argc, char *argv[])
{
	GError *err = NULL;
	BaconVideoWidget *bvw;
	GdkPixbuf *pixbuf;
	int i;

	g_thread_init (NULL);

	gtk_init (&argc, &argv);

	if (argc != 3)
		print_usage ();

	bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new (-1, -1, TRUE, &err));
	if (err != NULL)
	{
		g_print ("totem-video-thumbnailer couln't create the video "
				"widget.\nReason: %s.\n", err->message);
		g_error_free (err);
		exit (1);
	}

	//FIXME create a new thread that will watch over these 2 next
	//calls so that we know they won't take longer than 30 secs

	if (bacon_video_widget_open (bvw, argv[1], &err) == FALSE)
	{
		g_print ("totem-video-thumbnailer couln't open file '%s'\n"
				"Reason: %s.\n",
				argv[1], err->message);
		g_error_free (err);
		exit (1);
	}

	/* A 3rd into the file */
	bacon_video_widget_play (bvw, (int) (65535 / 3), 0, &err);
	if (err != NULL)
	{
		g_print ("totem-video-thumbnailer couln't play file: '%s'\n"
				"Reason: %s.\n",
				argv[1], err->message);
		g_error_free (err);
		exit (1);
	}

	if (bacon_video_widget_can_get_frames (bvw, &err) == FALSE)
	{
		g_print ("totem-video-thumbnailer: '%s' isn't thumbnailable\n"
				"Reason: %s\n",
				argv[1], err->message);
		bacon_video_widget_close (bvw);
		gtk_widget_destroy (GTK_WIDGET (bvw));
		g_error_free (err);

		exit (1);
	}

	/* 10 seconds! */
	i = 0;
	pixbuf = bacon_video_widget_get_current_frame (bvw);
	while (pixbuf == NULL && i < 10)
	{
		usleep (1000000);
		pixbuf = bacon_video_widget_get_current_frame (bvw);
		i++;
	}

	/* Cleanup */
	bacon_video_widget_close (bvw);
	gtk_widget_destroy (GTK_WIDGET (bvw));

	if (pixbuf == NULL)
	{
		g_print ("totem-video-thumbnailer couln't get a picture from "
					"'%s'\n", argv[1]);
		exit (1);
	}

	save_pixbuf (pixbuf, argv[2], argv[1]);
	gdk_pixbuf_unref (pixbuf);

	return 0;
}

