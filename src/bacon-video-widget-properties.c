/* bacon-video-widget-properties.c

   Copyright (C) 2002 Bastien Nocera

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"
#include "bacon-video-widget-properties.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <string.h>
#include "video-utils.h"
#include "totem-interface.h"

#include "debug.h"

struct BaconVideoWidgetPropertiesPrivate
{
	GladeXML *xml;
	GtkWidget *vbox;
};

static GtkWidgetClass *parent_class = NULL;

static void bacon_video_widget_properties_class_init
	(BaconVideoWidgetPropertiesClass *class);
static void bacon_video_widget_properties_init
	(BaconVideoWidgetProperties *props);

G_DEFINE_TYPE(BaconVideoWidgetProperties, bacon_video_widget_properties, GTK_TYPE_VBOX)

static void
bacon_video_widget_properties_init (BaconVideoWidgetProperties *props)
{
	props->priv = g_new0 (BaconVideoWidgetPropertiesPrivate, 1);
	props->priv->xml = NULL;
	props->priv->vbox = NULL;
}

static void
bacon_video_widget_properties_finalize (GObject *object)
{
	BaconVideoWidgetProperties *props = BACON_VIDEO_WIDGET_PROPERTIES (object);

	g_return_if_fail (object != NULL);

	g_object_unref (props->priv->xml);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
bacon_video_widget_properties_set_label (BaconVideoWidgetProperties *props,
			       const char *name, const char *text)
{
	GtkWidget *item;

	item = glade_xml_get_widget (props->priv->xml, name);
	gtk_label_set_text (GTK_LABEL (item), text);
}

static void
bacon_video_widget_properties_reset (BaconVideoWidgetProperties *props)
{
	GtkWidget *item;

	item = glade_xml_get_widget (props->priv->xml, "video_vbox");
	gtk_widget_show (item);
	item = glade_xml_get_widget (props->priv->xml, "video");
	gtk_widget_set_sensitive (item, FALSE);
	item = glade_xml_get_widget (props->priv->xml, "audio");
	gtk_widget_set_sensitive (item, FALSE);

	/* Title */
	bacon_video_widget_properties_set_label (props, "title", _("Unknown"));
	/* Artist */
	bacon_video_widget_properties_set_label (props, "artist", _("Unknown"));
	/* Album */
	bacon_video_widget_properties_set_label (props, "album", _("Unknown"));
	/* Year */
	bacon_video_widget_properties_set_label (props, "year", _("Unknown"));
	/* Duration */
	bacon_video_widget_properties_set_label (props, "duration", _("0 second"));
	/* Dimensions */
	bacon_video_widget_properties_set_label (props, "dimensions", _("0 x 0"));
	/* Video Codec */
	bacon_video_widget_properties_set_label (props, "vcodec", _("N/A"));
	/* Video Bitrate */
	bacon_video_widget_properties_set_label (props, "video_bitrate",
			_("0 kbps"));
	/* Framerate */
	bacon_video_widget_properties_set_label (props, "framerate",
			_("0 frames per second"));
	/* Audio Bitrate */
	bacon_video_widget_properties_set_label (props, "audio_bitrate",
			_("0 kbps"));
	/* Audio Codec */
	bacon_video_widget_properties_set_label (props, "acodec", _("N/A"));
}

static void
bacon_video_widget_properties_set_from_current
(BaconVideoWidgetProperties *props, BaconVideoWidget *bvw)
{
	GtkWidget *item;
	GValue value = { 0, };
	char *string;
	int x, y;
	gboolean has_type;

	/* General */
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_TITLE, &value);
	bacon_video_widget_properties_set_label (props, "title",
			g_value_get_string (&value)
			? g_value_get_string (&value)
			: _("Unknown"));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_ARTIST, &value);
	bacon_video_widget_properties_set_label (props, "artist",
			g_value_get_string (&value)
			? g_value_get_string (&value) : _("Unknown"));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_ALBUM, &value);
	bacon_video_widget_properties_set_label (props, "album",
			g_value_get_string (&value)
			? g_value_get_string (&value) : _("Unknown"));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_YEAR, &value);
	bacon_video_widget_properties_set_label (props, "year",
			g_value_get_string (&value)
			? g_value_get_string (&value) : _("Unknown"));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_DURATION, &value);
	string = totem_time_to_string_text
		(g_value_get_int (&value) * 1000);
	bacon_video_widget_properties_set_label (props, "duration", string);
	g_free (string);
	g_value_unset (&value);

	/* Video */
	item = glade_xml_get_widget (props->priv->xml, "video");
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_VIDEO, &value);
	has_type = g_value_get_boolean (&value);
	gtk_widget_set_sensitive (item, has_type);
	g_value_unset (&value);

	item = glade_xml_get_widget (props->priv->xml, "video_vbox");

	if (has_type != FALSE)
	{
		gtk_widget_show (item);
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_DIMENSION_X, &value);
		x = g_value_get_int (&value);
		g_value_unset (&value);
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_DIMENSION_Y, &value);
		y = g_value_get_int (&value);
		g_value_unset (&value);
		string = g_strdup_printf ("%d x %d", x, y);
		bacon_video_widget_properties_set_label
			(props, "dimensions", string);
		g_free (string);

		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_VIDEO_CODEC, &value);
		bacon_video_widget_properties_set_label (props, "vcodec",
				g_value_get_string (&value)
				? g_value_get_string (&value) : _("N/A"));
		g_value_unset (&value);

		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_FPS, &value);
		string = g_strdup_printf (_("%d frames per second"),
				g_value_get_int (&value));
		g_value_unset (&value);
		bacon_video_widget_properties_set_label
			(props, "framerate", string);
		g_free (string);

		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_VIDEO_BITRATE, &value);
		string = g_strdup_printf (_("%d kbps"),
				g_value_get_int (&value));
		g_value_unset (&value);
		bacon_video_widget_properties_set_label
			(props, "video_bitrate", string);
		g_free (string);

	} else {
		gtk_widget_hide (item);
	}

	/* Audio */
	item = glade_xml_get_widget (props->priv->xml, "audio");
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_AUDIO, &value);
	has_type = g_value_get_boolean (&value);
	gtk_widget_set_sensitive (item, has_type);
	g_value_unset (&value);

	if (has_type != FALSE)
	{
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_AUDIO_BITRATE, &value);
		string = g_strdup_printf (_("%d kbps"),
				g_value_get_int (&value));
		g_value_unset (&value);
		bacon_video_widget_properties_set_label
			(props, "audio_bitrate", string);
		g_free (string);

		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
				BVW_INFO_AUDIO_CODEC, &value);
		bacon_video_widget_properties_set_label (props, "acodec",
				g_value_get_string (&value)
				? g_value_get_string (&value) : _("N/A"));
		g_value_unset (&value);
	}
}

void
bacon_video_widget_properties_update (BaconVideoWidgetProperties *props,
		BaconVideoWidget *bvw,
		gboolean reset)
{
	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	if (reset != FALSE)
	{
		bacon_video_widget_properties_reset (props);
	} else {
		g_return_if_fail (bvw != NULL);
		bacon_video_widget_properties_set_from_current (props, bvw);
	}
}

GtkWidget*
bacon_video_widget_properties_new (void)
{
	BaconVideoWidgetProperties *props;
	GladeXML *xml;
	GtkWidget *vbox;

	xml = totem_interface_load_with_root ("properties.glade",
			"vbox1", _("Properties dialog"), TRUE, NULL);

	if (xml == NULL)
		return NULL;

	props = BACON_VIDEO_WIDGET_PROPERTIES (g_object_new
			(BACON_TYPE_VIDEO_WIDGET_PROPERTIES, NULL));

	props->priv->xml = xml;
	vbox = glade_xml_get_widget (props->priv->xml, "vbox1");
	gtk_box_pack_start (GTK_BOX (props), vbox, TRUE, TRUE, 0);

	bacon_video_widget_properties_update (props, NULL, TRUE);

	gtk_widget_show_all (GTK_WIDGET (props));

	return GTK_WIDGET (props);
}

static void
bacon_video_widget_properties_class_init (BaconVideoWidgetPropertiesClass *klass)
{
	parent_class = gtk_type_class (gtk_vbox_get_type ());

	G_OBJECT_CLASS (klass)->finalize = bacon_video_widget_properties_finalize;
}

