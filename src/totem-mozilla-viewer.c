
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <glade/glade.h>

#include "bacon-video-widget.h"
#include "totem-interface.h"
//FIXME damn build system!
#include "totem-interface.c"

typedef struct TotemEmbedded TotemEmbedded;

struct TotemEmbedded {
	GtkWidget *window;
	GladeXML *xml;
	int xid, width, height;
	gboolean use_xembed;
	char *filename, *orig_filename;
	BaconVideoWidget *bvw;
	gboolean embedded_done;
};

static void
totem_embedded_exit (TotemEmbedded *emb)
{
	//FIXME what happens when embedded, and we can't go on?
	exit (1);
}

static void
totem_embedded_error_and_exit (char *title, char *reason, TotemEmbedded *emb)
{
	totem_interface_error_blocking (title, reason,
			GTK_WINDOW (emb->window));
	totem_embedded_exit (emb);
}

static void
totem_embedded_open (TotemEmbedded *emb)
{
	GError *err = NULL;
	gboolean retval;

	g_message ("totem_embedded_open '%s'", emb->filename);

	retval = bacon_video_widget_open (emb->bvw, emb->filename, &err);
	if (retval == FALSE)
	{
		char *msg, *disp;

		//FIXME if emb->filename is fd://0 or stdin:///
		//we should use a better name than that
		disp = g_strdup (emb->filename);
		//disp = gnome_vfs_unescape_string_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
		g_free (disp);
		if (err != NULL)
			g_message ("error: %s", err->message);
		totem_embedded_error_and_exit (msg, err->message, emb);
		g_free (msg);

		g_error_free (err);
		retval = FALSE;
	}

	return;
//	return retval;
}

static void
on_got_redirect (GtkWidget *bvw, const char *mrl, TotemEmbedded *emb)
{
	char *new_mrl;

	g_message ("url: %s", emb->orig_filename);
	g_message ("redirect: %s", mrl);

	//FIXME write a proper one...
	if (mrl[0] != '/') {
		char *dir;

		dir = g_path_get_dirname (emb->orig_filename);
		new_mrl = g_strdup_printf ("%s/%s", dir, mrl);
		g_free (dir);
	} else {
		new_mrl = g_strdup (mrl);
	}

	g_free (emb->filename);
	emb->filename = new_mrl;
	bacon_video_widget_close (emb->bvw);

	totem_embedded_open (emb);
	bacon_video_widget_play (emb->bvw, NULL);
}

static void
totem_embedded_add_children (TotemEmbedded *emb)
{
	GtkWidget *child, *container;
	GError *err = NULL;

	emb->xml = totem_interface_load_with_root ("mozilla-viewer.glade",
			"vbox1", _("Plugin"), TRUE,
			GTK_WINDOW (emb->window));

	if (emb->xml == NULL)
	{
		totem_embedded_exit (emb);
	}

	child = glade_xml_get_widget (emb->xml, "vbox1");
	gtk_container_add (GTK_CONTAINER (emb->window), child);

	emb->bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new
			(-1, -1, FALSE, &err));

	if (emb->bvw == NULL)
	{
		totem_embedded_error_and_exit (_("The Totem plugin could not startup."), err != NULL ? err->message : _("No reason."), emb);
		if (err != NULL)
			g_error_free (err);
	}

	g_signal_connect (G_OBJECT(emb->bvw),
			"got-redirect",
			G_CALLBACK (on_got_redirect),
			emb);

	container = glade_xml_get_widget (emb->xml, "hbox4");
	gtk_container_add (GTK_CONTAINER (container), GTK_WIDGET (emb->bvw));
	gtk_widget_show (GTK_WIDGET (emb->bvw));

	gtk_widget_realize (emb->window);
	gtk_widget_set_size_request (emb->window, emb->width, emb->height);
}

static void embedded (GtkPlug *plug, TotemEmbedded *emb)
{
	printf("EMBEDDED!\n");
	emb->embedded_done = TRUE;
//	gtk_widget_show_all((GtkWidget *)data);
}

int main (int argc, char **argv)
{
	TotemEmbedded *emb;
	int i;

	emb = g_new0 (TotemEmbedded, 1);
	emb->use_xembed = TRUE;
	emb->width = emb->height = -1;

	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		totem_embedded_error_and_exit (_("Could not initialise the thread-safe libraries."), _("Verify your system installation. The Totem plugin will now exit."), NULL);
	}

	g_thread_init (NULL);
	gdk_threads_init ();

	gtk_init (&argc, &argv);

	g_print ("CMD line: ");
	for (i = 1; i < argc; i++) {
		g_print ("%s ", argv[i]);
	}
	g_print ("\n");

	/* TODO Add popt options */
	for (i = 1; i < argc; i++) {
		if (strcmp (argv[i], "--xid") == 0) {
			if (i + 1 < argc) {
				i++;
				emb->xid = atoi (argv[i]);
			}
		} else if (strcmp (argv[i], "--width") == 0) {
			if (i + 1 < argc) {
				i++;
				emb->width = atoi (argv[i]);
			}
		} else if (strcmp (argv[i], "--height") == 0) {
			if (i + 1 < argc) {
				i++;
				emb->height = atoi (argv[i]);
			}
		} else if (strcmp (argv[i], "--url") == 0) {
			if (i + 1 < argc) {
				i++;
				emb->orig_filename = argv[i];
			}
		} else if (i + 1 == argc) {
			emb->filename = g_strdup (argv[i]);
		}
	}

	/* XEMBED or stand-alone */
	if (emb->xid != 0) {
		GtkWidget *window;

		/* The miraculous XEMBED protocol */
		window = gtk_plug_new ((GdkNativeWindow)emb->xid);
		gtk_signal_connect(GTK_OBJECT(window), "embedded",
				G_CALLBACK (embedded), NULL);
		gtk_widget_realize (window);

		emb->window = window;
	} else {
		/* Stand-alone version */
		emb->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	}

	totem_embedded_add_children (emb);

	gtk_widget_show (emb->window);

	/* wait until we're embedded if we're to be */
	if (emb->xid != 0)
		while (emb->embedded_done == FALSE && gtk_events_pending ())
			gtk_main_iteration ();

	totem_embedded_open (emb);
	bacon_video_widget_play (emb->bvw, NULL);

	gtk_main ();

	return 0;
}

