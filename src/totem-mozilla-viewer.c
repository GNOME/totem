
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <glade/glade.h>

#include "bacon-video-widget.h"

typedef struct TotemEmbedded TotemEmbedded;

struct TotemEmbedded {
	GtkWidget *window;
	GladeXML *xml;
	int xid, width, height;
	gboolean use_xembed;
	char *filename;
	BaconVideoWidget *bvw;
	gboolean embedded_done;
};

static void
totem_embedded_error_and_exit (char *title, char *reason, TotemEmbedded *emb)
{
	GtkWidget *error_dialog;

	error_dialog =
		gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"<b>%s</b>\n%s", title, reason);
	gtk_container_set_border_width (GTK_CONTAINER (error_dialog), 5);
	gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (error_dialog)->label), TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
			GTK_RESPONSE_OK);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);
	gtk_dialog_run (GTK_DIALOG (error_dialog));

	exit (1);
}

static void
totem_embedded_open (TotemEmbedded *emb)
{
	GError *err = NULL;
	gboolean retval;
	gboolean caps;

	retval = bacon_video_widget_open (emb->bvw, emb->filename, &err);
	if (retval == FALSE)
	{
		char *msg, *disp;

		disp = g_strdup (emb->filename);
		//disp = gnome_vfs_unescape_string_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
		g_free (disp);
		if (err != NULL)
			g_message ("error: %s", err->message);
		//totem_action_error (msg, err->message, totem);
		g_free (msg);

		g_error_free (err);
		retval = FALSE;
	}

	return;
//	return retval;
}

static void
totem_embedded_add_children (TotemEmbedded *emb)
{
	GtkWidget *child, *container;
	char *filename;
	GError *err = NULL;

	filename = g_build_filename (DATADIR,
			"totem", "mozilla-viewer.glade", NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_free (filename);
		filename = g_build_filename ("..", "data", "mozilla-viewer.glade", NULL);
	}

	if (g_file_test (filename, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_free (filename);
		totem_embedded_error_and_exit (_("Couldn't load the main interface (mozilla-viewer.glade)."), _("Make sure that the Totem plugin is properly installed."), NULL);
	}

	emb->xml = glade_xml_new (filename, "vbox1", NULL);
	if (emb->xml == NULL)
	{
		g_free (filename);
		totem_embedded_error_and_exit (_("Couldn't load the main interface (mozilla-viewer.glade)."), _("Make sure that the Totem plugin is properly installed."), NULL);
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
		} else if (i + 1 == argc) {
			emb->filename = argv[i];
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

