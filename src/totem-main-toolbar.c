/* GTK - The GIMP Toolkit
 * Copyright (C) 2013-2014 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author:
 * Bastien Nocera <bnocera@redhat.com>
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
#include "totem-main-toolbar.h"
#include "griloresources.h"

/*
 * SECTION:totemmaintoolbar
 * @Short_description: An toolbar with oft-used buttons.
 * @Title: TotemMainToolbar
 *
 * #TotemMainToolbar is a toolbar that contains oft-used buttons such as toggles
 * for select mode, and find mode, or a new button. The widget will also be
 * styled properly when in specific mode.
 *
 * Since: 3.10
 */

struct _TotemMainToolbar {
  GtkBin       parent;

  GtkWidget   *header_bar;

  /* Template widgets */
  GtkWidget   *search_button;
  GtkWidget   *select_button;
  GtkWidget   *done_button;
  GtkWidget   *add_button;
  GtkWidget   *back_button;
  GtkWidget   *stack;

  /* Visibility */
  gboolean     show_search_button;
  gboolean     show_select_button;

  /* Modes */
  gboolean     search_mode;
  gboolean     select_mode;

  /* Normal title */
  GtkWidget   *title_label;
  GtkWidget   *subtitle_label;

  /* Custom title */
  GtkWidget   *custom_title;

  /* Search results */
  GtkWidget   *search_results_label;
  char        *search_string;

  /* Selection mode */
  guint        n_selected;
  GtkWidget   *selection_menu_button;

  /* Menu */
  GtkWidget   *main_menu_button;
};

G_DEFINE_TYPE(TotemMainToolbar, totem_main_toolbar, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_SEARCH_STRING,
  PROP_N_SELECTED,
  PROP_SEARCH_MODE,
  PROP_SHOW_SEARCH_BUTTON,
  PROP_SELECT_MODE,
  PROP_SELECT_MODE_AVAILABLE,
  PROP_SHOW_SELECT_BUTTON,
  PROP_SHOW_BACK_BUTTON,
  PROP_CUSTOM_TITLE,
  PROP_SELECT_MENU_MODEL,
  PROP_APP_MENU_MODEL,
};

#define DEFAULT_PAGE                   "title"
#define CUSTOM_TITLE_PAGE              "custom-title"
#define SEARCH_RESULTS_PAGE            "search-results"
#define SELECTION_PAGE                 "select"

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
update_toolbar_state (TotemMainToolbar *bar)
{
  if (bar->select_mode)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (bar->stack), SELECTION_PAGE);
      gtk_widget_hide (bar->select_button);
      gtk_widget_show (bar->done_button);

      if (bar->n_selected == 0)
        {
          gtk_button_set_label (GTK_BUTTON (bar->selection_menu_button), _("Click on items to select them"));
        }
      else
        {
          char *label;

          label = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d selected", "%d selected", bar->n_selected), bar->n_selected);
          gtk_button_set_label (GTK_BUTTON (bar->selection_menu_button), label);
          g_free (label);
        }

      change_class (GTK_WIDGET (bar), "selection-mode", TRUE);
    }
  else if (bar->search_mode)
    {
      if (!bar->search_string || *bar->search_string == '\0')
        {
          if (bar->custom_title)
            gtk_stack_set_visible_child_name (GTK_STACK (bar->stack), CUSTOM_TITLE_PAGE);
          else
            gtk_stack_set_visible_child_name (GTK_STACK (bar->stack), DEFAULT_PAGE);
        }
      else
        {
          char *label;

          gtk_stack_set_visible_child_name (GTK_STACK (bar->stack), SEARCH_RESULTS_PAGE);
          label = g_strdup_printf (_("Results for “%s”"), bar->search_string);

          gtk_label_set_label (GTK_LABEL (bar->search_results_label), label);
          g_free (label);
        }

      if (bar->show_select_button)
        gtk_widget_show (bar->select_button);
      gtk_widget_hide (bar->done_button);

      change_class (GTK_WIDGET (bar), "selection-mode", FALSE);
    }
  else
    {
      if (bar->custom_title == NULL)
        gtk_stack_set_visible_child_name (GTK_STACK (bar->stack), DEFAULT_PAGE);
      else
        gtk_stack_set_visible_child_name (GTK_STACK (bar->stack), CUSTOM_TITLE_PAGE);

      if (bar->show_select_button)
        gtk_widget_show (bar->select_button);
      gtk_widget_hide (bar->done_button);
      if (bar->show_search_button)
        gtk_widget_show (bar->search_button);

      change_class (GTK_WIDGET (bar), "selection-mode", FALSE);
    }
}

static void
done_button_clicked_cb (GtkButton        *button,
                        TotemMainToolbar *bar)
{
  totem_main_toolbar_set_select_mode (bar, FALSE);
}

static void
back_button_clicked_cb (GtkButton        *button,
                        TotemMainToolbar *bar)
{
  g_signal_emit_by_name (G_OBJECT (bar), "back-clicked", NULL);
}

static void
totem_main_toolbar_set_property (GObject         *object,
                                 guint            prop_id,
                                 const GValue    *value,
                                 GParamSpec      *pspec)
{
  TotemMainToolbar *bar = TOTEM_MAIN_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      totem_main_toolbar_set_title (bar, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      totem_main_toolbar_set_subtitle (bar, g_value_get_string (value));
      break;

    case PROP_SEARCH_STRING:
      totem_main_toolbar_set_search_string (bar, g_value_get_string (value));
      break;

    case PROP_N_SELECTED:
      totem_main_toolbar_set_n_selected (bar, g_value_get_uint (value));
      break;

    case PROP_SEARCH_MODE:
      totem_main_toolbar_set_search_mode (bar, g_value_get_boolean (value));
      break;

    case PROP_SHOW_SEARCH_BUTTON:
      bar->show_search_button = g_value_get_boolean (value);
      gtk_widget_set_visible (bar->search_button, bar->show_search_button);
      break;

    case PROP_SELECT_MODE:
      totem_main_toolbar_set_select_mode (bar, g_value_get_boolean (value));
      break;

    case PROP_SHOW_SELECT_BUTTON:
      bar->show_select_button = g_value_get_boolean (value);
      gtk_widget_set_visible (bar->select_button, bar->show_select_button);
      break;

    case PROP_SHOW_BACK_BUTTON:
      gtk_widget_set_visible (bar->back_button, g_value_get_boolean (value));
      break;

    case PROP_CUSTOM_TITLE:
      totem_main_toolbar_set_custom_title (bar, g_value_get_object (value));
      break;

    case PROP_SELECT_MENU_MODEL:
      totem_main_toolbar_set_select_menu_model (bar, g_value_get_object (value));
      break;

    case PROP_APP_MENU_MODEL:
      gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (bar->main_menu_button), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
totem_main_toolbar_get_property (GObject         *object,
                                 guint            prop_id,
                                 GValue          *value,
                                 GParamSpec      *pspec)
{
  TotemMainToolbar *bar = TOTEM_MAIN_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, hdy_header_bar_get_title (HDY_HEADER_BAR (object)));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, hdy_header_bar_get_subtitle (HDY_HEADER_BAR (object)));
      break;

    case PROP_SEARCH_STRING:
      g_value_set_string (value, totem_main_toolbar_get_search_string (bar));
      break;

    case PROP_N_SELECTED:
      g_value_set_uint (value, totem_main_toolbar_get_n_selected (bar));
      break;

    case PROP_SEARCH_MODE:
      g_value_set_boolean (value, totem_main_toolbar_get_search_mode (bar));
      break;

    case PROP_SHOW_SEARCH_BUTTON:
      g_value_set_boolean (value, bar->show_search_button);
      break;

    case PROP_SELECT_MODE:
      g_value_set_boolean (value, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (bar->select_button)));
      break;

    case PROP_SHOW_SELECT_BUTTON:
      g_value_set_boolean (value, bar->show_select_button);
      break;

    case PROP_SHOW_BACK_BUTTON:
      g_value_set_boolean (value, gtk_widget_get_visible (bar->back_button));
      break;

    case PROP_CUSTOM_TITLE:
      g_value_set_object (value, bar->custom_title);
      break;

    case PROP_SELECT_MENU_MODEL:
      g_value_set_object (value, totem_main_toolbar_get_select_menu_model (bar));
      break;

    case PROP_APP_MENU_MODEL:
      g_value_set_object (value, gtk_menu_button_get_menu_model (GTK_MENU_BUTTON (bar->main_menu_button)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
totem_main_toolbar_finalize (GObject *object)
{
  TotemMainToolbar *bar = TOTEM_MAIN_TOOLBAR (object);

  g_free (bar->search_string);

  G_OBJECT_CLASS (totem_main_toolbar_parent_class)->finalize (object);
}

static void
totem_main_toolbar_class_init (TotemMainToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = totem_main_toolbar_finalize;
  object_class->set_property = totem_main_toolbar_set_property;
  object_class->get_property = totem_main_toolbar_get_property;

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        "Title",
                                                        "The title",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_SUBTITLE,
                                   g_param_spec_string ("subtitle",
                                                        "Subtitle",
                                                        "The subtitle",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_SEARCH_STRING,
                                   g_param_spec_string ("search-string",
                                                        "Search String",
                                                        "The search string used in search mode",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

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
                                   PROP_SEARCH_MODE,
                                   g_param_spec_boolean ("search-mode",
                                                         "Search Mode",
                                                         "Whether the header bar is in search mode",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_SHOW_SEARCH_BUTTON,
                                   g_param_spec_boolean ("show-search-button",
                                                         "Show Search Button",
                                                         "Whether the search button is visible",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_SELECT_MODE,
                                   g_param_spec_boolean ("select-mode",
                                                         "Select Mode",
                                                         "Whether the header bar is in select mode",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_SHOW_SELECT_BUTTON,
                                   g_param_spec_boolean ("show-select-button",
                                                         "Show Select Button",
                                                         "Whether the select button is visible",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_SHOW_BACK_BUTTON,
                                   g_param_spec_boolean ("show-back-button",
                                                         "Show Back Button",
                                                         "Whether the back button is visible",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_CUSTOM_TITLE,
                                   g_param_spec_object ("custom-title",
                                                        "Custom Title",
                                                        "Custom title widget to display",
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_SELECT_MENU_MODEL,
                                   g_param_spec_object ("select-menu-model",
                                                        "menu-model",
                                                        "The selection dropdown menu's model.",
                                                        G_TYPE_MENU_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_APP_MENU_MODEL,
                                   g_param_spec_object ("app-menu-model",
                                                        "app-menu-model",
                                                        "The app dropdown menu's model.",
                                                        G_TYPE_MENU_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_signal_new ("back-clicked",
                G_OBJECT_CLASS_TYPE (klass),
                0,
                0,
                NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE, 0, G_TYPE_NONE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/grilo/totemmaintoolbar.ui");
  gtk_widget_class_bind_template_child (widget_class, TotemMainToolbar, header_bar);
  gtk_widget_class_bind_template_child (widget_class, TotemMainToolbar, search_button);
  gtk_widget_class_bind_template_child (widget_class, TotemMainToolbar, select_button);
  gtk_widget_class_bind_template_child (widget_class, TotemMainToolbar, selection_menu_button);
  gtk_widget_class_bind_template_child (widget_class, TotemMainToolbar, done_button);
  gtk_widget_class_bind_template_child (widget_class, TotemMainToolbar, add_button);
  gtk_widget_class_bind_template_child (widget_class, TotemMainToolbar, back_button);
  gtk_widget_class_bind_template_child (widget_class, TotemMainToolbar, stack);
  gtk_widget_class_bind_template_child (widget_class, TotemMainToolbar, main_menu_button);
}

static GtkWidget *
create_title_box (const char *title,
                  const char *subtitle,
                  GtkWidget **ret_title_label,
                  GtkWidget **ret_subtitle_label)
{
  GtkWidget *label_box;

  GtkWidget *title_label;
  GtkWidget *subtitle_label;
  GtkStyleContext *context;

  label_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_valign (label_box, GTK_ALIGN_CENTER);
  gtk_widget_show (label_box);

  title_label = gtk_label_new (title);
  context = gtk_widget_get_style_context (title_label);
  gtk_style_context_add_class (context, "title");
  gtk_label_set_line_wrap (GTK_LABEL (title_label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (title_label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (title_label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (label_box), title_label, FALSE, FALSE, 0);
  gtk_widget_show (title_label);

  subtitle_label = gtk_label_new (subtitle);
  context = gtk_widget_get_style_context (subtitle_label);
  gtk_style_context_add_class (context, "subtitle");
  gtk_style_context_add_class (context, "dim-label");
  gtk_label_set_line_wrap (GTK_LABEL (subtitle_label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (subtitle_label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (subtitle_label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (label_box), subtitle_label, FALSE, FALSE, 0);
  gtk_widget_set_no_show_all (subtitle_label, TRUE);

  if (ret_title_label)
    *ret_title_label = title_label;
  if (ret_subtitle_label)
    *ret_subtitle_label = subtitle_label;

  return label_box;
}


static void
totem_main_toolbar_init (TotemMainToolbar *bar)
{
  GtkWidget *title_widget;

  gtk_widget_init_template (GTK_WIDGET (bar));

  gtk_widget_set_no_show_all (bar->search_button, TRUE);
  gtk_widget_set_no_show_all (bar->select_button, TRUE);

  /* Back button */
  g_signal_connect (G_OBJECT (bar->back_button), "clicked",
                    G_CALLBACK (back_button_clicked_cb), bar);

  /* Titles */
  title_widget = create_title_box ("", "", &bar->title_label, &bar->subtitle_label);
  gtk_stack_add_named (GTK_STACK (bar->stack), title_widget, DEFAULT_PAGE);
  /* Custom title page will be added as needed in _set_custom_title() */

  title_widget = create_title_box ("Results", NULL, &bar->search_results_label, NULL);
  gtk_stack_add_named (GTK_STACK (bar->stack), title_widget, SEARCH_RESULTS_PAGE);
  /* The drop-down is added using _set_select_menu_model() */

  /* Select and Search buttons */
  g_signal_connect (G_OBJECT (bar->done_button), "clicked",
                    G_CALLBACK (done_button_clicked_cb), bar);
  g_object_bind_property (bar->search_button, "active",
                            bar, "search-mode", 0);
  g_object_bind_property (bar->select_button, "active",
                            bar, "select-mode", 0);
};

/**
 * totem_main_toolbar_new:
 *
 * Creates a #TotemMainToolbar.
 *
 * Return value: a new #TotemMainToolbar
 *
 * Since: 3.10
 **/
GtkWidget *
totem_main_toolbar_new (void)
{
  return GTK_WIDGET (g_object_new (TOTEM_TYPE_MAIN_TOOLBAR, NULL));
}

/**
 * totem_main_toolbar_set_search_mode:
 * @bar: a #TotemMainToolbar
 * @search_mode: Whether the search mode is on or off.
 *
 * Sets the new search mode toggling the search button on or
 * off as needed.
 *
 * Since: 3.10
 **/
void
totem_main_toolbar_set_search_mode (TotemMainToolbar *bar,
                                    gboolean          search_mode)
{
  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));

  if (bar->search_mode == search_mode)
    return;

  bar->search_mode = search_mode;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->search_button),
                                bar->search_mode);
  update_toolbar_state (bar);
  if (search_mode == FALSE)
    totem_main_toolbar_set_search_string (bar, "");
  g_object_notify (G_OBJECT (bar), "search-mode");
}

gboolean
totem_main_toolbar_get_search_mode (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), FALSE);

  return bar->search_mode;
}

void
totem_main_toolbar_set_select_mode (TotemMainToolbar *bar,
                                    gboolean          select_mode)
{
  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));

  if (bar->select_mode == select_mode)
    return;

  bar->select_mode = select_mode;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->select_button),
                                bar->select_mode);
  update_toolbar_state (bar);
  g_object_notify (G_OBJECT (bar), "select-mode");
}

gboolean
totem_main_toolbar_get_select_mode (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), FALSE);

  return bar->select_mode;
}

void
totem_main_toolbar_set_title (TotemMainToolbar *bar,
                              const char       *title)
{
  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));

  gtk_label_set_text (GTK_LABEL (bar->title_label), title);
  hdy_header_bar_set_title (HDY_HEADER_BAR (bar->header_bar), title);
}

const char *
totem_main_toolbar_get_title (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), NULL);

  return hdy_header_bar_get_title (HDY_HEADER_BAR (bar->header_bar));
}

void
totem_main_toolbar_set_subtitle (TotemMainToolbar *bar,
                                 const char       *subtitle)
{
  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));

  gtk_label_set_text (GTK_LABEL (bar->subtitle_label), subtitle);
  hdy_header_bar_set_subtitle (HDY_HEADER_BAR (bar->header_bar), subtitle);
}

const char *
totem_main_toolbar_get_subtitle (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), NULL);

  return hdy_header_bar_get_subtitle (HDY_HEADER_BAR (bar->header_bar));
}

void
totem_main_toolbar_set_search_string (TotemMainToolbar *bar,
                                      const char       *search_string)
{
  char *tmp;

  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));

  tmp = bar->search_string;
  bar->search_string = g_strdup (search_string);
  g_free (tmp);

  update_toolbar_state (bar);
  g_object_notify (G_OBJECT (bar), "search-string");
}

const char *
totem_main_toolbar_get_search_string (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), NULL);

  return bar->search_string;
}

void
totem_main_toolbar_set_n_selected (TotemMainToolbar *bar,
                                   guint             n_selected)
{
  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));

  if (bar->n_selected == n_selected)
    return;

  bar->n_selected = n_selected;

  update_toolbar_state (bar);
  g_object_notify (G_OBJECT (bar), "n-selected");
}

guint
totem_main_toolbar_get_n_selected (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), 0);

  return bar->n_selected;
}

void
totem_main_toolbar_pack_start (TotemMainToolbar *bar,
                               GtkWidget        *child)
{
  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));

  hdy_header_bar_pack_start (HDY_HEADER_BAR (bar->header_bar), child);
}

void
totem_main_toolbar_pack_end (TotemMainToolbar *bar,
                           GtkWidget          *child)
{
  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));

  hdy_header_bar_pack_end (HDY_HEADER_BAR (bar->header_bar), child);
}

void
totem_main_toolbar_set_select_menu_model (TotemMainToolbar *bar,
                                          GMenuModel       *model)
{
  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));

  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (bar->selection_menu_button), model);
}

GMenuModel *
totem_main_toolbar_get_select_menu_model (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), NULL);

  return gtk_menu_button_get_menu_model (GTK_MENU_BUTTON (bar->selection_menu_button));
}

void
totem_main_toolbar_set_custom_title (TotemMainToolbar *bar,
                                     GtkWidget        *title_widget)
{
  g_return_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar));
  if (title_widget)
    g_return_if_fail (GTK_IS_WIDGET (title_widget));

  /* No need to do anything if the custom widget stays the same */
  if (bar->custom_title == title_widget)
    return;

  if (bar->custom_title)
    {
      GtkWidget *custom = bar->custom_title;

      bar->custom_title = NULL;
      gtk_container_remove (GTK_CONTAINER (bar->stack), custom);
    }

  if (title_widget != NULL)
    {
      bar->custom_title = title_widget;

      gtk_stack_add_named (GTK_STACK (bar->stack), title_widget, CUSTOM_TITLE_PAGE);
      gtk_widget_show (title_widget);

      update_toolbar_state (bar);
    }
  else
    {
      gtk_stack_set_visible_child_name (GTK_STACK (bar->stack), DEFAULT_PAGE);
    }

  g_object_notify (G_OBJECT (bar), "custom-title");
}

GtkWidget *
totem_main_toolbar_get_custom_title (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), NULL);

  return bar->custom_title;
}

GtkWidget *
totem_main_toolbar_get_add_button (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), NULL);

  return bar->add_button;
}

GtkWidget *
totem_main_toolbar_get_main_menu_button (TotemMainToolbar *bar)
{
  g_return_val_if_fail (TOTEM_IS_MAIN_TOOLBAR (bar), NULL);

  return bar->main_menu_button;
}
