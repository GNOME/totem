/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) Matthieu Gautier 2015 <dev@mgautier.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>

#include "totem-plugin.h"
#include "totem-interface.h"

#define TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN		(totem_open_directory_plugin_get_type ())
#define TOTEM_OPEN_DIRECTORY_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN, TotemOpenDirectoryPlugin))
#define TOTEM_OPEN_DIRECTORY_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN, TotemOpenDirectoryPluginClass))
#define TOTEM_IS_OPEN_DIRECTORY_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN))
#define TOTEM_IS_OPEN_DIRECTORY_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN))
#define TOTEM_OPEN_DIRECTORY_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN, TotemOpenDirectoryPluginClass))

typedef struct {
	TotemObject *totem;
	char        *mrl;
	GSimpleAction *action;
} TotemOpenDirectoryPluginPrivate;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_OPEN_DIRECTORY_PLUGIN, TotemOpenDirectoryPlugin, totem_open_directory_plugin)

static char *
get_notification_id (void)
{
	GTimeVal time;

	g_get_current_time (&time);
	return g_strdup_printf ("%s_TIME%ld", "totem", time.tv_sec);
}

static void
totem_open_directory_plugin_open (GSimpleAction       *action,
			     GVariant            *parameter,
			     TotemOpenDirectoryPlugin *pi)
{


	GError *error = NULL;
	GDBusProxy *proxy;
	gchar* notification_id;
	GVariantBuilder *builder;
	GVariant *dbus_arguments;

	g_assert (pi->priv->mrl != NULL);

	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
					       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
					       NULL, /* GDBusInterfaceInfo */
					       "org.freedesktop.FileManager1",
					       "/org/freedesktop/FileManager1",
					       "org.freedesktop.FileManager1",
					       NULL, /* GCancellable */
					       &error);
	if (proxy == NULL) {
		g_warning ("Could not contact file manager: %s", error->message);
		g_error_free (error);
		return;
	}

	notification_id = get_notification_id();

	builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
	g_variant_builder_add (builder, "s", pi->priv->mrl);
	dbus_arguments = g_variant_new ("(ass)", builder, notification_id);
	g_variant_builder_unref (builder);
	g_free(notification_id);

	if (g_dbus_proxy_call_sync (proxy,
				"ShowItems", dbus_arguments,
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, &error) == FALSE) {
		g_warning ("Could not get file manager to display file: %s", error->message);
		g_error_free (error);
	}

	g_object_unref (proxy);
}

static void
totem_open_directory_file_closed (TotemObject *totem,
				 TotemOpenDirectoryPlugin *pi)
{
	g_clear_pointer (&pi->priv->mrl, g_free);

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->priv->action), FALSE);
}

static gboolean
scheme_is_supported (const char *scheme)
{
	const gchar * const *schemes;
	guint i;

	if (!scheme)
		return FALSE;

	if (g_str_equal (scheme, "http") ||
	    g_str_equal (scheme, "https"))
		return FALSE;

	schemes = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
	for (i = 0; schemes[i] != NULL; i++) {
		if (g_str_equal (schemes[i], scheme))
			return TRUE;
	}
	return FALSE;
}

static void
totem_open_directory_file_opened (TotemObject              *totem,
				  const char               *mrl,
				  TotemOpenDirectoryPlugin *pi)
{
	char *scheme;

	g_clear_pointer (&pi->priv->mrl, g_free);

	if (mrl == NULL)
		return;

	scheme = g_uri_parse_scheme (mrl);
	if (!scheme_is_supported (scheme)) {
		g_debug ("Not enabling open-directory as scheme for '%s' not supported", mrl);
		g_free (scheme);
		return;
	}
	g_free (scheme);

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->priv->action), TRUE);
	pi->priv->mrl = g_strdup (mrl);
}

static void
impl_activate (PeasActivatable *plugin)
{
	TotemOpenDirectoryPlugin *pi = TOTEM_OPEN_DIRECTORY_PLUGIN (plugin);
	TotemOpenDirectoryPluginPrivate *priv = pi->priv;
	GMenu *menu;
	GMenuItem *item;
	char *mrl;

	/* FIXME: This plugin will stop working if the file is outside
	 * what the Flatpak has access to
	if (g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
		return; */

	priv->totem = g_object_get_data (G_OBJECT (plugin), "object");

	g_signal_connect (priv->totem,
			  "file-opened",
			  G_CALLBACK (totem_open_directory_file_opened),
			  plugin);
	g_signal_connect (priv->totem,
			  "file-closed",
			  G_CALLBACK (totem_open_directory_file_closed),
			  plugin);

	priv->action = g_simple_action_new ("open-dir", NULL);
	g_signal_connect (G_OBJECT (priv->action), "activate",
			  G_CALLBACK (totem_open_directory_plugin_open), plugin);
	g_action_map_add_action (G_ACTION_MAP (priv->totem), G_ACTION (priv->action));
	g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->action), FALSE);

	/* add UI */
	menu = totem_object_get_menu_section (priv->totem, "opendirectory-placeholder");
	item = g_menu_item_new (_("Open Containing Folder"), "app.open-dir");
	g_menu_append_item (G_MENU (menu), item);

	mrl = totem_object_get_current_mrl (priv->totem);
	totem_open_directory_file_opened (priv->totem, mrl, pi);
	g_free (mrl);
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemOpenDirectoryPlugin *pi = TOTEM_OPEN_DIRECTORY_PLUGIN (plugin);
	TotemOpenDirectoryPluginPrivate *priv = pi->priv;

	g_signal_handlers_disconnect_by_func (priv->totem, totem_open_directory_file_opened, plugin);
	g_signal_handlers_disconnect_by_func (priv->totem, totem_open_directory_file_closed, plugin);

	totem_object_empty_menu_section (priv->totem, "opendirectory-placeholder");

	priv->totem = NULL;

	g_clear_pointer (&pi->priv->mrl, g_free);
}
