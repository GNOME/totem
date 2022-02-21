/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Philip Withnall <philip@tecnocode.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 * Author: Bastien Nocera <hadess@hadess.net>, Philip Withnall <philip@tecnocode.co.uk>
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <libpeas/peas-activatable.h>

#include "totem-plugin.h"
#include "totem-skipto.h"

#define TOTEM_TYPE_SKIPTO_PLUGIN		(totem_skipto_plugin_get_type ())
#define TOTEM_SKIPTO_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_SKIPTO_PLUGIN, TotemSkiptoPlugin))

typedef struct {
	PeasExtensionBase parent;

	TotemObject	*totem;
	TotemSkipto	*st;
	guint		handler_id_stream_length;
	guint		handler_id_seekable;
	guint		handler_id_key_press;
	GSimpleAction  *action;
} TotemSkiptoPlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_SKIPTO_PLUGIN, TotemSkiptoPlugin, totem_skipto_plugin)

static void
destroy_dialog (TotemSkiptoPlugin *pi)
{
	if (pi->st != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (pi->st),
					      (gpointer *)&(pi->st));
		gtk_widget_destroy (GTK_WIDGET (pi->st));
		pi->st = NULL;
	}
}

static void
totem_skipto_update_from_state (TotemObject *totem,
				TotemSkiptoPlugin *pi)
{
	gint64 _time;
	gboolean seekable;

	g_object_get (G_OBJECT (pi->totem),
				"stream-length", &_time,
				"seekable", &seekable,
				NULL);

	if (pi->st != NULL) {
		totem_skipto_update_range (pi->st, _time);
		totem_skipto_set_seekable (pi->st, seekable);
	}

	/* Update the action's sensitivity */
	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->action), seekable);
}

static void
property_notify_cb (TotemObject *totem,
		    GParamSpec *spec,
		    TotemSkiptoPlugin *pi)
{
	totem_skipto_update_from_state (totem, pi);
}

static void
skip_to_response_callback (GtkDialog *dialog, gint response, TotemSkiptoPlugin *pi)
{
	if (response != GTK_RESPONSE_OK) {
		destroy_dialog (pi);
		return;
	}

	gtk_widget_hide (GTK_WIDGET (dialog));

	totem_object_seek_time (pi->totem,
				totem_skipto_get_range (pi->st),
				TRUE);
	destroy_dialog (pi);
}

static void
run_skip_to_dialog (TotemSkiptoPlugin *pi)
{
	if (totem_object_is_seekable (pi->totem) == FALSE)
		return;

	if (pi->st != NULL) {
		gtk_window_present (GTK_WINDOW (pi->st));
		totem_skipto_set_current (pi->st, totem_object_get_current_time
					  (pi->totem));
		return;
	}

	pi->st = TOTEM_SKIPTO (totem_skipto_new (pi->totem));
	g_signal_connect (G_OBJECT (pi->st), "delete-event",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	g_signal_connect (G_OBJECT (pi->st), "response",
			  G_CALLBACK (skip_to_response_callback), pi);
	g_object_add_weak_pointer (G_OBJECT (pi->st),
				   (gpointer *)&(pi->st));
	totem_skipto_update_from_state (pi->totem, pi);
	totem_skipto_set_current (pi->st,
				  totem_object_get_current_time (pi->totem));
}

static void
skip_to_action_callback (GSimpleAction     *action,
			 GVariant          *parameter,
			 TotemSkiptoPlugin *pi)
{
	run_skip_to_dialog (pi);
}

static gboolean
on_window_key_press_event (GtkWidget *window, GdkEventKey *event, TotemSkiptoPlugin *pi)
{

	if (event->state == 0 || !(event->state & GDK_CONTROL_MASK))
		return FALSE;

	switch (event->keyval) {
		case GDK_KEY_k:
		case GDK_KEY_K:
			run_skip_to_dialog (pi);
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

static void
impl_activate (PeasActivatable *plugin)
{
	GtkWindow *window;
	TotemSkiptoPlugin *pi = TOTEM_SKIPTO_PLUGIN (plugin);
	GMenu *menu;
	GMenuItem *item;

	pi->totem = g_object_get_data (G_OBJECT (plugin), "object");
	pi->handler_id_stream_length = g_signal_connect (G_OBJECT (pi->totem),
				"notify::stream-length",
				G_CALLBACK (property_notify_cb),
				pi);
	pi->handler_id_seekable = g_signal_connect (G_OBJECT (pi->totem),
				"notify::seekable",
				G_CALLBACK (property_notify_cb),
				pi);

	/* Key press handler */
	window = totem_object_get_main_window (pi->totem);
	pi->handler_id_key_press = g_signal_connect (G_OBJECT(window),
				"key-press-event",
				G_CALLBACK (on_window_key_press_event),
				pi);
	g_object_unref (window);

	/* Install the menu */
	pi->action = g_simple_action_new ("skip-to", NULL);
	g_signal_connect (G_OBJECT (pi->action), "activate",
			  G_CALLBACK (skip_to_action_callback), plugin);
	g_action_map_add_action (G_ACTION_MAP (pi->totem), G_ACTION (pi->action));

	menu = totem_object_get_menu_section (pi->totem, "skipto-placeholder");
	item = g_menu_item_new (_("_Skip To…"), "app.skip-to");
	g_menu_item_set_attribute (item, "accel", "s", "<Primary>k");
	g_menu_append_item (G_MENU (menu), item);

	totem_skipto_update_from_state (pi->totem, pi);
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemSkiptoPlugin *pi = TOTEM_SKIPTO_PLUGIN (plugin);
	GtkWindow *window;
	TotemObject *totem;

	totem = g_object_get_data (G_OBJECT (plugin), "object");

	g_signal_handler_disconnect (G_OBJECT (totem),
				     pi->handler_id_stream_length);
	g_signal_handler_disconnect (G_OBJECT (totem),
				     pi->handler_id_seekable);

	if (pi->handler_id_key_press != 0) {
		window = totem_object_get_main_window (totem);
		g_signal_handler_disconnect (G_OBJECT(window),
					     pi->handler_id_key_press);
		pi->handler_id_key_press = 0;
		g_object_unref (window);
	}

	/* Remove the menu */
	totem_object_empty_menu_section (totem, "skipto-placeholder");

	destroy_dialog (TOTEM_SKIPTO_PLUGIN (plugin));
}

