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

#ifndef TOTEM_LIBRARY_H
#define TOTEM_LIBRARY_H

#include <gtk/gtk.h>
#include <totem.h>

G_BEGIN_DECLS

#define TOTEM_TYPE_GRILO                 (totem_library_get_type ())
#define TOTEM_LIBRARY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TOTEM_TYPE_GRILO, TotemLibrary))
#define TOTEM_LIBRARY_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_GRILO, TotemLibraryClass))
#define TOTEM_IS_GRILO(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TOTEM_TYPE_GRILO))
#define TOTEM_IS_GRILO_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_GRILO))
#define TOTEM_LIBRARY_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), TOTEM_TYPE_GRILO, TotemLibraryClass))

typedef struct _TotemLibrary        TotemLibrary;
typedef struct _TotemLibraryPrivate TotemLibraryPrivate;
typedef struct _TotemLibraryClass   TotemLibraryClass;

struct _TotemLibrary
{
  /*< private >*/
  GtkBox parent;

  TotemLibraryPrivate *priv;
};

struct _TotemLibraryClass
{
  GtkBoxClass parent_class;
};

typedef enum{
  TOTEM_LIBRARY_PAGE_VIDEOS,
  TOTEM_LIBRARY_PAGE_CHANNELS
} TotemLibraryPage;

GType           totem_library_get_type              (void) G_GNUC_CONST;
GtkWidget*      totem_library_new                   (TotemObject *totem,
                                                   GtkWidget   *header);
void            totem_library_start                 (TotemLibrary  *self);
void            totem_library_pause                 (TotemLibrary  *self);
void            totem_library_back_button_clicked   (TotemLibrary  *self);
gboolean        totem_library_get_show_back_button  (TotemLibrary  *self);
void            totem_library_set_current_page      (TotemLibrary     *self,
                                                   TotemLibraryPage  page);
TotemLibraryPage  totem_library_get_current_page      (TotemLibrary     *self);
gboolean        totem_library_add_item_to_recent    (TotemLibrary     *self,
                                                   const char     *uri,
                                                   const char     *title,
                                                   gboolean        is_web);

G_END_DECLS

#endif /* TOTEM_LIBRARY_H */
