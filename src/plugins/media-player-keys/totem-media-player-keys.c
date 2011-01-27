/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Jan Arne Petersen <jap@gnome.org>
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
 * See license_change file for details.
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <dbus/dbus-glib.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>
#include <string.h>

#include "totem-plugin.h"
#include "totem-marshal.h"
#include "totem.h"

#define TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN		(totem_media_player_keys_plugin_get_type ())
#define TOTEM_MEDIA_PLAYER_KEYS_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN, TotemMediaPlayerKeysPlugin))
#define TOTEM_MEDIA_PLAYER_KEYS_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN, TotemMediaPlayerKeysPluginClass))
#define TOTEM_IS_MEDIA_PLAYER_KEYS_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN))
#define TOTEM_IS_MEDIA_PLAYER_KEYS_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN))
#define TOTEM_MEDIA_PLAYER_KEYS_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN, TotemMediaPlayerKeysPluginClass))

typedef struct {
	DBusGProxy    *media_player_keys_proxy;
	guint          handler_id;
} TotemMediaPlayerKeysPluginPrivate;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN,
		      TotemMediaPlayerKeysPlugin,
		      totem_media_player_keys_plugin);

static void
on_media_player_key_pressed (DBusGProxy *proxy, const gchar *application, const gchar *key, TotemObject *totem)
{
	if (strcmp ("Totem", application) == 0) {
		if (strcmp ("Play", key) == 0)
			totem_action_play_pause (totem);
		else if (strcmp ("Previous", key) == 0)
			totem_action_previous (totem);
		else if (strcmp ("Next", key) == 0)
			totem_action_next (totem);
		else if (strcmp ("Stop", key) == 0)
			totem_action_pause (totem);
		else if (strcmp ("FastForward", key) == 0)
			totem_action_remote (totem, TOTEM_REMOTE_COMMAND_SEEK_FORWARD, NULL);
		else if (strcmp ("Rewind", key) == 0)
			totem_action_remote (totem, TOTEM_REMOTE_COMMAND_SEEK_BACKWARD, NULL);
		else if (strcmp ("Repeat", key) == 0) {
			gboolean value;

			value = totem_action_remote_get_setting (totem, TOTEM_REMOTE_SETTING_REPEAT);
			totem_action_remote_set_setting (totem, TOTEM_REMOTE_SETTING_REPEAT, !value);
		} else if (strcmp ("Shuffle", key) == 0) {
			gboolean value;

			value = totem_action_remote_get_setting (totem, TOTEM_REMOTE_SETTING_SHUFFLE);
			totem_action_remote_set_setting (totem, TOTEM_REMOTE_SETTING_SHUFFLE, !value);
		}
	}
}

static gboolean
on_window_focus_in_event (GtkWidget *window, GdkEventFocus *event, TotemMediaPlayerKeysPlugin *pi)
{
	if (pi->priv->media_player_keys_proxy != NULL) {
		dbus_g_proxy_call (pi->priv->media_player_keys_proxy,
				   "GrabMediaPlayerKeys", NULL,
				   G_TYPE_STRING, "Totem", G_TYPE_UINT, 0, G_TYPE_INVALID,
				   G_TYPE_INVALID);
	}

	return FALSE;
}

static void
proxy_destroy (DBusGProxy *proxy,
		  TotemMediaPlayerKeysPlugin* plugin)
{
	plugin->priv->media_player_keys_proxy = NULL;
}

static void
impl_activate (PeasActivatable *plugin)
{
	TotemMediaPlayerKeysPlugin *pi = TOTEM_MEDIA_PLAYER_KEYS_PLUGIN (plugin);
	TotemObject *totem;
	DBusGConnection *connection;
	GError *err = NULL;
	GtkWindow *window;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &err);
	if (connection == NULL) {
		g_warning ("Error connecting to D-Bus: %s", err->message);
		return;
	}

	/* Try the gnome-settings-daemon version,
	 * then the gnome-control-center version of things */
	pi->priv->media_player_keys_proxy = dbus_g_proxy_new_for_name_owner (connection,
								       "org.gnome.SettingsDaemon",
								       "/org/gnome/SettingsDaemon/MediaKeys",
								       "org.gnome.SettingsDaemon.MediaKeys",
								       NULL);
	if (pi->priv->media_player_keys_proxy == NULL) {
		pi->priv->media_player_keys_proxy = dbus_g_proxy_new_for_name_owner (connection,
									       "org.gnome.SettingsDaemon",
									       "/org/gnome/SettingsDaemon",
									       "org.gnome.SettingsDaemon",
									       &err);
	}

	dbus_g_connection_unref (connection);
	if (err != NULL) {
#if 0
		gboolean daemon_not_running;
		g_warning ("Failed to create dbus proxy for org.gnome.SettingsDaemon: %s",
			   err->message);
		daemon_not_running = (err->code == DBUS_GERROR_NAME_HAS_NO_OWNER);
		g_error_free (err);
		/* don't popup error if settings-daemon is not running,
 		 * ie when starting totem not under GNOME desktop */
		return daemon_not_running;
#endif
		return;
	} else {
		g_signal_connect_object (pi->priv->media_player_keys_proxy,
					 "destroy",
					 G_CALLBACK (proxy_destroy),
					 pi, 0);
	}

	dbus_g_proxy_call (pi->priv->media_player_keys_proxy,
			   "GrabMediaPlayerKeys", NULL,
			   G_TYPE_STRING, "Totem", G_TYPE_UINT, 0, G_TYPE_INVALID,
			   G_TYPE_INVALID);

	totem = g_object_get_data (G_OBJECT (plugin), "object");

	dbus_g_object_register_marshaller (totem_marshal_VOID__STRING_STRING,
			G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (pi->priv->media_player_keys_proxy, "MediaPlayerKeyPressed",
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (pi->priv->media_player_keys_proxy, "MediaPlayerKeyPressed",
			G_CALLBACK (on_media_player_key_pressed), totem, NULL);

	window = totem_get_main_window (totem);
	pi->priv->handler_id = g_signal_connect (G_OBJECT (window), "focus-in-event",
			G_CALLBACK (on_window_focus_in_event), pi);

	g_object_unref (G_OBJECT (window));
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemMediaPlayerKeysPlugin *pi = TOTEM_MEDIA_PLAYER_KEYS_PLUGIN (plugin);
	GtkWindow *window;

	if (pi->priv->media_player_keys_proxy != NULL) {
		dbus_g_proxy_call (pi->priv->media_player_keys_proxy,
				   "ReleaseMediaPlayerKeys", NULL,
				   G_TYPE_STRING, "Totem", G_TYPE_INVALID, G_TYPE_INVALID);
		g_object_unref (pi->priv->media_player_keys_proxy);
		pi->priv->media_player_keys_proxy = NULL;
	}

	if (pi->priv->handler_id != 0) {
		TotemObject *totem;

		totem = g_object_get_data (G_OBJECT (plugin), "object");
		window = totem_get_main_window (totem);
		if (window == NULL)
			return;

		g_signal_handler_disconnect (G_OBJECT (window), pi->priv->handler_id);

		g_object_unref (window);
	}
}

