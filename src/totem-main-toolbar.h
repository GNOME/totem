/* GTK - The GIMP Toolkit
 * Copyright (C) 2013-2014 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author:
 * Bastien Nocera <bnocera@redhat.com>
 */

/*
 * Modified by the GTK+ Team and others 2013.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#include <handy.h>

#define TOTEM_TYPE_MAIN_TOOLBAR                 (totem_main_toolbar_get_type ())
G_DECLARE_FINAL_TYPE (TotemMainToolbar, totem_main_toolbar, TOTEM, MAIN_TOOLBAR, GtkBin)

GType           totem_main_toolbar_get_type              (void) G_GNUC_CONST;
GtkWidget*      totem_main_toolbar_new                   (void);
void            totem_main_toolbar_set_search_mode       (TotemMainToolbar *bar,
							  gboolean          search_mode);
gboolean        totem_main_toolbar_get_search_mode       (TotemMainToolbar *bar);
void            totem_main_toolbar_set_select_mode       (TotemMainToolbar *bar,
							  gboolean          select_mode);
gboolean        totem_main_toolbar_get_select_mode       (TotemMainToolbar *bar);
void            totem_main_toolbar_set_title             (TotemMainToolbar *bar,
							  const char       *title);
const char *    totem_main_toolbar_get_title             (TotemMainToolbar *bar);
void            totem_main_toolbar_set_subtitle          (TotemMainToolbar *bar,
							  const char       *subtitle);
const char *    totem_main_toolbar_get_subtitle          (TotemMainToolbar *bar);
void            totem_main_toolbar_set_search_string     (TotemMainToolbar *bar,
						          const char       *search_string);
const char *    totem_main_toolbar_get_search_string     (TotemMainToolbar *bar);
void            totem_main_toolbar_set_n_selected        (TotemMainToolbar *bar,
							  guint             n_selected);
guint           totem_main_toolbar_get_n_selected        (TotemMainToolbar *bar);
void            totem_main_toolbar_set_custom_title      (TotemMainToolbar *bar,
							  GtkWidget        *title_widget);
GtkWidget *     totem_main_toolbar_get_custom_title      (TotemMainToolbar *bar);
void            totem_main_toolbar_set_select_menu_model (TotemMainToolbar *bar,
							  GMenuModel       *model);
GMenuModel *    totem_main_toolbar_get_select_menu_model (TotemMainToolbar *bar);
void            totem_main_toolbar_pack_start            (TotemMainToolbar *bar,
							  GtkWidget        *child);
void            totem_main_toolbar_pack_end              (TotemMainToolbar *bar,
							  GtkWidget        *child);
GtkWidget *     totem_main_toolbar_get_add_button        (TotemMainToolbar *bar);
GtkWidget *     totem_main_toolbar_get_main_menu_button  (TotemMainToolbar *bar);
