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

/* used in bacon_video_widget_properties_update() */
#define UPDATE_FROM_STRING(type, name) \
	do { \
		const char *temp; \
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw), \
						 type, &value); \
		if ((temp = g_value_get_string (&value)) != NULL) { \
			bacon_video_widget_properties_set_label (props, name, \
								 temp); \
		} \
		g_value_unset (&value); \
	} while (0)

#define UPDATE_FROM_INT(type, name, format, empty) \
	do { \
		char *temp; \
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw), \
						 type, &value); \
		if (g_value_get_int (&value) != 0) \
			temp = g_strdup_printf (gettext (format), \
					g_value_get_int (&value)); \
		else \
			temp = g_strdup (empty); \
		bacon_video_widget_properties_set_label (props, name, temp); \
		g_free (temp); \
		g_value_unset (&value); \
	} while (0)

#define UPDATE_FROM_INT2(type1, type2, name, format) \
	do { \
		int x, y; \
		char *temp; \
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw), \
						 type1, &value); \
		x = g_value_get_int (&value); \
		g_value_unset (&value); \
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw), \
						 type2, &value); \
		y = g_value_get_int (&value); \
		g_value_unset (&value); \
		temp = g_strdup_printf (gettext (format), x, y); \
		bacon_video_widget_properties_set_label (props, name, temp); \
		g_free (temp); \
	} while (0)

struct BaconVideoWidgetPropertiesPrivate
{
	GladeXML *xml;
	int time;
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
}

static void
bacon_video_widget_properties_finalize (GObject *object)
{
	BaconVideoWidgetProperties *props = BACON_VIDEO_WIDGET_PROPERTIES (object);

	g_return_if_fail (object != NULL);

	g_object_unref (props->priv->xml);
	g_free (props->priv);

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

void
bacon_video_widget_properties_reset (BaconVideoWidgetProperties *props)
{
	GtkWidget *item;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

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
	bacon_video_widget_properties_from_time (props, 0);

	/* Dimensions */
	bacon_video_widget_properties_set_label (props, "dimensions", _("N/A"));
	/* Video Codec */
	bacon_video_widget_properties_set_label (props, "vcodec", _("N/A"));
	/* Video Bitrate */
	bacon_video_widget_properties_set_label (props, "video_bitrate",
			_("N/A"));
	/* Framerate */
	bacon_video_widget_properties_set_label (props, "framerate",
			_("N/A"));
	/* Audio Bitrate */
	bacon_video_widget_properties_set_label (props, "audio_bitrate",
			_("N/A"));
	/* Audio Codec */
	bacon_video_widget_properties_set_label (props, "acodec", _("N/A"));
	/* Sample rate */
	bacon_video_widget_properties_set_label (props, "samplerate", _("0 Hz"));
	/* Channels */
	bacon_video_widget_properties_set_label (props, "channels", _("0 Channels"));
}

void
bacon_video_widget_properties_from_time (BaconVideoWidgetProperties *props,
					 int time)
{
	char *string;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	if (time == props->priv->time)
		return;

	string = totem_time_to_string_text (time);
	bacon_video_widget_properties_set_label (props, "duration", string);
	g_free (string);

	props->priv->time = time;
}

void
bacon_video_widget_properties_update (BaconVideoWidgetProperties *props,
				      BaconVideoWidget *bvw)
{
	GtkWidget *item;
	GValue value = { 0, };
	gboolean has_type;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));
	g_return_if_fail (bvw != NULL);

	/* General */
	UPDATE_FROM_STRING (BVW_INFO_TITLE, "title");
	UPDATE_FROM_STRING (BVW_INFO_ARTIST, "artist");
	UPDATE_FROM_STRING (BVW_INFO_ALBUM, "album");
	UPDATE_FROM_STRING (BVW_INFO_YEAR, "year");

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_DURATION, &value);
	bacon_video_widget_properties_from_time (props,
			g_value_get_int (&value) * 1000);
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
		UPDATE_FROM_INT2 (BVW_INFO_DIMENSION_X, BVW_INFO_DIMENSION_Y,
				  "dimensions", N_("%d x %d"));
		UPDATE_FROM_STRING (BVW_INFO_VIDEO_CODEC, "vcodec");
		UPDATE_FROM_INT (BVW_INFO_FPS, "framerate",
				 N_("%d frames per second"), _("N/A"));
		UPDATE_FROM_INT (BVW_INFO_VIDEO_BITRATE, "video_bitrate",
				 N_("%d kbps"), _("N/A"));
		gtk_widget_show (item);
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
		UPDATE_FROM_INT (BVW_INFO_AUDIO_BITRATE, "audio_bitrate",
				 N_("%d kbps"), _("N/A"));
		UPDATE_FROM_STRING (BVW_INFO_AUDIO_CODEC, "acodec");
		UPDATE_FROM_INT (BVW_INFO_AUDIO_SAMPLE_RATE, "samplerate",
				N_("%d Hz"), _("N/A"));
		UPDATE_FROM_STRING (BVW_INFO_AUDIO_CHANNELS, "channels");
	}

#undef UPDATE_FROM_STRING
#undef UPDATE_FROM_INT
#undef UPDATE_FROM_INT2
}

void
bacon_video_widget_properties_from_metadata (BaconVideoWidgetProperties *props,
					     const char *title,
					     const char *artist,
					     const char *album)
{
	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));
	g_return_if_fail (title != NULL);
	g_return_if_fail (artist != NULL);
	g_return_if_fail (album != NULL);

	bacon_video_widget_properties_set_label (props, "title", title);
	bacon_video_widget_properties_set_label (props, "artist", artist);
	bacon_video_widget_properties_set_label (props, "album", album);
}

GtkWidget*
bacon_video_widget_properties_new (void)
{
	BaconVideoWidgetProperties *props;
	GladeXML *xml;
	GtkWidget *vbox;
	GtkSizeGroup *group;
	const char *labels[] = { "title_label", "artist_label", "album_label",
			"year_label", "duration_label", "dimensions_label", "vcodec_label",
			"framerate_label", "vbitrate_label", "abitrate_label",
			"acodec_label", "samplerate_label", "channels_label" };
	guint i;

	xml = totem_interface_load_with_root ("properties.glade",
			"vbox1", _("Properties dialog"), TRUE, NULL);

	if (xml == NULL)
		return NULL;

	props = BACON_VIDEO_WIDGET_PROPERTIES (g_object_new
			(BACON_TYPE_VIDEO_WIDGET_PROPERTIES, NULL));

	props->priv->xml = xml;
	vbox = glade_xml_get_widget (props->priv->xml, "vbox1");
	gtk_box_pack_start (GTK_BOX (props), vbox, FALSE, FALSE, 0);

	bacon_video_widget_properties_reset (props);

	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	for (i = 0; i < G_N_ELEMENTS (labels); i++)
		gtk_size_group_add_widget (group, glade_xml_get_widget (xml,
				labels[i]));

	g_object_unref (group);

	gtk_widget_show_all (GTK_WIDGET (props));

	return GTK_WIDGET (props);
}

static void
bacon_video_widget_properties_class_init (BaconVideoWidgetPropertiesClass *klass)
{
	parent_class = gtk_type_class (gtk_vbox_get_type ());

	G_OBJECT_CLASS (klass)->finalize = bacon_video_widget_properties_finalize;
}

