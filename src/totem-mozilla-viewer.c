/* Totem Mozilla plugin
 *
 * Copyright (C) <2004-2005> Bastien Nocera <hadess@hadess.net>
 * Copyright (C) <2002> David A. Schleef <ds@schleef.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <glade/glade.h>

#include "bacon-video-widget.h"
#include "totem-interface.h"
#include "totem-mozilla-options.h"
//FIXME damn build system!
#include "totem-interface.c"

#define OPTION_IS(x) (strcmp(argv[i], x) == 0)

typedef struct TotemEmbedded TotemEmbedded;

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED
} TotemStates;

struct TotemEmbedded {
	GtkWidget *window;
	GladeXML *xml;
	int xid, width, height;
	const char *orig_filename;
	char *filename, *href;
	BaconVideoWidget *bvw;
	gboolean controller_hidden;

	/* XEmbed */
	gboolean use_xembed;
	gboolean embedded_done;
	TotemStates state;
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
totem_embedded_set_state (TotemEmbedded *emb, TotemStates state)
{
	const char *id = NULL;
	GtkWidget *image;

	if (state == emb->state)
		return;

	switch (state) {
	case STATE_STOPPED:
		id = GTK_STOCK_MEDIA_PLAY;
		break;
	case STATE_PAUSED:
		id = GTK_STOCK_MEDIA_PLAY;
		break;
	case STATE_PLAYING:
		id = GTK_STOCK_MEDIA_PAUSE;
		break;
	default:
		break;
	}

	image = glade_xml_get_widget (emb->xml, "emb_pp_button_image");
	gtk_image_set_from_stock (GTK_IMAGE (image), id, GTK_ICON_SIZE_MENU);

	emb->state = state;
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
		totem_interface_error_blocking (msg, err->message,
				GTK_WINDOW (emb->window));

		g_free (msg);

		g_error_free (err);
		retval = FALSE;
	} else {
		totem_embedded_set_state (emb, STATE_PLAYING);
	}

	return;
//	return retval;
}

static void
on_play_pause (GtkWidget *widget, TotemEmbedded *emb)
{
	if (emb->state == STATE_PLAYING) {
		bacon_video_widget_pause (emb->bvw);
		totem_embedded_set_state (emb, STATE_PAUSED);
	} else {
		if (bacon_video_widget_play (emb->bvw, NULL))
			totem_embedded_set_state (emb, STATE_PLAYING);
	}
}

static void
on_got_redirect (GtkWidget *bvw, const char *mrl, TotemEmbedded *emb)
{
	char *new_mrl;

	g_message ("url: %s", emb->orig_filename);
	g_message ("redirect: %s", mrl);

	//FIXME write a proper one...
	if (mrl[0] != '/' && strstr (mrl, "://") == NULL) {
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

static gboolean
on_video_button_press_event (BaconVideoWidget *bvw, GdkEventButton *event,
		TotemEmbedded *emb)
{
	if (event->type == GDK_BUTTON_PRESS &&
			event->button == 1 &&
			emb->href != NULL)
	{
		g_free (emb->filename);
		emb->filename = emb->href;
		emb->href = NULL;
		bacon_video_widget_close (emb->bvw);

		totem_embedded_open (emb);
		bacon_video_widget_play (emb->bvw, NULL);

		return TRUE;
	}

	return FALSE;
}

static void
on_eos_event (GtkWidget *bvw, TotemEmbedded *emb)
{
	totem_embedded_set_state (emb, STATE_PAUSED);
}

static void
totem_embedded_add_children (TotemEmbedded *emb)
{
	GtkWidget *child, *container, *pp_button;
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

	g_signal_connect (G_OBJECT(emb->bvw), "got-redirect",
			G_CALLBACK (on_got_redirect), emb);
	g_signal_connect (G_OBJECT (emb->bvw), "eos",
			G_CALLBACK (on_eos_event), emb);
	g_signal_connect (G_OBJECT(emb->bvw), "button-press-event",
			G_CALLBACK (on_video_button_press_event), emb);

	container = glade_xml_get_widget (emb->xml, "hbox4");
	gtk_container_add (GTK_CONTAINER (container), GTK_WIDGET (emb->bvw));
	gtk_widget_show (GTK_WIDGET (emb->bvw));

	pp_button = glade_xml_get_widget (emb->xml, "pp_button");
	g_signal_connect (G_OBJECT (pp_button), "clicked",
			  G_CALLBACK (on_play_pause), emb);

	gtk_widget_realize (emb->window);
	gtk_widget_set_size_request (emb->window, emb->width, emb->height);

	if (emb->controller_hidden != FALSE) {
		child = glade_xml_get_widget (emb->xml, "controls");
		gtk_widget_hide (child);
	}
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
	emb->state = STATE_STOPPED;

	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		totem_embedded_error_and_exit (_("Could not initialise the thread-safe libraries."), _("Verify your system installation. The Totem plugin will now exit."), NULL);
	}

	g_thread_init (NULL);
	bacon_video_widget_init_backend (NULL, NULL);
	gdk_threads_init ();

	gtk_init (&argc, &argv);

	g_print ("CMD line: ");
	for (i = 0; i < argc; i++) {
		g_print ("%s ", argv[i]);
	}
	g_print ("\n");

	/* TODO Add popt options */
	for (i = 1; i < argc; i++) {
		if (OPTION_IS (TOTEM_OPTION_XID)) {
			if (i + 1 < argc) {
				i++;
				emb->xid = atoi (argv[i]);
			}
		} else if (OPTION_IS (TOTEM_OPTION_WIDTH)) {
			if (i + 1 < argc) {
				i++;
				emb->width = atoi (argv[i]);
			}
		} else if (OPTION_IS (TOTEM_OPTION_HEIGHT)) {
			if (i + 1 < argc) {
				i++;
				emb->height = atoi (argv[i]);
			}
		} else if (OPTION_IS (TOTEM_OPTION_URL)) {
			if (i + 1 < argc) {
				i++;
				emb->orig_filename = (const char *) argv[i];
			}
		} else if (OPTION_IS (TOTEM_OPTION_CONTROLS_HIDDEN)) {
			emb->controller_hidden = TRUE;
		} else if (OPTION_IS (TOTEM_OPTION_HREF)) {
			if (i + 1 < argc) {
				i++;
				emb->href = g_strdup (argv[i]);
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

	/* wait until we're embedded if we're to be, or shown */
	if (emb->xid != 0) {
		while (emb->embedded_done == FALSE && gtk_events_pending ())
			gtk_main_iteration ();
	} else {
		while (gtk_events_pending ())
			gtk_main_iteration ();
	}

	totem_embedded_open (emb);
	bacon_video_widget_play (emb->bvw, NULL);

	gtk_main ();

	return 0;
}

