/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Javier Goday <jgoday@gmail.com>
 * Based on the sidebar-test totem plugin example 
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
#include <string.h>

#include "totem-plugin.h"
#include "totem.h"
#include "totem-tracker-widget.h"

#define TOTEM_TYPE_TRACKER_PLUGIN		(totem_tracker_plugin_get_type ())
#define TOTEM_TRACKER_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_TRACKER_PLUGIN, TotemTrackerPlugin))
#define TOTEM_TRACKER_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_TRACKER_PLUGIN, TotemTrackerPluginClass))
#define TOTEM_IS_TRACKER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_TRACKER_PLUGIN))
#define TOTEM_IS_TRACKER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_TRACKER_PLUGIN))
#define TOTEM_TRACKER_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_TRACKER_PLUGIN, TotemTrackerPluginClass))

typedef struct
{
	PeasExtensionBase parent;
} TotemTrackerPlugin;

TOTEM_PLUGIN_REGISTER (TOTEM_TYPE_TRACKER_PLUGIN, TotemTrackerPlugin, totem_tracker_plugin);

static void
totem_tracker_plugin_class_init (TotemTrackerPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = set_property;
	object_class->get_property = get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");
}

static void
totem_tracker_plugin_init (TotemTrackerPlugin *plugin)
{
}

static void
impl_activate (PeasActivatable *plugin)
{
	GtkWidget *widget;
	TotemObject *totem;

	g_object_get (plugin, "object", &totem, NULL);

	widget = totem_tracker_widget_new (totem);
	gtk_widget_show (widget);
	totem_add_sidebar_page (totem, "tracker", _("Local Search"), widget);

	g_object_unref (totem);
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemObject *totem;

	g_object_get (plugin, "object", &totem, NULL);
	totem_remove_sidebar_page (totem, "tracker");
	g_object_unref (totem);
}

