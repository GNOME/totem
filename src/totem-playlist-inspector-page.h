/*
 * Copyright (C) 2022 Red Hat Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include <gtk/gtk.h>

#define TOTEM_TYPE_PLAYLIST_INSPECTOR_PAGE (totem_playlist_inspector_page_get_type())
G_DECLARE_FINAL_TYPE (TotemPlaylistInspectorPage, totem_playlist_inspector_page, TOTEM, PLAYLIST_INSPECTOR_PAGE, GtkBox)
