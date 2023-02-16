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
  HdyHeaderBar parent;

  /* Template widgets */
  GtkWidget   *back_button;
};

G_DEFINE_TYPE(TotemPlayerToolbar, totem_player_toolbar, HDY_TYPE_HEADER_BAR)

static void
back_button_clicked_cb (GtkButton        *button,
                        TotemPlayerToolbar *bar)
{
  g_signal_emit_by_name (G_OBJECT (bar), "back-clicked", NULL);
}

static void
totem_player_toolbar_class_init (TotemPlayerToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_signal_new ("back-clicked",
                G_OBJECT_CLASS_TYPE (klass),
                0,
                0,
                NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE, 0, G_TYPE_NONE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/ui/totem-player-toolbar.ui");
  gtk_widget_class_bind_template_child (widget_class, TotemPlayerToolbar, back_button);
}

static void
totem_player_toolbar_init (TotemPlayerToolbar *bar)
{
  gtk_widget_init_template (GTK_WIDGET (bar));

  /* Back button */
  g_signal_connect (G_OBJECT (bar->back_button), "clicked",
                    G_CALLBACK (back_button_clicked_cb), bar);
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

  hdy_header_bar_set_title (HDY_HEADER_BAR (bar), title);
}

const char *
totem_player_toolbar_get_title (TotemPlayerToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_PLAYER_TOOLBAR (bar), NULL);

  return hdy_header_bar_get_title (HDY_HEADER_BAR (bar));
}
