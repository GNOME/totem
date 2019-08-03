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
 * Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "bacon-video-controls-actor.h"
#include "bacon-time-label.h"

struct BaconVideoControlsActorPrivate
{
	GtkBuilder *builder;
	GtkRange *seek;
	GObject *bvw;
};

G_DEFINE_TYPE_WITH_PRIVATE (BaconVideoControlsActor, bacon_video_controls_actor, GTK_CLUTTER_TYPE_ACTOR);

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
	gtk_style_context_add_class (gtk_widget_get_style_context (contents), "bottom");
	gtk_widget_override_background_color (gtk_clutter_actor_get_widget (GTK_CLUTTER_ACTOR (object)), 0, &transparent);
}

static void
bacon_video_controls_actor_class_init (BaconVideoControlsActorClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

        gobject_class->finalize = bacon_video_controls_actor_finalize;
        gobject_class->constructed = bacon_video_controls_actor_constructed;
}

static void
setup_object (BaconVideoControlsActor *controls,
	      const char              *name)
{
	GObject *obj;

	obj = gtk_builder_get_object (controls->priv->builder, name);

	/* Setup an easy way to lookup the widgets by name without
	 * exposing the API directly to totem */
	g_object_set_data (G_OBJECT (controls), name, obj);
}

static void
disable_popover_transitions (BaconVideoControlsActor *controls)
{
	GtkPopover *popover;
	GObject *obj;

	obj = gtk_builder_get_object (controls->priv->builder, "volume_button");
	popover = GTK_POPOVER (gtk_scale_button_get_popup (GTK_SCALE_BUTTON (obj)));
	gtk_popover_set_transitions_enabled (popover, FALSE);
}

static void
bacon_video_controls_actor_init (BaconVideoControlsActor *controls)
{
	const char *objects[] = { "toolbar", NULL };

	controls->priv = bacon_video_controls_actor_get_instance_private (controls);

	g_type_class_ref (BACON_TYPE_TIME_LABEL);

	controls->priv->builder = gtk_builder_new ();
	if (gtk_builder_add_objects_from_file (controls->priv->builder, DATADIR "/controls.ui", (gchar **) objects, NULL) == 0)
		g_assert_not_reached ();

	setup_object (controls, "seek_scale");
	setup_object (controls, "controls_box");
	setup_object (controls, "go_button");
	setup_object (controls, "volume_button");
	setup_object (controls, "time_label");
	setup_object (controls, "time_rem_label");

	disable_popover_transitions (controls);
}

ClutterActor *
bacon_video_controls_actor_new (void)
{
        return g_object_new (BACON_TYPE_VIDEO_CONTROLS_ACTOR, NULL);
}
