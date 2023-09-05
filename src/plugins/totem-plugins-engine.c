/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Plugin engine for Totem, heavily based on the code from Rhythmbox,
 * which is based heavily on the code from totem.
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 *               2006 James Livingston  <jrl@ids.org.au>
 *               2007 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <girepository.h>

#include "totem-dirs.h"
#include "totem-plugins-engine.h"
#include "totem-plugin-activatable.h"

struct _TotemPluginsEngine {
	GObject parent;

	PeasEngine *peas_engine;
	PeasExtensionSet *activatable_extensions;
	TotemObject *totem;
	GSettings *settings;
	guint garbage_collect_id;
};

G_DEFINE_TYPE (TotemPluginsEngine, totem_plugins_engine, G_TYPE_OBJECT)

static void totem_plugins_engine_dispose (GObject *object);

static gboolean
garbage_collect_cb (gpointer data)
{
	TotemPluginsEngine *engine = (TotemPluginsEngine *) data;
	peas_engine_garbage_collect (engine->peas_engine);
	return TRUE;
}

static void
totem_plugins_engine_class_init (TotemPluginsEngineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = totem_plugins_engine_dispose;
}

static void
on_activatable_extension_added (PeasExtensionSet *set,
				PeasPluginInfo   *info,
				GObject          *exten,
				TotemPluginsEngine *engine)
{
	totem_plugin_activatable_activate (TOTEM_PLUGIN_ACTIVATABLE (exten));
}

static void
on_activatable_extension_removed (PeasExtensionSet *set,
				  PeasPluginInfo   *info,
				  GObject          *exten,
				  TotemPluginsEngine *engine)
{
	totem_plugin_activatable_deactivate (TOTEM_PLUGIN_ACTIVATABLE (exten));
}

TotemPluginsEngine *
totem_plugins_engine_get_default (TotemObject *totem)
{
	static TotemPluginsEngine *engine = NULL;
	char **paths;
	guint i;

	if (G_LIKELY (engine != NULL))
		return g_object_ref (engine);

	g_return_val_if_fail (totem != NULL, NULL);

	g_irepository_require (g_irepository_get_default (), "Peas-2", "1.0", 0, NULL);
	g_irepository_require (g_irepository_get_default (), "Totem", TOTEM_API_VERSION, 0, NULL);

	paths = totem_get_plugin_paths ();

	engine = TOTEM_PLUGINS_ENGINE (g_object_new (TOTEM_TYPE_PLUGINS_ENGINE,
						     NULL));

	engine->peas_engine = peas_engine_new ();

	for (i = 0; paths[i] != NULL; i++) {
		/* Totem uses the libdir even for noarch data */
		peas_engine_add_search_path (engine->peas_engine,
					     paths[i], paths[i]);
	}
	g_strfreev (paths);

	peas_engine_enable_loader (engine->peas_engine, "python");

	g_object_add_weak_pointer (G_OBJECT (engine),
				   (gpointer) &engine);

	engine->totem = g_object_ref (totem);

	engine->activatable_extensions = peas_extension_set_new (engine->peas_engine,
								       TOTEM_TYPE_PLUGIN_ACTIVATABLE,
								       "object", totem,
								       NULL);

	g_signal_connect (engine->activatable_extensions, "extension-added",
			  G_CALLBACK (on_activatable_extension_added), engine);
	g_signal_connect (engine->activatable_extensions, "extension-removed",
			  G_CALLBACK (on_activatable_extension_removed), engine);

	g_settings_bind (engine->settings, "active-plugins",
			 engine->peas_engine, "loaded-plugins",
			 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	/* Load builtin plugins */
	g_object_freeze_notify (G_OBJECT (engine->peas_engine));
	for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (engine->peas_engine)); i++) {
		g_autoptr(PeasPluginInfo) plugin_info = PEAS_PLUGIN_INFO (g_list_model_get_item (G_LIST_MODEL (engine->peas_engine), i));

		if (peas_plugin_info_is_builtin (plugin_info)) {
			peas_engine_load_plugin (engine->peas_engine, plugin_info);
		}
	}
	g_object_thaw_notify (G_OBJECT (engine->peas_engine));

	return engine;
}

static void
on_plugin_shutdown (PeasExtensionSet *set,
                    PeasPluginInfo   *info,
                    GObject          *plugin,
                    gpointer          data)
{
	totem_plugin_activatable_deactivate (TOTEM_PLUGIN_ACTIVATABLE (plugin));
}

/* Necessary to break the reference cycle between activatable_extensions and the engine itself. Also useful to allow the plugins to be shut down
 * earlier than the rest of Totem, so that (for example) they can display modal save dialogues and the like. */
void
totem_plugins_engine_shut_down (TotemPluginsEngine *self)
{
	g_return_if_fail (TOTEM_IS_PLUGINS_ENGINE (self));
	g_return_if_fail (self->activatable_extensions != NULL);

	/* Disconnect from the signal handlers in case unreffing activatable_extensions doesn't finalise the PeasExtensionSet. */
	g_signal_handlers_disconnect_by_func (self->activatable_extensions, (GCallback) on_activatable_extension_added, self);
	g_signal_handlers_disconnect_by_func (self->activatable_extensions, (GCallback) on_activatable_extension_removed, self);

	/* We then explicitly deactivate all the extensions. Normally, this would be done extension-by-extension as they're unreffed when the
	 * PeasExtensionSet is finalised, but we've just removed the signal handler which would do that (extension-removed). */
	peas_extension_set_foreach (self->activatable_extensions, on_plugin_shutdown, NULL);

	g_clear_object (&self->activatable_extensions);
}

static void
totem_plugins_engine_init (TotemPluginsEngine *engine)
{
	engine->settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);

	engine->garbage_collect_id = g_timeout_add_seconds_full (G_PRIORITY_LOW, 20, garbage_collect_cb, engine, NULL);
	g_source_set_name_by_id (engine->garbage_collect_id, "[totem] garbage_collect_cb");
}

static void
totem_plugins_engine_dispose (GObject *object)
{
	TotemPluginsEngine *engine = TOTEM_PLUGINS_ENGINE (object);

	if (engine->activatable_extensions != NULL)
		totem_plugins_engine_shut_down (engine);

	g_clear_handle_id (&engine->garbage_collect_id, g_source_remove);
	peas_engine_garbage_collect (engine->peas_engine);

	g_clear_object (&engine->totem);
	g_clear_object (&engine->settings);
	g_clear_object (&engine->peas_engine);

	G_OBJECT_CLASS (totem_plugins_engine_parent_class)->dispose (object);
}

PeasEngine*
totem_plugins_engine_get_engine (TotemPluginsEngine *self)
{
	g_return_val_if_fail (TOTEM_IS_PLUGINS_ENGINE (self), NULL);

	return self->peas_engine;
}
