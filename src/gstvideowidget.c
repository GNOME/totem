/* GStreamer
 * Copyright (C) 1999,2000,2001,2002 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000,2001,2002 Wim Taymans <wtay@chello.be>
 *                              2002 Steve Baker <steve@stevebaker.org>
 *								2003 Julien Moutte <julien@moutte.net>
 *
 * gstvideowidget.c: Video widget for gst xvideosink window
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
 
#include "gstvideowidget.h"

/* Signals and Args */

enum {
	ARG_0,
	ARG_SCALE_FACTOR,
	ARG_AUTO_RESIZE,
	ARG_VISIBLE_CURSOR,
	ARG_LOGO_FOCUSED,
	ARG_EVENT_CATCHER,
	ARG_XID,
	ARG_SOURCE_WIDTH,
	ARG_SOURCE_HEIGHT,
	ARG_LOGO,
};

struct _GstVideoWidgetPrivate {

	GdkWindow *event_window;
	GdkWindow *video_window;

	GdkPixbuf *logo_pixbuf;
	
	gulong xembed_xid;
	
	guint source_width;
	guint source_height;
	
	guint width_mini;
	guint height_mini;
	
	gboolean auto_resize;
	gboolean cursor_visible;
	gboolean event_catcher;
	gboolean logo_focused;
	
	gboolean scale_override;
	gfloat scale_factor;
};

static GtkWidgetClass *parent_class = NULL;

/* ============================================================= */
/*                                                               */
/*                       Private Methods                         */
/*                                                               */
/* ============================================================= */

/* =========================================== */
/*                                             */
/*                  Tool Box                   */
/*                                             */
/* =========================================== */

/* Method updating cursor status depending on widget flag */

static void
gst_video_widget_update_cursor (GstVideoWidget *vw)
{
	GtkWidget *widget;
	
	g_return_if_fail(vw != NULL);
	g_return_if_fail(GST_IS_VIDEO_WIDGET(vw));
	
	widget = GTK_WIDGET (vw);
	
	if (widget->window == NULL)
		return;
	
	if (vw->priv->cursor_visible)
		gdk_window_set_cursor (widget->window, NULL);
	else {
	
		GdkBitmap *empty_bitmap;
		GdkColor useless;
		GdkCursor *cursor;
		char invisible_cursor_bits[] = { 0x0 }; 

		useless.red = useless.green = useless.blue = useless.pixel = 0;

		empty_bitmap = gdk_bitmap_create_from_data (	widget->window,
														invisible_cursor_bits,
														1, 1);

		cursor = gdk_cursor_new_from_pixmap (	empty_bitmap,
												empty_bitmap,
												&useless,
												&useless, 0, 0);
			
		gdk_window_set_cursor (widget->window, cursor);

		gdk_cursor_unref (cursor);

		g_object_unref (empty_bitmap);
	}	
}

/*
 * Method reordering event window and video window
 * depending on event_catcher flag
 */

static void
gst_video_widget_reorder_windows (GstVideoWidget *vw)
{
	g_return_if_fail(vw != NULL);
	g_return_if_fail(GST_IS_VIDEO_WIDGET(vw));
	
	if (vw->priv->event_catcher) {
		if (GDK_IS_WINDOW(vw->priv->event_window))
			gdk_window_raise(vw->priv->event_window);
	}
	else {
		if (GDK_IS_WINDOW(vw->priv->video_window))
			gdk_window_raise(vw->priv->video_window);
	}
	
	if ( (vw->priv->logo_focused) && (GDK_IS_WINDOW(vw->priv->video_window)) ) {
		gdk_window_hide(vw->priv->video_window);
	}
	else if (	(!vw->priv->logo_focused) &&
				(GDK_IS_WINDOW(vw->priv->video_window)) ) {
		gdk_window_show(vw->priv->video_window);
	}
	else {
		gtk_widget_queue_draw (GTK_WIDGET(vw));
	}
}

/* =========================================== */
/*                                             */
/*          Event Handlers, Callbacks          */
/*                                             */
/* =========================================== */

/* Realizing GstVideoWidget */

static void
gst_video_widget_realize (GtkWidget *widget)
{
	GstVideoWidget *vw = GST_VIDEO_WIDGET (widget);
	GdkWindowAttr attributes;
	gint attributes_mask;
	
	g_return_if_fail (vw != NULL);
	
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
	
	/* Creating our widget's window */
	
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK;
								
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	
	widget->window = gdk_window_new (	gtk_widget_get_parent_window (widget), 
										&attributes, attributes_mask);
	
	gdk_window_set_user_data (widget->window, widget);

	/* Creating our event window */
	
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = 0;
	attributes.y = 0;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.event_mask = GDK_ALL_EVENTS_MASK;
	
	attributes_mask = GDK_WA_X | GDK_WA_Y;

  	vw->priv->event_window = gdk_window_new (
								widget->window,
								&attributes, attributes_mask);
								
	gdk_window_set_user_data (vw->priv->event_window, widget);
	
	gdk_window_show (vw->priv->event_window);
	
	widget->style = gtk_style_attach (widget->style, widget->window);
	
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
	
	gst_video_widget_update_cursor (vw);
	
}

/* Unrealizing GstVideoWidget */

static void
gst_video_widget_unrealize (GtkWidget *widget)
{
	GstVideoWidget *vw;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GST_IS_VIDEO_WIDGET(widget));
	
	vw = GST_VIDEO_WIDGET (widget);
	
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);
	
	/* Hide all windows */

	if (GTK_WIDGET_MAPPED (widget))
		gtk_widget_unmap (widget);
  
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	/* Destroying event window */
	
	if (GDK_IS_WINDOW(vw->priv->event_window)) {
		gdk_window_set_user_data(vw->priv->event_window, NULL);
		gdk_window_destroy(vw->priv->event_window);
		vw->priv->event_window = NULL;
	}
	
	if (GDK_IS_WINDOW(vw->priv->video_window)) {
		gdk_window_set_user_data(vw->priv->video_window, NULL);
		gdk_window_destroy(vw->priv->video_window);
		vw->priv->video_window = NULL;
	}
	
	/* Chaining up */
	
	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

/* GstVideoWidget got exposed */

static gint
gst_video_widget_expose(GtkWidget *widget, GdkEventExpose *event)
{
	GstVideoWidget *vw;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GST_IS_VIDEO_WIDGET(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);
	
	vw = GST_VIDEO_WIDGET (widget);
	
	g_message ("expose %d, %d", widget->allocation.width, widget->allocation.height);
		
	if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget)) {
		
		if ( (vw->priv->logo_focused) &&  (vw->priv->logo_pixbuf) ) {
			GdkPixbuf *frame;
			guchar *pixels;
			int rowstride;
			gint width, height, alloc_width, alloc_height, logo_x, logo_y;
			gfloat width_ratio, height_ratio;

			frame = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
							FALSE, 8, widget->allocation.width,
							widget->allocation.height);
			
			width = gdk_pixbuf_get_width (vw->priv->logo_pixbuf);
			height = gdk_pixbuf_get_height (vw->priv->logo_pixbuf);
			alloc_width = widget->allocation.width;
			alloc_height = widget->allocation.height;
			
			/* Checking if allocated space is smaller than our logo */
			
			if ( (alloc_width < width) || (alloc_height < height) ) {
				width_ratio = (gfloat) alloc_width / (gfloat) width;
				height_ratio = (gfloat) alloc_height / (gfloat) height;
				width_ratio = MIN(width_ratio, height_ratio);
				height_ratio = width_ratio;
			}
			else 
				width_ratio = height_ratio = 1.0;
						
			logo_x = (alloc_width / 2) - (width * width_ratio / 2);
			logo_y = (alloc_height / 2) - (height * height_ratio / 2);
			
			/* Scaling to available space */
			
			gdk_pixbuf_composite (
					vw->priv->logo_pixbuf,
					frame,
					0, 0, 
					alloc_width, alloc_height,
					logo_x, logo_y,
					width_ratio, height_ratio,
					GDK_INTERP_BILINEAR,
					255);
			
			/* Drawing our frame */
			
			rowstride = gdk_pixbuf_get_rowstride (frame);

			pixels = gdk_pixbuf_get_pixels (frame) +
						rowstride * event->area.y + event->area.x * 3;
			
			gdk_draw_rgb_image_dithalign (
							widget->window, widget->style->black_gc,
							event->area.x, event->area.y,
							event->area.width, event->area.height,
							GDK_RGB_DITHER_NORMAL,
							pixels, rowstride,
							event->area.x, event->area.y);
							
			g_object_unref (frame);
		}
		else {
			gdk_draw_rectangle (
							widget->window, widget->style->black_gc, TRUE, 
		                    event->area.x, event->area.y,
							event->area.width, event->area.height);
		}
			
	}
	
	return FALSE;
}

/* Size request for our widget */

static void
gst_video_widget_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GstVideoWidget *vw;
	gint width, height;
	gfloat temp;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GST_IS_VIDEO_WIDGET(widget));
	
	vw = GST_VIDEO_WIDGET (widget);
	
	if (!vw->priv->auto_resize) {
		requisition->width = 1;
		requisition->height = 1;
		return;
	}
	
	if (	(vw->priv->source_width) &&
			(vw->priv->source_height) &&
			(vw->priv->scale_factor) ) {
		temp = (vw->priv->scale_factor * vw->priv->source_width + 0.5);
		width = (gint) temp > G_MAXINT ? G_MAXINT : (gint) temp;
		temp = (vw->priv->scale_factor * vw->priv->source_height + 0.5);
		height = (gint) temp > G_MAXINT ? G_MAXINT : (gint) temp;
	
		/* don't make us want to be bigger than the screen */
	
		if (width > gdk_screen_width()) {
			height = height * gdk_screen_width() / width;
			width = gdk_screen_width();
		}
		if (height > gdk_screen_height()) {
			width = width * gdk_screen_height() / height;
			height = gdk_screen_height();
		}
	}
	else {
		if (vw->priv->logo_pixbuf) {
			width = gdk_pixbuf_get_width(vw->priv->logo_pixbuf);
			height = gdk_pixbuf_get_height(vw->priv->logo_pixbuf);
			vw->priv->width_mini = width;
			vw->priv->height_mini = height;
		}
		else {
			width = 100;
			height = 100;
		}
	}
	
	g_message ("requesting %d, %d", width, height);
	
	requisition->width = width;
	requisition->height = height;
}

/* Allocate method for our widget */

static void
gst_video_widget_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GstVideoWidget *vw;
	gfloat width_ratio, height_ratio, temp, scale_factor = 1.0;
	guint width, height;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GST_IS_VIDEO_WIDGET(widget));
	
	vw = GST_VIDEO_WIDGET (widget);

	g_message ("allocated %d, %d", allocation->width, allocation->height);
	
	/* Choosing best ratio */
	
	if (vw->priv->scale_override) {
		scale_factor = vw->priv->scale_factor;
		vw->priv->scale_override = FALSE;
	}
	else {
		
		/* Ratio get impacted only if video window loaded */
		if (	(vw->priv->source_width) &&
				(vw->priv->source_height) &&
				(GDK_IS_WINDOW(vw->priv->video_window)) )
		{
			width_ratio = (gfloat) allocation->width /
							(gfloat) vw->priv->source_width;
			height_ratio = (gfloat) allocation->height /
							(gfloat) vw->priv->source_height;
	
			scale_factor = MIN (width_ratio, height_ratio);
		}
	}
	
	/* Calculating width & height with optimal ratio */
	
	temp = (scale_factor * vw->priv->source_width + 0.5);
	width = (gint) temp > G_MAXINT ? G_MAXINT : (gint) temp;
	temp = (scale_factor * vw->priv->source_height + 0.5);
	height = (gint) temp > G_MAXINT ? G_MAXINT : (gint) temp;

	if (vw->priv->auto_resize) {
		if (width < vw->priv->width_mini)
			width = vw->priv->width_mini;
		if (height < vw->priv->height_mini)
			height = vw->priv->height_mini;
		
		allocation->width = width;
		allocation->height = height;
	}
	
	widget->allocation = *allocation;
	
	g_message ("allocation now is %d, %d", allocation->width, allocation->height);
	
	if (GTK_WIDGET_REALIZED (widget)) {
		g_message ("source %d, %d size %d, %d auto %d scale %f", vw->priv->source_width, vw->priv->source_height, width, height, vw->priv->auto_resize, vw->priv->scale_factor);
		gdk_window_move_resize (	widget->window,
									allocation->x, 
									allocation->y,
									allocation->width, 
									allocation->height);
		
		if (GDK_IS_WINDOW(vw->priv->event_window))
			gdk_window_move_resize (	vw->priv->event_window,
										0, 
										0,
										allocation->width, 
										allocation->height);
		
		if (GDK_IS_WINDOW(vw->priv->video_window)) {
			gint video_x, video_y;
						
			video_x = (allocation->width / 2) - (width / 2);
			video_y = (allocation->height / 2) - (height / 2);
					
			gdk_window_move_resize (
							vw->priv->video_window,
							video_x, 
							video_y,
							width, 
							height);
		}
    }
}

/* GstVideoWidget methods to set properties */

static void
gst_video_widget_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GstVideoWidget *vw;

	g_return_if_fail (object != NULL);

	vw = GST_VIDEO_WIDGET (object);

	switch (prop_id) {
		case ARG_SCALE_FACTOR:
			vw->priv->scale_factor = g_value_get_float (value);
			vw->priv->scale_override = TRUE;
			gtk_widget_queue_resize (GTK_WIDGET(vw));
		break;
		case ARG_AUTO_RESIZE:
			vw->priv->auto_resize = g_value_get_boolean (value);
			gtk_widget_queue_resize (GTK_WIDGET(vw));
		break;
		case ARG_VISIBLE_CURSOR:
			vw->priv->cursor_visible = g_value_get_boolean (value);
			gst_video_widget_update_cursor (vw);
		break;
		case ARG_LOGO_FOCUSED:
			vw->priv->logo_focused = g_value_get_boolean (value);
			gst_video_widget_reorder_windows (vw);
		break;
		case ARG_EVENT_CATCHER:
			vw->priv->event_catcher = g_value_get_boolean (value);
			gst_video_widget_reorder_windows (vw);
		break;
		case ARG_XID:
			gst_video_widget_set_xembed_xid (vw, g_value_get_ulong(value));
		break;
		case ARG_SOURCE_WIDTH:
			vw->priv->source_width = g_value_get_int (value);
		break;
		case ARG_SOURCE_HEIGHT:
			vw->priv->source_height = g_value_get_int (value);
		break;
		case ARG_LOGO:
		{
			GdkPixbuf *image;

			image = (GdkPixbuf*) g_value_get_object (value);

			gst_video_widget_set_logo (vw, image);
		}
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* GstVideoWidget methods to get properties */

static void
gst_video_widget_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GstVideoWidget *vw;

	g_return_if_fail (object != NULL);

	vw = GST_VIDEO_WIDGET (object);

	switch (prop_id) {
		case ARG_SCALE_FACTOR:
			g_value_set_float (value, vw->priv->scale_factor);
		break;
		case ARG_AUTO_RESIZE:
			g_value_set_boolean (value, vw->priv->auto_resize);
		break;
		case ARG_VISIBLE_CURSOR:
			g_value_set_boolean (value, vw->priv->cursor_visible);
		break;
		case ARG_LOGO_FOCUSED:
			g_value_set_boolean (value, vw->priv->logo_focused);
		break;
		case ARG_EVENT_CATCHER:
			g_value_set_boolean (value, vw->priv->event_catcher);
		break;
		case ARG_XID:
			g_value_set_ulong (value, vw->priv->xembed_xid);
		break;
		case ARG_SOURCE_WIDTH:
			g_value_set_int (value, vw->priv->source_width);
		break;
		case ARG_SOURCE_HEIGHT:
			g_value_set_int (value, vw->priv->source_height);
		break;
		case ARG_LOGO:
			g_value_set_object (value,
                          (GObject*) vw->priv->logo_pixbuf);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* =========================================== */
/*                                             */
/*              Init & Class init              */
/*                                             */
/* =========================================== */

/* Class initialization for GstVideoWidget */

static void
gst_video_widget_class_init (GstVideoWidgetClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass*) klass;
	widget_class = (GtkWidgetClass*) klass;

	parent_class = gtk_type_class (gtk_widget_get_type ());
	
	gobject_class->set_property = gst_video_widget_set_property;
	gobject_class->get_property = gst_video_widget_get_property;
	
	g_object_class_install_property (
				gobject_class,
				ARG_SCALE_FACTOR,
				g_param_spec_float (
						"scale_factor",
						"scale factor",
						"size the video should be scaled to",
						0, G_MAXFLOAT / G_MAXINT, 1, G_PARAM_READWRITE));

	g_object_class_install_property (
				gobject_class,
				ARG_AUTO_RESIZE,
				g_param_spec_boolean (
						"auto_resize",
						"auto resize",
						"Is the video widget resizing automatically",
						FALSE, G_PARAM_READWRITE));
	
	g_object_class_install_property (
				gobject_class,
				ARG_VISIBLE_CURSOR,
				g_param_spec_boolean (
						"visible_cursor",
						"visible cursor",
						"Is the mouse pointer (cursor) visible or not",
						TRUE, G_PARAM_READWRITE));
						
	g_object_class_install_property (
				gobject_class,
				ARG_LOGO_FOCUSED,
				g_param_spec_boolean (
						"logo_focused",
						"logo is focused",
						"Is the logo focused or not",
						TRUE, G_PARAM_READWRITE));
	
	g_object_class_install_property (
				gobject_class,
				ARG_EVENT_CATCHER,
				g_param_spec_boolean (
						"event_catcher",
						"Event catcher",
						"Should the widget catch events over the video window",
						TRUE, G_PARAM_READWRITE));
	
	g_object_class_install_property (
				gobject_class,
				ARG_XID,
				g_param_spec_ulong (
						"video_xid",
						"video window xid",
						"Video playback Xwindow XID",
						0, G_MAXLONG, 1, G_PARAM_READWRITE));
	
	g_object_class_install_property (
				gobject_class,
				ARG_SOURCE_WIDTH,
				g_param_spec_int (
						"source_width",
						"video source width",
						"Video playback source width",
						0, G_MAXINT, 1, G_PARAM_READWRITE));
						
	g_object_class_install_property (
				gobject_class,
				ARG_SOURCE_HEIGHT,
				g_param_spec_int (
						"source_height",
						"video source height",
						"Video playback source height",
						0, G_MAXINT, 1, G_PARAM_READWRITE));
	
	 g_object_class_install_property (
	 			gobject_class,
				ARG_LOGO,
				g_param_spec_object (
						"logo",
						"Logo",
						"Picture that should appear as a logo when no video",
						gdk_pixbuf_get_type (), G_PARAM_READWRITE));

	widget_class->realize = gst_video_widget_realize;
	widget_class->unrealize = gst_video_widget_unrealize;
	widget_class->expose_event = gst_video_widget_expose;
	widget_class->size_request = gst_video_widget_size_request;
	widget_class->size_allocate = gst_video_widget_allocate;

}

/* Initing our widget */

static void
gst_video_widget_init (GstVideoWidget *vw)
{
	
	vw->priv = g_new0 (GstVideoWidgetPrivate, 1);
	
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET(vw), GTK_CAN_FOCUS);
	
	vw->priv->source_width	=	0;
	vw->priv->source_height	=	0;
	vw->priv->width_mini	=	0;
	vw->priv->height_mini	=	0;
	vw->priv->scale_factor	=	1.0;
	vw->priv->auto_resize	=	FALSE;
	vw->priv->scale_override=	FALSE;
	vw->priv->cursor_visible=	TRUE;
	vw->priv->event_catcher	=	TRUE;
	vw->priv->logo_focused	=	TRUE;
	vw->priv->event_window	=	NULL;
	vw->priv->video_window	=	NULL;
	vw->priv->logo_pixbuf	=	NULL;
}

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

/**
 * gst_video_widget_set_xembed_xid:
 * @vw: a #GstVideoWidget
 * @xid: the window ID of an existing window.
 * 
 * Reparents a pre-existing video window into a #GstVideoWidget. This is
 *  meant to embed a foreign Xwindow created by xvideosink for example.
 *
 * Remember you can set this value trough the "video_xid" property. This will
 *  trigger the embedding the same way than calling this method.
 *
 **/
void
gst_video_widget_set_xembed_xid (GstVideoWidget *vw, gulong xid)
{
	GtkWidget *widget = GTK_WIDGET (vw);
	
	gdk_threads_enter ();
		
	vw->priv->logo_focused = FALSE;
	
	if (GDK_IS_WINDOW(vw->priv->video_window)) {
		gdk_window_destroy (vw->priv->video_window);
		vw->priv->video_window = NULL;
	}
	
	vw->priv->video_window = gdk_window_foreign_new (xid);
	
	if (vw->priv->video_window) {
		gint video_x, video_y, video_width, video_height, video_depth;
		
		gdk_window_reparent (vw->priv->video_window, widget->window, 0, 0);
		
		gdk_window_show (vw->priv->video_window);
		
		gdk_window_get_geometry (	vw->priv->video_window,
									&video_x, &video_y,
									&video_width, &video_height, &video_depth);
		
		vw->priv->source_width = video_width;
		vw->priv->source_height = video_height;
		
		if (vw->priv->event_catcher)
			gdk_window_raise (vw->priv->event_window);
	
		vw->priv->xembed_xid = xid;
		
		gtk_widget_queue_resize (GTK_WIDGET(vw));

	}
	else
		g_warning ("Trying to embed a window which has been destroyed");
	
	gdk_threads_leave ();
}

/**
 * gst_video_widget_get_xembed_xid:
 * @vw: a #GstVideoWidget
 * 
 * Get current embeded window xid.
 *
 * Remember you can get this value trough the "video_xid" property.
 *
 * Return value: a #gulong referencing embeded video window.
 **/
gulong
gst_video_widget_get_xembed_xid (GstVideoWidget *vw)
{
	g_return_val_if_fail(vw != NULL, 0);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), 0);
	return vw->priv->xembed_xid;
}

/**
 * gst_video_widget_set_source_size:
 * @vw: a #GstVideoWidget
 * @width: a #gint indicating source video's width.
 * @height: a #gint indicating source video's height.
 * 
 * Set video source size of a #GstVideoWidget and queue a resize request
 *  for the widget.
 *
 * The #GstVideoWidget must have already been created
 *  before you can make this call.
 * 
 * Remember you can set these values trough "source_width" and "source_height"
 *  properties, but you will have to queue the resize request yourself.
 * 
 * Return value: a #gboolean indicating wether the call succeeded or not.
 **/
gboolean
gst_video_widget_set_source_size (GstVideoWidget *vw, gint width, gint height)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	
	vw->priv->source_width = width;
	vw->priv->source_height = height;
	gtk_widget_queue_resize (GTK_WIDGET(vw));
	
	return TRUE;
}

/**
 * gst_video_widget_get_source_size:
 * @socket_: a #GstVideoWidget
 * @width: a pointer to a #gint where source video's width will be put.
 * @height: a pointer to a #gint where source video's height will be put.
 * 
 * Fills @width and @height with source video's dimensions.
 *
 * Remember you can get these values trough "source_width" and "source_height"
 *  properties.
 *
 * Return value: a #gboolean indicating wether the call succeeded or not.
 **/
gboolean
gst_video_widget_get_source_size (GstVideoWidget *vw, gint *width, gint *height)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);

	*width = vw->priv->source_width;
	*height = vw->priv->source_height;
	
	return TRUE;
}

/**
 * gst_video_widget_set_minimum_size:
 * @vw: a #GstVideoWidget
 * @width: a #gint indicating minimum width.
 * @height: a #gint indicating minimum height.
 * 
 * Set minimum size of a #GstVideoWidget and queue a resize request
 *  for the widget. This method is usefull when the #GstVideoWidget is set
 *  to auto resize, it won't go under this size.
 *
 * The #GstVideoWidget must have already been created
 *  before you can make this call.
 * 
 * Remember you can set these values trough "width_mini" and "height_mini"
 *  properties, but you will have to queue the resize request yourself.
 * 
 * Return value: a #gboolean indicating wether the call succeeded or not.
 **/
gboolean
gst_video_widget_set_minimum_size (GstVideoWidget *vw, gint width, gint height)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	
	vw->priv->width_mini = width;
	vw->priv->height_mini = height;
	gtk_widget_queue_resize (GTK_WIDGET(vw));
	
	return TRUE;
}

/**
 * gst_video_widget_get_minimum_size:
 * @socket_: a #GstVideoWidget
 * @width: a pointer to a #gint where minimum width will be put.
 * @height: a pointer to a #gint where minimum height will be put.
 * 
 * Fills @width and @height with minimum dimensions.
 *
 * Remember you can get these values trough "width_mini" and "height_mini"
 *  properties.
 *
 * Return value: a #gboolean indicating wether the call succeeded or not.
 **/
gboolean
gst_video_widget_get_minimum_size (GstVideoWidget *vw, gint *width, gint *height)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);

	*width = vw->priv->width_mini;
	*height = vw->priv->height_mini;
	
	return TRUE;
}

/**
 * gst_video_widget_set_cursor_visible:
 * @vw: a #GstVideoWidget
 * @visible: a #gboolean indicating wether or not the cursor should be visible.
 * 
 * Set cursor visible or not over embeded video window.
 *
 * Remember you can set this flag trough "visible_cursor" property.
 *
 * Return value: a #gboolean indicating wether the call succeeded or not.
 **/
gboolean
gst_video_widget_set_cursor_visible (GstVideoWidget *vw, gboolean visible)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	
	vw->priv->cursor_visible = visible;
	
	gst_video_widget_update_cursor (vw);
	
	return TRUE;
}

/**
 * gst_video_widget_get_cursor_visible:
 * @vw: a #GstVideoWidget
 * 
 * Get cursor visible status over embeded video window.
 *
 * Remember you can get this flag trough "visible_cursor" property.
 *
 * Return value: a #gboolean indicating wether the cursor is visible or not.
 **/
gboolean
gst_video_widget_get_cursor_visible (GstVideoWidget *vw)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	return vw->priv->cursor_visible;
}

/**
 * gst_video_widget_set_logo_focus:
 * @vw: a #GstVideoWidget
 * @visible: a #gboolean indicating wether or not the logo should have focus.
 * 
 * Set logo's focus over embeded video window.
 *
 * Remember you can set this flag trough "logo_focused" property.
 *
 * Return value: a #gboolean indicating wether the call succeeded or not.
 **/
gboolean
gst_video_widget_set_logo_focus (GstVideoWidget *vw, gboolean focused)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	
	vw->priv->logo_focused = focused;
	
	gst_video_widget_reorder_windows (vw);
	
	return TRUE;
}

/**
 * gst_video_widget_get_logo_focus:
 * @vw: a #GstVideoWidget
 * 
 * Get logo focus status over embeded video window.
 *
 * Remember you can get this flag trough "logo_focused" property.
 *
 * Return value: a #gboolean indicating wether the logo has focus or not.
 **/
gboolean
gst_video_widget_get_logo_focus (GstVideoWidget *vw)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	return vw->priv->logo_focused;
}

/**
 * gst_video_widget_set_event_catcher:
 * @vw: a #GstVideoWidget
 * @event_catcher: a #gboolean indicating wether the widget should catch events
 *  over the embeded window or not.
 * 
 * Set a #GstVideoWidget to catch events over the embeded window or not.
 *
 * Remember you can set this flag trough the "event_catcher" property.
 *
 * Return value: a #gboolean indicating wether the call succeeded or not.
 **/
gboolean
gst_video_widget_set_event_catcher (GstVideoWidget *vw, gboolean event_catcher)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	
	vw->priv->event_catcher = event_catcher;
	
	gst_video_widget_reorder_windows (vw);
	
	return TRUE;
}

/**
 * gst_video_widget_get_event_catcher:
 * @vw: a #GstVideoWidget
 * 
 * Get event catcher status from a #GstVideoWidget to know if the widget
 *  catch events over embeded window or not.
 *
 * Remember you can get this flag trough "event_catcher" property.
 *
 * Return value: a #gboolean indicating wether the widget catch events or not.
 **/
gboolean
gst_video_widget_get_event_catcher (GstVideoWidget *vw)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	return vw->priv->event_catcher;
}

/**
 * gst_video_widget_set_auto_resize:
 * @vw: a #GstVideoWidget
 * @resize: a #gboolean indicating auto resize mode that will be used by @vw.
 * 
 * Set if a #GstVideoWidget will auto resize or not.
 *
 * Remember you can set this flag trough the "auto_resize" property.
 *
 * Return value: a #gboolean indicating wether the call succeeded or not.
 **/
gboolean
gst_video_widget_set_auto_resize (GstVideoWidget *vw, gboolean resize)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	vw->priv->auto_resize = resize;
	gtk_widget_queue_resize (GTK_WIDGET(vw));
	return TRUE;
}

/**
 * gst_video_widget_get_auto_resize:
 * @vw: a #GstVideoWidget
 * 
 * Get used auto resize mode for a #GstVideoWidget.
 *
 * Remember you can get this value trough "auto_resize" property.
 *
 * Return value: a #gboolean indicating wether the video widget
 * is in auto_resize mode or not.
 **/
gboolean
gst_video_widget_get_auto_resize (GstVideoWidget *vw)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	return vw->priv->auto_resize;
}

/**
 * gst_video_widget_set_scale:
 * @vw: a #GstVideoWidget
 * @scale: a #gfloat indicating scale factor that will be used by @vw.
 * 
 * Set a scale factor for a #GstVideoWidget.
 *
 * Remember you can set this flag trough the "scale_factor" property.
 *
 * Return value: a #gboolean indicating wether the call succeeded or not.
 **/
gboolean
gst_video_widget_set_scale (GstVideoWidget *vw, gfloat scale)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	vw->priv->scale_factor = scale;
	vw->priv->scale_override = TRUE;
	gtk_widget_queue_resize (GTK_WIDGET(vw));
	return TRUE;
}

/**
 * gst_video_widget_get_scale:
 * @vw: a #GstVideoWidget
 * 
 * Get used scale factor for a #GstVideoWidget.
 *
 * Remember you can get this value trough "scale_factor" property.
 *
 * Return value: a #gfloat indicating scale factor used.
 **/
gfloat
gst_video_widget_get_scale (GstVideoWidget *vw)
{
	g_return_val_if_fail(vw != NULL, FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET (vw), FALSE);
	return vw->priv->scale_factor;
}

/** 
 * gst_video_widget_set_logo:
 * @vw: a #GstVideoWidget.
 * @logo: a #GdkPixbuf to set as the image for the logo.
 * 
 * Sets the image of @vw to the given @logo pixbuf.
 * 
 * Warning @logo should not be freed after this call unless you destroyed
 * widget. Indeed no copy is done. We use your #GdkPixbuf !
 **/ 
void
gst_video_widget_set_logo (GstVideoWidget *vw, GdkPixbuf *logo_pixbuf)
{
	g_return_if_fail (vw != NULL);
	g_return_if_fail (GST_IS_VIDEO_WIDGET(vw));

	if (logo_pixbuf == vw->priv->logo_pixbuf)
		return;

	if (vw->priv->logo_pixbuf)
		g_object_unref (vw->priv->logo_pixbuf);

	vw->priv->logo_pixbuf = logo_pixbuf;
}

/** 
 * gst_video_widget_get_logo:
 * @vw: a #GstVideoWidget.
 * @returns: the #GdkPixbuf set as logo of @vw.
 * 
 * Gets the logo of @vw.
 **/ 
GdkPixbuf*
gst_video_widget_get_logo (GstVideoWidget *vw)
{
	g_return_if_fail (vw != NULL);
	g_return_if_fail (GST_IS_VIDEO_WIDGET(vw));
	return vw->priv->logo_pixbuf;
}

/* =========================================== */
/*                                             */
/*          Widget typing & Creation           */
/*                                             */
/* =========================================== */

/* Get type function for GstVideoWidget */

GType
gst_video_widget_get_type (void)
{
	static GType vw_type = 0;

	if (!vw_type) {
		static const GTypeInfo vw_info = {
			sizeof (GstVideoWidgetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gst_video_widget_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GstVideoWidget),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gst_video_widget_init,
		};
		vw_type = g_type_register_static (	GTK_TYPE_WIDGET,
											"GstVideoWidget",
			 								&vw_info,
											(GTypeFlags)0);
	}
	return vw_type;
}


/**
 * gst_video_widget_new:
 *
 * Create a new empty #GstVideoWidget.
 * 
 * Return value:  the new #GstVideoWidget.
 **/
GtkWidget *
gst_video_widget_new (void)
{
	GstVideoWidget *widget = g_object_new (GST_TYPE_VIDEO_WIDGET, NULL);
	
	return GTK_WIDGET(widget);
}
