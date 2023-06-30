/* -*- Mode: C; indent-tabs-mode: t -*- */

/*
 * Copyright (C) 2010, 2011 Igalia S.L. <info@igalia.com>
 *
 * SPDX-License-Identifier: GPL-3-or-later
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
