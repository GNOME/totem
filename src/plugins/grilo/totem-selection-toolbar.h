/* GTK - The GIMP Toolkit
 * Copyright (C) 2013-2014 Red Hat, Inc.
 *
 * Authors:
 * - Bastien Nocera <bnocera@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 2013.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __TOTEM_SELECTION_TOOLBAR_H__
#define __TOTEM_SELECTION_TOOLBAR_H__

#include <gtk/gtkbox.h>

G_BEGIN_DECLS

#define TOTEM_TYPE_SELECTION_TOOLBAR                 (totem_selection_toolbar_get_type ())
#define TOTEM_SELECTION_TOOLBAR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TOTEM_TYPE_SELECTION_TOOLBAR, TotemSelectionToolbar))
#define TOTEM_SELECTION_TOOLBAR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_SELECTION_TOOLBAR, TotemSelectionToolbarClass))
#define TOTEM_IS_SELECTION_TOOLBAR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TOTEM_TYPE_SELECTION_TOOLBAR))
#define TOTEM_IS_SELECTION_TOOLBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_SELECTION_TOOLBAR))
#define TOTEM_SELECTION_TOOLBAR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), TOTEM_TYPE_SELECTION_TOOLBAR, TotemSelectionToolbarClass))

typedef struct _TotemSelectionToolbar        TotemSelectionToolbar;
typedef struct _TotemSelectionToolbarPrivate TotemSelectionToolbarPrivate;
typedef struct _TotemSelectionToolbarClass   TotemSelectionToolbarClass;

struct _TotemSelectionToolbar
{
  /*< private >*/
  GtkActionBar parent;

  TotemSelectionToolbarPrivate *priv;
};

struct _TotemSelectionToolbarClass
{
  GtkActionBarClass parent_class;
};

GType           totem_selection_toolbar_get_type               (void) G_GNUC_CONST;
GtkWidget*      totem_selection_toolbar_new                    (void);

void            totem_selection_toolbar_set_n_selected         (TotemSelectionToolbar *bar,
                                                               guint                  n_selected);
guint           totem_selection_toolbar_get_n_selected         (TotemSelectionToolbar *bar);

void            totem_selection_toolbar_set_show_delete_button (TotemSelectionToolbar *bar,
                                                                gboolean               show_delete_button);
gboolean        totem_selection_toolbar_get_show_delete_button (TotemSelectionToolbar *bar);

void            totem_selection_toolbar_set_delete_button_sensitive (TotemSelectionToolbar *bar,
                                                                     gboolean               sensitive);
gboolean        totem_selection_toolbar_get_delete_button_sensitive (TotemSelectionToolbar *bar);

G_END_DECLS

#endif /* __TOTEM_SELECTION_TOOLBAR_H__ */
