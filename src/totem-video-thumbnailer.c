
#include "config.h"

#include <gnome.h>
#include <gconf/gconf-client.h>
#include "gtk-xine.h"

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

	if (gdk_pixbuf_save (small, path, "png", NULL, NULL) == FALSE)
	{
		g_print (_("totem-video-thumbnailer couln't write the thumbnail '%s' for video '%s'\n"),
				path, video_path);
		gdk_pixbuf_unref (small);
		exit (1);
	}

	gdk_pixbuf_unref (small);
}

int main (int argc, char *argv[])
{
	GError *err = NULL;
	GtkWidget *gtx, *toplevel;
	GdkPixbuf *pixbuf;

	gnome_program_init ("totem-video-thumbnailer", VERSION,
			LIBGNOMEUI_MODULE,
			argc, argv,
			GNOME_PARAM_APP_DATADIR, DATADIR,
			NULL);

	if (argc != 3)
		print_usage ();

	gconf_init (argc, argv, &err);
	if (err != NULL)
	{
		g_print (_("totem-video-thumbnailer couln't initialise the "
				"configuration engine:\n%s"), err->message);
		exit (1);
	}

	toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtx = gtk_xine_new (-1, -1, TRUE);
	gtk_container_add (GTK_CONTAINER (toplevel), gtx);
	gtk_widget_realize (gtx);

	while (gtk_xine_check (GTK_XINE (gtx)) == FALSE)
		usleep (100000);

	if (gtk_xine_open (GTK_XINE (gtx), argv[1]) == FALSE)
	{
		g_print (_("totem-video-thumbnailer couln't open file '%s'\n"),
					argv[1]);
		exit (1);
	}

	gtk_xine_play (GTK_XINE (gtx), 21845, 0);
	gtk_xine_set_speed (GTK_XINE (gtx), SPEED_PAUSE);

	pixbuf = gtk_xine_get_current_frame (GTK_XINE (gtx));
	gdk_pixbuf_ref (pixbuf);

	/* Cleanup */
	gtk_widget_unrealize (gtx);
	gtk_widget_destroy (gtx);
	gtk_widget_destroy (toplevel);

	if (pixbuf == NULL)
	{
		g_print (_("totem-video-thumbnailer couln't get a picture from "
					"'%s'\n"), argv[1]);
		exit (1);
	}

	/* For debug only */
	//show_pixbuf (pixbuf);

	save_pixbuf (pixbuf, argv[2], argv[1]);

	return 0;
}


