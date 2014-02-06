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

#ifndef TOTEM_GRILO_H
#define TOTEM_GRILO_H

#include <gtk/gtk.h>
#include <totem.h>

G_BEGIN_DECLS

#define TOTEM_TYPE_GRILO                 (totem_grilo_get_type ())
#define TOTEM_GRILO(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TOTEM_TYPE_GRILO, TotemGrilo))
#define TOTEM_GRILO_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_GRILO, TotemGriloClass))
#define TOTEM_IS_GRILO(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TOTEM_TYPE_GRILO))
#define TOTEM_IS_GRILO_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_GRILO))
#define TOTEM_GRILO_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), TOTEM_TYPE_GRILO, TotemGriloClass))

typedef struct _TotemGrilo        TotemGrilo;
typedef struct _TotemGriloPrivate TotemGriloPrivate;
typedef struct _TotemGriloClass   TotemGriloClass;

struct _TotemGrilo
{
  /*< private >*/
  GtkBox parent;

  TotemGriloPrivate *priv;
};

struct _TotemGriloClass
{
  GtkBoxClass parent_class;
};

typedef enum{
  TOTEM_GRILO_PAGE_RECENT,
  TOTEM_GRILO_PAGE_CHANNELS
} TotemGriloPage;

GType           totem_grilo_get_type              (void) G_GNUC_CONST;
GtkWidget*      totem_grilo_new                   (TotemObject *totem,
                                                   GtkWidget   *header);
void            totem_grilo_back_button_clicked   (TotemGrilo  *self);
gboolean        totem_grilo_get_show_back_button  (TotemGrilo  *self);
void            totem_grilo_set_current_page      (TotemGrilo     *self,
                                                   TotemGriloPage  page);

G_END_DECLS

#endif /* TOTEM_GRILO_H */
