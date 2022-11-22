/*
 *
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
 * The Totem project hereby grant permission for non-GPL compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * See license_change file for details.
 */

#pragma once

#include <handy.h>
#include <libpeas/peas-plugin-info.h>

#define TOTEM_TYPE_PREFERENCES_PLUGIN_ROW              (totem_preferences_plugin_row_get_type ())
G_DECLARE_FINAL_TYPE(TotemPreferencesPluginRow, totem_preferences_plugin_row, TOTEM, PREFERENCES_PLUGIN_ROW, HdyExpanderRow)

GType		 totem_preferences_plugins_engine_get_type	(void) G_GNUC_CONST;

GtkWidget	*totem_preferences_plugin_row_new		(PeasPluginInfo *plugin_info);
