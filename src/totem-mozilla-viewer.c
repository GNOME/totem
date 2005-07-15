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

#include <dbus/dbus-glib.h>
#include <sys/types.h>
#include <unistd.h>

#include "bacon-video-widget.h"
#include "totem-interface.h"
#include "totem-mozilla-options.h"
#include "totem-volume.h"
//FIXME damn build system!
#include "totem-interface.c"

GtkWidget *totem_volume_create (void);

#define OPTION_IS(x) (strcmp(argv[i], x) == 0)

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED
} TotemStates;

#define TOTEM_TYPE_EMBEDDED (totem_embedded_get_type ())
typedef GObjectClass TotemEmbeddedClass;
typedef struct _TotemEmbedded {
	GObject parent;

	GtkWidget *window;
	GladeXML *menuxml, *xml;
	GtkWidget *about;
	int width, height;
	const char *orig_filename;
	char *filename, *href;
	BaconVideoWidget *bvw;
	gboolean controller_hidden;
	TotemStates state;
	GdkCursor *cursor;

	/* Seek bits */
	GtkAdjustment *seekadj;
	GtkWidget *seek;

	/* XEmbed */
	gboolean embedded_done;
} TotemEmbedded;
G_DEFINE_TYPE (TotemEmbedded, totem_embedded, G_TYPE_OBJECT);
static void totem_embedded_class_init (TotemEmbeddedClass *klass) { }
static void totem_embedded_init (TotemEmbedded *emb) { }

gboolean totem_embedded_play (TotemEmbedded *emb, GError **err);
gboolean totem_embedded_pause (TotemEmbedded *emb, GError **err);
gboolean totem_embedded_stop (TotemEmbedded *emb, GError **err);

#include "totem-mozilla-interface.h"

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
	GdkCursor *cursor;

	if (state == emb->state)
		return;

	cursor = NULL;

	switch (state) {
	case STATE_STOPPED:
		id = GTK_STOCK_MEDIA_PLAY;
		if (emb->href != NULL)
			cursor = emb->cursor;
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
	gdk_window_set_cursor (GTK_WIDGET (emb->bvw)->window, cursor);

	emb->state = state;
}

static void
totem_embedded_set_pp_state (TotemEmbedded *emb, gboolean state)
{
	GtkWidget *item;

	item = glade_xml_get_widget (emb->xml, "pp_button");
	gtk_widget_set_sensitive (item, state);
}

static gboolean
totem_embedded_open (TotemEmbedded *emb)
{
	GError *err = NULL;
	gboolean retval;

	g_message ("totem_embedded_open '%s'", emb->filename);

	retval = bacon_video_widget_open (emb->bvw, emb->filename, &err);
	if (retval == FALSE)
	{
		char *msg, *disp;

		totem_embedded_set_state (emb, STATE_STOPPED);

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
	} else {
		totem_embedded_set_state (emb, STATE_PAUSED);
		totem_embedded_set_pp_state (emb, TRUE);
	}

	return retval;
}

gboolean
totem_embedded_play (TotemEmbedded *emb, GError **err)
{
	if (bacon_video_widget_play (emb->bvw, NULL))
		totem_embedded_set_state (emb, STATE_PLAYING);
	return TRUE;
}

gboolean
totem_embedded_pause (TotemEmbedded *emb, GError **err)
{
	bacon_video_widget_pause (emb->bvw);
	totem_embedded_set_state (emb, STATE_PAUSED);
	return TRUE;
}

gboolean
totem_embedded_stop (TotemEmbedded *emb, GError **err)
{
	bacon_video_widget_stop (emb->bvw);
	totem_embedded_set_state (emb, STATE_STOPPED);
	return TRUE;
}

static void
on_about1_activate (GtkButton *button, TotemEmbedded *emb)
{
	GdkPixbuf *pixbuf = NULL;
	const char *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
		"Ronald Bultje <rbultje@ronald.bitfreak.net>",
		NULL
	};
	char *backend_version, *description;
	char *filename;

	if (emb->about != NULL)
	{
		gtk_window_present (GTK_WINDOW (emb->about));
		return;
	}

	filename = g_build_filename (DATADIR,
			"totem", "media-player-48.png", NULL);
	pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	backend_version = bacon_video_widget_get_backend_name (emb->bvw);
	description = g_strdup_printf (_("Movie Player using %s"),
				backend_version);

	emb->about = g_object_new (GTK_TYPE_ABOUT_DIALOG,
			"name", _("Totem Mozilla Plugin"),
			"version", VERSION,
			"copyright", _("Copyright \xc2\xa9 2002-2005 Bastien Nocera"),
			"comments", description,
			"authors", authors,
			"translator-credits", _("translator-credits"),
			"logo", pixbuf,
			NULL);

	g_free (backend_version);
	g_free (description);

	if (pixbuf != NULL)
		gdk_pixbuf_unref (pixbuf);

	g_object_add_weak_pointer (G_OBJECT (emb->about),
			(gpointer *)&emb->about);
	gtk_window_set_transient_for (GTK_WINDOW (emb->about),
			GTK_WINDOW (emb->window));
	g_signal_connect (G_OBJECT (emb->about), "response",
			G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show(emb->about);
}

static void
on_play_pause (GtkWidget *widget, TotemEmbedded *emb)
{
	if (emb->state == STATE_PLAYING) {
		totem_embedded_pause (emb, NULL);
	} else {
		totem_embedded_play (emb, NULL);
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
	totem_embedded_set_state (emb, STATE_STOPPED);

	if (totem_embedded_open (emb) != FALSE)
		totem_embedded_play (emb, NULL);
g_print ("open result\n");
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
		totem_embedded_set_state (emb, STATE_STOPPED);

		if (emb->controller_hidden != FALSE) {
			GtkWidget *controls;
			controls = glade_xml_get_widget (emb->xml, "controls");
			gtk_widget_show (controls);
		}


		if (totem_embedded_open (emb) != FALSE)
			totem_embedded_play (emb, NULL);

		return TRUE;
	} else if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
		GtkMenu *menu;

		menu = GTK_MENU (glade_xml_get_widget (emb->menuxml, "menu"));
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
				event->button, event->time);

		return TRUE;
	}

	return FALSE;
}

static void
on_eos_event (GtkWidget *bvw, TotemEmbedded *emb)
{
	totem_embedded_set_state (emb, STATE_STOPPED);
	gtk_adjustment_set_value (emb->seekadj, 0);
	if (strcmp (emb->filename, "fd://0") == 0) {
		totem_embedded_set_pp_state (emb, FALSE);
	}
}

static void
cb_vol (GtkWidget *val, TotemEmbedded *emb)
{
	bacon_video_widget_set_volume (emb->bvw,
		totem_volume_button_get_value (TOTEM_VOLUME_BUTTON (val)));
}

static void
on_tick (GtkWidget *bvw,
		gint64 current_time,
		gint64 stream_length,
		float current_position,
		gboolean seekable,
		TotemEmbedded *emb)
{
	if (emb->state != STATE_STOPPED) {
		gtk_widget_set_sensitive (emb->seek, seekable);
		gtk_adjustment_set_value (emb->seekadj,
				current_position * 65535);
	}
}

static void
totem_embedded_add_children (TotemEmbedded *emb)
{
	GtkWidget *child, *container, *pp_button, *vbut;
	GError *err = NULL;

	emb->xml = totem_interface_load_with_root ("mozilla-viewer.glade",
			"vbox1", _("Plugin"), TRUE,
			GTK_WINDOW (emb->window));
	emb->menuxml = totem_interface_load_with_root ("mozilla-viewer.glade",
			"menu", _("Menu"), TRUE,
			GTK_WINDOW (emb->window));

	if (emb->xml == NULL || emb->menuxml == NULL)
	{
		totem_embedded_exit (emb);
	}

	child = glade_xml_get_widget (emb->xml, "vbox1");
	gtk_container_add (GTK_CONTAINER (emb->window), child);

	emb->bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new
			(-1, -1, BVW_USE_TYPE_VIDEO, &err));

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
	g_signal_connect (G_OBJECT(emb->bvw), "tick",
			G_CALLBACK (on_tick), emb);

	container = glade_xml_get_widget (emb->xml, "hbox4");
	gtk_container_add (GTK_CONTAINER (container), GTK_WIDGET (emb->bvw));
	gtk_widget_realize (GTK_WIDGET (emb->bvw));
	gtk_widget_show (GTK_WIDGET (emb->bvw));

	emb->seek = glade_xml_get_widget (emb->xml, "time_hscale");
	emb->seekadj = gtk_range_get_adjustment (GTK_RANGE (emb->seek));

	pp_button = glade_xml_get_widget (emb->xml, "pp_button");
	g_signal_connect (G_OBJECT (pp_button), "clicked",
			  G_CALLBACK (on_play_pause), emb);

	vbut = glade_xml_get_widget (emb->xml, "volume_button");
	totem_volume_button_set_value (TOTEM_VOLUME_BUTTON (vbut),
			bacon_video_widget_get_volume (emb->bvw));
	g_signal_connect (G_OBJECT (vbut), "value-changed",
			  G_CALLBACK (cb_vol), emb);

	gtk_widget_realize (emb->window);
	gtk_widget_set_size_request (emb->window, emb->width, emb->height);

	if (emb->controller_hidden != FALSE) {
		child = glade_xml_get_widget (emb->xml, "controls");
		gtk_widget_hide (child);
	}

	/* popup */
	child = glade_xml_get_widget (emb->menuxml, "about1");
	g_signal_connect (G_OBJECT (child), "activate",
			  G_CALLBACK (on_about1_activate), emb);
}

static void
totem_embedded_create_cursor (TotemEmbedded *emb)
{
	GtkWidget *label;
	GdkPixbuf *icon;

	label = gtk_label_new ("");
	icon = gtk_widget_render_icon (label, GTK_STOCK_MEDIA_PLAY,
			GTK_ICON_SIZE_BUTTON, NULL);
	gtk_widget_destroy (label);
	emb->cursor = gdk_cursor_new_from_pixbuf (gdk_display_get_default (),
			icon,
			gdk_pixbuf_get_width (icon) / 2,
			gdk_pixbuf_get_height (icon) / 2);
	gdk_pixbuf_unref (icon);
}

static void embedded (GtkPlug *plug, TotemEmbedded *emb)
{
	emb->embedded_done = TRUE;
}

GtkWidget *
totem_volume_create (void)
{
	GtkWidget *widget;

	widget = totem_volume_button_new (0, 100, -1);
	gtk_widget_show (widget);

	return widget;
}

int main (int argc, char **argv)
{
	TotemEmbedded *emb;
	DBusGProxy *proxy;
	DBusGConnection *conn;
	int i;
	guint res;
	Window xid;
	gchar *svcname;
	GError *e = NULL;

	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		totem_embedded_error_and_exit (_("Could not initialise the thread-safe libraries."), _("Verify your system installation. The Totem plugin will now exit."), NULL);
	}

	g_thread_init (NULL);
	bacon_video_widget_init_backend (NULL, NULL);
	//gdk_threads_init ();

	gtk_init (&argc, &argv);
	dbus_g_object_type_install_info (TOTEM_TYPE_EMBEDDED,
		&dbus_glib_totem_embedded_object_info);
	svcname = g_strdup_printf ("org.totem_%d.MozillaPluginService",
				   getpid());
	if (!(conn = dbus_g_bus_get (DBUS_BUS_SESSION, &e)) ||
	    !(proxy = dbus_g_proxy_new_for_name (conn, "org.freedesktop.DBus",
						 "/org/freedesktop/DBus",
						 "org.freedesktop.DBus")) ||
	    !dbus_g_proxy_call (proxy, "RequestName", &e,
			G_TYPE_STRING, svcname,
			G_TYPE_UINT, DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT,
			G_TYPE_INVALID,
			G_TYPE_UINT, &res,
			G_TYPE_INVALID)) {
		g_print ("Failed to get DBUS connection for %s: %s\n",
			 svcname, e->message);
		return 1;
	}
	g_free (svcname);

	emb = g_object_new (TOTEM_TYPE_EMBEDDED, NULL);
	emb->width = emb->height = -1;
	emb->state = STATE_STOPPED;
	dbus_g_connection_register_g_object (conn, "/TotemEmbedded",
					     G_OBJECT (emb));

	g_print ("CMD line: ");
	for (i = 0; i < argc; i++) {
		g_print ("%s ", argv[i]);
	}
	g_print ("\n");

	xid = 0;
	/* TODO Add popt options */
	for (i = 1; i < argc; i++) {
		if (OPTION_IS (TOTEM_OPTION_XID)) {
			if (i + 1 < argc) {
				i++;
				xid = (Window) g_ascii_strtoull (argv[i], NULL, 10);
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
	if (xid != 0) {
		GtkWidget *window;

		/* The miraculous XEMBED protocol */
		window = gtk_plug_new ((GdkNativeWindow)xid);
		gtk_signal_connect(GTK_OBJECT(window), "embedded",
				G_CALLBACK (embedded), NULL);
		gtk_widget_realize (window);

		emb->window = window;
	} else {
		/* Stand-alone version */
		emb->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	}

	totem_embedded_add_children (emb);
	totem_embedded_create_cursor (emb);
	gtk_widget_show (emb->window);

	/* wait until we're embedded if we're to be, or shown */
	if (xid != 0) {
		while (emb->embedded_done == FALSE && gtk_events_pending ())
			gtk_main_iteration ();
	} else {
		while (gtk_events_pending ())
			gtk_main_iteration ();
	}

	if (totem_embedded_open (emb) != FALSE)
		totem_embedded_play (emb, NULL);

	gtk_main ();

	return 0;
}

