/*
 * Copyright (C) 2022 Red Hat Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */

#include "totem.h"
#include "totem-playlist-inspector-page.h"

void
g_io_module_load (GIOModule *module)
{
  GApplication *app;

  app = g_application_get_default ();
  if (!app)
    return;
  if (!g_str_has_prefix (g_application_get_application_id (app), "org.gnome.Totem"))
    return;

  g_type_module_use (G_TYPE_MODULE (module));

  g_io_extension_point_implement ("gtk-inspector-page",
				  TOTEM_TYPE_PLAYLIST_INSPECTOR_PAGE,
				  "totem-playlist",
				  10);
}

void
g_io_module_unload (GIOModule *module)
{
}
