/* 
 * Copyright (C) 2001-2002 the xine project
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

#ifndef HAVE_GTK_XINE_H
#define HAVE_GTK_XINE_H

#include <gtk/gtkwidget.h>
#include <xine.h>

G_BEGIN_DECLS

typedef enum {
	GTX_STARTUP,
	GTX_NO_INPUT_PLUGIN,
	GTX_NO_DEMUXER_PLUGIN,
	GTX_DEMUXER_FAILED,
	GTX_NO_CODEC,
} GtkXineError;

#define GTK_XINE(obj)              (GTK_CHECK_CAST ((obj), gtk_xine_get_type (), GtkXine))
#define GTK_XINE_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), gtk_xine_get_type (), GtkXineClass))
#define GTK_IS_XINE(obj)           (GTK_CHECK_TYPE (obj, gtk_xine_get_type ()))
#define GTK_IS_XINE_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), gtk_xine_get_type ()))

typedef struct GtkXinePrivate GtkXinePrivate;

typedef struct {
	GtkWidget widget;
	GtkXinePrivate *priv;
} GtkXine;

typedef struct {
	GtkWidgetClass parent_class;

	void (*error) (GtkWidget *gtx, GtkXineError error, const char *message);
	void (*mouse_motion) (GtkWidget *gtx);
	void (*eos) (GtkWidget *gtx);
} GtkXineClass;

GtkType gtk_xine_get_type              (void);
GtkWidget *gtk_xine_new                (int width, int height);
gboolean  gtk_xine_check               (GtkXine *gtx);

/* Actions */
gint gtk_xine_play                     (GtkXine *gtx, gchar * mrl, gint pos,
		                        gint start_time);
void gtk_xine_stop                     (GtkXine *gtx);

/* Properties */
void gtk_xine_set_speed                (GtkXine *gtx, gint speed);
gint gtk_xine_get_speed                (GtkXine *gtx);

void gtk_xine_set_fullscreen           (GtkXine *gtx, gboolean fullscreen);
gint gtk_xine_is_fullscreen            (GtkXine *gtx);

gboolean gtk_xine_can_set_volume       (GtkXine *gtx);
void gtk_xine_set_volume               (GtkXine *gtx, gint volume);
gint gtk_xine_get_volume               (GtkXine *gtx);

void gtk_xine_set_show_cursor          (GtkXine *gtx, gboolean use_cursor);
gboolean gtk_xine_get_show_cursor      (GtkXine *gtx);

void gtk_xine_set_audio_channel        (GtkXine *gtx, gint audio_channel);
gint gtk_xine_get_audio_channel        (GtkXine *gtx);
#if 0
void gtk_xine_set_video_property       (GtkXine *gtx,
				        gint property, gint value);
gint gtk_xine_get_video_property       (GtkXine *gtx, gint property);
#endif
void gtk_xine_toggle_aspect_ratio      (GtkXine *gtx);
void gtk_xine_set_scale_ratio          (GtkXine *gtx, gfloat ratio);

gint gtk_xine_get_position             (GtkXine *gtx);
gint gtk_xine_get_current_time         (GtkXine *gtx);
gint gtk_xine_get_stream_length        (GtkXine *gtx);
gboolean gtk_xine_is_playing           (GtkXine *gtx);
gboolean gtk_xine_is_seekable          (GtkXine *gtx);

//gchar **gtk_xine_get_autoplay_plugins  (GtkXine *gtx);

#if 0
gint gtk_xine_get_log_section_count    (GtkXine *gtx);
gchar **gtk_xine_get_log_names         (GtkXine *gtx);
gchar **gtk_xine_get_log               (GtkXine *gtx, gint buf);
#endif
G_END_DECLS

#endif				/* HAVE_GTK_XINE_H */
