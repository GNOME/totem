/*
 * Copyright (C) 2001,2002,2003,2004,2005 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#ifndef HAVE_BACON_VIDEO_WIDGET_H
#define HAVE_BACON_VIDEO_WIDGET_H

#include <gtk/gtkbox.h>
#include <popt.h>

/* for optical disc enumeration type */
#include "totem-disc.h"

G_BEGIN_DECLS

#define BACON_VIDEO_WIDGET(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), bacon_video_widget_get_type (), BaconVideoWidget))
#define BACON_VIDEO_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), bacon_video_widget_get_type (), BaconVideoWidgetClass))
#define BACON_IS_VIDEO_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_TYPE (obj, bacon_video_widget_get_type ()))
#define BACON_IS_VIDEO_WIDGET_CLASS(klass)   (G_CHECK_INSTANCE_GET_CLASS ((klass), bacon_video_widget_get_type ()))
#define BVW_ERROR bacon_video_widget_error_quark ()

typedef struct BaconVideoWidgetPrivate BaconVideoWidgetPrivate;

typedef struct {
	GtkBox parent;
	BaconVideoWidgetPrivate *priv;
} BaconVideoWidget;

typedef struct {
	GtkBoxClass parent_class;

	void (*error) (GtkWidget *bvw, const char *message,
                       gboolean playback_stopped, gboolean fatal);
	void (*eos) (GtkWidget *bvw);
	void (*got_metadata) (GtkWidget *bvw);
	void (*got_redirect) (GtkWidget *bvw, const char *mrl);
	void (*title_change) (GtkWidget *bvw, const char *title);
	void (*channels_change) (GtkWidget *bvw);
	void (*tick) (GtkWidget *bvw, gint64 current_time, gint64 stream_length,
			float current_position, gboolean seekable);
	void (*buffering) (GtkWidget *bvw, guint progress);
	void (*speed_warning) (GtkWidget *bvw);
} BaconVideoWidgetClass;

typedef enum {
	/* Plugins */
	BVW_ERROR_AUDIO_PLUGIN,
	BVW_ERROR_NO_PLUGIN_FOR_FILE,
	BVW_ERROR_VIDEO_PLUGIN,
	BVW_ERROR_AUDIO_BUSY,
	/* File */
	BVW_ERROR_BROKEN_FILE,
	BVW_ERROR_FILE_GENERIC,
	BVW_ERROR_FILE_PERMISSION,
	BVW_ERROR_FILE_ENCRYPTED,
	BVW_ERROR_FILE_NOT_FOUND,
	/* Devices */
	BVW_ERROR_DVD_ENCRYPTED,
	BVW_ERROR_INVALID_DEVICE,
	/* Network */
	BVW_ERROR_UNKNOWN_HOST,
	BVW_ERROR_NETWORK_UNREACHABLE,
	BVW_ERROR_CONNECTION_REFUSED,
	/* Generic */
	BVW_ERROR_UNVALID_LOCATION,
	BVW_ERROR_GENERIC,
	BVW_ERROR_CODEC_NOT_HANDLED,
	BVW_ERROR_AUDIO_ONLY,
	BVW_ERROR_CANNOT_CAPTURE,
	BVW_ERROR_READ_ERROR,
	BVW_ERROR_PLUGIN_LOAD,
	BVW_ERROR_STILL_IMAGE,
	BVW_ERROR_EMPTY_FILE
} BvwError;

GQuark bacon_video_widget_error_quark		 (void) G_GNUC_CONST;
GType bacon_video_widget_get_type                (void);
struct poptOption *bacon_video_widget_get_popt_table    (void);
/* This can be used if the app does not use popt */
void bacon_video_widget_init_backend		 (int *argc, char ***argv);

typedef enum {
	BVW_USE_TYPE_VIDEO,
	BVW_USE_TYPE_AUDIO,
	BVW_USE_TYPE_CAPTURE,
	BVW_USE_TYPE_METADATA
} BvwUseType;

GtkWidget *bacon_video_widget_new		 (int width, int height,
						  BvwUseType type,
						  GError **error);

char *bacon_video_widget_get_backend_name (BaconVideoWidget *bvw);

/* Actions */
#define bacon_video_widget_open(bvw, mrl, error) bacon_video_widget_open_with_subtitle(bvw, mrl, NULL, error)
gboolean bacon_video_widget_open_with_subtitle	 (BaconVideoWidget *bvw,
						  const char *mrl,
						  const char *subtitle_uri,
						  GError **error);
gboolean bacon_video_widget_play                 (BaconVideoWidget *bvw,
						  GError **error);
void bacon_video_widget_pause			 (BaconVideoWidget *bvw);
gboolean bacon_video_widget_is_playing           (BaconVideoWidget *bvw);

/* Seeking and length */
gboolean bacon_video_widget_is_seekable          (BaconVideoWidget *bvw);
gboolean bacon_video_widget_seek		 (BaconVideoWidget *bvw,
						  float position,
						  GError **error);
gboolean bacon_video_widget_seek_time		 (BaconVideoWidget *bvw,
						  gint64 time,
						  GError **error);
gboolean bacon_video_widget_can_direct_seek	 (BaconVideoWidget *bvw);
float bacon_video_widget_get_position            (BaconVideoWidget *bvw);
gint64 bacon_video_widget_get_current_time       (BaconVideoWidget *bvw);
gint64 bacon_video_widget_get_stream_length      (BaconVideoWidget *bvw);

void bacon_video_widget_stop                     (BaconVideoWidget *bvw);
void bacon_video_widget_close                    (BaconVideoWidget *bvw);

/* Audio volume */
gboolean bacon_video_widget_can_set_volume       (BaconVideoWidget *bvw);
void bacon_video_widget_set_volume               (BaconVideoWidget *bvw,
						  int volume);
int bacon_video_widget_get_volume                (BaconVideoWidget *bvw);

/* Properties */
void bacon_video_widget_set_logo		 (BaconVideoWidget *bvw,
						  char *filename);
void  bacon_video_widget_set_logo_mode		 (BaconVideoWidget *bvw,
						  gboolean logo_mode);
gboolean bacon_video_widget_get_logo_mode	 (BaconVideoWidget *bvw);

void bacon_video_widget_set_proprietary_plugins_path
						 (BaconVideoWidget *bvw,
				                  const char *path);

void bacon_video_widget_set_fullscreen		 (BaconVideoWidget *bvw,
						  gboolean fullscreen);

void bacon_video_widget_set_show_cursor          (BaconVideoWidget *bvw,
						  gboolean use_cursor);
gboolean bacon_video_widget_get_show_cursor      (BaconVideoWidget *bvw);

gboolean bacon_video_widget_get_auto_resize	 (BaconVideoWidget *bvw);
void bacon_video_widget_set_auto_resize		 (BaconVideoWidget *bvw,
						  gboolean auto_resize);

void bacon_video_widget_set_connection_speed     (BaconVideoWidget *bvw,
						  int speed);
int bacon_video_widget_get_connection_speed      (BaconVideoWidget *bvw);

void bacon_video_widget_set_media_device         (BaconVideoWidget *bvw,
						  const char *path);
gboolean bacon_video_widget_can_play             (BaconVideoWidget *bvw,
						  MediaType type);
gchar **bacon_video_widget_get_mrls		 (BaconVideoWidget *bvw,
						  MediaType type);
void bacon_video_widget_set_subtitle_font	 (BaconVideoWidget *bvw,
						  const char *font);

/* Video devices */
void bacon_video_widget_set_video_device	 (BaconVideoWidget *bvw,
						  const char *path);

/* Metadata */
typedef enum {
	BVW_INFO_TITLE,
	BVW_INFO_ARTIST,
	BVW_INFO_YEAR,
	BVW_INFO_ALBUM,
	BVW_INFO_DURATION,
	BVW_INFO_TRACK_NUMBER,
	/* Video */
	BVW_INFO_HAS_VIDEO,
	BVW_INFO_DIMENSION_X,
	BVW_INFO_DIMENSION_Y,
	BVW_INFO_VIDEO_BITRATE,
	BVW_INFO_VIDEO_CODEC,
	BVW_INFO_FPS,
	/* Audio */
	BVW_INFO_HAS_AUDIO,
	BVW_INFO_AUDIO_BITRATE,
	BVW_INFO_AUDIO_CODEC,
} BaconVideoWidgetMetadataType;

void bacon_video_widget_get_metadata		 (BaconVideoWidget *bvw,
						  BaconVideoWidgetMetadataType
						  type,
						  GValue *value);

/* Visualisation functions */
typedef enum {
	VISUAL_SMALL,
	VISUAL_NORMAL,
	VISUAL_LARGE,
	VISUAL_EXTRA_LARGE
} VisualsQuality;

gboolean bacon_video_widget_set_show_visuals	  (BaconVideoWidget *bvw,
						   gboolean show_visuals);
GList *bacon_video_widget_get_visuals_list	  (BaconVideoWidget *bvw);
gboolean bacon_video_widget_set_visuals		  (BaconVideoWidget *bvw,
						   const char *name);
void bacon_video_widget_set_visuals_quality	  (BaconVideoWidget *bvw,
						   VisualsQuality quality);

/* Picture settings */
typedef enum {
	BVW_VIDEO_BRIGHTNESS,
	BVW_VIDEO_CONTRAST,
	BVW_VIDEO_SATURATION,
	BVW_VIDEO_HUE
} BaconVideoWidgetVideoProperty;

typedef enum {
	BVW_RATIO_AUTO,
	BVW_RATIO_SQUARE,
	BVW_RATIO_FOURBYTHREE,
	BVW_RATIO_ANAMORPHIC,
	BVW_RATIO_DVB
} BaconVideoWidgetAspectRatio;

void bacon_video_widget_set_deinterlacing        (BaconVideoWidget *bvw,
						  gboolean deinterlace);
gboolean bacon_video_widget_get_deinterlacing    (BaconVideoWidget *bvw);

void bacon_video_widget_set_aspect_ratio         (BaconVideoWidget *bvw,
						  BaconVideoWidgetAspectRatio
						  ratio);
BaconVideoWidgetAspectRatio bacon_video_widget_get_aspect_ratio
						 (BaconVideoWidget *bvw);

void bacon_video_widget_set_scale_ratio          (BaconVideoWidget *bvw,
						  float ratio);

gboolean bacon_video_widget_can_set_zoom	 (BaconVideoWidget *bvw);
void bacon_video_widget_set_zoom		 (BaconVideoWidget *bvw,
						  int zoom);
int bacon_video_widget_get_zoom			 (BaconVideoWidget *bvw);

int bacon_video_widget_get_video_property        (BaconVideoWidget *bvw,
						  BaconVideoWidgetVideoProperty
						  type);
void bacon_video_widget_set_video_property       (BaconVideoWidget *bvw,
						  BaconVideoWidgetVideoProperty
						  type,
						  int value);

/* DVD functions */
typedef enum {
	BVW_DVD_ROOT_MENU,
	BVW_DVD_TITLE_MENU,
	BVW_DVD_SUBPICTURE_MENU,
	BVW_DVD_AUDIO_MENU,
	BVW_DVD_ANGLE_MENU,
	BVW_DVD_CHAPTER_MENU,
	BVW_DVD_NEXT_CHAPTER,
	BVW_DVD_PREV_CHAPTER,
	BVW_DVD_NEXT_TITLE,
	BVW_DVD_PREV_TITLE,
	BVW_DVD_NEXT_ANGLE,
	BVW_DVD_PREV_ANGLE
} BaconVideoWidgetDVDEvent;

void bacon_video_widget_dvd_event                (BaconVideoWidget *bvw,
						  BaconVideoWidgetDVDEvent
						  type);
GList *bacon_video_widget_get_languages          (BaconVideoWidget *bvw);
int bacon_video_widget_get_language              (BaconVideoWidget *bvw);
void bacon_video_widget_set_language             (BaconVideoWidget *bvw,
		                                  int language);

GList *bacon_video_widget_get_subtitles          (BaconVideoWidget *bvw);
int bacon_video_widget_get_subtitle              (BaconVideoWidget *bvw);
void bacon_video_widget_set_subtitle             (BaconVideoWidget *bvw,
		                                  int subtitle);

/* Screenshot functions */
gboolean bacon_video_widget_can_get_frames       (BaconVideoWidget *bvw,
						  GError **error);
GdkPixbuf *bacon_video_widget_get_current_frame (BaconVideoWidget *bvw);

/* TV-Out functions */
typedef enum {
	TV_OUT_NONE,
	TV_OUT_DXR3,
	TV_OUT_NVTV_PAL,
	TV_OUT_NVTV_NTSC
} TvOutType;

gboolean bacon_video_widget_fullscreen_mode_available (BaconVideoWidget *bvw,
						       TvOutType tvout);

gboolean bacon_video_widget_set_tv_out           (BaconVideoWidget *bvw,
						  TvOutType tvout);
TvOutType bacon_video_widget_get_tv_out          (BaconVideoWidget *bvw);

/* Audio-out functions */
typedef enum {
	BVW_AUDIO_SOUND_STEREO,
	BVW_AUDIO_SOUND_4CHANNEL,
	BVW_AUDIO_SOUND_41CHANNEL,
	BVW_AUDIO_SOUND_5CHANNEL,
	BVW_AUDIO_SOUND_51CHANNEL,
	BVW_AUDIO_SOUND_AC3PASSTHRU
} BaconVideoWidgetAudioOutType;

BaconVideoWidgetAudioOutType bacon_video_widget_get_audio_out_type
						 (BaconVideoWidget *bvw);
gboolean bacon_video_widget_set_audio_out_type   (BaconVideoWidget *bvw,
						  BaconVideoWidgetAudioOutType
						  type);

G_END_DECLS

#endif				/* HAVE_BACON_VIDEO_WIDGET_H */
