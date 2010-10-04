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
#include <girepository.h>
#include <libpeas/peas-activatable.h>
#include <libpeas/peas-extension-set.h>

#include "totem-dirs.h"
#include "totem-plugins-engine.h"

typedef struct _TotemPluginsEnginePrivate{
	PeasExtensionSet *activatable_extensions;
	TotemObject *totem;
	GSettings *settings;
	guint garbage_collect_id;
} _TotemPluginsEnginePrivate;

G_DEFINE_TYPE(TotemPluginsEngine, totem_plugins_engine, PEAS_TYPE_ENGINE)

static void totem_plugins_engine_finalize (GObject *object);
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

static void
on_activatable_extension_added (PeasExtensionSet *set,
				PeasPluginInfo   *info,
				PeasExtension    *exten,
				TotemPluginsEngine *engine)
{
	g_message ("on_activatable_extension_added");
	peas_extension_call (exten, "activate");
}

static void
on_activatable_extension_removed (PeasExtensionSet *set,
				  PeasPluginInfo   *info,
				  PeasExtension    *exten,
				  TotemPluginsEngine *engine)
{
	g_message ("on_activatable_extension_removed");
	peas_extension_call (exten, "deactivate");
}

TotemPluginsEngine *
totem_plugins_engine_get_default (TotemObject *totem)
{
	static TotemPluginsEngine *engine = NULL;
	char **paths;
	guint i;

	if (G_LIKELY (engine != NULL))
		return engine;

	g_return_val_if_fail (totem != NULL, NULL);

	g_irepository_require (g_irepository_get_default (), "Peas", "1.0", 0, NULL);
	g_irepository_require (g_irepository_get_default (), "PeasUI", "1.0", 0, NULL);
	g_irepository_require (g_irepository_get_default (), "Totem", TOTEM_API_VERSION, 0, NULL);

	paths = totem_get_plugin_paths ();

	engine = TOTEM_PLUGINS_ENGINE (g_object_new (TOTEM_TYPE_PLUGINS_ENGINE,
						     NULL));
	for (i = 0; paths[i] != NULL; i++) {
		/* Totem uses the libdir even for noarch data */
		peas_engine_add_search_path (PEAS_ENGINE (engine),
					     paths[i], paths[i]);
	}
	g_strfreev (paths);

	g_object_add_weak_pointer (G_OBJECT (engine),
				   (gpointer) &engine);

	engine->priv->totem = g_object_ref (totem);

	engine->priv->activatable_extensions = peas_extension_set_new (PEAS_ENGINE (engine),
								       PEAS_TYPE_ACTIVATABLE,
								       "object", totem,
								       NULL);

	g_signal_connect (engine->priv->activatable_extensions, "extension-added",
			  G_CALLBACK (on_activatable_extension_added), engine);
	g_signal_connect (engine->priv->activatable_extensions, "extension-removed",
			  G_CALLBACK (on_activatable_extension_removed), engine);

	g_settings_bind (engine->priv->settings, "active-plugins", engine, "loaded-plugins", G_SETTINGS_BIND_DEFAULT);

	return engine;
}

static void
totem_plugins_engine_init (TotemPluginsEngine *engine)
{
	engine->priv = G_TYPE_INSTANCE_GET_PRIVATE (engine,
						    TOTEM_TYPE_PLUGINS_ENGINE,
						    TotemPluginsEnginePrivate);

	engine->priv->settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);

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
		peas_extension_set_call (engine->priv->activatable_extensions, "deactivate");

		g_object_unref (engine->priv->totem);
		engine->priv->totem = NULL;
	}

	if (engine->priv->garbage_collect_id > 0)
		g_source_remove (engine->priv->garbage_collect_id);
	engine->priv->garbage_collect_id = 0;
	peas_engine_garbage_collect (PEAS_ENGINE (engine));

	if (engine->priv->settings != NULL)
		g_object_unref (engine->priv->settings);
	engine->priv->settings = NULL;

	G_OBJECT_CLASS (totem_plugins_engine_parent_class)->finalize (object);
}
