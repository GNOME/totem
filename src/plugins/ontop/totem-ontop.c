/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include <string.h>

#include "totem-plugin.h"
#include "totem.h"

#define TOTEM_TYPE_ONTOP_PLUGIN		(totem_ontop_plugin_get_type ())
#define TOTEM_ONTOP_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_ONTOP_PLUGIN, TotemOntopPlugin))
#define TOTEM_ONTOP_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_ONTOP_PLUGIN, TotemOntopPluginClass))
#define TOTEM_IS_ONTOP_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_ONTOP_PLUGIN))
#define TOTEM_IS_ONTOP_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_ONTOP_PLUGIN))
#define TOTEM_ONTOP_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_ONTOP_PLUGIN, TotemOntopPluginClass))

typedef struct
{
	TotemPlugin   parent;

	GtkWindow     *window;
	guint          handler_id;
} TotemOntopPlugin;

typedef struct
{
	TotemPluginClass parent_class;
} TotemOntopPluginClass;


G_MODULE_EXPORT GType register_totem_plugin		(GTypeModule *module);
GType	totem_ontop_plugin_get_type		(void) G_GNUC_CONST;

static void totem_ontop_plugin_init		(TotemOntopPlugin *plugin);
static void totem_ontop_plugin_finalize		(GObject *object);
static gboolean impl_activate			(TotemPlugin *plugin, TotemObject *totem, GError **error);
static void impl_deactivate			(TotemPlugin *plugin, TotemObject *totem);

TOTEM_PLUGIN_REGISTER(TotemOntopPlugin, totem_ontop_plugin)

static void
totem_ontop_plugin_class_init (TotemOntopPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TotemPluginClass *plugin_class = TOTEM_PLUGIN_CLASS (klass);

	object_class->finalize = totem_ontop_plugin_finalize;

	plugin_class->activate = impl_activate;
	plugin_class->deactivate = impl_deactivate;
}

static void
totem_ontop_plugin_init (TotemOntopPlugin *plugin)
{
}

static void
totem_ontop_plugin_finalize (GObject *object)
{
	G_OBJECT_CLASS (totem_ontop_plugin_parent_class)->finalize (object);
}

static void
totem_ontop_update_from_state (TotemObject *totem,
			       TotemOntopPlugin *pi)
{
	GtkWindow *window;

	window = totem_get_main_window (totem);
	if (window == NULL)
		return;

	gtk_window_set_keep_above (window,
				   totem_is_playing (totem) != FALSE);
	g_object_unref (window);
}

static void
property_notify_cb (TotemObject *totem,
		    GParamSpec *spec,
		    TotemOntopPlugin *pi)
{
	totem_ontop_update_from_state (totem, pi);
}

static gboolean
impl_activate (TotemPlugin *plugin,
	       TotemObject *totem,
	       GError **error)
{
	TotemOntopPlugin *pi = TOTEM_ONTOP_PLUGIN (plugin);

	pi->handler_id = g_signal_connect (G_OBJECT (totem),
					   "notify::playing",
					   G_CALLBACK (property_notify_cb),
					   pi);

	totem_ontop_update_from_state (totem, pi);

	return TRUE;
}

static void
impl_deactivate	(TotemPlugin *plugin,
		 TotemObject *totem)
{
	TotemOntopPlugin *pi = TOTEM_ONTOP_PLUGIN (plugin);
	GtkWindow *window;

	g_signal_handler_disconnect (G_OBJECT (totem), pi->handler_id);

	window = totem_get_main_window (totem);
	if (window == NULL)
		return;

	/* We can't really "restore" the previous state, as there's
	 * no way to find the old state */
	gtk_window_set_keep_above (window, FALSE);
	g_object_unref (window);
}

