/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Plugin engine for Totem, heavily based on the code from Rhythmbox,
 * which is based heavily on the code from totem.
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 *               2006 James Livingston  <jrl@ids.org.au>
 *               2007 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 *
 * Sunday 13th May 2007: Bastien Nocera: Add exception clause.
 * See license_change file for details.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <gconf/gconf-client.h>
#include <girepository.h>
#include <libpeas/peas-activatable.h>
#include <libpeas/peas-extension-set.h>

#include "totem-dirs.h"
#include "totem-plugins-engine.h"

#define GCONF_PREFIX_PLUGINS GCONF_PREFIX"/plugins"
#define GCONF_PREFIX_PLUGIN GCONF_PREFIX"/plugins/%s"
#define GCONF_PLUGIN_ACTIVE GCONF_PREFIX_PLUGINS"/%s/active"
#define GCONF_PLUGIN_HIDDEN GCONF_PREFIX_PLUGINS"/%s/hidden"

typedef struct _TotemPluginsEnginePrivate{
	PeasExtensionSet *activatable_extensions;
	TotemObject *totem;
	GConfClient *client;
	guint notification_id;
	guint garbage_collect_id;
} _TotemPluginsEnginePrivate;

G_DEFINE_TYPE(TotemPluginsEngine, totem_plugins_engine, PEAS_TYPE_ENGINE)

static void totem_plugins_engine_finalize (GObject *object);
static void totem_plugins_engine_gconf_cb (GConfClient *gconf_client,
					   guint cnxn_id,
					   GConfEntry *entry,
					   TotemPluginsEngine *engine);
#if 0
static void totem_plugins_engine_activate_plugin (PeasEngine     *engine,
						  PeasPluginInfo *info);
static void totem_plugins_engine_deactivate_plugin (PeasEngine     *engine,
						    PeasPluginInfo *info);
#endif
static gboolean
garbage_collect_cb (gpointer data)
{
	TotemPluginsEngine *engine = (TotemPluginsEngine *) data;
	peas_engine_garbage_collect (PEAS_ENGINE (engine));
	return TRUE;
}

static void
totem_plugins_engine_class_init (TotemPluginsEngineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = totem_plugins_engine_finalize;
	g_type_class_add_private (klass, sizeof (TotemPluginsEnginePrivate));
}

static gboolean
plugin_is_builtin (PeasPluginInfo *info)
{
	const GHashTable *keys;
	const GValue *value;
	gboolean builtin;

	keys = peas_plugin_info_get_keys (info);
	if (keys == NULL)
		return FALSE;
	value = g_hash_table_lookup ((GHashTable *) keys, "Builtin");
	if (value == NULL)
		return FALSE;

	builtin = g_value_get_boolean (value);
	if (builtin != FALSE)
		peas_plugin_info_set_visible (info, FALSE);

	return builtin;
}

static void
totem_plugins_engine_load_all (TotemPluginsEngine *engine)
{
	const GList *list, *l;
	GPtrArray *activate;

	g_message ("totem_plugins_engine_load_all");

	activate = g_ptr_array_new ();
	list = peas_engine_get_plugin_list (PEAS_ENGINE (engine));
	for (l = list; l != NULL; l = l->next) {
		PeasPluginInfo *info = l->data;
		char *key_name;

		g_message ("checking peas_plugin_info_get_module_name (info) %s", peas_plugin_info_get_module_name (info));

		/* Builtin plugins are activated by default; other plugins aren't */
		if (plugin_is_builtin (info)) {
			g_ptr_array_add (activate, (gpointer) peas_plugin_info_get_module_name (info));
			g_message ("peas_plugin_info_get_module_name (info) %s, to activate", peas_plugin_info_get_module_name (info));
			continue;
		}
		key_name = g_strdup_printf (GCONF_PLUGIN_ACTIVE, peas_plugin_info_get_module_name (info));
		if (gconf_client_get_bool (engine->priv->client, key_name, NULL) != FALSE) {
			g_message ("peas_plugin_info_get_module_name (info) %s, to activate", peas_plugin_info_get_module_name (info));
			g_ptr_array_add (activate, (gpointer) peas_plugin_info_get_module_name (info));
		}
		g_free (key_name);

		key_name = g_strdup_printf (GCONF_PLUGIN_HIDDEN, peas_plugin_info_get_module_name (info));
		if (gconf_client_get_bool (engine->priv->client, key_name, NULL) != FALSE)
			peas_plugin_info_set_visible (info, FALSE);
		g_free (key_name);
	}
	g_ptr_array_add (activate, NULL);

	peas_engine_set_loaded_plugins (PEAS_ENGINE (engine), (const char **) activate->pdata);
	g_ptr_array_free (activate, TRUE);
}

static void
totem_plugins_engine_monitor (TotemPluginsEngine *engine)
{
	engine->priv->notification_id = gconf_client_notify_add (engine->priv->client,
								GCONF_PREFIX_PLUGINS,
								(GConfClientNotifyFunc)totem_plugins_engine_gconf_cb,
								engine,
								NULL,
								NULL);
}

static void
on_activatable_extension_added (PeasExtensionSet *set,
				PeasPluginInfo   *info,
				PeasExtension    *exten,
				TotemPluginsEngine *engine)
{
	g_message ("on_activatable_extension_added");
	if (peas_extension_call (exten, "activate", engine->priv->totem)) {
		if (peas_plugin_info_get_visible (info)) {
			char *key_name;

			key_name = g_strdup_printf (GCONF_PLUGIN_ACTIVE,
						    peas_plugin_info_get_module_name (info));
			gconf_client_set_bool (engine->priv->client, key_name, TRUE, NULL);
		}
	}
}

static void
on_activatable_extension_removed (PeasExtensionSet *set,
				  PeasPluginInfo   *info,
				  PeasExtension    *exten,
				  TotemPluginsEngine *engine)
{
	g_message ("on_activatable_extension_removed");
	peas_extension_call (exten, "deactivate", engine->priv->totem);

	if (peas_plugin_info_get_visible (info)) {
		char *key_name;

		key_name = g_strdup_printf (GCONF_PLUGIN_ACTIVE,
					    peas_plugin_info_get_module_name (info));
		gconf_client_set_bool (engine->priv->client, key_name, FALSE, NULL);
	}
}

TotemPluginsEngine *
totem_plugins_engine_get_default (TotemObject *totem)
{
	static TotemPluginsEngine *engine = NULL;
	char **paths;
	GPtrArray *array;
	guint i;

	if (G_LIKELY (engine != NULL))
		return engine;

	g_return_val_if_fail (totem != NULL, NULL);

	g_irepository_require (g_irepository_get_default (), "Peas", "1.0", 0, NULL);

	paths = totem_get_plugin_paths ();

	/* Totem uses the libdir even for noarch data */
	array = g_ptr_array_new ();
	for (i = 0; paths[i] != NULL; i++) {
		g_ptr_array_add (array, paths[i]);
		g_ptr_array_add (array, paths[i]);
	}
	g_ptr_array_add (array, NULL);

	engine = TOTEM_PLUGINS_ENGINE (g_object_new (TOTEM_TYPE_PLUGINS_ENGINE,
						     "app-name", "Totem",
						     "search-paths", array->pdata,
						     "base-module-dir", TOTEM_PLUGIN_DIR,
						     NULL));
	g_strfreev (paths);
	g_ptr_array_free (array, TRUE);

	g_object_add_weak_pointer (G_OBJECT (engine),
				   (gpointer) &engine);

	/* FIXME
	 * Disable python loader for now */
	peas_engine_disable_loader (PEAS_ENGINE (engine), "python");

	engine->priv->totem = g_object_ref (totem);

	engine->priv->activatable_extensions = peas_extension_set_new (PEAS_ENGINE (engine),
								       PEAS_TYPE_ACTIVATABLE);
	totem_plugins_engine_load_all (engine);
	totem_plugins_engine_monitor (engine);

	peas_extension_set_call (engine->priv->activatable_extensions, "activate", engine->priv->totem);

	g_signal_connect (engine->priv->activatable_extensions, "extension-added",
			  G_CALLBACK (on_activatable_extension_added), engine);
	g_signal_connect (engine->priv->activatable_extensions, "extension-removed",
			  G_CALLBACK (on_activatable_extension_removed), engine);

	return engine;
}

static void
totem_plugins_engine_init (TotemPluginsEngine *engine)
{
	engine->priv = G_TYPE_INSTANCE_GET_PRIVATE (engine,
						    TOTEM_TYPE_PLUGINS_ENGINE,
						    TotemPluginsEnginePrivate);
	engine->priv->client = gconf_client_get_default ();
	gconf_client_add_dir (engine->priv->client, GCONF_PREFIX_PLUGINS, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

	/* Commented out because it's a no-op. A further section is commented out below, and more's commented out
	 * in totem-python-module.c. */
	engine->priv->garbage_collect_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, 20, garbage_collect_cb, engine, NULL);
}

static void
totem_plugins_engine_finalize (GObject *object)
{
	TotemPluginsEngine *engine = TOTEM_PLUGINS_ENGINE (object);

	g_signal_handlers_disconnect_by_func (engine->priv->activatable_extensions,
					      G_CALLBACK (on_activatable_extension_added), engine);
	g_signal_handlers_disconnect_by_func (engine->priv->activatable_extensions,
					      G_CALLBACK (on_activatable_extension_removed), engine);

	if (engine->priv->totem) {
		peas_extension_set_call (engine->priv->activatable_extensions,
					 "deactivate", engine->priv->totem);

		g_object_unref (engine->priv->totem);
		engine->priv->totem = NULL;
	}

	if (engine->priv->garbage_collect_id > 0)
		g_source_remove (engine->priv->garbage_collect_id);
	engine->priv->garbage_collect_id = 0;
	peas_engine_garbage_collect (PEAS_ENGINE (engine));

	if (engine->priv->notification_id > 0)
		gconf_client_notify_remove (engine->priv->client,
					    engine->priv->notification_id);
	engine->priv->notification_id = 0;

	if (engine->priv->client != NULL)
		g_object_unref (engine->priv->client);
	engine->priv->client = NULL;

	G_OBJECT_CLASS (totem_plugins_engine_parent_class)->finalize (object);
}

static void
totem_plugins_engine_gconf_cb (GConfClient *gconf_client,
			       guint cnxn_id,
			       GConfEntry *entry,
			       TotemPluginsEngine *engine)
{
	char *dirname;
	char *plugin_name;
	char *action_name;
	PeasPluginInfo *info;

	dirname = g_path_get_dirname (gconf_entry_get_key (entry));
	plugin_name = g_path_get_basename (dirname);
	g_free (dirname);

	info = peas_engine_get_plugin_info (PEAS_ENGINE (engine), plugin_name);
	g_free (plugin_name);

	if (info == NULL)
		return;

	action_name = g_path_get_basename (gconf_entry_get_key (entry));
	if (action_name == NULL)
		return;

	if (g_str_equal (action_name, "active") != FALSE) {
		if (gconf_value_get_bool (entry->value)) {
			peas_engine_load_plugin (PEAS_ENGINE (engine), info);
		} else {
			peas_engine_unload_plugin (PEAS_ENGINE (engine), info);
		}
	} else if (g_str_equal (action_name, "hidden") != FALSE) {
		peas_plugin_info_set_visible (info, !gconf_value_get_bool (entry->value));
	}

	g_free (action_name);
}

