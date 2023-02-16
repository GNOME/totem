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
void            totem_player_toolbar_set_search_mode       (TotemPlayerToolbar *bar,
							  gboolean          search_mode);
gboolean        totem_player_toolbar_get_search_mode       (TotemPlayerToolbar *bar);
void            totem_player_toolbar_set_select_mode       (TotemPlayerToolbar *bar,
							  gboolean          select_mode);
gboolean        totem_player_toolbar_get_select_mode       (TotemPlayerToolbar *bar);
void            totem_player_toolbar_set_title             (TotemPlayerToolbar *bar,
							  const char       *title);
const char *    totem_player_toolbar_get_title             (TotemPlayerToolbar *bar);
void            totem_player_toolbar_set_subtitle          (TotemPlayerToolbar *bar,
							  const char       *subtitle);
const char *    totem_player_toolbar_get_subtitle          (TotemPlayerToolbar *bar);
void            totem_player_toolbar_set_search_string     (TotemPlayerToolbar *bar,
						          const char       *search_string);
const char *    totem_player_toolbar_get_search_string     (TotemPlayerToolbar *bar);
void            totem_player_toolbar_set_n_selected        (TotemPlayerToolbar *bar,
							  guint             n_selected);
guint           totem_player_toolbar_get_n_selected        (TotemPlayerToolbar *bar);
void            totem_player_toolbar_set_custom_title      (TotemPlayerToolbar *bar,
							  GtkWidget        *title_widget);
GtkWidget *     totem_player_toolbar_get_custom_title      (TotemPlayerToolbar *bar);
void            totem_player_toolbar_set_select_menu_model (TotemPlayerToolbar *bar,
							  GMenuModel       *model);
GMenuModel *    totem_player_toolbar_get_select_menu_model (TotemPlayerToolbar *bar);
void            totem_player_toolbar_pack_start            (TotemPlayerToolbar *bar,
							  GtkWidget        *child);
void            totem_player_toolbar_pack_end              (TotemPlayerToolbar *bar,
							  GtkWidget        *child);
