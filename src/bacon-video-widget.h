/* 
 * Copyright (C) 2001-2002 the xine project
 * 	Heavily modified by Bastien Nocera <hadess@hadess.net>
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
 * $Id$
 *
 * the xine engine in a widget - header
 */

#ifndef HAVE_BACON_VIDEO_WIDGET_H
#define HAVE_BACON_VIDEO_WIDGET_H

#include <gtk/gtkvbox.h>

G_BEGIN_DECLS

typedef enum {
	SPEED_PAUSE,
	SPEED_NORMAL,
} Speeds;

typedef enum {
	MEDIA_DVD,
	MEDIA_VCD,
	MEDIA_CDDA,
} MediaType;

typedef enum {
	TV_OUT_NONE,
	TV_OUT_DXR3,
	TV_OUT_TVMODE,
} TvOutType;

typedef enum {
	BVW_INFO_TITLE,
	BVW_INFO_ARTIST,
	BVW_INFO_YEAR,
	BVW_INFO_DURATION,
	/* Video */
	BVW_INFO_HAS_VIDEO,
	BVW_INFO_DIMENSION_X,
	BVW_INFO_DIMENSION_Y,
	BVW_INFO_VIDEO_CODEC,
	BVW_INFO_VIDEO_FOURCC,
	BVW_INFO_FPS,
	/* Audio */
	BVW_INFO_HAS_AUDIO,
	BVW_INFO_BITRATE,
	BVW_INFO_AUDIO_CODEC,
	BVW_INFO_AUDIO_FOURCC,
} BaconVideoWidgetMetadataType;

#define BACON_VIDEO_WIDGET(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), bacon_video_widget_get_type (), BaconVideoWidget))
#define BACON_VIDEO_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), bacon_video_widget_get_type (), BaconVideoWidgetClass))
#define BACON_IS_VIDEO_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_TYPE (obj, bacon_video_widget_get_type ()))
#define BACON_IS_VIDEO_WIDGET_CLASS(klass)   (G_CHECK_INSTANCE_GET_CLASS ((klass), bacon_video_widget_get_type ()))

typedef struct BaconVideoWidgetPrivate BaconVideoWidgetPrivate;

typedef struct {
	GtkVBox parent;
	BaconVideoWidgetPrivate *priv;
} BaconVideoWidget;

typedef struct {
	GtkVBoxClass parent_class;

	void (*error) (GtkWidget *bvw, const char *message,
                       gboolean playback_stopped);
	void (*eos) (GtkWidget *bvw);
	void (*got_metadata) (GtkWidget *bvw);
	void (*title_change) (GtkWidget *bvw, const char *title);
	void (*channels_change) (GtkWidget *bvw);
	void (*tick) (GtkWidget *bvw, int current_time, int stream_length,
			int current_position);
	void (*buffering) (GtkWidget *bvw, guint progress);
} BaconVideoWidgetClass;

GType bacon_video_widget_get_type                (void);
GtkWidget *bacon_video_widget_new		 (int width, int height,
						  gboolean null_video_out,
						  GError **error);

char *bacon_video_widget_get_backend_name (BaconVideoWidget *bvw);

/* Actions */
gboolean bacon_video_widget_open		 (BaconVideoWidget *bvw,
						  const gchar *mrl,
						  GError **error);

/* This is used for seeking:
 * @pos is used for seeking, from 0 (start) to 65535 (end)
 * @start_time is in milliseconds */
gboolean bacon_video_widget_play                 (BaconVideoWidget *bvw,
						  guint pos,
						  guint start_time,
						  GError **error);
void bacon_video_widget_stop                     (BaconVideoWidget *bvw);
void bacon_video_widget_close                    (BaconVideoWidget *bvw);
gboolean bacon_video_widget_eject                (BaconVideoWidget *bvw);
void bacon_video_widget_set_logo                 (BaconVideoWidget *bvw,
                                                  gchar *filename);
/* Properties */
void  bacon_video_widget_set_logo_mode		 (BaconVideoWidget *bvw,
						  gboolean logo_mode);
gboolean bacon_video_widget_get_logo_mode	 (BaconVideoWidget *bvw);

void bacon_video_widget_set_speed                (BaconVideoWidget *bvw,
		                                  Speeds speed);
int bacon_video_widget_get_speed                 (BaconVideoWidget *bvw);

void bacon_video_widget_set_proprietary_plugins_path
						 (BaconVideoWidget *bvw,
				                  const char *path);

gboolean bacon_video_widget_can_set_volume       (BaconVideoWidget *bvw);
void bacon_video_widget_set_volume               (BaconVideoWidget *bvw,
						  int volume);
int bacon_video_widget_get_volume                (BaconVideoWidget *bvw);

void bacon_video_widget_set_fullscreen		 (BaconVideoWidget *bvw,
						  gboolean fullscreen);
void bacon_video_widget_set_show_cursor          (BaconVideoWidget *bvw,
						  gboolean use_cursor);
gboolean bacon_video_widget_get_show_cursor      (BaconVideoWidget *bvw);

void bacon_video_widget_set_media_device	 (BaconVideoWidget *bvw,
						  const char *path);
gboolean bacon_video_widget_get_auto_resize (BaconVideoWidget *bvw);
void bacon_video_widget_set_auto_resize		 (BaconVideoWidget *bvw,
						  gboolean auto_resize);

void bacon_video_widget_set_connection_speed     (BaconVideoWidget *bvw,
						  int speed);
int bacon_video_widget_get_connection_speed      (BaconVideoWidget *bvw);

void bacon_video_widget_set_deinterlacing	 (BaconVideoWidget *bvw,
						  gboolean deinterlace);
gboolean bacon_video_widget_get_deinterlacing	 (BaconVideoWidget *bvw);

gboolean bacon_video_widget_set_tv_out		 (BaconVideoWidget *bvw,
						  TvOutType tvout);
TvOutType bacon_video_widget_get_tv_out		 (BaconVideoWidget *bvw);

void bacon_video_widget_toggle_aspect_ratio      (BaconVideoWidget *bvw);
void bacon_video_widget_set_scale_ratio          (BaconVideoWidget *bvw,
						  gfloat ratio);

int bacon_video_widget_get_position              (BaconVideoWidget *bvw);
int bacon_video_widget_get_current_time          (BaconVideoWidget *bvw);
int bacon_video_widget_get_stream_length         (BaconVideoWidget *bvw);
gboolean bacon_video_widget_is_playing           (BaconVideoWidget *bvw);
gboolean bacon_video_widget_is_seekable          (BaconVideoWidget *bvw);

gboolean bacon_video_widget_can_play             (BaconVideoWidget *bvw,
						  MediaType type);
G_CONST_RETURN gchar **bacon_video_widget_get_mrls
						 (BaconVideoWidget *bvw,
						  MediaType type);

void bacon_video_widget_get_metadata		 (BaconVideoWidget *bvw,
						  BaconVideoWidgetMetadataType
						  type,
						  GValue *value);

/* Visualisation functions */
typedef enum {
	VISUAL_SMALL,
	VISUAL_NORMAL,
	VISUAL_LARGE,
	VISUAL_EXTRA_LARGE,
} VisualsQuality;

gboolean bacon_video_widget_set_show_visuals	  (BaconVideoWidget *bvw,
						   gboolean show_visuals);
GList *bacon_video_widget_get_visuals_list	  (BaconVideoWidget *bvw);
gboolean bacon_video_widget_set_visuals		  (BaconVideoWidget *bvw,
						   const char *name);
void bacon_video_widget_set_visuals_quality	  (BaconVideoWidget *bvw,
						   VisualsQuality quality);

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

G_END_DECLS

#endif				/* HAVE_BACON_VIDEO_WIDGET_H */
