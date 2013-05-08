/*
 * Overlaid controls
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

#include "bacon-video-controls-actor.h"
#include "bacon-time-label.h"

#define BACON_VIDEO_CONTROLS_ACTOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), BACON_TYPE_VIDEO_CONTROLS_ACTOR, BaconVideoControlsActorPrivate))

struct BaconVideoControlsActorPrivate
{
	GtkBuilder *builder;
	GtkRange *seek;
	GObject *bvw;
};

G_DEFINE_TYPE (BaconVideoControlsActor, bacon_video_controls_actor, GTK_CLUTTER_TYPE_ACTOR);

static void
bacon_video_controls_actor_finalize (GObject *object)
{
	BaconVideoControlsActor *controls;

	controls = BACON_VIDEO_CONTROLS_ACTOR (object);

	g_object_unref (controls->priv->builder);

	G_OBJECT_CLASS (bacon_video_controls_actor_parent_class)->finalize (object);
}

static void
bacon_video_controls_actor_constructed (GObject *object)
{
	GtkWidget *contents;
	GdkRGBA transparent = { 0, 0, 0, 0 };
	BaconVideoControlsActor *controls;

	controls = BACON_VIDEO_CONTROLS_ACTOR (object);

	contents = GTK_WIDGET (gtk_builder_get_object (controls->priv->builder, "toolbar"));
	g_object_set (object, "contents", contents, "opacity", OVERLAY_OPACITY, "x-expand", TRUE, NULL);

	/* Theming */
	gtk_style_context_add_class (gtk_widget_get_style_context (contents), "osd");
	gtk_widget_override_background_color (gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (object)), 0, &transparent);
}

static void
bacon_video_controls_actor_class_init (BaconVideoControlsActorClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

        gobject_class->finalize = bacon_video_controls_actor_finalize;
        gobject_class->constructed = bacon_video_controls_actor_constructed;

        g_type_class_add_private (klass, sizeof (BaconVideoControlsActorPrivate));
}

static void
setup_object (BaconVideoControlsActor *controls,
	      const char              *name)
{
	GObject *obj;

	obj = gtk_builder_get_object (controls->priv->builder, name);

	/* Setup an easy way to lookup the widgets by name without
	 * exposing the API directly to totem or the plugin viewer */
	g_object_set_data (G_OBJECT (controls), name, obj);
}

static void
bacon_video_controls_actor_init (BaconVideoControlsActor *controls)
{
	char *objects[] = { "toolbar", NULL };

	controls->priv = BACON_VIDEO_CONTROLS_ACTOR_GET_PRIVATE (G_OBJECT (controls));

	g_type_class_ref (BACON_TYPE_TIME_LABEL);

	controls->priv->builder = gtk_builder_new ();
	if (gtk_builder_add_objects_from_file (controls->priv->builder, DATADIR "/controls.ui", objects, NULL) == 0)
		g_assert_not_reached ();

	setup_object (controls, "seek_scale");
	setup_object (controls, "controls_box");
	setup_object (controls, "go_button");
	setup_object (controls, "volume_button");
	setup_object (controls, "time_label");
	setup_object (controls, "time_rem_label");
}

ClutterActor *
bacon_video_controls_actor_new (void)
{
        return g_object_new (BACON_TYPE_VIDEO_CONTROLS_ACTOR, NULL);
}
