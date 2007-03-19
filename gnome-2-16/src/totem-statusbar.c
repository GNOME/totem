/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * TotemStatusbar Copyright (C) 1998 Shawn T. Amundson
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <gtk/gtkframe.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkvseparator.h>
#include <gtk/gtkwindow.h>
#include <glib/gi18n.h>

#include "totem-statusbar.h"
#include "video-utils.h"

static void     totem_statusbar_class_init     (TotemStatusbarClass *class);
static void     totem_statusbar_init           (TotemStatusbar      *statusbar);
static void     totem_statusbar_destroy        (GtkObject         *object);
static void     totem_statusbar_size_allocate  (GtkWidget         *widget,
					      GtkAllocation     *allocation);
static void     totem_statusbar_realize        (GtkWidget         *widget);
static void     totem_statusbar_unrealize      (GtkWidget         *widget);
static void     totem_statusbar_map            (GtkWidget         *widget);
static void     totem_statusbar_unmap          (GtkWidget         *widget);
static gboolean totem_statusbar_button_press   (GtkWidget         *widget,
					      GdkEventButton    *event);
static gboolean totem_statusbar_expose_event   (GtkWidget         *widget,
					      GdkEventExpose    *event);
static void     totem_statusbar_size_request   (GtkWidget         *widget,
                                              GtkRequisition    *requisition);
static void     totem_statusbar_size_allocate  (GtkWidget         *widget,
                                              GtkAllocation     *allocation);
static void     totem_statusbar_create_window  (TotemStatusbar      *statusbar);
static void     totem_statusbar_destroy_window (TotemStatusbar      *statusbar);

static GtkContainerClass *parent_class;

G_DEFINE_TYPE(TotemStatusbar, totem_statusbar, GTK_TYPE_HBOX)

static void
totem_statusbar_class_init (TotemStatusbarClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  parent_class = g_type_class_peek_parent (class);
  
  object_class->destroy = totem_statusbar_destroy;

  widget_class->realize = totem_statusbar_realize;
  widget_class->unrealize = totem_statusbar_unrealize;
  widget_class->map = totem_statusbar_map;
  widget_class->unmap = totem_statusbar_unmap;
  
  widget_class->button_press_event = totem_statusbar_button_press;
  widget_class->expose_event = totem_statusbar_expose_event;

  widget_class->size_request = totem_statusbar_size_request;
  widget_class->size_allocate = totem_statusbar_size_allocate;

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_enum ("shadow_type",
                                                              _("Shadow type"),
                                                              _("Style of bevel around the statusbar text"),
                                                              GTK_TYPE_SHADOW_TYPE,
                                                              GTK_SHADOW_IN,
                                                              G_PARAM_READABLE));
}

static void
totem_statusbar_init (TotemStatusbar *statusbar)
{
  GtkBox *box;
  GtkShadowType shadow_type;
  GtkWidget *packer, *hbox;
  
  box = GTK_BOX (statusbar);

  box->spacing = 2;
  box->homogeneous = FALSE;

  statusbar->has_resize_grip = TRUE;
  statusbar->time = 0;
  statusbar->length = -1;

  gtk_widget_style_get (GTK_WIDGET (statusbar), "shadow_type", &shadow_type, NULL);
 
  statusbar->frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (statusbar->frame), shadow_type);
  gtk_box_pack_start (box, statusbar->frame, TRUE, TRUE, 0);
  gtk_widget_show (statusbar->frame);
  hbox = gtk_hbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
  gtk_widget_show (hbox);

  statusbar->label = gtk_label_new (_("Stopped"));
  gtk_misc_set_alignment (GTK_MISC (statusbar->label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), statusbar->label, FALSE, FALSE, 0);
  gtk_widget_show (statusbar->label);

  /* progressbar for network streams */
  statusbar->progress = gtk_progress_bar_new ();
  gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (statusbar->progress),
				    GTK_ORIENTATION_HORIZONTAL);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (statusbar->progress), 0.);
  gtk_box_pack_start (GTK_BOX (hbox), statusbar->progress, FALSE, TRUE, 0);
  gtk_widget_set_size_request (statusbar->progress, 150, 10);
  gtk_widget_hide (statusbar->progress);

  packer = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), packer, FALSE, FALSE, 0);
  gtk_widget_show (packer);

  statusbar->time_label = gtk_label_new (_("0:00 / 0:00"));
  gtk_misc_set_alignment (GTK_MISC (statusbar->label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (hbox), statusbar->time_label, FALSE, FALSE, 0);
  gtk_widget_show (statusbar->time_label);

  /* don't expand the size request for the label; if we
   * do that then toplevels weirdly resize
   */
  gtk_container_add (GTK_CONTAINER (statusbar->frame), hbox);
}

GtkWidget* 
totem_statusbar_new (void)
{
  return g_object_new (TOTEM_TYPE_STATUSBAR, NULL);
}

GtkWidget *
totem_statusbar_new_from_glade (gchar *widget_name,
		gchar *string1, gchar *string2,
		gint int1, gint int2)
{
	GtkWidget *widget;

	widget = totem_statusbar_new ();
	gtk_widget_show (widget);

	return widget;
}

static void
totem_statusbar_update_time (TotemStatusbar *statusbar)
{
  char *time, *length, *label;

  time = totem_time_to_string (statusbar->time * 1000);

  if (statusbar->length < 0) {
    label = g_strdup_printf (_("%s (Streaming)"), time);
  } else {
    length = totem_time_to_string
	    (statusbar->length == -1 ? 0 : statusbar->length * 1000);

    if (statusbar->seeking == FALSE)
      /* Elapsed / Total Length */
      label = g_strdup_printf (_("%s / %s"), time, length);
    else
      /* Seeking to Time / Total Length */
      label = g_strdup_printf (_("Seek to %s / %s"), time, length);

    g_free (length);
  }

  gtk_label_set_text (GTK_LABEL (statusbar->time_label), label);

  g_free (time);
  g_free (label);
}

void
totem_statusbar_set_text (TotemStatusbar *statusbar, const char *label)
{
  gtk_label_set_text (GTK_LABEL (statusbar->label), label);
  g_free (statusbar->saved_label);
  statusbar->saved_label = g_strdup (label);
}

void
totem_statusbar_set_time (TotemStatusbar *statusbar, gint time)
{
  g_return_if_fail (TOTEM_IS_STATUSBAR (statusbar));

  if (statusbar->time == time)
    return;

  statusbar->time = time;
  totem_statusbar_update_time (statusbar);
}

static gboolean
totem_statusbar_timeout_pop (TotemStatusbar *statusbar)
{
  gtk_label_set_text (GTK_LABEL (statusbar->label), statusbar->saved_label);
  g_free (statusbar->saved_label);
  statusbar->saved_label = NULL;
  statusbar->pushed = 0;
  gtk_widget_hide (statusbar->progress);

  return FALSE;
}

void
totem_statusbar_push (TotemStatusbar *statusbar, guint percentage)
{
  char *label;
  statusbar->pushed = 1;

  if (statusbar->timeout != 0)
  {
    g_source_remove (statusbar->timeout);
  }

  if (statusbar->saved_label == NULL)
  {
    statusbar->saved_label = g_strdup
	    (gtk_label_get_text (GTK_LABEL (statusbar->label)));
  }

  gtk_label_set_text (GTK_LABEL (statusbar->label), _("Buffering"));

  /* eg: 75 % */
  label = g_strdup_printf (_("%d %%"), percentage);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (statusbar->progress), label);
  g_free (label);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (statusbar->progress),
				 percentage / 100.);
  gtk_widget_show (statusbar->progress);

  statusbar->timeout = g_timeout_add (3000,
		  (GSourceFunc) totem_statusbar_timeout_pop, statusbar);
}

void
totem_statusbar_pop (TotemStatusbar *statusbar)
{
  statusbar->pushed = 0;
}

void
totem_statusbar_set_time_and_length (TotemStatusbar *statusbar,
				     gint time, gint length)
{
  g_return_if_fail (TOTEM_IS_STATUSBAR (statusbar));

  if (time != statusbar->time ||
      length != statusbar->length) {
    statusbar->time = time;
    statusbar->length = length;

    totem_statusbar_update_time (statusbar);
  }
}

void
totem_statusbar_set_seeking (TotemStatusbar *statusbar,
			     gboolean seeking)
{
  g_return_if_fail (TOTEM_IS_STATUSBAR (statusbar));

  statusbar->seeking = seeking;

  totem_statusbar_update_time (statusbar);
}

void
totem_statusbar_set_has_resize_grip (TotemStatusbar *statusbar,
				   gboolean      setting)
{
  g_return_if_fail (TOTEM_IS_STATUSBAR (statusbar));

  setting = setting != FALSE;

  if (setting != statusbar->has_resize_grip)
    {
      statusbar->has_resize_grip = setting;
      gtk_widget_queue_draw (GTK_WIDGET (statusbar));

      if (GTK_WIDGET_REALIZED (statusbar))
        {
          if (statusbar->has_resize_grip && statusbar->grip_window == NULL)
            totem_statusbar_create_window (statusbar);
          else if (!statusbar->has_resize_grip && statusbar->grip_window != NULL)
            totem_statusbar_destroy_window (statusbar);
        }
    }
}

gboolean
totem_statusbar_get_has_resize_grip (TotemStatusbar *statusbar)
{
  g_return_val_if_fail (TOTEM_IS_STATUSBAR (statusbar), FALSE);

  return statusbar->has_resize_grip;
}

static void
totem_statusbar_destroy (GtkObject *object)
{
  TotemStatusbar *statusbar;
  TotemStatusbarClass *class;

  g_return_if_fail (TOTEM_IS_STATUSBAR (object));

  statusbar = TOTEM_STATUSBAR (object);
  class = TOTEM_STATUSBAR_GET_CLASS (statusbar);

  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static GdkWindowEdge
get_grip_edge (TotemStatusbar *statusbar)
{
  GtkWidget *widget = GTK_WIDGET (statusbar);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) 
    return GDK_WINDOW_EDGE_SOUTH_EAST; 
  else
    return GDK_WINDOW_EDGE_SOUTH_WEST; 
}

static void
get_grip_rect (TotemStatusbar *statusbar,
               GdkRectangle *rect)
{
  GtkWidget *widget;
  gint w, h;
  
  widget = GTK_WIDGET (statusbar);

  /* These are in effect the max/default size of the grip. */
  w = 18;
  h = 18;

  if (w > (widget->allocation.width))
    w = widget->allocation.width;

  if (h > (widget->allocation.height - widget->style->ythickness))
    h = widget->allocation.height - widget->style->ythickness;
  
  rect->width = w;
  rect->height = h;
  rect->y = widget->allocation.y + widget->allocation.height - h;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) 
      rect->x = widget->allocation.x + widget->allocation.width - w;
  else 
      rect->x = widget->allocation.x + widget->style->xthickness;
}

static void
totem_statusbar_create_window (TotemStatusbar *statusbar)
{
  GtkWidget *widget;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkRectangle rect;
  
  g_return_if_fail (GTK_WIDGET_REALIZED (statusbar));
  g_return_if_fail (statusbar->has_resize_grip);
  
  widget = GTK_WIDGET (statusbar);

  get_grip_rect (statusbar, &rect);

  attributes.x = rect.x;
  attributes.y = rect.y;
  attributes.width = rect.width;
  attributes.height = rect.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget) |
    GDK_BUTTON_PRESS_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  statusbar->grip_window = gdk_window_new (widget->window,
                                           &attributes, attributes_mask);
  gdk_window_set_user_data (statusbar->grip_window, widget);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    statusbar->cursor = gdk_cursor_new (GDK_BOTTOM_LEFT_CORNER);
  else
    statusbar->cursor = gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);
  gdk_window_set_cursor (statusbar->grip_window, statusbar->cursor);
}

static void
totem_statusbar_destroy_window (TotemStatusbar *statusbar)
{
  gdk_window_set_user_data (statusbar->grip_window, NULL);
  gdk_window_destroy (statusbar->grip_window);
  gdk_cursor_unref (statusbar->cursor);
  statusbar->cursor = NULL;
  statusbar->grip_window = NULL;
}

static void
totem_statusbar_realize (GtkWidget *widget)
{
  TotemStatusbar *statusbar;

  statusbar = TOTEM_STATUSBAR (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

  if (statusbar->has_resize_grip)
    totem_statusbar_create_window (statusbar);
}

static void
totem_statusbar_unrealize (GtkWidget *widget)
{
  TotemStatusbar *statusbar;

  statusbar = TOTEM_STATUSBAR (widget);

  if (statusbar->grip_window)
    totem_statusbar_destroy_window (statusbar);
  
  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
totem_statusbar_map (GtkWidget *widget)
{
  TotemStatusbar *statusbar;

  statusbar = TOTEM_STATUSBAR (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->map) (widget);
  
  if (statusbar->grip_window)
    gdk_window_show (statusbar->grip_window);
}

static void
totem_statusbar_unmap (GtkWidget *widget)
{
  TotemStatusbar *statusbar;

  statusbar = TOTEM_STATUSBAR (widget);

  if (statusbar->grip_window)
    gdk_window_hide (statusbar->grip_window);
  
  (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static gboolean
totem_statusbar_button_press (GtkWidget      *widget,
                            GdkEventButton *event)
{
  TotemStatusbar *statusbar;
  GtkWidget *ancestor;
  GdkWindowEdge edge;
  
  statusbar = TOTEM_STATUSBAR (widget);
  
  if (!statusbar->has_resize_grip ||
    event->type != GDK_BUTTON_PRESS)
    return FALSE;
  
  ancestor = gtk_widget_get_toplevel (widget);

  if (!GTK_IS_WINDOW (ancestor))
    return FALSE;

  edge = get_grip_edge (statusbar);

  if (event->button == 1)
    gtk_window_begin_resize_drag (GTK_WINDOW (ancestor),
                                  edge,
                                  event->button,
                                  event->x_root, event->y_root,
                                  event->time);
  else if (event->button == 2)
    gtk_window_begin_move_drag (GTK_WINDOW (ancestor),
                                event->button,
                                event->x_root, event->y_root,
                                event->time);
  else
    return FALSE;
  
  return TRUE;
}

static gboolean
totem_statusbar_expose_event (GtkWidget      *widget,
                            GdkEventExpose *event)
{
  TotemStatusbar *statusbar;
  GdkRectangle rect;
  
  statusbar = TOTEM_STATUSBAR (widget);

  GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);

  if (statusbar->has_resize_grip)
    {
      GdkWindowEdge edge;
      
      edge = get_grip_edge (statusbar);

      get_grip_rect (statusbar, &rect);

      gtk_paint_resize_grip (widget->style,
                             widget->window,
                             GTK_WIDGET_STATE (widget),
                             NULL,
                             widget,
                             "statusbar",
                             edge,
                             rect.x, rect.y,
                             /* don't draw grip over the frame, though you
                              * can click on the frame.
                              */
                             rect.width - widget->style->xthickness,
                             rect.height - widget->style->ythickness);
    }

  return FALSE;
}

static void
totem_statusbar_size_request   (GtkWidget      *widget,
                              GtkRequisition *requisition)
{
  TotemStatusbar *statusbar;
  GtkShadowType shadow_type;
  
  statusbar = TOTEM_STATUSBAR (widget);

  gtk_widget_style_get (GTK_WIDGET (statusbar), "shadow_type", &shadow_type, NULL);  
  gtk_frame_set_shadow_type (GTK_FRAME (statusbar->frame), shadow_type);

  GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

  if (statusbar->has_resize_grip)
    {
      GdkRectangle rect;

      /* x, y in the grip rect depend on size allocation, but
       * w, h do not so this is OK
       */
      get_grip_rect (statusbar, &rect);
      
      requisition->width += rect.width;
      requisition->height = MAX (requisition->height, rect.height);
    }
}

static void
totem_statusbar_size_allocate  (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  TotemStatusbar *statusbar;
  
  statusbar = TOTEM_STATUSBAR (widget);

  if (statusbar->has_resize_grip)
    {
      GdkRectangle rect;
      GtkRequisition saved_req;
      
      widget->allocation = *allocation; /* get_grip_rect needs this info */
      get_grip_rect (statusbar, &rect);
  
      if (statusbar->grip_window)
        gdk_window_move_resize (statusbar->grip_window,
                                rect.x, rect.y,
                                rect.width, rect.height);
      
      /* enter the bad hack zone */      
      saved_req = widget->requisition;
      widget->requisition.width -= rect.width; /* HBox::size_allocate needs this */
      if (widget->requisition.width < 0)
        widget->requisition.width = 0;
      GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
      widget->requisition = saved_req;
    }
  else
    {
      /* chain up normally */
      GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
    }
}
