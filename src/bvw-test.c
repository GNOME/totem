
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "bacon-video-widget.h"
#include <X11/Xlib.h>

GtkWidget *win;
GtkWidget *xine;
char *mrl, *argument;

void test_xine_set_mrl(char *path)
{
	mrl=g_strdup(path);
	bacon_video_widget_open (BACON_VIDEO_WIDGET (xine), mrl, NULL);
}

void on_eos_event(GtkWidget *widget, gpointer user_data)
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

	if (argc < 2) {
		g_warning ("Need at least 2 arguments");
		return 1;
	}

	XInitThreads();
	gtk_init (&argc, &argv);
	g_thread_init (NULL);
	gdk_threads_init ();

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (win), width, height);

	xine = bacon_video_widget_new (width, height, FALSE, NULL);

	gtk_container_add(GTK_CONTAINER(win),xine);

	gtk_widget_realize (GTK_WIDGET(win));
	gtk_widget_realize (xine);

	g_signal_connect(G_OBJECT (xine),"eos",G_CALLBACK (on_eos_event),NULL);

	gtk_widget_show(win);
	gtk_widget_show(xine);

	mrl = NULL;
	test_xine_set_mrl(argv[1]);
	argument = g_strdup (argv[1]);
	bacon_video_widget_play (BACON_VIDEO_WIDGET (xine), NULL);
	gtk_main();

	return 0;
}

