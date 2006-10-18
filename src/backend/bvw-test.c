
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "bacon-video-widget.h"
#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#endif

GtkWidget *win;
GtkWidget *xine;
char *mrl, *argument;

static void test_xine_set_mrl(char *path)
{
	mrl=g_strdup(path);
	bacon_video_widget_open (BACON_VIDEO_WIDGET (xine), mrl, NULL);
}

static void on_eos_event(GtkWidget *widget, gpointer user_data)
{
	bacon_video_widget_stop (BACON_VIDEO_WIDGET (xine));
	bacon_video_widget_close(BACON_VIDEO_WIDGET (xine));
	g_free(mrl);

	test_xine_set_mrl(argument);

	bacon_video_widget_play (BACON_VIDEO_WIDGET (xine), NULL);
}

int main(int argc, char *argv[])
{
	guint32 height = 500;
	guint32 width = 500;

	if (argc > 2) {
		g_warning ("Usage: %s <file>", argv[0]);
		return 1;
	}

#ifdef GDK_WINDOWING_X11
	XInitThreads();
#endif
	g_thread_init (NULL);
	gtk_init (&argc, &argv);
	bacon_video_widget_init_backend (NULL, NULL);
	gdk_threads_init ();

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (win), width, height);
	g_signal_connect (G_OBJECT (win), "destroy",
			G_CALLBACK (gtk_main_quit), NULL);

	xine = bacon_video_widget_new (width, height,
			BVW_USE_TYPE_VIDEO, NULL);

	gtk_container_add(GTK_CONTAINER(win),xine);

	gtk_widget_realize (GTK_WIDGET(win));
	gtk_widget_realize (xine);

	g_signal_connect(G_OBJECT (xine),"eos",G_CALLBACK (on_eos_event),NULL);

	gtk_widget_show(win);
	gtk_widget_show(xine);

	mrl = NULL;
	test_xine_set_mrl(argv[1] ? argv[1] : LOGO_PATH);
	argument = g_strdup (argv[1] ? argv[1] : LOGO_PATH);
	bacon_video_widget_play (BACON_VIDEO_WIDGET (xine), NULL);
	gtk_main();

	return 0;
}

