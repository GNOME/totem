
#include "config.h"

#include <gnome.h>
#include "bacon-video-widget.h"

static void
print_usage (void)
{
	g_print ("usage: totem-video-thumbnailer <infile> <outfile>\n");
	exit (1);
}

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

static void
save_pixbuf (GdkPixbuf *pixbuf, const char *path, const char *video_path)
{
	GdkPixbuf *small;
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
	gdk_pixbuf_unref (pixbuf);

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
		exit (0);
	}

	gdk_pixbuf_unref (small);
}

int main (int argc, char *argv[])
{
	GError *err = NULL;
	BaconVideoWidget *bvw;
	GdkPixbuf *pixbuf;
	int i;

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
		g_print ("totem-video-thumbnailer couln't play file: '%s'\n",
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

	/* For debug only */
	//show_pixbuf (pixbuf);

	save_pixbuf (pixbuf, argv[2], argv[1]);

	return 0;
}

