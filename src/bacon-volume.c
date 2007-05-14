/* Volume Button / popup widget
 * (c) copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "bacon-volume.h"

#define SCALE_SIZE 100
#define CLICK_TIMEOUT 250

enum {
  SIGNAL_VALUE_CHANGED,
  NUM_SIGNALS
};

struct _BaconVolumeButton {
  GtkButton parent;

  /* popup */
  GtkWidget *dock, *scale, *image, *plus, *min;
  GtkTooltips *tooltips;
  GtkIconSize size;
  gint click_id;
  float direction;
  guint32 pop_time;
  guint timeout : 1;
  guint icon : 3;
};

static void	bacon_volume_button_class_init	(BaconVolumeButtonClass * klass);
static void	bacon_volume_button_init	(BaconVolumeButton * button);
static void	bacon_volume_button_dispose	(GObject        * object);

static gboolean	bacon_volume_button_scroll	(GtkWidget      * widget,
						 GdkEventScroll * event);
static gboolean	bacon_volume_button_press	(GtkWidget      * widget,
						 GdkEventButton * event);
static gboolean bacon_volume_key_release	(GtkWidget      * widget,
    						 GdkEventKey    * event);
static gboolean cb_dock_button_press		(GtkWidget      * widget,
						 GdkEventButton * event,
						 gpointer         data);
static gboolean cb_dock_key_release		(GtkWidget      * widget,
						 GdkEventKey    * event,
						 gpointer         data);
static gboolean cb_dock_key_press		(GtkWidget      * widget,
						 GdkEventKey    * event,
						 gpointer         data);

static gboolean cb_button_press			(GtkWidget      * widget,
						 GdkEventButton * event,
						 gpointer         data);
static gboolean cb_button_release		(GtkWidget      * widget,
						 GdkEventButton * event,
						 gpointer         data);
static void     bacon_volume_button_update_icon (BaconVolumeButton *button);
static void	bacon_volume_button_update_tip	(BaconVolumeButton *button);
static void	bacon_volume_scale_value_changed(GtkRange       * range);

/* see below for scale definitions */
static GtkWidget *bacon_volume_scale_new	(BaconVolumeButton * button,
						 float min, float max,
						 float step);

static GtkButtonClass *parent_class;
static guint signals[NUM_SIGNALS];

GType
bacon_volume_button_get_type (void)
{
  static GType bacon_volume_button_type = 0;

  if (G_UNLIKELY (bacon_volume_button_type == 0)) {
    const GTypeInfo bacon_volume_button_info = {
      sizeof (BaconVolumeButtonClass),
      NULL,
      NULL,
      (GClassInitFunc) bacon_volume_button_class_init,
      NULL,
      NULL,
      sizeof (BaconVolumeButton),
      0,
      (GInstanceInitFunc) bacon_volume_button_init,
      NULL
    };

    bacon_volume_button_type =
	g_type_register_static (GTK_TYPE_BUTTON, 
				"BaconVolumeButton",
				&bacon_volume_button_info, 0);
  }

  return bacon_volume_button_type;
}

static void
bacon_volume_button_class_init (BaconVolumeButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  /* events */
  gobject_class->dispose = bacon_volume_button_dispose;
  gtkwidget_class->button_press_event = bacon_volume_button_press;
  gtkwidget_class->key_release_event = bacon_volume_key_release;
  gtkwidget_class->scroll_event = bacon_volume_button_scroll;

  /* signals */
  signals[SIGNAL_VALUE_CHANGED] = g_signal_new ("value-changed",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (BaconVolumeButtonClass, value_changed),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
bacon_volume_button_init (BaconVolumeButton *button)
{
  button->timeout = FALSE;
  button->click_id = 0;
  button->dock = button->scale = NULL;
  button->icon = 7; /* to force update */
  /* FIXME: there really is no need for a complete tooltips object here! just use one in a parent window! */
  button->tooltips = gtk_tooltips_new ();
}

static void
bacon_volume_button_dispose (GObject *object)
{
  BaconVolumeButton *button = BACON_VOLUME_BUTTON (object);

  if (button->dock) {
    gtk_widget_destroy (button->dock);
    button->dock = NULL;
  }

  if (button->click_id != 0) {
    g_source_remove (button->click_id);
    button->click_id = 0;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

/*
 * public API.
 */

GtkWidget *
bacon_volume_button_new (GtkIconSize size,
			 float min, float max,
			 float step)
{
  BaconVolumeButton *button;
  GtkWidget *frame, *box;

  button = g_object_new (BACON_TYPE_VOLUME_BUTTON, NULL);
  atk_object_set_name (gtk_widget_get_accessible (GTK_WIDGET (button)),
		       _("Volume"));
  button->size = size;
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);

  /* image */
  button->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (button), button->image);
  gtk_widget_show_all (button->image);

  /* window */
  button->dock = gtk_window_new (GTK_WINDOW_POPUP);
  g_signal_connect (button->dock, "button-press-event",
		    G_CALLBACK (cb_dock_button_press), button);
  g_signal_connect (button->dock, "key-release-event",
		    G_CALLBACK (cb_dock_key_release), button);
  g_signal_connect (button->dock, "key-press-event",
		    G_CALLBACK (cb_dock_key_press), button);
  gtk_window_set_decorated (GTK_WINDOW (button->dock), FALSE);

  /* frame */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (button->dock), frame);
  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);

  /* + */
  button->plus = gtk_button_new_with_label (_("+"));
  atk_object_set_name (gtk_widget_get_accessible (button->plus),
		       _("Volume Up"));
  gtk_button_set_relief (GTK_BUTTON (button->plus), GTK_RELIEF_NONE);
  g_signal_connect (button->plus, "button-press-event",
		    G_CALLBACK (cb_button_press), button);
  g_signal_connect (button->plus, "button-release-event",
		    G_CALLBACK (cb_button_release), button);
  gtk_box_pack_start (GTK_BOX (box), button->plus, TRUE, FALSE, 0);

  /* scale */
  button->scale = bacon_volume_scale_new (button, min, max, step);
  gtk_widget_set_size_request (button->scale, -1, SCALE_SIZE);
  gtk_scale_set_draw_value (GTK_SCALE (button->scale), FALSE);
  gtk_range_set_inverted (GTK_RANGE (button->scale), TRUE);
  gtk_box_pack_start (GTK_BOX (box), button->scale, TRUE, FALSE, 0);

  /* - */
  button->min = gtk_button_new_with_label (_("-"));
  atk_object_set_name (gtk_widget_get_accessible (button->min),
		       _("Volume Down"));
  gtk_button_set_relief (GTK_BUTTON (button->min), GTK_RELIEF_NONE);
  g_signal_connect (button->min, "button-press-event",
		   G_CALLBACK (cb_button_press), button);
  g_signal_connect (button->min, "button-release-event",
		    G_CALLBACK (cb_button_release), button);
  gtk_box_pack_start (GTK_BOX (box), button->min, TRUE, FALSE, 0);

  /* initially set icon and tooltip */
  bacon_volume_button_update_icon (button);
  bacon_volume_button_update_tip (button);

  return GTK_WIDGET (button);
}

float
bacon_volume_button_get_value (BaconVolumeButton * button)
{
  g_return_val_if_fail (button != NULL, 0);

  return gtk_range_get_value (GTK_RANGE (button->scale));
}

void
bacon_volume_button_set_value (BaconVolumeButton * button,
			       float value)
{
  g_return_if_fail (button != NULL);

  gtk_range_set_value (GTK_RANGE (button->scale), value);
}

/*
 * button callbacks.
 */

static gboolean
bacon_volume_button_scroll (GtkWidget      * widget,
			    GdkEventScroll * event)
{
  BaconVolumeButton *button = BACON_VOLUME_BUTTON (widget);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));
  float d;

  if (event->type != GDK_SCROLL)
    return FALSE;

  d = bacon_volume_button_get_value (button);
  if (event->direction == GDK_SCROLL_UP) {
    d += adj->step_increment;
    if (d > adj->upper)
      d = adj->upper;
  } else {
    d -= adj->step_increment;
    if (d < adj->lower)
      d = adj->lower;
  }
  bacon_volume_button_set_value (button, d);

  return TRUE;
}

static gboolean
bacon_volume_button_press (GtkWidget      * widget,
			   GdkEventButton * event)
{
  BaconVolumeButton *button = BACON_VOLUME_BUTTON (widget);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));
  gint x, y, m, dx, dy, sx, sy, ystartoff, mouse_y;
  float v;
  GdkEventButton *e;
  GdkDisplay *display;
  GdkScreen *screen;

  display = gtk_widget_get_display (widget);
  screen = gtk_widget_get_screen (widget);

  /* position roughly */
  gtk_window_set_screen (GTK_WINDOW (button->dock), screen);

  gdk_window_get_origin (widget->window, &x, &y);
  x += widget->allocation.x;
  y += widget->allocation.y;
  gtk_window_move (GTK_WINDOW (button->dock), x, y - (SCALE_SIZE / 2));
  gtk_widget_show_all (button->dock);
  gdk_window_get_origin (button->dock->window, &dx, &dy);
  dy += button->dock->allocation.y;
  gdk_window_get_origin (button->scale->window, &sx, &sy);
  sy += button->scale->allocation.y;
  ystartoff = sy - dy;
  mouse_y = event->y;
  button->timeout = TRUE;

  /* position (needs widget to be shown already) */
  v = bacon_volume_button_get_value (button) / (adj->upper - adj->lower);
  x += (widget->allocation.width - button->dock->allocation.width) / 2;
  y -= ystartoff;
  y -= GTK_RANGE (button->scale)->min_slider_size / 2;
  m = button->scale->allocation.height -
      GTK_RANGE (button->scale)->min_slider_size;
  y -= m * (1.0 - v);
  y += mouse_y;
  gtk_window_move (GTK_WINDOW (button->dock), x, y);
  gdk_window_get_origin (button->scale->window, &sx, &sy);

  GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);

  /* grab focus */
  gtk_grab_add (button->dock);

  if (gdk_pointer_grab (button->dock->window, TRUE,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			GDK_POINTER_MOTION_MASK, NULL, NULL, event->time)
      != GDK_GRAB_SUCCESS) {
	  gtk_grab_remove (button->dock);
	  gtk_widget_hide (button->dock);
	  return FALSE;
   }

  if (gdk_keyboard_grab (button->dock->window, TRUE, event->time) != GDK_GRAB_SUCCESS) {
	  gdk_display_pointer_ungrab (display, event->time);
	  gtk_grab_remove (button->dock);
	  gtk_widget_hide (button->dock);
	  return FALSE;
  }

  gtk_widget_grab_focus (button->dock);

  /* forward event to the slider */
  e = (GdkEventButton *) gdk_event_copy ((GdkEvent *) event);
  e->window = button->scale->window;

  /* position: the X position isn't relevant, halfway will work just fine.
   * The vertical position should be *exactly* in the middle of the slider
   * of the scale; if we don't do that correctly, it'll move from its current
   * position, which means a position change on-click, which is bad. */
  e->x = button->scale->allocation.width / 2;
  m = button->scale->allocation.height -
      GTK_RANGE (button->scale)->min_slider_size;
  e->y = ((1.0 - v) * m) + GTK_RANGE (button->scale)->min_slider_size / 2;
  gtk_widget_event (button->scale, (GdkEvent *) e);
  e->window = event->window;
  gdk_event_free ((GdkEvent *) e);

  button->pop_time = event->time;

  return TRUE;
}

static gboolean
bacon_volume_key_release (GtkWidget      * widget,
    			  GdkEventKey    * event)
{
  BaconVolumeButton *button = BACON_VOLUME_BUTTON (widget);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));
  gint x, y, m, dx, dy, sx, sy, ystartoff;
  float v;
  GdkDisplay *display;
  GdkScreen *screen;

  if (event->keyval != GDK_space && event->keyval != GDK_Return)
    return FALSE;

  display = gtk_widget_get_display (widget);
  screen = gtk_widget_get_screen (widget);

  /* position roughly */
  gtk_window_set_screen (GTK_WINDOW (button->dock), screen);

  gdk_window_get_origin (widget->window, &x, &y);
  x += widget->allocation.x;
  y += widget->allocation.y;
  gtk_window_move (GTK_WINDOW (button->dock), x, y - (SCALE_SIZE / 2));
  gtk_widget_show_all (button->dock);
  gdk_window_get_origin (button->dock->window, &dx, &dy);
  dy += button->dock->allocation.y;
  gdk_window_get_origin (button->scale->window, &sx, &sy);
  sy += button->scale->allocation.y;
  ystartoff = sy - dy;
  button->timeout = TRUE;

  /* position (needs widget to be shown already) */
  v = bacon_volume_button_get_value (button) / (adj->upper - adj->lower);
  x += (widget->allocation.width - button->dock->allocation.width) / 2;
  y -= ystartoff;
  y -= GTK_RANGE (button->scale)->min_slider_size / 2;
  m = button->scale->allocation.height -
      GTK_RANGE (button->scale)->min_slider_size;
  y -= m * (1.0 - v);
  gtk_window_move (GTK_WINDOW (button->dock), x, y);
  gdk_window_get_origin (button->scale->window, &sx, &sy);

  /* grab focus */
  gtk_grab_add (button->dock);

  if (gdk_pointer_grab (button->dock->window, TRUE,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
			NULL, NULL, event->time)
      != GDK_GRAB_SUCCESS) {
	  gtk_grab_remove (button->dock);
	  gtk_widget_hide (button->dock);
	  return FALSE;
   }

  if (gdk_keyboard_grab (button->dock->window, TRUE, event->time) != GDK_GRAB_SUCCESS) {
	  gdk_display_pointer_ungrab (display, event->time);
	  gtk_grab_remove (button->dock);
	  gtk_widget_hide (button->dock);
	  return FALSE;
  }

  gtk_widget_grab_focus (button->scale);

  button->pop_time = event->time;

  return TRUE;
}

/*
 * +/- button callbacks.
 */

static gboolean
cb_button_timeout (gpointer data)
{
  BaconVolumeButton *button = BACON_VOLUME_BUTTON (data);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));
  float val;
  gboolean res = TRUE;

  if (button->click_id == 0)
    return FALSE;

  val = bacon_volume_button_get_value (button);
  val += button->direction;
  if (val <= adj->lower) {
    res = FALSE;
    val = adj->lower;
  } else if (val > adj->upper) {
    res = FALSE;
    val = adj->upper;
  }
  bacon_volume_button_set_value (button, val);

  if (!res) {
    g_source_remove (button->click_id);
    button->click_id = 0;
  }

  return res;
}

static gboolean
cb_button_press (GtkWidget      * widget,
		 GdkEventButton * event,
		 gpointer         data)
{
  BaconVolumeButton *button = BACON_VOLUME_BUTTON (data);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));

  if (button->click_id != 0)
    g_source_remove (button->click_id);
  button->direction = (widget == button->plus) ?
      fabs (adj->page_increment) : - fabs (adj->page_increment);
  button->click_id = g_timeout_add (CLICK_TIMEOUT,
				    (GSourceFunc) cb_button_timeout, button);
  cb_button_timeout (button);

  return TRUE;
}

static gboolean
cb_button_release (GtkWidget      * widget,
		   GdkEventButton * event,
		   gpointer         data)
{
  BaconVolumeButton *button = BACON_VOLUME_BUTTON (data);

  if (button->click_id != 0) {
    g_source_remove (button->click_id);
    button->click_id = 0;
  }

  return TRUE;
}

/*
 * Scale callbacks.
 */

static void
bacon_volume_release_grab (BaconVolumeButton *button,
			   GdkEventButton * event)
{
  GdkEventButton *e;
  GdkDisplay *display;

  /* ungrab focus */
  display = gtk_widget_get_display (GTK_WIDGET (button));
  gdk_display_keyboard_ungrab (display, event->time);
  gdk_display_pointer_ungrab (display, event->time);
  gtk_grab_remove (button->dock);

  /* hide again */
  gtk_widget_hide (button->dock);
  button->timeout = FALSE;

  e = (GdkEventButton *) gdk_event_copy ((GdkEvent *) event);
  e->window = GTK_WIDGET (button)->window;
  e->type = GDK_BUTTON_RELEASE;
  gtk_widget_event (GTK_WIDGET (button), (GdkEvent *) e);
  e->window = event->window;
  gdk_event_free ((GdkEvent *) e);
}

static gboolean
cb_dock_button_press (GtkWidget      * widget,
		      GdkEventButton * event,
		      gpointer         data)
{
  //GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *) event);
  BaconVolumeButton *button = BACON_VOLUME_BUTTON (data);

  if (/*ewidget == button->dock &&*/ event->type == GDK_BUTTON_PRESS) {
    bacon_volume_release_grab (button, event);
    return TRUE;
  }

  return FALSE;
}

static gboolean
cb_dock_key_release (GtkWidget      * widget,
		     GdkEventKey    * event,
		     gpointer         data)
{
  BaconVolumeButton *button = BACON_VOLUME_BUTTON (data);

  if (event->keyval == GDK_Escape) {
    GdkDisplay *display;

    /* ungrab focus */
    display = gtk_widget_get_display (widget);
    gdk_display_keyboard_ungrab (display, event->time);
    gdk_display_pointer_ungrab (display, event->time);
    gtk_grab_remove (button->dock);

    /* hide again */
    gtk_widget_hide (button->dock);
    button->timeout = FALSE;
   
    return TRUE;
  }
  return FALSE;
}

static gboolean
cb_dock_key_press (GtkWidget      * widget,
		   GdkEventKey    * event,
		   gpointer         data)
{
  if (event->keyval == GDK_Escape) {
    return TRUE;
  }
  return FALSE;
}

/*
 * Scale stuff.
 */

#define BACON_TYPE_VOLUME_SCALE \
  (bacon_volume_scale_get_type ())
#define BACON_VOLUME_SCALE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), BACON_TYPE_VOLUME_SCALE, \
			       BaconVolumeScale))

typedef struct _BaconVolumeScale {
  GtkVScale parent;
  BaconVolumeButton *button;
} BaconVolumeScale;

static GType	bacon_volume_scale_get_type	 (void);

static void	bacon_volume_scale_class_init    (GtkVScaleClass * klass);

static gboolean	bacon_volume_scale_press	 (GtkWidget      * widget,
						  GdkEventButton * event);
static gboolean bacon_volume_scale_release	 (GtkWidget      * widget,
						  GdkEventButton * event);

static GtkVScaleClass *scale_parent_class = NULL;

static GType
bacon_volume_scale_get_type (void)
{
  static GType bacon_volume_scale_type = 0;

  if (G_UNLIKELY (bacon_volume_scale_type == 0)) {
    const GTypeInfo bacon_volume_scale_info = {
      sizeof (GtkVScaleClass),
      NULL,
      NULL,
      (GClassInitFunc) bacon_volume_scale_class_init,
      NULL,
      NULL,
      sizeof (BaconVolumeScale),
      0,
      NULL,
      NULL
    };

    bacon_volume_scale_type =
        g_type_register_static (GTK_TYPE_VSCALE,
				"BaconVolumeScale",
				&bacon_volume_scale_info, 0);
  }

  return bacon_volume_scale_type;
}

static void
bacon_volume_scale_class_init (GtkVScaleClass * klass)
{
  GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS (klass);
  GtkRangeClass *gtkrange_class = GTK_RANGE_CLASS (klass);

  scale_parent_class = g_type_class_peek_parent (klass);

  gtkwidget_class->button_press_event = bacon_volume_scale_press;
  gtkwidget_class->button_release_event = bacon_volume_scale_release;
  gtkrange_class->value_changed = bacon_volume_scale_value_changed;
}

static GtkWidget *
bacon_volume_scale_new (BaconVolumeButton * button,
			float min, float max,
			float step)
{
  BaconVolumeScale *scale = g_object_new (BACON_TYPE_VOLUME_SCALE, NULL);
  GtkObject *adj;

  adj = gtk_adjustment_new (min, min, max, step, 10 * step, 0);
  gtk_range_set_adjustment (GTK_RANGE (scale), GTK_ADJUSTMENT (adj));
  scale->button = button;

  return GTK_WIDGET (scale);
}

static gboolean
bacon_volume_scale_press (GtkWidget      * widget,
			  GdkEventButton * event)
{
  BaconVolumeScale *scale = BACON_VOLUME_SCALE (widget);
  BaconVolumeButton *button = scale->button;

  /* the scale will grab input; if we have input grabbed, all goes
   * horribly wrong, so let's not do that. */
  gtk_grab_remove (button->dock);

  return GTK_WIDGET_CLASS (scale_parent_class)->button_press_event (widget, event);
}

static gboolean
bacon_volume_scale_release (GtkWidget      * widget,
			    GdkEventButton * event)
{
  BaconVolumeScale *scale = BACON_VOLUME_SCALE (widget);
  BaconVolumeButton *button = scale->button;
  gboolean res;

  if (button->timeout) {
    /* if we did a quick click, leave the window open; else, hide it */
    if (event->time > button->pop_time + CLICK_TIMEOUT) {
      bacon_volume_release_grab (button, event);
      GTK_WIDGET_CLASS (scale_parent_class)->button_release_event (widget, event);
      return TRUE;
    }
    button->timeout = FALSE;
  }

  res = GTK_WIDGET_CLASS (scale_parent_class)->button_release_event (widget, event);

  /* the scale will release input; right after that, we *have to* grab
   * it back so we can catch out-of-scale clicks and hide the popup,
   * so I basically want a g_signal_connect_after_always(), but I can't
   * find that, so we do this complex 'first-call-parent-then-do-actual-
   * action' thingy... */
  gtk_grab_add (button->dock);

  return res;
}

static void
bacon_volume_button_update_icon (BaconVolumeButton *button)
{
  static const char icon_name[][21] = {
    "audio-volume-muted",
    "audio-volume-low",
    "audio-volume-medium",
    "audio-volume-high"
  };
  GtkRange *range = GTK_RANGE (button->scale);
  float val = gtk_range_get_value (range);
  GtkAdjustment *adj;
  float step;
  guint icon;

  adj = gtk_range_get_adjustment (range);
  step = (adj->upper - adj->lower) / 4;

  if (val == adj->lower)
    icon = 0;
  else if (val > adj->lower && val <= adj->lower + step)
    icon = 1;
  else if (val > adj->lower + step && val <= adj->lower + step * 2)
    icon = 2;
  else
    icon = 3;

  if (button->icon == icon)
    return;

  button->icon = icon;
  gtk_image_set_from_icon_name (GTK_IMAGE (button->image),
			        icon_name[icon],
			        button->size);
}

static void
bacon_volume_button_update_tip (BaconVolumeButton *button)
{
  GtkRange *range = GTK_RANGE (button->scale);
  float val = gtk_range_get_value (range);
  GtkAdjustment *adj;
  char *str;

  adj = gtk_range_get_adjustment (range);

  if (val == adj->lower) {
    str = g_strdup (_("Muted"));
  } else if (val == adj->upper) {
    str = g_strdup (_("Full Volume"));
  } else {
    str = g_strdup_printf ("%d %%",
			   (int) ((val - adj->lower) / (adj->upper - adj->lower) * 100));
  }

  gtk_tooltips_set_tip (button->tooltips, GTK_WIDGET (button),
			str, NULL);
  g_free (str);
}

static void
bacon_volume_scale_value_changed (GtkRange * range)
{
  BaconVolumeScale *scale = BACON_VOLUME_SCALE (range);
  BaconVolumeButton *button = scale->button;

  bacon_volume_button_update_icon (button);
  bacon_volume_button_update_tip (button);

  /* signal */
  g_signal_emit (button, signals[SIGNAL_VALUE_CHANGED], 0);
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
