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
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
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
	guint timeout_id, try;
};

static GObjectClass *parent_class = NULL;
static void totem_properties_view_init (TotemPropertiesView *self);
static void totem_properties_view_class_init (TotemPropertiesViewClass *class);
static void totem_properties_view_finalize (GObject *object);

G_DEFINE_TYPE (TotemPropertiesView, totem_properties_view, GTK_TYPE_TABLE)

void
totem_properties_view_register_type (GTypeModule *module)
{
	totem_properties_view_get_type ();
}

static void
totem_properties_view_class_init (TotemPropertiesViewClass *class)
{
	parent_class = g_type_class_peek_parent (class);
	G_OBJECT_CLASS (class)->finalize = totem_properties_view_finalize;
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, TotemPropertiesView *props)
{
	bacon_video_widget_properties_update
		(props->priv->props, props->priv->bvw, FALSE);
}

static gboolean
on_timeout_event (TotemPropertiesView *props)
{
	/* FIXME: hack for the GStreamer backend which signals metadata
	 * in small chunks instead of all-at-once. */
	if (props->priv->try++ >= 5) {
		bacon_video_widget_close (props->priv->bvw);
		g_free (props->priv->location);
		props->priv->location = NULL;
		props->priv->timeout_id = 0;

		return FALSE;
	}

	return TRUE;
}

static void
totem_properties_view_init (TotemPropertiesView *props)
{
	GError *err = NULL;

	props->priv = g_new0 (TotemPropertiesViewPriv, 1);

	props->priv->bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new
			(-1, -1, BVW_USE_TYPE_METADATA, &err));

	if (props->priv->bvw != NULL)
	{
		/* Reference it, so that it's not floating */
		g_object_ref (props->priv->bvw);

		//FIXME

		g_signal_connect (G_OBJECT (props->priv->bvw),
				"got-metadata",
				G_CALLBACK (on_got_metadata_event),
				props);
	} else {
		g_error ("Error: %s", err ? err->message : "bla");
	}

	props->priv->vbox = bacon_video_widget_properties_new ();
	gtk_table_resize (GTK_TABLE (props), 1, 1);
	gtk_container_add (GTK_CONTAINER (props), props->priv->vbox);
	gtk_widget_show (GTK_WIDGET (props));

	props->priv->props = BACON_VIDEO_WIDGET_PROPERTIES (props->priv->vbox);
}

static void
totem_properties_view_finalize (GObject *object)
{
	TotemPropertiesView *props;

	props = TOTEM_PROPERTIES_VIEW (object);

	if (props->priv != NULL)
	{
		g_object_unref (G_OBJECT (props->priv->bvw));
		props->priv->bvw = NULL;
		g_free (props->priv->location);
		props->priv->location = NULL;
		if (props->priv->timeout_id != 0) {
	                g_source_remove (props->priv->timeout_id);
        	        props->priv->timeout_id = 0;
		}
		g_free (props->priv);
	}
	props->priv = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
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
			g_free (props->priv->location);
			props->priv->location = NULL;
			g_warning ("Couldn't open %s: %s", location, error->message);
			g_error_free (error);
			return;
		}
		/* Already closed? */
		if (props->priv->location == NULL)
			return;

		if (bacon_video_widget_play (props->priv->bvw, &error) == FALSE) {
			g_free (props->priv->location);
			props->priv->location = NULL;
			g_warning ("Couldn't play %s: %s", location, error->message);
			g_error_free (error);
			bacon_video_widget_close (props->priv->bvw);
		}
		props->priv->timeout_id =
			g_timeout_add (200, (GSourceFunc) on_timeout_event,
				       props);
	} else {
		bacon_video_widget_properties_update (props->priv->props,
				props->priv->bvw, TRUE);
	}
}

