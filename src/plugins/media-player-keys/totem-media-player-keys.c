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

#include "totem-marshal.h"
#include "totem.h"

#define TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN		(totem_media_player_keys_plugin_get_type ())
#define TOTEM_MEDIA_PLAYER_KEYS_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN, TotemMediaPlayerKeysPlugin))
#define TOTEM_MEDIA_PLAYER_KEYS_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN, TotemMediaPlayerKeysPluginClass))
#define TOTEM_IS_MEDIA_PLAYER_KEYS_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN))
#define TOTEM_IS_MEDIA_PLAYER_KEYS_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN))
#define TOTEM_MEDIA_PLAYER_KEYS_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN, TotemMediaPlayerKeysPluginClass))

typedef struct
{
	PeasExtensionBase parent;

	DBusGProxy    *media_player_keys_proxy;

	guint          handler_id;
} TotemMediaPlayerKeysPlugin;

typedef struct
{
	PeasExtensionBaseClass parent_class;
} TotemMediaPlayerKeysPluginClass;

G_MODULE_EXPORT void peas_register_types		(PeasObjectModule *module);
GType	totem_media_player_keys_plugin_get_type		(void) G_GNUC_CONST;

static void peas_activatable_iface_init			(PeasActivatableInterface *iface);
static void totem_media_player_keys_plugin_finalize	(GObject *object);
static void impl_activate				(PeasActivatable *plugin, GObject *object);
static void impl_deactivate				(PeasActivatable *plugin, GObject *object);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (TotemMediaPlayerKeysPlugin,
				totem_media_player_keys_plugin,
				PEAS_TYPE_EXTENSION_BASE,
				0,
				G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_TYPE_ACTIVATABLE,
							       peas_activatable_iface_init))

static void
totem_media_player_keys_plugin_class_init (TotemMediaPlayerKeysPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = totem_media_player_keys_plugin_finalize;
}

static void
totem_media_player_keys_plugin_class_finalize (TotemMediaPlayerKeysPluginClass *klass)
{
}


static void
peas_activatable_iface_init (PeasActivatableInterface *iface)
{
	iface->activate = impl_activate;
	iface->deactivate = impl_deactivate;
}

static void
totem_media_player_keys_plugin_init (TotemMediaPlayerKeysPlugin *plugin)
{
}

static void
totem_media_player_keys_plugin_finalize (GObject *object)
{
	G_OBJECT_CLASS (totem_media_player_keys_plugin_parent_class)->finalize (object);
}

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
	}
}

static gboolean
on_window_focus_in_event (GtkWidget *window, GdkEventFocus *event, TotemMediaPlayerKeysPlugin *pi)
{
	if (pi->media_player_keys_proxy != NULL) {
		dbus_g_proxy_call (pi->media_player_keys_proxy,
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
	plugin->media_player_keys_proxy = NULL;
}

static void
impl_activate (PeasActivatable *plugin,
	       GObject *object)
{
	TotemMediaPlayerKeysPlugin *pi = TOTEM_MEDIA_PLAYER_KEYS_PLUGIN (plugin);
	TotemObject *totem = TOTEM_OBJECT (object);
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
	pi->media_player_keys_proxy = dbus_g_proxy_new_for_name_owner (connection,
								       "org.gnome.SettingsDaemon",
								       "/org/gnome/SettingsDaemon/MediaKeys",
								       "org.gnome.SettingsDaemon.MediaKeys",
								       NULL);
	if (pi->media_player_keys_proxy == NULL) {
		pi->media_player_keys_proxy = dbus_g_proxy_new_for_name_owner (connection,
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
		g_signal_connect_object (pi->media_player_keys_proxy,
					 "destroy",
					 G_CALLBACK (proxy_destroy),
					 pi, 0);
	}

	dbus_g_proxy_call (pi->media_player_keys_proxy,
			   "GrabMediaPlayerKeys", NULL,
			   G_TYPE_STRING, "Totem", G_TYPE_UINT, 0, G_TYPE_INVALID,
			   G_TYPE_INVALID);

	dbus_g_object_register_marshaller (totem_marshal_VOID__STRING_STRING,
			G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (pi->media_player_keys_proxy, "MediaPlayerKeyPressed",
			G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (pi->media_player_keys_proxy, "MediaPlayerKeyPressed",
			G_CALLBACK (on_media_player_key_pressed), totem, NULL);

	window = totem_get_main_window (totem);
	pi->handler_id = g_signal_connect (G_OBJECT (window), "focus-in-event",
			G_CALLBACK (on_window_focus_in_event), pi);

	g_object_unref (G_OBJECT (window));
}

static void
impl_deactivate	(PeasActivatable *plugin,
		 GObject *object)
{
	TotemMediaPlayerKeysPlugin *pi = TOTEM_MEDIA_PLAYER_KEYS_PLUGIN (plugin);
	TotemObject *totem = TOTEM_OBJECT (object);
	GtkWindow *window;

	if (pi->media_player_keys_proxy != NULL) {
		dbus_g_proxy_call (pi->media_player_keys_proxy,
				   "ReleaseMediaPlayerKeys", NULL,
				   G_TYPE_STRING, "Totem", G_TYPE_INVALID, G_TYPE_INVALID);
		g_object_unref (pi->media_player_keys_proxy);
		pi->media_player_keys_proxy = NULL;
	}

	if (pi->handler_id != 0) {
		window = totem_get_main_window (totem);
		if (window == NULL)
			return;

		g_signal_handler_disconnect (G_OBJECT (window), pi->handler_id);

		g_object_unref (window);
	}
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	totem_media_player_keys_plugin_register_type (G_TYPE_MODULE (module));

	peas_object_module_register_extension_type (module,
						    PEAS_TYPE_ACTIVATABLE,
						    TOTEM_TYPE_MEDIA_PLAYER_KEYS_PLUGIN);
}

