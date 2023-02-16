/*
 * Copyright (C) 2022 Red Hat Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include "totem-player-toolbar.h"
#include "griloresources.h"

/*
 * SECTION:totemplayertoolbar
 * @Short_description: An toolbar for the player view.
 * @Title: TotemPlayerToolbar
 *
 * #TotemPlayerToolbar is a toolbar that contains buttons like fullscreen/unfullscreen mode,
 * or the subtitles menu.
 *
 * Since: 3.10
 */

struct _TotemPlayerToolbar {
  GtkBin       parent;

  GtkWidget   *header_bar;

  /* Template widgets */
  GtkWidget   *back_button;
  GtkWidget   *fullscreen_button;
  GtkWidget   *unfullscreen_button;

  // Menus
  GtkWidget   *subtitles_menu_button;
  GtkWidget   *player_menu_button;
};

G_DEFINE_TYPE(TotemPlayerToolbar, totem_player_toolbar, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_SUBTITLES_MENU_MODEL,
  PROP_PLAYER_MENU_MODEL
};

static void
back_button_clicked_cb (GtkButton        *button,
                        TotemPlayerToolbar *bar)
{
  g_signal_emit_by_name (G_OBJECT (bar), "back-clicked", NULL);
}

static void
totem_player_toolbar_set_property (GObject         *object,
                                   guint            prop_id,
                                   const GValue    *value,
                                   GParamSpec      *pspec)
{
  TotemPlayerToolbar *self = TOTEM_PLAYER_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_SUBTITLES_MENU_MODEL:
      gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->subtitles_menu_button), g_value_get_object (value));
      break;

    case PROP_PLAYER_MENU_MODEL:
      gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->player_menu_button), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
totem_player_toolbar_get_property (GObject         *object,
                                   guint            prop_id,
                                   GValue          *value,
                                   GParamSpec      *pspec)
{
  TotemPlayerToolbar *self = TOTEM_PLAYER_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_SUBTITLES_MENU_MODEL:
      g_value_set_object (value, gtk_menu_button_get_menu_model (GTK_MENU_BUTTON (self->subtitles_menu_button)));
      break;

    case PROP_PLAYER_MENU_MODEL:
      g_value_set_object (value, gtk_menu_button_get_menu_model (GTK_MENU_BUTTON (self->player_menu_button)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
totem_player_toolbar_class_init (TotemPlayerToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = totem_player_toolbar_set_property;
  object_class->get_property = totem_player_toolbar_get_property;

  g_object_class_install_property (object_class,
                                   PROP_SUBTITLES_MENU_MODEL,
                                   g_param_spec_object ("subtitles-menu-model",
                                                        "subtitles-menu-model",
                                                        "The subtitles dropdown menu's model.",
                                                        G_TYPE_MENU_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_PLAYER_MENU_MODEL,
                                   g_param_spec_object ("player-menu-model",
                                                        "player-menu-model",
                                                        "The player dropdown menu's model.",
                                                        G_TYPE_MENU_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_signal_new ("back-clicked",
                G_OBJECT_CLASS_TYPE (klass),
                0,
                0,
                NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE, 0, G_TYPE_NONE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/ui/totem-player-toolbar.ui");
  gtk_widget_class_bind_template_child (widget_class, TotemPlayerToolbar, header_bar);
  gtk_widget_class_bind_template_child (widget_class, TotemPlayerToolbar, fullscreen_button);
  gtk_widget_class_bind_template_child (widget_class, TotemPlayerToolbar, unfullscreen_button);
  gtk_widget_class_bind_template_child (widget_class, TotemPlayerToolbar, subtitles_menu_button);
  gtk_widget_class_bind_template_child (widget_class, TotemPlayerToolbar, player_menu_button);
  gtk_widget_class_bind_template_child (widget_class, TotemPlayerToolbar, back_button);

  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked_cb);
}

static void
totem_player_toolbar_init (TotemPlayerToolbar *bar)
{
  gtk_widget_init_template (GTK_WIDGET (bar));
};

/**
 * totem_player_toolbar_new:
 *
 * Creates a #TotemPlayerToolbar.
 *
 * Return value: a new #TotemPlayerToolbar
 *
 * Since: 3.10
 **/
GtkWidget *
totem_player_toolbar_new (void)
{
  return GTK_WIDGET (g_object_new (TOTEM_TYPE_PLAYER_TOOLBAR, NULL));
}

void
totem_player_toolbar_set_title (TotemPlayerToolbar *bar,
                              const char       *title)
{
  g_return_if_fail (TOTEM_IS_PLAYER_TOOLBAR (bar));

  hdy_header_bar_set_title (HDY_HEADER_BAR (bar->header_bar), title);
}

const char *
totem_player_toolbar_get_title (TotemPlayerToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_PLAYER_TOOLBAR (bar), NULL);

  return hdy_header_bar_get_title (HDY_HEADER_BAR (bar->header_bar));
}

void
totem_player_toolbar_set_fullscreen_mode (TotemPlayerToolbar *bar,
                                          gboolean            is_fullscreen)
{
  g_return_if_fail (TOTEM_IS_PLAYER_TOOLBAR (bar));


  gtk_widget_set_visible(bar->unfullscreen_button, is_fullscreen);
  gtk_widget_set_visible(bar->fullscreen_button, !is_fullscreen);

  hdy_header_bar_set_show_close_button (HDY_HEADER_BAR (bar->header_bar), !is_fullscreen);
}

GtkWidget *
totem_player_toolbar_get_subtitles_button (TotemPlayerToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_PLAYER_TOOLBAR (bar), NULL);

  return bar->subtitles_menu_button;
}

GtkWidget *
totem_player_toolbar_get_player_button (TotemPlayerToolbar *self)
{
  g_return_val_if_fail (TOTEM_IS_PLAYER_TOOLBAR (self), NULL);

  return self->player_menu_button;
}
