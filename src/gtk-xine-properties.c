/* gtk-xine-properties.c

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
#include "gtk-xine-properties.h"

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "debug.h"

struct GtkXinePropertiesPrivate
{
	GladeXML *xml;
	GtkWidget *vbox;
	gboolean properties_reset_state;
};

static GtkWidgetClass *parent_class = NULL;

static void gtk_xine_properties_class_init (GtkXinePropertiesClass *class);
static void gtk_xine_properties_init       (GtkXineProperties      *label);

static void init_treeview (GtkWidget *treeview, GtkXineProperties *playlist);
static gboolean gtk_xine_properties_unset_playing (GtkXineProperties *playlist);

GtkType
gtk_xine_properties_get_type (void)
{
	static GtkType gtk_xine_properties_type = 0;

	if (!gtk_xine_properties_type) {
		static const GTypeInfo gtk_xine_properties_info = {
			sizeof (GtkXinePropertiesClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_xine_properties_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GtkXineProperties),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gtk_xine_properties_init,
		};

		gtk_xine_properties_type = g_type_register_static
			(GTK_TYPE_DIALOG, "GtkXineProperties",
			 &gtk_xine_properties_info, (GTypeFlags)0);
	}

	return gtk_xine_properties_type;
}

static void
gtk_xine_properties_init (GtkXineProperties *playlist)
{
	playlist->priv = g_new0 (GtkXinePropertiesPrivate, 1);
	playlist->priv->xml = NULL;
	playlist->priv->vbox = NULL;
	playlist->priv->properties_reset_state = FALSE;
}

static void
gtk_xine_properties_finalize (GObject *object)
{
	GtkXineProperties *props = GTK_XINE_PROPERTIES (object);

	g_return_if_fail (object != NULL);

	g_object_unref (props->priv->xml);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static char
*time_to_string (int time)
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
gtk_xine_properties_set_label (GtkXineProperties *props,
			       const char *name, const char *text)
{
	GtkWidget *item;

	item = glade_xml_get_widget (props->priv->xml, name);
	gtk_label_set_text (GTK_LABEL (item), text);
}

static void
gtk_xine_properties_reset (GtkXineProperties *props)
{
	GtkWidget *item;

	item = glade_xml_get_widget (props->priv->xml, "video");
	gtk_widget_set_sensitive (item, FALSE);
	item = glade_xml_get_widget (props->priv->xml, "audio");
	gtk_widget_set_sensitive (item, FALSE);

	/* Title */
	gtk_xine_properties_set_label (props, "title", _("Unknown"));
	/* Artist */
	gtk_xine_properties_set_label (props, "artist", _("Unknown"));
	/* Year */
	gtk_xine_properties_set_label (props, "year", _("N/A"));
	/* Duration */
	gtk_xine_properties_set_label (props, "duration", _("0 second"));
	/* Dimensions */
	gtk_xine_properties_set_label (props, "dimensions", _("0 x 0"));
	/* Video Codec */
	gtk_xine_properties_set_label (props, "vcodec", _("N/A"));
	/* Framerate */
	gtk_xine_properties_set_label (props, "framerate",
			_("0 frames per second"));
	/* Bitrate */
	gtk_xine_properties_set_label (props, "bitrate", _("0 kbps"));
	/* Audio Codec */
	gtk_xine_properties_set_label (props, "acodec", _("N/A"));
}

static void
gtk_xine_properties_set_from_current (GtkXineProperties *props, GtkXine *gtx)
{
	GtkWidget *item;
	const char *text;
	char *string;
	int fps;
#if 0
	/* General */
	text = xine_get_meta_info (gtx->priv->stream, XINE_META_INFO_TITLE);
	gtk_xine_properties_set_label (gtx, "title",
			text ? text : _("Unknown"));

	text = xine_get_meta_info (gtx->priv->stream, XINE_META_INFO_ARTIST);
	gtk_xine_properties_set_label (gtx, "artist",
			text ? text : _("Unknown"));

	text = xine_get_meta_info (gtx->priv->stream, XINE_META_INFO_YEAR);
	gtk_xine_properties_set_label (gtx, "year",
			text ? text : _("N/A"));

	string = time_to_string (gtk_xine_get_stream_length (gtx) / 1000);
	gtk_xine_properties_set_label (gtx, "duration", string);
	g_free (string);

	/* Video */
	item = glade_xml_get_widget (gtx->priv->xml, "video");
	if (xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_HAS_VIDEO) == FALSE)
		gtk_widget_set_sensitive (item, FALSE);
	else
		gtk_widget_set_sensitive (item, TRUE);

	string = g_strdup_printf ("%d x %d",
			xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_VIDEO_WIDTH),
			xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_VIDEO_HEIGHT));
	gtk_xine_properties_set_label (gtx, "dimensions", string);
	g_free (string);

	text = xine_get_meta_info (gtx->priv->stream,
			XINE_META_INFO_VIDEOCODEC);
	gtk_xine_properties_set_label (gtx, "vcodec",
			text ? text : _("N/A"));

	if (xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_FRAME_DURATION) != 0)
	{
		fps = 90000 / xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_FRAME_DURATION);
	} else {
		fps = 0;
	}
	string = g_strdup_printf (_("%d frames per second"), fps);
	gtk_xine_properties_set_label (gtx, "framerate", string);
	g_free (string);

	/* Audio */
	item = glade_xml_get_widget (gtx->priv->xml, "audio");
	if (xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_HAS_AUDIO) == FALSE)
		gtk_widget_set_sensitive (item, FALSE);
	else
		gtk_widget_set_sensitive (item, TRUE);

	string = g_strdup_printf (_("%d kbps"),
			xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_AUDIO_BITRATE) / 1000);
	gtk_xine_properties_set_label (gtx, "bitrate", string);
	g_free (string);

	text = xine_get_meta_info (gtx->priv->stream,
			XINE_META_INFO_AUDIOCODEC);
	gtk_xine_properties_set_label (gtx, "acodec",
			text ? text : _("N/A"));
#endif
}

void
gtk_xine_properties_update (GtkXineProperties *props, GtkXine *gtx,
			    gboolean reset)
{
	g_return_if_fail (props != NULL);
	g_return_if_fail (GTK_IS_XINE_PROPERTIES (props));

	props->priv->properties_reset_state = reset;

	if (reset == TRUE)
	{
		gtk_xine_properties_reset (props);
	} else {
		g_return_if_fail (gtx != NULL);
		gtk_xine_properties_set_from_current (props, gtx);
	}
}

static void
hide_dialog (GtkWidget *widget, int trash, gpointer user_data)
{
	gtk_widget_hide (widget);
}


GtkWidget*
gtk_xine_properties_new (void)
{
	GtkXineProperties *props;
	GladeXML *xml;
	char *filename;

	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "properties.glade", NULL);

	xml = glade_xml_new (filename, "vbox1", NULL);
	g_free (filename);

	if (xml == NULL)
		return NULL;

	props = GTK_XINE_PROPERTIES (g_object_new
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

	gtk_xine_properties_update (props, NULL, FALSE);

	gtk_widget_show_all (GTK_DIALOG (props)->vbox);

	return GTK_WIDGET (props);
}

static void
gtk_xine_properties_class_init (GtkXinePropertiesClass *klass)
{
	parent_class = gtk_type_class (gtk_dialog_get_type ());

	G_OBJECT_CLASS (klass)->finalize = gtk_xine_properties_finalize;
}

