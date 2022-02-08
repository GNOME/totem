/* -*- Mode: C; indent-tabs-mode: t -*- */

/*
 * Copyright (C) 2010, 2011 Igalia S.L. <info@igalia.com>
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

#include <gtk/gtk.h>
#include <totem.h>

#define TOTEM_TYPE_GRILO                 (totem_grilo_get_type ())
G_DECLARE_FINAL_TYPE (TotemGrilo, totem_grilo, TOTEM, GRILO, GtkBox)

typedef enum{
  TOTEM_GRILO_PAGE_RECENT,
  TOTEM_GRILO_PAGE_CHANNELS
} TotemGriloPage;

GType           totem_grilo_get_type              (void) G_GNUC_CONST;
GtkWidget*      totem_grilo_new                   (TotemObject *totem,
                                                   GtkWidget   *header);
void            totem_grilo_start                 (TotemGrilo  *self);
void            totem_grilo_pause                 (TotemGrilo  *self);
void            totem_grilo_back_button_clicked   (TotemGrilo  *self);
gboolean        totem_grilo_get_show_back_button  (TotemGrilo  *self);
void            totem_grilo_set_current_page      (TotemGrilo     *self,
                                                   TotemGriloPage  page);
TotemGriloPage  totem_grilo_get_current_page      (TotemGrilo     *self);
gboolean        totem_grilo_add_item_to_recent    (TotemGrilo     *self,
                                                   const char     *uri,
                                                   const char     *title,
                                                   gboolean        is_web);
