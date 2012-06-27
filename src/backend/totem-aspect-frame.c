/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * A container that respects the aspect ratio of its child
 *
 * Copyright 2010, 2011 Intel Corporation.
 * Copyright 2012, Red Hat, Inc.
 *
 * Based upon mx-aspect-frame.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 */

#include <math.h>

#include "totem-aspect-frame.h"

G_DEFINE_TYPE (TotemAspectFrame, totem_aspect_frame, CLUTTER_TYPE_ACTOR)

#define ASPECT_FRAME_PRIVATE(o)                         \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o),                    \
                                TOTEM_TYPE_ASPECT_FRAME,   \
                                TotemAspectFramePrivate))

enum
{
  PROP_0,

  PROP_EXPAND,
};

struct _TotemAspectFramePrivate
{
  guint expand : 1;
  gdouble rotation;
};


static void
totem_aspect_frame_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  TotemAspectFrame *frame = TOTEM_ASPECT_FRAME (object);

  switch (property_id)
    {
    case PROP_EXPAND:
      g_value_set_boolean (value, totem_aspect_frame_get_expand (frame));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
totem_aspect_frame_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  switch (property_id)
    {
    case PROP_EXPAND:
      totem_aspect_frame_set_expand (TOTEM_ASPECT_FRAME (object),
                                   g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
totem_aspect_frame_dispose (GObject *object)
{
  G_OBJECT_CLASS (totem_aspect_frame_parent_class)->dispose (object);
}

static void
totem_aspect_frame_finalize (GObject *object)
{
  G_OBJECT_CLASS (totem_aspect_frame_parent_class)->finalize (object);
}

static void
totem_aspect_frame_get_preferred_width (ClutterActor *actor,
                                        gfloat        for_height,
                                        gfloat       *min_width_p,
                                        gfloat       *nat_width_p)
{
  gboolean override;

  if (for_height >= 0)
    override = FALSE;
  else
    g_object_get (G_OBJECT (actor), "natural-height-set", &override, NULL);

  if (override)
    g_object_get (G_OBJECT (actor), "natural-height", &for_height, NULL);

  CLUTTER_ACTOR_CLASS (totem_aspect_frame_parent_class)->
    get_preferred_width (actor, for_height, min_width_p, nat_width_p);
}

static void
totem_aspect_frame_get_preferred_height (ClutterActor *actor,
                                         gfloat        for_width,
                                         gfloat       *min_height_p,
                                         gfloat       *nat_height_p)
{
  gboolean override;

  if (for_width >= 0)
    override = FALSE;
  else
    g_object_get (G_OBJECT (actor), "natural-width-set", &override, NULL);

  if (override)
    g_object_get (G_OBJECT (actor), "natural-width", &for_width, NULL);

  CLUTTER_ACTOR_CLASS (totem_aspect_frame_parent_class)->
    get_preferred_height (actor, for_width, min_height_p, nat_height_p);
}

static void
totem_aspect_frame_set_rotation_internal (TotemAspectFrame *frame,
					  gdouble           rotation,
					  gboolean          animate)
{
  ClutterActor *actor;
  gfloat w, h, center_x, center_y;
  gdouble scale;

  g_return_if_fail (TOTEM_IS_ASPECT_FRAME (frame));

  g_object_get (G_OBJECT (frame),
		"width", &w,
		"height", &h,
		NULL);

  /* So that the larger side fits the smaller side
   * FIXME: check the angle instead */
  if (rotation == 90.0 || rotation == 270.0)
    {
      if (w > h)
        scale = h / (double) w;
      else
        scale = w / (double) h;
    }
  else
    {
      scale = 1.0;
    }

  center_x = w * 0.5f;
  center_y = h * 0.5f;

  actor = CLUTTER_ACTOR (frame);

  if (animate)
    {
      clutter_actor_save_easing_state (actor);
      clutter_actor_set_easing_duration (actor, 500);
    }
  /* FIXME: When animated, make sure that we go in the right direction,
   * otherwise we'll spin in the wrong direction going back to 0 from 270 */
  clutter_actor_set_rotation (actor, CLUTTER_Z_AXIS, rotation, center_x, center_y, 0);
  clutter_actor_set_scale_full (actor, scale, scale, center_x, center_y);

  if (animate)
    clutter_actor_restore_easing_state (actor);
}

static void
totem_aspect_frame_allocate (ClutterActor           *actor,
                             const ClutterActorBox  *box,
                             ClutterAllocationFlags  flags)
{
  ClutterActor *child;
  ClutterActorBox child_box;
  gfloat aspect, child_aspect, width, height, box_width, box_height;
  gdouble rotation;

  TotemAspectFramePrivate *priv = TOTEM_ASPECT_FRAME (actor)->priv;

  CLUTTER_ACTOR_CLASS (totem_aspect_frame_parent_class)->
    allocate (actor, box, flags);

  child = clutter_actor_get_child_at_index (actor, 0);
  if (!child)
    return;

  box_width = box->x2 - box->x1;
  box_height = box->y2 - box->y1;
  clutter_actor_get_preferred_size (child, NULL, NULL, &width, &height);

  if (width <= 0.0f || height <= 0.0f)
    return;

  aspect = box_width / box_height;
  child_aspect = width / height;

  if ((aspect < child_aspect) ^ priv->expand)
    {
      width = box_width;
      height = box_width / child_aspect;
    }
  else
    {
      height = box_height;
      width = box_height * child_aspect;
    }

  child_box.x1 = (box_width - width) / 2;
  child_box.y1 = (box_height - height) / 2;
  child_box.x2 = child_box.x1 + width;
  child_box.y2 = child_box.y1 + height;

  clutter_actor_allocate (child, &child_box, flags);

  clutter_actor_set_easing_duration (child, 0);

  /* FIXME: We should swap height and width if the actor is on its side */
  rotation = totem_aspect_frame_get_rotation (TOTEM_ASPECT_FRAME (actor));
  totem_aspect_frame_set_rotation_internal (TOTEM_ASPECT_FRAME (actor),
					    rotation,
					    FALSE);
}

static void
totem_aspect_frame_paint (ClutterActor *actor)
{
  ClutterActor *child;
  TotemAspectFramePrivate *priv = TOTEM_ASPECT_FRAME (actor)->priv;

  child = clutter_actor_get_child_at_index (actor, 0);

  if (!child)
    return;

  if (priv->expand)
    {
      gfloat width, height;

      clutter_actor_get_size (actor, &width, &height);

      cogl_clip_push_rectangle (0.0, 0.0, width, height);
      clutter_actor_paint (child);
      cogl_clip_pop ();
    }
  else
    clutter_actor_paint (child);
}

static void
totem_aspect_frame_pick (ClutterActor       *actor,
                         const ClutterColor *color)
{
  ClutterActorBox box;
  ClutterActor *child;
  TotemAspectFramePrivate *priv = TOTEM_ASPECT_FRAME (actor)->priv;

  clutter_actor_get_allocation_box (actor, &box);

  cogl_set_source_color4ub (color->red, color->green,
                            color->blue, color->alpha);
  cogl_rectangle (box.x1, box.y1, box.x2, box.y2);

  child = clutter_actor_get_child_at_index (actor, 0);

  if (!child)
    return;

  if (priv->expand)
    {
      cogl_clip_push_rectangle (0.0, 0.0, box.x2 - box.x1, box.y2 - box.y1);
      clutter_actor_paint (child);
      cogl_clip_pop ();
    }
  else
    clutter_actor_paint (child);
}

static void
totem_aspect_frame_class_init (TotemAspectFrameClass *klass)
{
  GParamSpec *pspec;

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TotemAspectFramePrivate));

  object_class->get_property = totem_aspect_frame_get_property;
  object_class->set_property = totem_aspect_frame_set_property;
  object_class->dispose = totem_aspect_frame_dispose;
  object_class->finalize = totem_aspect_frame_finalize;

  actor_class->get_preferred_width = totem_aspect_frame_get_preferred_width;
  actor_class->get_preferred_height = totem_aspect_frame_get_preferred_height;
  actor_class->allocate = totem_aspect_frame_allocate;
  actor_class->paint = totem_aspect_frame_paint;
  actor_class->pick = totem_aspect_frame_pick;

  pspec = g_param_spec_boolean ("expand",
                                "Expand",
                                "Fill the allocated area with the child and "
                                "clip off the excess.",
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_EXPAND, pspec);
}

static void
totem_aspect_frame_init (TotemAspectFrame *self)
{
  self->priv = ASPECT_FRAME_PRIVATE (self);
}

ClutterActor *
totem_aspect_frame_new (void)
{
  return g_object_new (TOTEM_TYPE_ASPECT_FRAME, NULL);
}

void
totem_aspect_frame_set_expand (TotemAspectFrame *frame, gboolean expand)
{
  TotemAspectFramePrivate *priv;

  g_return_if_fail (TOTEM_IS_ASPECT_FRAME (frame));

  priv = frame->priv;
  if (priv->expand != expand)
    {
      ClutterActor *child;

      priv->expand = expand;
      child = clutter_actor_get_child_at_index (CLUTTER_ACTOR (frame), 0);
      if (child)
        {
          /* Duration will be reset in _allocate() */
          clutter_actor_set_easing_duration (child, 500);
          clutter_actor_queue_relayout (CLUTTER_ACTOR (frame));
        }

      g_object_notify (G_OBJECT (frame), "expand");
    }
}

gboolean
totem_aspect_frame_get_expand (TotemAspectFrame *frame)
{
  g_return_val_if_fail (TOTEM_IS_ASPECT_FRAME (frame), FALSE);
  return frame->priv->expand;
}

void
totem_aspect_frame_set_child   (TotemAspectFrame *frame,
				ClutterActor     *child)
{
  g_return_if_fail (TOTEM_IS_ASPECT_FRAME (frame));

  clutter_actor_add_child (CLUTTER_ACTOR (frame), child);
}

void
totem_aspect_frame_set_rotation (TotemAspectFrame *frame,
				 gdouble           rotation)
{
  g_return_if_fail (TOTEM_IS_ASPECT_FRAME (frame));

  rotation = fmod (rotation, 360.0);

  g_debug ("Setting rotation to '%lf'", rotation);

  frame->priv->rotation = rotation;
  totem_aspect_frame_set_rotation_internal (frame, rotation, TRUE);
}

gdouble
totem_aspect_frame_get_rotation (TotemAspectFrame *frame)
{
  gdouble rotation;

  g_return_val_if_fail (TOTEM_IS_ASPECT_FRAME (frame), 0.0);

  rotation = fmod (frame->priv->rotation, 360.0);
  g_debug ("Got rotation %lf", rotation);

  return rotation;
}
