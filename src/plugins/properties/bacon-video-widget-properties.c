/* bacon-video-widget-properties.c

   Copyright (C) 2002 Bastien Nocera

   SPDX-License-Identifier: GPL-3-or-later

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <math.h>
#include <string.h>

#include "bacon-video-widget-properties.h"

static void bacon_video_widget_properties_set_property (GObject      *object,
                                                        guint         prop_id,
                                                        const GValue *value,
                                                        GParamSpec   *pspec);
static void bacon_video_widget_properties_set_label    (GtkLabel *label, const char *text);

struct _BaconVideoWidgetProperties {
	HdyWindow parent;

	/* General */
	GtkLabel *title;
	GtkLabel *artist;
	GtkLabel *album;
	GtkLabel *year;
	GtkLabel *duration;
	GtkLabel *comment;
	GtkLabel *container;

	/* Video */
	GtkWidget *video_vbox;
	GtkWidget *video;

	GtkLabel *dimensions;
	GtkLabel *vcodec;
	GtkLabel *framerate;
	GtkLabel *video_bitrate;

	/* Audio */
	GtkWidget *audio;

	GtkLabel *acodec;
	GtkLabel *channels;
	GtkLabel *samplerate;
	GtkLabel *audio_bitrate;

	int time;
};

G_DEFINE_TYPE (BaconVideoWidgetProperties, bacon_video_widget_properties, HDY_TYPE_WINDOW)

enum {
	PROP_0,
	PROP_TITLE,
	PROP_ARITST,
	PROP_ALBUM,
	PROP_YEAR,
	PROP_DURATION,
	PROP_COMMENT,
	PROP_CONTAINER,
	PROP_DIMENSIONS,
	PROP_VIDEO_CODEC,
	PROP_VIDEO_FRAMERATE,
	PROP_VIDEO_BITRATE,
	PROP_AUDIO_CODEC,
	PROP_AUDIO_CHANNELS,
	PROP_AUDIO_SAMPLERATE,
	PROP_AUDIO_BITRATE,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
bacon_video_widget_properties_class_init (BaconVideoWidgetPropertiesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->set_property = bacon_video_widget_properties_set_property;

	/* General */
	properties[PROP_TITLE] = g_param_spec_string ("media-title",
	                                              "Title",
	                                              "",
	                                              NULL,
	                                              G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_ARITST] = g_param_spec_string ("artist",
	                                               "Artist",
	                                               "",
	                                               NULL,
	                                               G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_ALBUM] = g_param_spec_string ("album",
	                                              "Album",
	                                              "",
	                                              NULL,
	                                              G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_YEAR] = g_param_spec_string ("year",
	                                             "Year",
	                                             "",
	                                             NULL,
	                                             G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_DURATION] = g_param_spec_int ("duration",
	                                              "Duration",
	                                              "",
	                                              0,
	                                              G_MAXINT,
	                                              0,
	                                              G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_COMMENT] = g_param_spec_string ("comment",
	                                                "Comment",
	                                                "",
	                                                NULL,
	                                                G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_CONTAINER] = g_param_spec_string ("container",
	                                                  "Container",
	                                                  "",
	                                                  NULL,
	                                                  G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	/* Video */
	properties[PROP_DIMENSIONS] = g_param_spec_string ("dimensions",
	                                                   "Dimensions",
	                                                   "",
	                                                   NULL,
	                                                   G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_VIDEO_CODEC] = g_param_spec_string ("video-codec",
	                                                    "Video codec",
	                                                    "",
	                                                    NULL,
	                                                    G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_VIDEO_FRAMERATE] = g_param_spec_float ("framerate",
	                                                       "Video frame rate",
	                                                       "",
	                                                       0.f,
	                                                       G_MAXFLOAT,
	                                                       0.f,
	                                                       G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_VIDEO_BITRATE] = g_param_spec_string ("video-bitrate",
	                                                      "Video bit rate",
	                                                      "",
	                                                      NULL,
	                                                      G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	/* Audio */
	properties[PROP_AUDIO_CODEC] = g_param_spec_string ("audio-codec",
	                                                    "Audio bit rate",
	                                                    "",
	                                                    NULL,
	                                                    G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_AUDIO_CHANNELS] = g_param_spec_string ("channels",
	                                                       "Audio channels",
	                                                       "",
	                                                       NULL,
	                                                       G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_AUDIO_SAMPLERATE] = g_param_spec_string ("samplerate",
	                                                         "Audio sample rate",
	                                                         "",
	                                                         NULL,
	                                                         G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	properties[PROP_AUDIO_BITRATE] = g_param_spec_string ("audio-bitrate",
	                                                      "Audio bit rate",
	                                                      "",
	                                                      NULL,
	                                                      G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/properties/properties.ui");

	/* General */
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, title);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, artist);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, album);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, year);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, duration);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, comment);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, container);

	/* Video */
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, video_vbox);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, video);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, dimensions);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, vcodec);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, framerate);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, video_bitrate);

	/* Audio */
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, audio);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, acodec);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, channels);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, samplerate);
	gtk_widget_class_bind_template_child (widget_class, BaconVideoWidgetProperties, audio_bitrate);
}

static void
bacon_video_widget_properties_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
	BaconVideoWidgetProperties *props = BACON_VIDEO_WIDGET_PROPERTIES (object);

	switch (prop_id)
	{
		case PROP_TITLE:
			bacon_video_widget_properties_set_label (props->title, g_value_get_string (value));
			break;
		case PROP_ARITST:
			bacon_video_widget_properties_set_label (props->artist, g_value_get_string (value));
			break;
		case PROP_ALBUM:
			bacon_video_widget_properties_set_label (props->album, g_value_get_string (value));
			break;
		case PROP_YEAR:
			bacon_video_widget_properties_set_label (props->year, g_value_get_string (value));
			break;
		case PROP_DURATION:
			bacon_video_widget_properties_set_duration (props, g_value_get_int (value));
			break;
		case PROP_COMMENT:
			bacon_video_widget_properties_set_label (props->comment, g_value_get_string (value));
			break;
		case PROP_CONTAINER:
			bacon_video_widget_properties_set_label (props->container, g_value_get_string (value));
			break;
		case PROP_DIMENSIONS:
			bacon_video_widget_properties_set_label (props->dimensions, g_value_get_string (value));
			break;
		case PROP_VIDEO_CODEC:
			bacon_video_widget_properties_set_label (props->vcodec, g_value_get_string (value));
			break;
		case PROP_VIDEO_FRAMERATE:
			bacon_video_widget_properties_set_framerate (props, g_value_get_float (value));
			break;
		case PROP_VIDEO_BITRATE:
			bacon_video_widget_properties_set_label (props->video_bitrate, g_value_get_string (value));
			break;
		case PROP_AUDIO_CODEC:
			bacon_video_widget_properties_set_label (props->acodec, g_value_get_string (value));
			break;
		case PROP_AUDIO_CHANNELS:
			bacon_video_widget_properties_set_label (props->channels, g_value_get_string (value));
			break;
		case PROP_AUDIO_SAMPLERATE:
			bacon_video_widget_properties_set_label (props->samplerate, g_value_get_string (value));
			break;
		case PROP_AUDIO_BITRATE:
			bacon_video_widget_properties_set_label (props->audio_bitrate, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
bacon_video_widget_properties_set_label (GtkLabel   *label,
                                         const char *text)
{
	gtk_label_set_text (label, text);

	gtk_widget_set_visible (GTK_WIDGET (label), text != NULL && *text != '\0');
}

static void
bacon_video_widget_properties_init (BaconVideoWidgetProperties *props)
{
	gtk_widget_init_template (GTK_WIDGET (props));

	bacon_video_widget_properties_reset (props);
}

void
bacon_video_widget_properties_reset (BaconVideoWidgetProperties *props)
{
	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	gtk_widget_show (props->video_vbox);
	gtk_widget_set_sensitive (props->video, FALSE);
	gtk_widget_set_sensitive (props->audio, FALSE);

	/* Title */
	bacon_video_widget_properties_set_label (props->title, NULL);
	/* Artist */
	bacon_video_widget_properties_set_label (props->artist, NULL);
	/* Album */
	bacon_video_widget_properties_set_label (props->album, NULL);
	/* Year */
	bacon_video_widget_properties_set_label (props->year, NULL);
	/* Duration */
	bacon_video_widget_properties_set_duration (props, 0);
	/* Comment */
	bacon_video_widget_properties_set_label (props->comment, "");
	/* Container */
	bacon_video_widget_properties_set_label (props->container, NULL);

	/* Dimensions */
	bacon_video_widget_properties_set_label (props->dimensions, C_("Dimensions", "N/A"));
	/* Video Codec */
	bacon_video_widget_properties_set_label (props->vcodec, C_("Video codec", "N/A"));
	/* Video Bitrate */
	bacon_video_widget_properties_set_label (props->video_bitrate,
			C_("Video bit rate", "N/A"));
	/* Framerate */
	bacon_video_widget_properties_set_label (props->framerate,
			C_("Frame rate", "N/A"));

	/* Audio Bitrate */
	bacon_video_widget_properties_set_label (props->audio_bitrate,
			C_("Audio bit rate", "N/A"));
	/* Audio Codec */
	bacon_video_widget_properties_set_label (props->acodec, C_("Audio codec", "N/A"));
	/* Sample rate */
	bacon_video_widget_properties_set_label (props->samplerate, _("0 Hz"));
	/* Channels */
	bacon_video_widget_properties_set_label (props->channels, _("0 Channels"));
}

static char *
time_to_string_text (gint64 msecs)
{
	char *secs, *mins, *hours, *string;
	int sec, min, hour, _time;

	_time = (int) (msecs / 1000);
	sec = _time % 60;
	_time = _time - sec;
	min = (_time % (60*60)) / 60;
	_time = _time - (min * 60);
	hour = _time / (60*60);

	hours = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d hour", "%d hours", hour), hour);

	mins = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d minute",
					  "%d minutes", min), min);

	secs = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d second",
					  "%d seconds", sec), sec);

	if (hour > 0)
	{
		if (min > 0 && sec > 0)
		{
			/* 5 hours 2 minutes 12 seconds */
			string = g_strdup_printf (C_("hours minutes seconds", "%s %s %s"), hours, mins, secs);
		} else if (min > 0) {
			/* 5 hours 2 minutes */
			string = g_strdup_printf (C_("hours minutes", "%s %s"), hours, mins);
		} else {
			/* 5 hours */
			string = g_strdup_printf (C_("hours", "%s"), hours);
		}
	} else if (min > 0) {
		if (sec > 0)
		{
			/* 2 minutes 12 seconds */
			string = g_strdup_printf (C_("minutes seconds", "%s %s"), mins, secs);
		} else {
			/* 2 minutes */
			string = g_strdup_printf (C_("minutes", "%s"), mins);
		}
	} else if (sec > 0) {
		/* 10 seconds */
		string = g_strdup (secs);
	} else {
		/* 0 seconds */
		string = g_strdup (_("0 seconds"));
	}

	g_free (hours);
	g_free (mins);
	g_free (secs);

	return string;
}

void
bacon_video_widget_properties_set_duration (BaconVideoWidgetProperties *props,
					    int _time)
{
	char *string;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	if (_time == props->time)
		return;

	string = time_to_string_text (_time);
	bacon_video_widget_properties_set_label (props->duration, string);
	g_free (string);

	props->time = _time;
}

void
bacon_video_widget_properties_set_has_type (BaconVideoWidgetProperties *props,
					    gboolean                    has_video,
					    gboolean                    has_audio)
{
	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

	/* Video */
	gtk_widget_set_sensitive (props->video, has_video);
	gtk_widget_set_visible (props->video_vbox, has_video);

	/* Audio */
	gtk_widget_set_sensitive (props->audio, has_audio);
}

void
bacon_video_widget_properties_set_framerate (BaconVideoWidgetProperties *props,
					     float                       framerate)
{
	gchar *temp;

	g_return_if_fail (props != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));

/* https://en.wikipedia.org/wiki/24p#23.976p */
#define _24P_FPS (24000.0/1001.0)

	if (framerate > 1.0) {
		if (G_APPROX_VALUE (framerate, _24P_FPS, .000001)) {
			temp = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%0.3f frame per second", "%0.3f frames per second", (int) (ceilf (framerate))),
						framerate);
		} else {
			temp = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%0.2f frame per second", "%0.2f frames per second", (int) (ceilf (framerate))),
						framerate);
		}
	} else {
		temp = g_strdup (C_("Frame rate", "N/A"));
	}
	bacon_video_widget_properties_set_label (props->framerate, temp);
	g_free (temp);
}

GtkWidget*
bacon_video_widget_properties_new (GtkWindow *transient_for)
{
	return g_object_new (BACON_TYPE_VIDEO_WIDGET_PROPERTIES,
	                     "transient-for", transient_for,
	                     NULL);
}

