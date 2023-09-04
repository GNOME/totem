/*
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Krifa75 <yahiaoui.fakhri@gmail.com>
 */

#pragma once

#include <handy.h>
#include <libpeas.h>

#define TOTEM_TYPE_PREFERENCES_PLUGIN_ROW              (totem_preferences_plugin_row_get_type ())
G_DECLARE_FINAL_TYPE(TotemPreferencesPluginRow, totem_preferences_plugin_row, TOTEM, PREFERENCES_PLUGIN_ROW, HdyExpanderRow)

GType		 totem_preferences_plugins_engine_get_type	(void) G_GNUC_CONST;

GtkWidget	*totem_preferences_plugin_row_new		(PeasPluginInfo *plugin_info);
