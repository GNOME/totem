/*
 * (C) Copyright 2007 Bastien Nocera <hadess@hadess.net>
 *
 * Glow code from libwnck/libwnck/tasklist.c:
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Kim Woelders
 * Copyright (C) 2003 Red Hat, Inc.
 *
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

#include <math.h>
#include <gtk/gtk.h>
#include "totem-glow-button.h"

#define FADE_OPACITY_DEFAULT 0.6

struct _TotemGlowButton {
	GtkButton parent;

	GdkPixbuf *screenshot;
	GdkPixbuf *screenshot_faded;

	gdouble glow_start_time;

	guint button_glow;

	guint glow : 1;
	guint anim_enabled : 1;
};

static void	totem_glow_button_class_init	(TotemGlowButtonClass * klass);
static void	totem_glow_button_init		(TotemGlowButton      * button);

static GtkButtonClass *parent_class;

G_DEFINE_TYPE(TotemGlowButton, totem_glow_button, GTK_TYPE_BUTTON);

static GdkPixbuf *
glow_pixbuf (TotemGlowButton *button,
	     gdouble          factor)
{
	GdkPixbuf *destination;

	destination = gdk_pixbuf_copy (button->screenshot);
	if (destination == NULL)
		return NULL;

	gdk_pixbuf_composite (button->screenshot_faded, destination, 0, 0,
			      gdk_pixbuf_get_width (button->screenshot),
			      gdk_pixbuf_get_height (button->screenshot),
			      0, 0, 1, 1, GDK_INTERP_NEAREST,
			      ABS((int)(factor * G_MAXUINT8)));

	return destination;
}

static void
totem_glow_button_do_expose (TotemGlowButton *button)
{
	GdkRectangle area;
	GtkWidget *buttonw;

	buttonw = GTK_WIDGET (button);
	if (buttonw->window == NULL)
		return;

	area.x = buttonw->allocation.x;
	area.y = buttonw->allocation.y;
	area.width = buttonw->allocation.width;
	area.height = buttonw->allocation.height;

	/* Send a fake expose event */
	gdk_window_invalidate_rect (buttonw->window, &area, TRUE);
	gdk_window_process_updates (buttonw->window, TRUE);
}

static gboolean
totem_glow_button_glow (TotemGlowButton *button)
{ 
	GdkPixbuf *glowing_screenshot;
	GtkWidget *buttonw;
	GTimeVal tv;
	gdouble glow_factor, now;
	gfloat fade_opacity, loop_time;

	buttonw = GTK_WIDGET (button);

	if (GTK_WIDGET_REALIZED (buttonw) == FALSE)
		return TRUE;

	if (button->screenshot == NULL) {
		/* Send a fake expose event to get a screenshot */
		totem_glow_button_do_expose (button);
		if (button->screenshot == NULL)
			return TRUE;
	}

	if (button->anim_enabled != FALSE) {
		g_get_current_time (&tv); 
		now = (tv.tv_sec * (1.0 * G_USEC_PER_SEC) +
		       tv.tv_usec) / G_USEC_PER_SEC;

		if (button->glow_start_time <= G_MINDOUBLE)
			button->glow_start_time = now;

		/* Hard-coded values */
		fade_opacity = FADE_OPACITY_DEFAULT;
		loop_time = 3.0;

		glow_factor = fade_opacity * (0.5 - 0.5 * cos ((now - button->glow_start_time) * M_PI * 2.0 / loop_time));
	} else {
		glow_factor = FADE_OPACITY_DEFAULT;
	}

	glowing_screenshot = glow_pixbuf (button, glow_factor);
	if (glowing_screenshot == NULL)
		return TRUE;

	gdk_draw_pixbuf (buttonw->window,
			 buttonw->style->fg_gc[GTK_WIDGET_STATE (button)],
			 glowing_screenshot,
			 0, 0, 
			 buttonw->allocation.x,
			 buttonw->allocation.y,
			 gdk_pixbuf_get_width (glowing_screenshot),
			 gdk_pixbuf_get_height (glowing_screenshot),
			 GDK_RGB_DITHER_NORMAL, 0, 0);
	g_object_unref (glowing_screenshot);

	return button->anim_enabled;
}

static void
totem_glow_button_clear_glow_start_timeout_id (TotemGlowButton *button)
{ 
	button->button_glow = 0;
}

static void
cleanup_screenshots (TotemGlowButton *button)
{
	if (button->screenshot != NULL) {
		g_object_unref (button->screenshot);
		button->screenshot = NULL;
	}
	if (button->screenshot_faded != NULL) {
		g_object_unref (button->screenshot_faded);
		button->screenshot_faded = NULL;
	}
}

static void
fake_expose_widget (GtkWidget *widget,
		    GdkPixmap *pixmap,
		    gint       x,
		    gint       y)
{
	GdkWindow *tmp_window;
	GdkEventExpose event;

	event.type = GDK_EXPOSE;
	event.window = pixmap;
	event.send_event = FALSE;
	event.region = NULL;
	event.count = 0;

	tmp_window = widget->window;
	widget->window = pixmap;
	widget->allocation.x += x;
	widget->allocation.y += y;

	event.area = widget->allocation;

	gtk_widget_send_expose (widget, (GdkEvent *) &event);

	widget->window = tmp_window;
	widget->allocation.x -= x;
	widget->allocation.y -= y;
}

static GdkPixbuf*
take_screenshot (TotemGlowButton *button)
{
	GtkWidget *buttonw;
	GdkPixmap *pixmap;
	GdkPixbuf *screenshot;
	gint width, height;

	buttonw = GTK_WIDGET (button);

	width = buttonw->allocation.width;
	height = buttonw->allocation.height;

	pixmap = gdk_pixmap_new (buttonw->window, width, height, -1);

	/* Draw a rectangle with bg[SELECTED] */
	gdk_draw_rectangle (pixmap, buttonw->style->bg_gc[GTK_STATE_SELECTED],
			    TRUE, 0, 0, width + 1, height + 1);

	/* then the image */
	fake_expose_widget (gtk_button_get_image (GTK_BUTTON(button)), pixmap,
			    -buttonw->allocation.x, -buttonw->allocation.y);

	/* get the screenshot, and return */
	screenshot = gdk_pixbuf_get_from_drawable (NULL, pixmap, NULL, 0, 0,
						   0, 0, width, height);
	g_object_unref (pixmap);

	return screenshot;
}

static gboolean
totem_glow_button_expose (GtkWidget        *buttonw,
			  GdkEventExpose   *event)
{
	TotemGlowButton *button;

	button = TOTEM_GLOW_BUTTON (buttonw);

	(* GTK_WIDGET_CLASS (parent_class)->expose_event) (buttonw, event);

	if (button->glow != FALSE && button->screenshot == NULL) {
		button->screenshot = gdk_pixbuf_get_from_drawable (NULL,
								   buttonw->window,
								   NULL,
								   buttonw->allocation.x,
								   buttonw->allocation.y,
								   0, 0,
								   buttonw->allocation.width,
								   buttonw->allocation.height);

		/* we also need to take a screenshot for the faded state */
		button->screenshot_faded = take_screenshot (button);
		return FALSE;
	}

	return FALSE;
}

static void
totem_glow_button_map (GtkWidget *buttonw)
{
	TotemGlowButton *button;

	(* GTK_WIDGET_CLASS (parent_class)->map) (buttonw);

	button = TOTEM_GLOW_BUTTON (buttonw);

	if (button->glow != FALSE && button->button_glow == 0) {
		totem_glow_button_set_glow (button, TRUE);
	}
}

static void
totem_glow_button_unmap (GtkWidget *buttonw)
{
	TotemGlowButton *button;

	button = TOTEM_GLOW_BUTTON (buttonw);

	if (button->button_glow >= 0) {
		g_source_remove (button->button_glow);
		button->button_glow = 0;
	}

	cleanup_screenshots (button);

	(* GTK_WIDGET_CLASS (parent_class)->unmap) (buttonw);
}

static void
totem_glow_button_finalize (GObject *object)
{
	TotemGlowButton *button = TOTEM_GLOW_BUTTON (object);

	totem_glow_button_set_glow (button, FALSE);
	cleanup_screenshots (button);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
totem_glow_button_class_init (TotemGlowButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = totem_glow_button_finalize;
	widget_class->expose_event = totem_glow_button_expose;
	widget_class->map = totem_glow_button_map;
	widget_class->unmap = totem_glow_button_unmap;
}

static void
totem_glow_button_init (TotemGlowButton *button)
{
	button->glow_start_time = 0.0;
	button->button_glow = 0;
	button->screenshot = NULL;
	button->screenshot_faded = NULL;
}

GtkWidget *
totem_glow_button_new (void)
{
	return g_object_new (TOTEM_TYPE_GLOW_BUTTON, NULL);
}

void
totem_glow_button_set_glow (TotemGlowButton *button, gboolean glow)
{
	GtkSettings *settings;
	gboolean anim_enabled;

	g_return_if_fail (TOTEM_IS_GLOW_BUTTON (button));

	if (GTK_WIDGET_MAPPED (GTK_WIDGET (button)) == FALSE
	    && glow != FALSE) {
		button->glow = glow;
		return;
	}

	settings = gtk_settings_get_for_screen
		(gtk_widget_get_screen (GTK_WIDGET (button)));
	g_object_get (G_OBJECT (settings),
		      "gtk-enable-animations", &anim_enabled,
		      NULL);
	button->anim_enabled = anim_enabled;

	if (glow == FALSE && button->button_glow == 0 && button->anim_enabled != FALSE)
		return;
	if (glow != FALSE && button->button_glow != 0)
		return;

	button->glow = glow;

	if (glow != FALSE) {
		button->glow_start_time = 0.0;

		/* The animation doesn't speed up or slow down based on the
		 * timeout value, but instead will just appear smoother or 
		 * choppier.
		 */
		button->button_glow =
			g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
					    100,
					    (GSourceFunc) totem_glow_button_glow, button,
					    (GDestroyNotify) totem_glow_button_clear_glow_start_timeout_id);
	} else {
		if (button->button_glow > 0) {
			g_source_remove (button->button_glow);
			button->button_glow = 0;
		}
		cleanup_screenshots (button);
		totem_glow_button_do_expose (button);
	}
}

gboolean
totem_glow_button_get_glow (TotemGlowButton *button)
{
	return button->glow != FALSE;
}

