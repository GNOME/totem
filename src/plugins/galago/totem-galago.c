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
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
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
#include "totem-galago.h"

#define TOTEM_TYPE_GALAGO_PLUGIN		(totem_galago_plugin_get_type ())
#define TOTEM_GALAGO_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_GALAGO_PLUGIN, TotemGalagoPlugin))
#define TOTEM_GALAGO_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_GALAGO_PLUGIN, TotemGalagoPluginClass))
#define TOTEM_IS_GALAGO_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_GALAGO_PLUGIN))
#define TOTEM_IS_GALAGO_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_GALAGO_PLUGIN))
#define TOTEM_GALAGO_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_GALAGO_PLUGIN, TotemGalagoPluginClass))

typedef struct
{
	TotemPlugin   parent;

	TotemObject   *totem;
	TotemGalago   *ggo;
	guint          handler_id;
} TotemGalagoPlugin;

typedef struct
{
	TotemPluginClass parent_class;
} TotemGalagoPluginClass;


G_MODULE_EXPORT GType register_totem_plugin		(GTypeModule *module);
GType	totem_galago_plugin_get_type			(void) G_GNUC_CONST;

static void totem_galago_plugin_init			(TotemGalagoPlugin *plugin);
static void totem_galago_plugin_finalize		(GObject *object);
static void impl_activate				(TotemPlugin *plugin, TotemObject *totem);
static void impl_deactivate				(TotemPlugin *plugin, TotemObject *totem);

TOTEM_PLUGIN_REGISTER(TotemGalagoPlugin, totem_galago_plugin)

static void
totem_galago_plugin_class_init (TotemGalagoPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	TotemPluginClass *plugin_class = TOTEM_PLUGIN_CLASS (klass);

	object_class->finalize = totem_galago_plugin_finalize;

	plugin_class->activate = impl_activate;
	plugin_class->deactivate = impl_deactivate;
}

static void
totem_galago_plugin_init (TotemGalagoPlugin *plugin)
{
	plugin->ggo = totem_galago_new ();
}

static void
totem_galago_plugin_finalize (GObject *object)
{
	TotemGalagoPlugin *plugin = TOTEM_GALAGO_PLUGIN (object);

	g_object_unref (plugin->ggo);

	G_OBJECT_CLASS (totem_galago_plugin_parent_class)->finalize (object);
}

static void
totem_galago_update_from_state (TotemObject *totem,
				     TotemGalagoPlugin *pi)
{
	if (totem_is_playing (totem) != FALSE
	    && totem_is_fullscreen (totem) != FALSE) {
		totem_galago_idle (pi->ggo);
	} else {
		totem_galago_not_idle (pi->ggo);
	}
}

static void
property_notify_cb (TotemObject *totem,
		    GParamSpec *spec,
		    TotemGalagoPlugin *pi)
{
	if (strcmp ("fullscreen", spec->name) != 0
	    && strcmp ("playing", spec->name) != 0) {
		return;
	}

	totem_galago_update_from_state (totem, pi);
}

static void
impl_activate (TotemPlugin *plugin,
	       TotemObject *totem)
{
	TotemGalagoPlugin *pi = TOTEM_GALAGO_PLUGIN (plugin);

	pi->handler_id = g_signal_connect (G_OBJECT (totem),
					   "notify",
					   G_CALLBACK (property_notify_cb),
					   pi);

	/* Force setting the current status */
	totem_galago_update_from_state (totem, pi);
}

static void
impl_deactivate	(TotemPlugin *plugin,
		 TotemObject *totem)
{
	TotemGalagoPlugin *pi = TOTEM_GALAGO_PLUGIN (plugin);

	g_signal_handler_disconnect (G_OBJECT (totem), pi->handler_id);
	totem_galago_not_idle (pi->ggo);
}
