/* GStreamer
 * Copyright (C) 1999,2000,2001,2002 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000,2001,2002 Wim Taymans <wtay@chello.be>
 *                              2002 Steve Baker <steve@stevebaker.org>
 *								2003 Julien Moutte <julien@moutte.net>
 *
 * gstvideowidget.h: Video widget for gst xvideosink window
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifndef __GST_VIDEO_WIDGET_H__
#define __GST_VIDEO_WIDGET_H__

#include <config.h>

#include <gtk/gtkwidget.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define GST_TYPE_VIDEO_WIDGET          (gst_video_widget_get_type ())
#define GST_VIDEO_WIDGET(obj)          (GTK_CHECK_CAST ((obj), GST_TYPE_VIDEO_WIDGET, GstVideoWidget))
#define GST_VIDEO_WIDGET_CLASS(klass)  (GTK_CHECK_CLASS_CAST ((klass), GST_TYPE_VIDEO_WIDGET, GstVideoWidgetClass))
#define GST_IS_VIDEO_WIDGET(obj)       (GTK_CHECK_TYPE ((obj), GST_TYPE_VIDEO_WIDGET))
#define GST_IS_VIDEO_WIDGET_CLASS(obj) (GTK_CHECK_CLASS_TYPE ((klass), GST_TYPE_VIDEO_WIDGET))

typedef struct _GstVideoWidget GstVideoWidget;
typedef struct _GstVideoWidgetClass GstVideoWidgetClass;
typedef struct _GstVideoWidgetPrivate  GstVideoWidgetPrivate;
	
struct _GstVideoWidget {
	
	GtkWidget parent;

	GstVideoWidgetPrivate *priv;
};
	
struct _GstVideoWidgetClass {
	
	GtkWidgetClass parent_class;
	
};

GType 	gst_video_widget_get_type		(void);

GtkWidget*	gst_video_widget_new		(void);

/* Set/Get video source size */

gboolean gst_video_widget_set_source_size	(	GstVideoWidget *vw,
											gint width, gint height);

gboolean gst_video_widget_get_source_size	(	GstVideoWidget *vw,
											gint *width, gint *height);

/* Set/Get minimum video widget size */

gboolean gst_video_widget_set_minimum_size	(	GstVideoWidget *vw,
											gint width, gint height);

gboolean gst_video_widget_get_minimum_size	(	GstVideoWidget *vw,
											gint *width, gint *height);

/* Set/Get mouse pointer visible or not */

gboolean gst_video_widget_set_cursor_visible (	GstVideoWidget *vw,
												gboolean visible);
gboolean gst_video_widget_get_cursor_visible (GstVideoWidget *vw);

/* Set/Get mouse pointer visible or not */

gboolean gst_video_widget_set_logo_focus (	GstVideoWidget *vw,
												gboolean focused);
gboolean gst_video_widget_get_logo_focus (GstVideoWidget *vw);

/* Set/Get if the widget should catch events over embeded window */

gboolean gst_video_widget_set_event_catcher (	GstVideoWidget *vw,
												gboolean event_catcher);
gboolean gst_video_widget_get_event_catcher (GstVideoWidget *vw);

/* Set/Get auto resize mode used by the widget */

gboolean gst_video_widget_set_auto_resize (GstVideoWidget *vw, gboolean resize);
gboolean gst_video_widget_get_auto_resize (GstVideoWidget *vw);

/* Set/Get scale factor used by the widget */

gboolean gst_video_widget_set_scale (GstVideoWidget *vw, gfloat scale);
gfloat gst_video_widget_get_scale (GstVideoWidget *vw);

/* Set/Get the XID of the Xwindow to be embedded */

void	gst_video_widget_set_xembed_xid		(GstVideoWidget *vw, gulong xid);
gulong	gst_video_widget_get_xembed_xid		(GstVideoWidget *vw);

/* Set/Get the GdkPixbuf used for logo display */

void gst_video_widget_set_logo (GstVideoWidget *vw, GdkPixbuf *logo_pixbuf);
GdkPixbuf* gst_video_widget_get_logo (GstVideoWidget *vw);

#endif /* __GST_VIDEO_WIDGET_H__ */
