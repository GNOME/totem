/* GTK - The GIMP Toolkit
 * Copyright (C) 2013-2014 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author:
 * Bastien Nocera <bnocera@redhat.com>
 *
 */

/*
 * Modified by the GTK+ Team and others 2013-2014.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include "totem-selection-toolbar.h"
#include "griloresources.h"

/*
 * SECTION:totemselectiontoolbar
 * @Short_description: An toolbar with oft-used buttons.
 * @Title: TotemSelectionToolbar
 *
 * #TotemSelectionToolbar is a toolbar that contains oft-used buttons such as toggles
 * for select mode, and find mode, or a new button. The widget will also be
 * styled properly when in specific mode.
 */

struct _TotemSelectionToolbar {
  GtkBin       parent;

  /* Template widgets */
  GtkWidget   *add_to_fav;
  GtkWidget   *play;
  GtkWidget   *shuffle;
  GtkWidget   *delete;

  /* Delete button */
  gboolean     show_delete_button;
  gboolean     delete_sensitive;

  /* Selection mode */
  guint        n_selected;
};

G_DEFINE_TYPE (TotemSelectionToolbar, totem_selection_toolbar, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_SHOW_DELETE_BUTTON,
  PROP_N_SELECTED,
  PROP_DELETE_BUTTON_SENSITIVE
};

static void
change_class (GtkWidget  *widget,
              const char *class,
              gboolean    add)
{
  GtkStyleContext *style;

  style = gtk_widget_get_style_context (widget);
  if (add)
    gtk_style_context_add_class (style, class);
  else
    gtk_style_context_remove_class (style, class);
}

static void
update_toolbar_state (TotemSelectionToolbar *bar)
{
  gboolean sensitive;

  if (bar->n_selected == 0)
    {
      sensitive = FALSE;
      change_class (GTK_WIDGET (bar->delete), "destructive-action", FALSE);
    }
  else
    {
      sensitive = TRUE;
      change_class (GTK_WIDGET (bar->delete), "destructive-action", TRUE);
    }

  gtk_widget_set_sensitive (bar->add_to_fav, sensitive);
  gtk_widget_set_sensitive (bar->play, sensitive);
  gtk_widget_set_sensitive (bar->shuffle, sensitive);
}

static void
add_to_fav_clicked_cb (GtkButton        *button,
                       TotemSelectionToolbar *bar)
{
  g_signal_emit_by_name (G_OBJECT (bar), "add-to-favourites-clicked", NULL);
}

static void
delete_clicked_cb (GtkButton             *button,
                   TotemSelectionToolbar *bar)
{
  g_signal_emit_by_name (G_OBJECT (bar), "delete-clicked", NULL);
}

static void
play_clicked_cb (GtkButton             *button,
                 TotemSelectionToolbar *bar)
{
  g_signal_emit_by_name (G_OBJECT (bar), "play-clicked", NULL);
}

static void
shuffle_clicked_cb (GtkButton             *button,
                    TotemSelectionToolbar *bar)
{
  g_signal_emit_by_name (G_OBJECT (bar), "shuffle-clicked", NULL);
}

static void
totem_selection_toolbar_set_property (GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec)
{
  TotemSelectionToolbar *bar = TOTEM_SELECTION_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_N_SELECTED:
      totem_selection_toolbar_set_n_selected (bar, g_value_get_uint (value));
      break;

    case PROP_SHOW_DELETE_BUTTON:
      totem_selection_toolbar_set_show_delete_button (bar, g_value_get_boolean (value));
      break;

    case PROP_DELETE_BUTTON_SENSITIVE:
      totem_selection_toolbar_set_delete_button_sensitive (bar, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
totem_selection_toolbar_get_property (GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec)
{
  TotemSelectionToolbar *bar = TOTEM_SELECTION_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_N_SELECTED:
      g_value_set_uint (value, totem_selection_toolbar_get_n_selected (bar));
      break;

    case PROP_SHOW_DELETE_BUTTON:
      g_value_set_boolean (value, bar->show_delete_button);
      break;

    case PROP_DELETE_BUTTON_SENSITIVE:
      g_value_set_boolean (value, bar->delete_sensitive);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
totem_selection_toolbar_class_init (TotemSelectionToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = totem_selection_toolbar_set_property;
  object_class->get_property = totem_selection_toolbar_get_property;

  g_object_class_install_property (object_class,
                                   PROP_N_SELECTED,
                                   g_param_spec_uint ("n-selected",
                                                      "Number of Selected Items",
                                                      "The number of selected items",
                                                      0,
                                                      G_MAXUINT,
                                                      0,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_SHOW_DELETE_BUTTON,
                                   g_param_spec_boolean ("show-delete-button",
                                                         "Show Delete Button",
                                                         "Whether the delete button is visible",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_DELETE_BUTTON_SENSITIVE,
                                   g_param_spec_boolean ("delete-button-sensitive",
                                                         "Delete Button Sensitive",
                                                         "Whether the delete button is sensitive",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_signal_new ("add-to-favourites-clicked",
                G_OBJECT_CLASS_TYPE (klass),
                0,
                0,
                NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE, 0, G_TYPE_NONE);

  g_signal_new ("delete-clicked",
                G_OBJECT_CLASS_TYPE (klass),
                0,
                0,
                NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE, 0, G_TYPE_NONE);

  g_signal_new ("play-clicked",
                G_OBJECT_CLASS_TYPE (klass),
                0,
                0,
                NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE, 0, G_TYPE_NONE);

  g_signal_new ("shuffle-clicked",
                G_OBJECT_CLASS_TYPE (klass),
                0,
                0,
                NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE, 0, G_TYPE_NONE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/grilo/totemselectiontoolbar.ui");
  gtk_widget_class_bind_template_child (widget_class, TotemSelectionToolbar, add_to_fav);
  gtk_widget_class_bind_template_child (widget_class, TotemSelectionToolbar, delete);
  gtk_widget_class_bind_template_child (widget_class, TotemSelectionToolbar, play);
  gtk_widget_class_bind_template_child (widget_class, TotemSelectionToolbar, shuffle);

  gtk_widget_class_bind_template_callback (widget_class, add_to_fav_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, delete_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, play_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, shuffle_clicked_cb);
}

static void
totem_selection_toolbar_init (TotemSelectionToolbar *bar)
{
  gtk_widget_init_template (GTK_WIDGET (bar));

  /* So that the default FALSE actually gets applied */
  bar->delete_sensitive = TRUE;
};

/**
 * totem_selection_toolbar_new:
 *
 * Creates a #TotemSelectionToolbar.
 *
 * Return value: a new #TotemSelectionToolbar
 *
 * Since: 3.10
 **/
GtkWidget *
totem_selection_toolbar_new (void)
{
  return GTK_WIDGET (g_object_new (TOTEM_TYPE_SELECTION_TOOLBAR, NULL));
}

void
totem_selection_toolbar_set_n_selected (TotemSelectionToolbar *bar,
                                   guint             n_selected)
{
  g_return_if_fail (TOTEM_IS_SELECTION_TOOLBAR (bar));

  if (bar->n_selected == n_selected)
    return;

  bar->n_selected = n_selected;

  update_toolbar_state (bar);
  g_object_notify (G_OBJECT (bar), "n-selected");
}

guint
totem_selection_toolbar_get_n_selected (TotemSelectionToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_SELECTION_TOOLBAR (bar), 0);

  return bar->n_selected;
}

void
totem_selection_toolbar_set_show_delete_button (TotemSelectionToolbar *bar,
                                                gboolean               show_delete_button)
{
  g_return_if_fail (TOTEM_IS_SELECTION_TOOLBAR (bar));

  if (bar->show_delete_button == show_delete_button)
    return;

  bar->show_delete_button = show_delete_button;
  gtk_widget_set_visible (bar->delete, bar->show_delete_button);

  g_object_notify (G_OBJECT (bar), "show-delete-button");
}

gboolean
totem_selection_toolbar_get_show_delete_button (TotemSelectionToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_SELECTION_TOOLBAR (bar), 0);

  return bar->show_delete_button;
}

void
totem_selection_toolbar_set_delete_button_sensitive (TotemSelectionToolbar *bar,
                                                     gboolean               sensitive)
{
  g_return_if_fail (TOTEM_IS_SELECTION_TOOLBAR (bar));

  if (bar->delete_sensitive == sensitive)
    return;

  bar->delete_sensitive = sensitive;
  gtk_widget_set_sensitive (bar->delete, sensitive);

  g_object_notify (G_OBJECT (bar), "delete-button-sensitive");
}

gboolean
totem_selection_toolbar_get_delete_button_sensitive (TotemSelectionToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_SELECTION_TOOLBAR (bar), 0);

  return bar->delete_sensitive;
}
