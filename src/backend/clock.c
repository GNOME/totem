/*
 * Copyright © 2013 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Joaquim Rocha <me@joaquimrocha.com>
 */

#include <math.h>
#include "clock.h"

#define CLOCK_RADIUS                   150
#define CLOCK_LINE_WIDTH               40
#define CLOCK_LINE_PADDING             10
#define ANGLE_PROP_NAME                "angle"

G_DEFINE_TYPE (Clock, clock, CLUTTER_TYPE_ACTOR);

enum {
  PROP_0,
  PROP_ANGLE,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
draw_clock (ClutterCairoTexture *texture,
            cairo_t             *cr,
            gint                 width,
            gint                 height,
            Clock               *self)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  /* Draw the clock background */
  cairo_arc(cr, width / 2, height / 2, CLOCK_RADIUS / 2, 0.0, 2.0 * M_PI);
  cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
  cairo_fill_preserve(cr);
  cairo_stroke(cr);

  cairo_set_line_width(cr, CLOCK_LINE_WIDTH);

  cairo_arc(cr,
            width / 2,
            height / 2,
            (CLOCK_RADIUS - CLOCK_LINE_WIDTH - CLOCK_LINE_PADDING) / 2,
            3 * M_PI_2,
            3 * M_PI_2 + self->angle * M_PI / 180.0);
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_stroke(cr);
}

static void
clock_set_property (GObject      *object,
                    guint         property_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
  Clock *self = CLOCK (object);
  ClutterContent *content;
  content = clutter_actor_get_content (CLUTTER_ACTOR (self));

  switch (property_id)
    {
    case PROP_ANGLE:
      self->angle = g_value_get_float (value);
      if (content)
        clutter_content_invalidate (content);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
clock_get_property (GObject      *object,
                    guint         property_id,
                    GValue       *value,
                    GParamSpec   *pspec)
{
  Clock *self = CLOCK (object);

  switch (property_id)
    {
    case PROP_ANGLE:
      g_value_set_float (value, self->angle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
clock_init (Clock *self)
{
  self->angle = 0;

  ClutterContent *content;
  content = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (content),
                           CLOCK_RADIUS + 2,
                           CLOCK_RADIUS + 2);
  clutter_actor_set_content (CLUTTER_ACTOR (self), content);
  g_signal_connect (CLUTTER_CANVAS (content),
                    "draw",
                    G_CALLBACK (draw_clock),
                    self);
  g_object_unref (content);
}

static void
clock_get_preferred_width (ClutterActor *actor,
                           gfloat        for_height,
                           gfloat       *min_width_p,
                           gfloat       *natural_width_p)
{
  *min_width_p = CLOCK_RADIUS + 2;
  *natural_width_p = CLOCK_RADIUS + 2;
}

static void
clock_get_preferred_height (ClutterActor *actor,
                            gfloat        for_width,
                            gfloat       *min_height_p,
                            gfloat       *natural_height_p)
{
  *min_height_p = CLOCK_RADIUS + 2;
  *natural_height_p = CLOCK_RADIUS + 2;
}


static void
clock_class_init (ClockClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *clutter_actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->set_property = clock_set_property;
  gobject_class->get_property = clock_get_property;

  clutter_actor_class->get_preferred_width = clock_get_preferred_width;
  clutter_actor_class->get_preferred_height = clock_get_preferred_height;

  obj_properties[PROP_ANGLE] =
    g_param_spec_float (ANGLE_PROP_NAME,
                        "The angle of the clock's progress",
                        "Set the angle of the clock's progress",
                        .0,
                        360.0,
                        .0,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

ClutterActor *
clock_new (void)
{
  return g_object_new (CLOCK_TYPE, NULL);
}
