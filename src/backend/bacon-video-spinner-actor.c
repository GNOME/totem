/*
 * Overlaid spinner
 *
 * Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include "bacon-video-spinner-actor.h"
#include "clock.h"

#define BACON_VIDEO_SPINNER_ACTOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), BACON_TYPE_VIDEO_SPINNER_ACTOR, BaconVideoSpinnerActorPrivate))

struct BaconVideoSpinnerActorPrivate
{
	ClutterActor     *clock;
};

G_DEFINE_TYPE (BaconVideoSpinnerActor, bacon_video_spinner_actor, CLUTTER_TYPE_ACTOR);

enum {
	PROP_0,
	PROP_PERCENT
};

static void
bacon_video_spinner_actor_set_property (GObject      *object,
					guint         property_id,
					const GValue *value,
					GParamSpec   *pspec)
{
	BaconVideoSpinnerActor *spinner = BACON_VIDEO_SPINNER_ACTOR (object);

	switch (property_id) {
	case PROP_PERCENT:
		g_object_set (G_OBJECT (spinner->priv->clock), "angle", g_value_get_float (value) * 360.0 / 100.0, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
bacon_video_spinner_actor_get_property (GObject      *object,
					guint         property_id,
					GValue       *value,
					GParamSpec   *pspec)
{
	BaconVideoSpinnerActor *spinner = BACON_VIDEO_SPINNER_ACTOR (object);
	gfloat angle;

	switch (property_id) {
	case PROP_PERCENT:
		g_object_get (G_OBJECT (spinner->priv->clock), "angle", &angle, NULL);
		g_value_set_float (value, angle / 360.0 * 100.0);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
bacon_video_spinner_actor_class_init (BaconVideoSpinnerActorClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->set_property = bacon_video_spinner_actor_set_property;
	gobject_class->get_property = bacon_video_spinner_actor_get_property;

	g_object_class_install_property (gobject_class, PROP_PERCENT,
					 g_param_spec_float ("percent", "Percent",
							     "Percentage fill",
							     0.0, 100.0, 0.0,
							     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        g_type_class_add_private (klass, sizeof (BaconVideoSpinnerActorPrivate));
}

static void
bacon_video_spinner_actor_init (BaconVideoSpinnerActor *spinner)
{
	ClutterActor *actor, *layout;
	ClutterConstraint *constraint;
	ClutterColor *color;

	spinner->priv = BACON_VIDEO_SPINNER_ACTOR_GET_PRIVATE (G_OBJECT (spinner));
	actor = CLUTTER_ACTOR (spinner);

	/* We'll set that colour on the layout, as the child doesn't
	 * take the whole space */
	color = clutter_color_copy (clutter_color_get_static (CLUTTER_COLOR_BLACK));
	color->alpha = 128;

	spinner->priv->clock = clock_new ();
	layout = g_object_new (CLUTTER_TYPE_ACTOR,
			       "layout-manager", clutter_bin_layout_new (CLUTTER_BIN_ALIGNMENT_CENTER, CLUTTER_BIN_ALIGNMENT_CENTER),
			       "background-color", color,
			       NULL);
	clutter_color_free (color);
	clutter_actor_add_child (layout, spinner->priv->clock);
	clutter_actor_add_child (actor, layout);

	constraint = clutter_bind_constraint_new (actor, CLUTTER_BIND_SIZE, 0.0);
	clutter_actor_add_constraint_with_name (layout, "size", constraint);
}

ClutterActor *
bacon_video_spinner_actor_new (void)
{
        return g_object_new (BACON_TYPE_VIDEO_SPINNER_ACTOR, NULL);
}
