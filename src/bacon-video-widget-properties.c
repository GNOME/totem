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
#include <libgnomeui/gnome-href.h>
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

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

GtkType
bacon_video_widget_properties_get_type (void)
{
	static GtkType bacon_video_widget_properties_type = 0;

	if (!bacon_video_widget_properties_type) {
		static const GTypeInfo bacon_video_widget_properties_info = {
			sizeof (BaconVideoWidgetPropertiesClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) bacon_video_widget_properties_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (BaconVideoWidgetProperties),
			0 /* n_preallocs */,
			(GInstanceInitFunc) bacon_video_widget_properties_init,
		};

		bacon_video_widget_properties_type = g_type_register_static
			(GTK_TYPE_DIALOG, "BaconVideoWidgetProperties",
			 &bacon_video_widget_properties_info, (GTypeFlags)0);
	}

	return bacon_video_widget_properties_type;
}

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

char *
bacon_video_widget_properties_time_to_string (int time)
{
	char *secs, *mins, *hours, *string;
	int sec, min, hour;

	sec = time % 60;
	time = time - sec;
	min = (time % (60*60)) / 60;
	time = time - (min * 60);
	hour = time / (60*60);

	if (hour == 1)
		/* One hour */
		hours = g_strdup_printf (_("%d hour"), hour);
	else
		/* Multiple hours */
		hours = g_strdup_printf (_("%d hours"), hour);

	if (min == 1)
		/* One minute */
		mins = g_strdup_printf (_("%d minute"), min);
	else
		/* Multiple minutes */
		mins = g_strdup_printf (_("%d minutes"), min);

	if (sec == 1)
		/* One second */
		secs = g_strdup_printf (_("%d second"), sec);
	else
		/* Multiple seconds */
		secs = g_strdup_printf (_("%d seconds"), sec);

	if (hour > 0)
	{
		/* hour:minutes:seconds */
		string = g_strdup_printf (_("%s %s %s"), hours, mins, secs);
	} else if (min > 0) {
		/* minutes:seconds */
		string = g_strdup_printf (_("%s %s"), mins, secs);
	} else if (sec > 0) {
		/* seconds */
		string = g_strdup_printf (_("%s"), secs);
	} else {
		/* 0 seconds */
		string = g_strdup (_("0 seconds"));
	}

	g_free (hours);
	g_free (mins);
	g_free (secs);

	return string;
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

	item = glade_xml_get_widget (props->priv->xml, "video");
	gtk_widget_set_sensitive (item, FALSE);
	item = glade_xml_get_widget (props->priv->xml, "audio");
	gtk_widget_set_sensitive (item, FALSE);

	/* Title */
	bacon_video_widget_properties_set_label (props, "title", _("Unknown"));
	/* Artist */
	bacon_video_widget_properties_set_label (props, "artist", _("Unknown"));
	/* Year */
	bacon_video_widget_properties_set_label (props, "year", _("N/A"));
	/* Duration */
	bacon_video_widget_properties_set_label (props, "duration", _("0 second"));
	/* Dimensions */
	bacon_video_widget_properties_set_label (props, "dimensions", _("0 x 0"));
	/* Video Codec */
	bacon_video_widget_properties_set_label (props, "vcodec", _("N/A"));
	/* Framerate */
	bacon_video_widget_properties_set_label (props, "framerate",
			_("0 frames per second"));
	/* Bitrate */
	bacon_video_widget_properties_set_label (props, "bitrate", _("0 kbps"));
	/* Audio Codec */
	bacon_video_widget_properties_set_label (props, "acodec", _("N/A"));

	item = glade_xml_get_widget (props->priv->xml, "href1");
	gtk_widget_set_sensitive (item, FALSE);
	gnome_href_set_text (GNOME_HREF (item), _("No Link"));
}

static void
bacon_video_widget_properties_set_from_current (BaconVideoWidgetProperties *props, BaconVideoWidget *bvw, const char *name)
{
	GtkWidget *item;
	GValue value = { 0, };
	char *string;
	int x, y;

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
			BVW_INFO_YEAR, &value);
	bacon_video_widget_properties_set_label (props, "year",
			g_value_get_string (&value)
			? g_value_get_string (&value) : _("N/A"));
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_DURATION, &value);
	string = bacon_video_widget_properties_time_to_string
		(g_value_get_int (&value));
	bacon_video_widget_properties_set_label (props, "duration", string);
	g_free (string);
	g_value_unset (&value);

	/* Video */
	item = glade_xml_get_widget (props->priv->xml, "video");
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_VIDEO, &value);
	if (g_value_get_boolean (&value) == FALSE)
		gtk_widget_set_sensitive (item, FALSE);
	else
		gtk_widget_set_sensitive (item, TRUE);
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_DIMENSION_X, &value);
	x = g_value_get_int (&value);
	g_value_unset (&value);
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_DIMENSION_Y, &value);
	y = g_value_get_int (&value);
	g_value_unset (&value);
	string = g_strdup_printf ("%d x %d", x, y);
	bacon_video_widget_properties_set_label (props, "dimensions", string);
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
	bacon_video_widget_properties_set_label (props, "framerate", string);
	g_free (string);
	g_value_unset (&value);

	/* Audio */
	item = glade_xml_get_widget (props->priv->xml, "audio");
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_HAS_AUDIO, &value);
	if (g_value_get_boolean (&value) == FALSE)
		gtk_widget_set_sensitive (item, FALSE);
	else
		gtk_widget_set_sensitive (item, TRUE);
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_BITRATE, &value);
	string = g_strdup_printf (_("%d kbps"), g_value_get_int (&value));
	bacon_video_widget_properties_set_label (props, "bitrate", string);
	g_free (string);
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
			BVW_INFO_AUDIO_CODEC, &value);
	bacon_video_widget_properties_set_label (props, "acodec",
			g_value_get_string (&value)
			? g_value_get_string (&value) : _("N/A"));
	g_value_unset (&value);

	item = glade_xml_get_widget (props->priv->xml, "href1");
	gtk_widget_set_sensitive (item, TRUE);
	string = g_strdup_printf ("http://us.imdb.com/Tsearch?title=%s", name);
	gnome_href_set_url (GNOME_HREF (item), string);
	g_free (string);

	string = g_markup_escape_text (name, strlen (name));
	gnome_href_set_text (GNOME_HREF (item), string);
	g_free (string);
}

void
bacon_video_widget_properties_update (BaconVideoWidgetProperties *props,
		BaconVideoWidget *bvw,
		const char *name,
		gboolean reset)
{
	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	if (reset == TRUE)
	{
		bacon_video_widget_properties_reset (props);
	} else {
		g_return_if_fail (bvw != NULL);
		g_return_if_fail (name != NULL);
		bacon_video_widget_properties_set_from_current (props, bvw, name);
	}
}

static void
hide_dialog (GtkWidget *widget, int trash, gpointer user_data)
{
	gtk_widget_hide (widget);
}


GtkWidget*
bacon_video_widget_properties_new (void)
{
	BaconVideoWidgetProperties *props;
	GladeXML *xml;
	char *filename;

	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "properties.glade", NULL);

	xml = glade_xml_new (filename, "vbox1", NULL);
	g_free (filename);

	if (xml == NULL)
		return NULL;

	props = BACON_VIDEO_WIDGET_PROPERTIES (g_object_new
			(GTK_TYPE_XINE_PROPERTIES, NULL));
	props->priv->xml = xml;
	props->priv->vbox = glade_xml_get_widget (props->priv->xml, "vbox1");

	gtk_window_set_title (GTK_WINDOW (props), _("Properties"));
	gtk_dialog_add_buttons (GTK_DIALOG (props),
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (props)->vbox),
			props->priv->vbox,
			TRUE,       /* expand */
			TRUE,       /* fill */
			0);         /* padding */

	g_signal_connect (G_OBJECT (props),
			"response", G_CALLBACK (hide_dialog), NULL);
	g_signal_connect (G_OBJECT (props), "delete-event",
			G_CALLBACK (hide_dialog), NULL);

	bacon_video_widget_properties_update (props, NULL, NULL, TRUE);

	gtk_widget_show (GTK_DIALOG (props)->vbox);

	return GTK_WIDGET (props);
}

static void
bacon_video_widget_properties_class_init (BaconVideoWidgetPropertiesClass *klass)
{
	parent_class = gtk_type_class (gtk_dialog_get_type ());

	G_OBJECT_CLASS (klass)->finalize = bacon_video_widget_properties_finalize;
}

