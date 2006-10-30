/* Totem Mozilla Embedded Viewer
 *
 * Copyright (C) <2004-2006> Bastien Nocera <hadess@hadess.net>
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

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <totem-pl-parser.h>

#include <dbus/dbus-glib.h>
#include <sys/types.h>
#include <unistd.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "bacon-video-widget.h"
#include "totem-interface.h"
#include "totem-mozilla-options.h"
#include "totem-statusbar.h"
#include "bacon-volume.h"
#include "video-utils.h"

GtkWidget *totem_statusbar_create (void);
GtkWidget *totem_volume_create (void);

#define IS_FD_STREAM (strcmp(emb->filename, "fd://0") == 0)

#define VOLUME_DOWN_OFFSET -8
#define VOLUME_UP_OFFSET 8

/* For newer D-Bus version */
#ifndef DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT
#define DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT 0
#endif

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED
} TotemStates;

#define TOTEM_TYPE_EMBEDDED (totem_embedded_get_type ())
typedef struct {
	GObjectClass parent_class;

	void (*stop_sending_data) (GObject *emb);
} TotemEmbeddedClass;

typedef struct _TotemEmbedded {
	GObject parent;

	GtkWidget *window;
	GladeXML *menuxml, *xml;
	GtkWidget *about;
	int width, height;
	const char *orig_filename;
	const char *mimetype;
	char *filename, *href, *target;
	BaconVideoWidget *bvw;
	TotemStates state;
	GdkCursor *cursor;

	/* Playlist */
	GList *playlist, *current;
	GMainLoop *loop;
	int num_items;

	/* Open menu item */
	GnomeVFSMimeApplication *app;
	GtkWidget *menu_item;

	/* Seek bits */
	GtkAdjustment *seekadj;
	GtkWidget *seek;

	guint is_playlist : 1;
	guint embedded_done : 1;
	guint controller_hidden : 1;
	guint hidden : 1;
	guint repeat : 1;
	guint seeking : 1;
	guint noautostart : 1;
} TotemEmbedded;

GType totem_embedded_get_type (void);

G_DEFINE_TYPE (TotemEmbedded, totem_embedded, G_TYPE_OBJECT);
static void totem_embedded_init (TotemEmbedded *emb) { }

gboolean totem_embedded_play (TotemEmbedded *emb, GError **err);
gboolean totem_embedded_pause (TotemEmbedded *emb, GError **err);
gboolean totem_embedded_stop (TotemEmbedded *emb, GError **err);
gboolean totem_embedded_set_local_file (TotemEmbedded *emb, const char *url, GError **err);

static void totem_embedded_set_menu (TotemEmbedded *emb, gboolean enable);
static void on_open1_activate (GtkButton *button, TotemEmbedded *emb);

#include "totem-mozilla-interface.h"

enum {
	STOP_SENDING_DATA,
	LAST_SIGNAL
};
static int totem_emb_table_signals[LAST_SIGNAL] = { 0 };

static void totem_embedded_class_init (TotemEmbeddedClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	totem_emb_table_signals[STOP_SENDING_DATA] =
		g_signal_new ("stop-sending-data",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemEmbeddedClass, stop_sending_data),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);
}

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
totem_embedded_emit_stop_sending_data (TotemEmbedded *emb)
{
	g_return_if_fail (emb->filename != NULL);
	if (IS_FD_STREAM) {
		g_signal_emit (G_OBJECT (emb),
				totem_emb_table_signals[STOP_SENDING_DATA],
				0, NULL);
	}
}

static void
totem_embedded_set_state (TotemEmbedded *emb, TotemStates state)
{
	char *id = NULL;
	GtkWidget *image;
	GdkCursor *cursor;

	if (state == emb->state)
		return;

	cursor = NULL;
	image = glade_xml_get_widget (emb->xml, "emb_pp_button_image");

	switch (state) {
	case STATE_STOPPED:
		if (emb->href != NULL)
			cursor = emb->cursor;
		/* Follow through */
	case STATE_PAUSED:
		id = g_strdup_printf ("gtk-media-play-%s",
				gtk_widget_get_direction (image) ? "ltr" : "rtl");
		break;
	case STATE_PLAYING:
		id = g_strdup ("gtk-media-pause");
		break;
	default:
		break;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (image), id, GTK_ICON_SIZE_MENU);
	g_free (id);
	if (emb->hidden == FALSE && cursor != NULL)
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

		totem_embedded_emit_stop_sending_data (emb);

		totem_embedded_set_state (emb, STATE_STOPPED);

		//FIXME if emb->filename is fd://0 or stdin:///
		//we should use a better name than that
		disp = g_strdup (emb->filename);
		//disp = gnome_vfs_unescape_string_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play '%s'."), disp);
		g_free (disp);

		g_message ("error: %s", err->message);
		totem_interface_error_blocking (msg, err->message,
				GTK_WINDOW (emb->window));
		g_free (msg);
		g_error_free (err);

		totem_embedded_set_pp_state (emb, FALSE);
	} else {
		totem_embedded_set_state (emb, STATE_PAUSED);
		totem_embedded_set_pp_state (emb, TRUE);
	}

	if (IS_FD_STREAM && emb->num_items > 1) {
		totem_embedded_set_menu (emb, FALSE);
	} else {
		totem_embedded_set_menu (emb, TRUE);
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

gboolean
totem_embedded_set_local_file (TotemEmbedded *emb,
			       const char *path, GError **err)
{
	g_message ("Setting the current path to %s", path);

	g_free (emb->filename);
	emb->filename = g_filename_to_uri (path, NULL, NULL);

	return TRUE;
}

static void
totem_embedded_set_menu (TotemEmbedded *emb, gboolean enable)
{
	GtkWidget *menu, *item, *image;
	GtkWidget *copy;
	char *label;

	copy = glade_xml_get_widget (emb->menuxml, "copy_location1");
	gtk_widget_set_sensitive (copy, enable);

	if (emb->menu_item != NULL) {
		gtk_widget_destroy (emb->menu_item);
		emb->menu_item = NULL;
	}
	if (emb->app != NULL) {
		gnome_vfs_mime_application_free (emb->app);
		emb->app = NULL;
	}

	if (enable == FALSE)
		return;

	if (IS_FD_STREAM) {
		emb->app = gnome_vfs_mime_get_default_application_for_uri
			(emb->orig_filename, emb->mimetype);
	} else {
		emb->app = gnome_vfs_mime_get_default_application_for_uri
			(emb->filename, emb->mimetype);
	}

	if (emb->app == NULL)
		return;

	/* translators: this is:
	 * Open With ApplicationName
	 * as in nautilus' right-click menu */
	label = g_strdup_printf ("_Open with \"%s\"", emb->app->name);
	item = gtk_image_menu_item_new_with_mnemonic (label);
	g_free (label);
	image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_open1_activate), emb);
	gtk_widget_show (item);
	emb->menu_item = item;

	menu = glade_xml_get_widget (emb->menuxml, "menu");
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
}

static void
on_open1_activate (GtkButton *button, TotemEmbedded *emb)
{
	GList *l = NULL;

	g_return_if_fail (emb->app != NULL);

	if (IS_FD_STREAM) {
		l = g_list_prepend (l, (gpointer) emb->orig_filename);
	} else if (emb->href != NULL) {
		l = g_list_prepend (l, emb->href);
	} else {
		l = g_list_prepend (l, emb->filename);
	}

	if (gnome_vfs_mime_application_launch (emb->app, l) == GNOME_VFS_OK) {
		totem_embedded_stop (emb, NULL);
	}

	g_list_free (l);
}

static void
on_about1_activate (GtkButton *button, TotemEmbedded *emb)
{
	char *backend_version, *description;

	const char *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
		"Ronald Bultje <rbultje@ronald.bitfreak.net>",
		NULL
	};
	
	if (emb->about != NULL)
	{
		gtk_window_present (GTK_WINDOW (emb->about));
		return;
	}

	backend_version = bacon_video_widget_get_backend_name (emb->bvw);
	description = g_strdup_printf (_("Browser Plugin using %s"),
				       backend_version);

	emb->about = g_object_new (GTK_TYPE_ABOUT_DIALOG,
				   "name", _("Totem Browser Plugin"),
				   "version", VERSION,
				   "copyright", _("Copyright \xc2\xa9 2002-2006 Bastien Nocera"),
				   "comments", description,
				   "authors", authors,
				   "translator-credits", _("translator-credits"),
				   "logo-icon-name", "totem",
				   NULL);

	g_free (backend_version);
	g_free (description);

	totem_interface_set_transient_for (GTK_WINDOW (emb->about),
			GTK_WINDOW (emb->window));

	g_object_add_weak_pointer (G_OBJECT (emb->about),
			(gpointer *)&emb->about);
	g_signal_connect (G_OBJECT (emb->about), "response",
			G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (emb->about);
}

static void
on_copy_location1_activate (GtkButton *button, TotemEmbedded *emb)
{
	GtkClipboard *clip;
	const char *filename;

	if (IS_FD_STREAM) {
		filename = emb->orig_filename;
	} else if (emb->href != NULL) {
		filename = emb->href;
	} else {
		filename = emb->filename;
	}

	/* Set both the middle-click and the super-paste buffers */
	clip = gtk_clipboard_get_for_display
		(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clip, filename, -1);
	clip = gtk_clipboard_get_for_display
		(gdk_display_get_default(), GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clip, filename, -1);
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
	gchar *new_filename;

	g_message ("url: %s", emb->orig_filename);
	g_message ("redirect: %s", mrl);

	if (emb->orig_filename)
		new_filename = totem_resolve_relative_link (emb->orig_filename, mrl);
	else
		new_filename = totem_resolve_relative_link (emb->filename, mrl);

	g_free (emb->filename);
	emb->filename = new_filename;

	bacon_video_widget_close (emb->bvw);
	totem_embedded_set_state (emb, STATE_STOPPED);

	if (totem_embedded_open (emb) != FALSE)
		totem_embedded_play (emb, NULL);
}

static gboolean
on_video_button_press_event (BaconVideoWidget *bvw, GdkEventButton *event,
		TotemEmbedded *emb)
{
	if (event->type == GDK_BUTTON_PRESS &&
			event->button == 1 &&
			emb->href != NULL)
	{
		if (emb->target != NULL &&
		    !g_ascii_strcasecmp (emb->target, "QuicktimePlayer")) {
			gchar *cmd;
			GError *err = NULL;

			if (g_file_test ("./totem",
					 G_FILE_TEST_EXISTS) != FALSE) {
				cmd = g_strdup_printf ("./totem %s",
						       emb->href);
			} else {
				cmd = g_strdup_printf (BINDIR"/totem %s",
						       emb->href);
			}
			if (!g_spawn_command_line_async (cmd, &err)) {
				totem_interface_error_blocking (
					_("Failed to start stand-alone movie player"),
					err ? err->message : _("Unknown reason"),
					GTK_WINDOW (emb->window));
			}
			g_free (cmd);
		} else {
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
		}
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

	/* No playlist if we have fd://0, right? */
	if (IS_FD_STREAM) {
		totem_embedded_set_pp_state (emb, FALSE);
	} else if (emb->num_items == 1) {
		if (g_str_has_prefix (emb->filename, "file://") != FALSE) {
			if (bacon_video_widget_is_seekable (emb->bvw) != FALSE) {
				bacon_video_widget_pause (emb->bvw);
				bacon_video_widget_seek (emb->bvw, 0.0, NULL);
			} else {
				bacon_video_widget_close (emb->bvw);
				totem_embedded_open (emb);
			}
		} else {
			bacon_video_widget_close (emb->bvw);
			totem_embedded_open (emb);
		}
		if (emb->repeat != FALSE && emb->noautostart == FALSE)
			totem_embedded_play (emb, NULL);
	} else {
		/* Multiple items on the playlist */
		gboolean eop = FALSE, res;

		if (emb->current->next == NULL) {
			emb->current = emb->playlist;
			eop = TRUE;
		} else {
			emb->current = emb->current->next;
		}

		g_free (emb->filename);
		emb->filename = g_strdup (emb->current->data);
		bacon_video_widget_close (emb->bvw);
		res = totem_embedded_open (emb);
		if (res != FALSE &&
				((eop != FALSE && emb->repeat != FALSE)
				 || (eop == FALSE))) {
			totem_embedded_play (emb, NULL);
		}
	}
}

static void
on_error_event (BaconVideoWidget *bvw, char *message,
                gboolean playback_stopped, gboolean fatal, TotemEmbedded *emb)
{
	if (playback_stopped)
		totem_embedded_set_state (emb, STATE_STOPPED);

	if (fatal == FALSE) {
		totem_interface_error (_("An error occurred"), message,
		                       GTK_WINDOW (emb->window));
	} else {
		totem_embedded_error_and_exit (_("An error occurred"),
				message, emb);
	}
}

static void
cb_vol (GtkWidget *val, TotemEmbedded *emb)
{
	bacon_video_widget_set_volume (emb->bvw,
		bacon_volume_button_get_value (BACON_VOLUME_BUTTON (val)));
}

static gboolean
on_volume_scroll_event (GtkWidget *win, GdkEventScroll *event, TotemEmbedded *emb)
{
	GtkWidget *vbut;
	int vol, offset;

	switch (event->direction) {
	case GDK_SCROLL_UP:
		offset = VOLUME_UP_OFFSET;
		break;
	case GDK_SCROLL_DOWN:
		offset = VOLUME_DOWN_OFFSET;
		break;
	default:
		return FALSE;
	}

	vbut = glade_xml_get_widget (emb->xml, "volume_button");
	vol = bacon_volume_button_get_value (BACON_VOLUME_BUTTON (vbut));
	bacon_volume_button_set_value (BACON_VOLUME_BUTTON (vbut),
			vol + offset);

	return FALSE;
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
		if (emb->seeking == FALSE)
			gtk_adjustment_set_value (emb->seekadj,
					current_position * 65535);
	}
}

static gboolean
on_seek_start (GtkWidget *widget, GdkEventButton *event, TotemEmbedded *emb)
{
	emb->seeking = TRUE;

	return FALSE;
}

static gboolean
cb_on_seek (GtkWidget *widget, GdkEventButton *event, TotemEmbedded *emb)
{
	bacon_video_widget_seek (emb->bvw,
		gtk_range_get_value (GTK_RANGE (widget)) / 65535, NULL);
	emb->seeking = FALSE;

	return FALSE;
}

static void
totem_embedded_add_children (TotemEmbedded *emb)
{
	GtkWidget *child, *container, *pp_button, *vbut;
	BvwUseType type = BVW_USE_TYPE_VIDEO;
	GError *err = NULL;
	GConfClient *gc;
	int volume;

	emb->xml = totem_interface_load_with_root ("mozilla-viewer.glade",
			"vbox1", _("Plugin"), TRUE,
			GTK_WINDOW (emb->window));
	emb->menuxml = totem_interface_load_with_root ("mozilla-viewer.glade",
			"menu", _("Menu"), TRUE,
			GTK_WINDOW (emb->window));

	if (emb->xml == NULL || emb->menuxml == NULL)
		totem_embedded_exit (emb);

	child = glade_xml_get_widget (emb->xml, "vbox1");
	gtk_container_add (GTK_CONTAINER (emb->window), child);

	if (emb->hidden)
		type = BVW_USE_TYPE_AUDIO;

	emb->bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new
			(-1, -1, type, &err));

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
	g_signal_connect (G_OBJECT (emb->bvw), "error",
			G_CALLBACK (on_error_event), emb);
	g_signal_connect (G_OBJECT(emb->bvw), "button-press-event",
			G_CALLBACK (on_video_button_press_event), emb);
	g_signal_connect (G_OBJECT(emb->bvw), "tick",
			G_CALLBACK (on_tick), emb);

	container = glade_xml_get_widget (emb->xml, "hbox4");
	gtk_container_add (GTK_CONTAINER (container), GTK_WIDGET (emb->bvw));
	if (type == BVW_USE_TYPE_VIDEO) {
		gtk_widget_realize (GTK_WIDGET (emb->bvw));
		gtk_widget_show (GTK_WIDGET (emb->bvw));
	}

	emb->seek = glade_xml_get_widget (emb->xml, "time_hscale");
	emb->seekadj = gtk_range_get_adjustment (GTK_RANGE (emb->seek));
	g_signal_connect (emb->seek, "button-press-event",
			  G_CALLBACK (on_seek_start), emb);
	g_signal_connect (emb->seek, "button-release-event",
			  G_CALLBACK (cb_on_seek), emb);

	pp_button = glade_xml_get_widget (emb->xml, "pp_button");
	g_signal_connect (G_OBJECT (pp_button), "clicked",
			  G_CALLBACK (on_play_pause), emb);

	gc = gconf_client_get_default ();
	volume = gconf_client_get_int (gc, GCONF_PREFIX"/volume", NULL);
	g_object_unref (G_OBJECT (gc));

	bacon_video_widget_set_volume (emb->bvw, volume);
	vbut = glade_xml_get_widget (emb->xml, "volume_button");
	bacon_volume_button_set_value (BACON_VOLUME_BUTTON (vbut), volume);
	g_signal_connect (G_OBJECT (vbut), "value-changed",
			  G_CALLBACK (cb_vol), emb);
	gtk_widget_add_events (vbut, GDK_SCROLL_MASK);
	g_signal_connect (G_OBJECT (vbut), "scroll_event",
			G_CALLBACK (on_volume_scroll_event), emb);

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
	child = glade_xml_get_widget (emb->menuxml, "copy_location1");
	g_signal_connect (G_OBJECT (child), "activate",
			G_CALLBACK (on_copy_location1_activate), emb);
	child = glade_xml_get_widget (emb->menuxml, "preferences1");
	gtk_widget_hide (child);
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

static void
entry_added (TotemPlParser *parser, const char *uri, const char *title,
		                const char *genre, gpointer data)
{
	TotemEmbedded *emb = (TotemEmbedded *) data;

	g_print ("added URI '%s' with title '%s' genre '%s'\n", uri,
			title ? title : "empty", genre);

	//FIXME need new struct to hold that
	emb->playlist = g_list_prepend (emb->playlist, g_strdup (uri));
	emb->num_items++;
}

static gboolean
totem_embedded_push_parser (gpointer data)
{
	TotemPlParser *parser = totem_pl_parser_new ();
	TotemEmbedded *emb = (TotemEmbedded *) data;
	TotemPlParserResult res;

	parser = totem_pl_parser_new ();
	g_object_set (G_OBJECT (parser), "force", TRUE, NULL);
	g_object_set (G_OBJECT (parser), "disable-unsafe", TRUE, NULL);
	g_signal_connect (G_OBJECT (parser), "entry", G_CALLBACK (entry_added), emb);
	res = totem_pl_parser_parse (parser, emb->filename, FALSE);
	g_object_unref (parser);

	if (res != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		//FIXME show a proper error message
		switch (res) {
		case TOTEM_PL_PARSER_RESULT_UNHANDLED:
			g_print ("url '%s' unhandled\n", emb->filename);
			break;
		case TOTEM_PL_PARSER_RESULT_ERROR:
			g_print ("error handling url '%s'\n", emb->filename);
			break;
		case TOTEM_PL_PARSER_RESULT_IGNORED:
			g_print ("ignored url '%s'\n", emb->filename);
			break;
		default:
			g_assert_not_reached ();
			;;
		}
	}

	/* Check if we have anything in the playlist now */
	if (emb->playlist == NULL) {
		g_message ("NO PLAYLIST");
		totem_embedded_error_and_exit ("Can't parse that",
				"no files",
				emb);
		//FIXME error out
		return FALSE;
	}

	emb->playlist = g_list_reverse (emb->playlist);
	g_free (emb->filename);
	emb->filename = g_strdup (emb->playlist->data);
	emb->current = emb->playlist;

	g_main_loop_quit (emb->loop);

	return FALSE;
}

static void embedded (GtkPlug *plug, TotemEmbedded *emb)
{
	emb->embedded_done = TRUE;
}

static int arg_width = -1;
static int arg_height = -1;
static char *arg_url = NULL;
static char *arg_href = NULL;
static char *arg_target = NULL;
static char *arg_mime_type = NULL;
static char **arg_remaining = NULL;
static Window xid = 0;
static gboolean arg_no_controls = FALSE;
static gboolean arg_no_statusbar = TRUE;
static gboolean arg_hidden = FALSE;
static gboolean arg_is_playlist = FALSE;
static gboolean arg_repeat = FALSE;
static gboolean arg_no_autostart = FALSE;

GtkWidget *
totem_volume_create (void)
{
	GtkWidget *widget;

	widget = bacon_volume_button_new (GTK_ICON_SIZE_MENU,
					  0, 100, -1);
	gtk_widget_show (widget);

	return widget;
}

GtkWidget *
totem_statusbar_create (void)
{
	GtkWidget *widget;

	widget = totem_statusbar_new ();
	totem_statusbar_set_has_resize_grip (TOTEM_STATUSBAR (widget), FALSE);
	if (arg_no_statusbar == FALSE)
		gtk_widget_show (widget);

	return widget;
}

static gboolean
parse_xid (const gchar *option_name,
	   const gchar *value,
	   gpointer data,
	   GError **error)
{
	xid = (Window) g_ascii_strtoull (value, NULL, 10);

	return TRUE;
}

static GOptionEntry option_entries [] =
{
	{ TOTEM_OPTION_WIDTH, 0, 0, G_OPTION_ARG_INT, &arg_width, NULL, NULL },
	{ TOTEM_OPTION_HEIGHT, 0, 0, G_OPTION_ARG_INT, &arg_height, NULL, NULL },
	{ TOTEM_OPTION_URL, 0, 0, G_OPTION_ARG_STRING, &arg_url, NULL, NULL },
	{ TOTEM_OPTION_HREF, 0, 0, G_OPTION_ARG_STRING, &arg_href, NULL, NULL },
	{ TOTEM_OPTION_XID, 0, 0, G_OPTION_ARG_CALLBACK, parse_xid, NULL, NULL },
	{ TOTEM_OPTION_TARGET, 0, 0, G_OPTION_ARG_STRING, &arg_target, NULL, NULL },
	{ TOTEM_OPTION_MIMETYPE, 0, 0, G_OPTION_ARG_STRING, &arg_mime_type, NULL, NULL },
	{ TOTEM_OPTION_CONTROLS_HIDDEN, 0, 0, G_OPTION_ARG_NONE, &arg_no_controls, NULL, NULL },
	{ TOTEM_OPTION_STATUSBAR_HIDDEN, 0, 0, G_OPTION_ARG_NONE, &arg_no_statusbar, NULL, NULL },
	{ TOTEM_OPTION_HIDDEN, 0, 0, G_OPTION_ARG_NONE, &arg_hidden, NULL, NULL },
	{ TOTEM_OPTION_PLAYLIST, 0, 0, G_OPTION_ARG_NONE, &arg_is_playlist, NULL, NULL },
	{ TOTEM_OPTION_REPEAT, 0, 0, G_OPTION_ARG_NONE, &arg_repeat, NULL, NULL },
	{ TOTEM_OPTION_NOAUTOSTART, 0, 0, G_OPTION_ARG_NONE, &arg_no_autostart, NULL, NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY /* STRING? */, &arg_remaining, NULL },
	{ NULL }
};


int main (int argc, char **argv)
{
	TotemEmbedded *emb;
	DBusGProxy *proxy;
	DBusGConnection *conn;
	guint res;
	gchar *svcname;
	GError *e = NULL;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

#ifdef GNOME_ENABLE_DEBUG
	{
		int i;
		g_print ("Viewer args: ");
		for (i = 0; i < argc; i++)
			g_print ("%s ", argv[i]);
		g_print ("\n");
	}
#endif
	
	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		totem_embedded_error_and_exit (_("Could not initialise the thread-safe libraries."), _("Verify your system installation. The Totem plugin will now exit."), NULL);
	}

	g_thread_init (NULL);

	if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &e))
	{
		g_print ("%s\n", e->message);
		g_error_free (e);
		exit (1);
	}

	/* FIXME: are there enough checks? */
	if (arg_remaining == NULL) {
		return 1;
	}

	bacon_video_widget_init_backend (NULL, NULL);
	gnome_vfs_init ();

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

	emb->state = STATE_STOPPED;
	emb->width = arg_width;
	emb->height = arg_height;
	emb->controller_hidden = arg_no_controls;
	emb->orig_filename = arg_url;
	emb->href = arg_href;
	emb->filename = arg_remaining ? arg_remaining[0] : NULL;
	emb->target = arg_target;
	if (arg_mime_type != NULL) {
		emb->mimetype = arg_mime_type;
	} else {
		emb->mimetype = g_strdup (gnome_vfs_get_mime_type_for_name (emb->filename));
	}
	emb->hidden = arg_hidden;
	emb->is_playlist = arg_is_playlist;
	emb->repeat = arg_repeat;
	emb->noautostart = arg_no_autostart;

	dbus_g_connection_register_g_object (conn, "/TotemEmbedded",
					     G_OBJECT (emb));

	/* XEMBED or stand-alone */
	if (xid != 0) {
		GtkWidget *window;

		/* The miraculous XEMBED protocol */
		window = gtk_plug_new ((GdkNativeWindow) xid);
		gtk_signal_connect (GTK_OBJECT(window), "embedded",
				G_CALLBACK (embedded), NULL);
		gtk_widget_realize (window);

		emb->window = window;
	} else {
		/* Stand-alone version */
		emb->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	}

	/* Hidden or not as well? */
	totem_embedded_add_children (emb);
	totem_embedded_create_cursor (emb);
	if (emb->hidden == FALSE)
		gtk_widget_show (emb->window);

	/* wait until we're embedded if we're to be, or shown */
	if (xid != 0) {
		while (emb->embedded_done == FALSE && gtk_events_pending ())
			gtk_main_iteration ();
	} else {
		while (gtk_events_pending ())
			gtk_main_iteration ();
	}

	/* Do we have a playlist we need to setup ourselves? */
	if (emb->is_playlist != FALSE) {
		g_idle_add (totem_embedded_push_parser, emb);
		emb->loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (emb->loop);
		g_main_loop_unref (emb->loop);
	} else {
		emb->playlist = g_list_prepend (emb->playlist,
				g_strdup (emb->filename));
		emb->num_items++;
	}

	if (totem_embedded_open (emb) != FALSE
			&& emb->noautostart == FALSE) {
		totem_embedded_play (emb, NULL);
	}

	gtk_main ();

	return 0;
}

