/*
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 * Copyright (C) 2004  Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more priv.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <config.h>

#include "totem-properties-view.h"

#include "bacon-video-widget-properties.h"
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

struct TotemPropertiesViewPriv {
	char *location;
	GtkWidget *vbox;
	BaconVideoWidgetProperties *props;
	BaconVideoWidget *bvw;
};

static GType tpv_type = 0;
static GObjectClass *parent_class = NULL;
static void totem_properties_view_init (TotemPropertiesView *self);
static void totem_properties_view_class_init (TotemPropertiesViewClass *class);
static void totem_properties_view_destroy (GtkObject *object);

GType
totem_properties_view_get_type (void)
{
	return tpv_type;
}

//XXX use G_DEFINE_TYPE

void
totem_properties_view_register_type (GTypeModule *module)
{
    static const GTypeInfo info = {
	    sizeof (TotemPropertiesViewClass),
	    (GBaseInitFunc) NULL,
	    (GBaseFinalizeFunc) NULL,
	    (GClassInitFunc) totem_properties_view_class_init,
	    NULL,
	    NULL,
	    sizeof (TotemPropertiesView),
	    0,
	    (GInstanceInitFunc) totem_properties_view_init
    };

    tpv_type = g_type_module_register_type (module, GTK_TYPE_TABLE,
		    "TotemPropertiesView",
		    &info, 0);
}

static void
totem_properties_view_class_init (TotemPropertiesViewClass *class)
{
	g_type_class_add_private (class, sizeof (TotemPropertiesViewPriv));
	parent_class = g_type_class_peek_parent (class);
	GTK_OBJECT_CLASS (class)->destroy = totem_properties_view_destroy;
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, TotemPropertiesView *props)
{
	bacon_video_widget_properties_update
		(props->priv->props, props->priv->bvw, FALSE);
	bacon_video_widget_close (props->priv->bvw);
}
static void
totem_properties_view_init (TotemPropertiesView *props)
{
	GError *err = NULL;

	props->priv = G_TYPE_INSTANCE_GET_PRIVATE (props,
			TOTEM_TYPE_PROPERTIES_VIEW,
			TotemPropertiesViewPriv);

	props->priv->bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new
			(-1, -1, BVW_USE_TYPE_METADATA, &err));
	if (props->priv->bvw == NULL)
		g_error ("Error: %s", err ? err->message : "bla");
	//FIXME

	g_signal_connect (G_OBJECT (props->priv->bvw),
			"got-metadata",
			G_CALLBACK (on_got_metadata_event),
			props);

	props->priv->vbox = bacon_video_widget_properties_new ();
	gtk_widget_show (props->priv->vbox);
	props->priv->props = BACON_VIDEO_WIDGET_PROPERTIES (props->priv->vbox);
}

static void
totem_properties_view_destroy (GtkObject *object)
{
	TotemPropertiesView *props;

	props = TOTEM_PROPERTIES_VIEW (object);

	g_object_unref (G_OBJECT (props->priv->bvw));
	props->priv->bvw = NULL;
	g_free(props->priv->location);
	props->priv->location = NULL;
	g_object_unref (G_OBJECT (props->priv->props));
	props->priv->props = NULL;

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

GtkWidget *
totem_properties_view_new (const char *location)
{
    TotemPropertiesView *self;

    self = g_object_new (TOTEM_TYPE_PROPERTIES_VIEW, NULL);
    totem_properties_view_set_location (self, location);

    return GTK_WIDGET (self);
}

void
totem_properties_view_set_location (TotemPropertiesView *props,
				     const char *location)
{
	g_assert (TOTEM_IS_PROPERTIES_VIEW (props));

	if (location != NULL) {
		GError *error = NULL;

		g_free(props->priv->location);
		bacon_video_widget_close (props->priv->bvw);
		props->priv->location = g_strdup (location);
		bacon_video_widget_properties_update (props->priv->props,
				props->priv->bvw, TRUE);
		if (bacon_video_widget_open (props->priv->bvw, location, &error) == FALSE) {
			g_warning ("Couldn't open %s: %s", location, error->message);
			g_error_free (error);
			return;
		}
		if (bacon_video_widget_play (props->priv->bvw, &error) == FALSE) {
			g_warning ("Couldn't play %s: %s", location, error->message);
			g_error_free (error);
			bacon_video_widget_close (props->priv->bvw);
		}
	} else {
		bacon_video_widget_properties_update (props->priv->props,
				props->priv->bvw, TRUE);
	}
}

