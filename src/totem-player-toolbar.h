/*
 * Copyright (C) 2022 Red Hat Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include <handy.h>

#define TOTEM_TYPE_PLAYER_TOOLBAR                 (totem_player_toolbar_get_type ())
G_DECLARE_FINAL_TYPE (TotemPlayerToolbar, totem_player_toolbar, TOTEM, PLAYER_TOOLBAR, HdyHeaderBar)

GType           totem_player_toolbar_get_type              (void) G_GNUC_CONST;
GtkWidget*      totem_player_toolbar_new                   (void);
void            totem_player_toolbar_set_title             (TotemPlayerToolbar *bar,
							  const char       *title);
const char *    totem_player_toolbar_get_title             (TotemPlayerToolbar *bar);
