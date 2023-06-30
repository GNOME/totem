/*
 * Copyright (C) 2022 Red Hat Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 */

#include "totem-playlist-inspector-page.h"
#include "totem-private.h"

struct _TotemPlaylistInspectorPage {
	GtkBox parent_instance;

	TotemObject *totem;
	GObject *object;
};

G_DEFINE_FINAL_TYPE (TotemPlaylistInspectorPage, totem_playlist_inspector_page, GTK_TYPE_BOX)

enum {
	PROP_0,
	PROP_TITLE,
	PROP_OBJECT,
	LAST_PROP,
};

static GParamSpec *props[LAST_PROP];

static gboolean
insert_playlist_widget (TotemPlaylistInspectorPage *self)
{
	GApplication *app;
	TotemObject *totem;

	if (self->totem)
		return G_SOURCE_REMOVE;
	app = g_application_get_default ();
	if (!app || !TOTEM_IS_OBJECT (app)) {
		g_warning ("TotemPlaylistInspectorPage instantiated for non-totem app");
		return G_SOURCE_REMOVE;
	}
	totem = TOTEM_OBJECT (app);
	if (!totem->playlist)
		return G_SOURCE_CONTINUE;
	gtk_container_add (GTK_CONTAINER (self),
			   GTK_WIDGET (totem->playlist));
	gtk_widget_set_hexpand (GTK_WIDGET (totem->playlist), TRUE);
	gtk_widget_show_all (GTK_WIDGET (self));
	self->totem = totem;
	return G_SOURCE_REMOVE;
}

static void
totem_playlist_inspector_page_get_property (GObject    *object,
					    guint       prop_id,
					    GValue     *value,
					    GParamSpec *pspec)
{
	TotemPlaylistInspectorPage *self = TOTEM_PLAYLIST_INSPECTOR_PAGE (object);

	switch (prop_id) {
	case PROP_TITLE:
		g_value_set_string (value, "Playlist");
		break;
	case PROP_OBJECT:
		g_value_set_object (value, self->object);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
totem_playlist_inspector_page_set_property (GObject      *object,
					    guint         prop_id,
					    const GValue *value,
					    GParamSpec   *pspec)
{
	TotemPlaylistInspectorPage *self = TOTEM_PLAYLIST_INSPECTOR_PAGE (object);

	switch (prop_id) {
	case PROP_OBJECT:
		g_set_object (&self->object, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
totem_playlist_inspector_page_dispose (GObject *object)
{
	TotemPlaylistInspectorPage *self = TOTEM_PLAYLIST_INSPECTOR_PAGE (object);

	g_clear_object (&self->object);

	G_OBJECT_CLASS (totem_playlist_inspector_page_parent_class)->dispose (object);
}

static void
totem_playlist_inspector_page_class_init (TotemPlaylistInspectorPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = totem_playlist_inspector_page_get_property;
	object_class->set_property = totem_playlist_inspector_page_set_property;
	object_class->dispose = totem_playlist_inspector_page_dispose;

	props[PROP_TITLE] =
		g_param_spec_string ("title",
				     "Title",
				     "Title",
				     "Playlist",
				     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	props[PROP_OBJECT] =
		g_param_spec_object ("object",
				     "Object",
				     "Object",
				     G_TYPE_OBJECT,
				     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
totem_playlist_inspector_page_init (TotemPlaylistInspectorPage *self)
{
	g_idle_add ((GSourceFunc) insert_playlist_widget, self);
}
