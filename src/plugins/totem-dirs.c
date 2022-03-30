/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * heavily based on code from Rhythmbox and Gedit
 *
 * Copyright (C) 2002-2005 Paolo Maggi
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 *
 * Sunday 13th May 2007: Bastien Nocera: Add exception clause.
 * See license_change file for details.
 *
 */

/**
 * SECTION:totem-plugin
 * @short_description: base plugin class and loading/unloading functions
 * @stability: Unstable
 * @include: totem-dirs.h
 *
 * libpeas is used as a general-purpose architecture for adding plugins to Totem, with
 * derived support for different programming languages.
 *
 * The functions in totem-dirs.h are used to allow plugins to find and load files installed alongside the plugins, such as UI files.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "totem-dirs.h"
#include "totem-plugins-engine.h"
#include "totem-uri.h"

#define UNINSTALLED_PLUGINS_LOCATION "plugins"

/**
 * totem_get_plugin_paths:
 *
 * Return a %NULL-terminated array of paths to directories which can contain Totem plugins. This respects the GSettings disable_user_plugins setting.
 *
 * Return value: (transfer full): a %NULL-terminated array of paths to plugin directories
 *
 * Since: 2.90.0
 **/
char **
totem_get_plugin_paths (void)
{
	GPtrArray *paths;
	char  *path;
	GSettings *settings;

	paths = g_ptr_array_new ();

	settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);
	if (g_settings_get_boolean (settings, "disable-user-plugins") == FALSE) {
		path = g_build_filename (totem_data_dot_dir (), "plugins", NULL);
		g_ptr_array_add (paths, path);
	}

	g_object_unref (settings);

	path = g_strdup (TOTEM_PLUGIN_DIR);
	g_ptr_array_add (paths, path);

	/* And null-terminate the array */
	g_ptr_array_add (paths, NULL);

	return (char **) g_ptr_array_free (paths, FALSE);
}
