/* 
 * Copyright (C) 2003 the Gstreamer project
 * 	Julien Moutte <julien@moutte.net>
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
 */

#include <config.h>

/* libgstplay */
#include <gst/play/play.h>

/* gstgconf */
#include <gst/gconf/gconf.h>

/* system */
#include <string.h>
#include <stdio.h>

/* gtk+/gnome */
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "bacon-video-widget.h"
#include "baconvideowidget-marshal.h"
#include "scrsaver.h"
#include "video-utils.h"
#include "gstvideowidget.h"

#include <libintl.h>
#define _(String) gettext (String)
#ifdef gettext_noop
#   define N_(String) gettext_noop (String)
#else
#   define N_(String) (String)
#endif

#define DEFAULT_HEIGHT 420
#define DEFAULT_WIDTH 315
#define CONFIG_FILE ".gnome2"G_DIR_SEPARATOR_S"totem_config"
#define DEFAULT_TITLE _("Totem Video Window")

/* Signals */
enum {
	ERROR,
	EOS,
	TITLE_CHANGE,
	TICK,
	LAST_SIGNAL
};

/* Enum for none-signal stuff that needs to go through the AsyncQueue */
enum {
	RATIO = LAST_SIGNAL
};

/* Arguments */
enum {
	PROP_0,
	PROP_LOGO_MODE,
	PROP_FULLSCREEN,
	PROP_SPEED,
	PROP_POSITION,
	PROP_CURRENT_TIME,
	PROP_STREAM_LENGTH,
	PROP_PLAYING,
	PROP_SEEKABLE,
	PROP_SHOWCURSOR,
	PROP_MEDIADEV,
	PROP_SHOW_VISUALS,
};

struct BaconVideoWidgetPrivate {
	double display_ratio;

	GstPlay *play;
	GstVideoWidget *vw;
	
	GdkPixbuf *logo_pixbuf;
	
	gboolean media_has_video;
	
	/* Configuration */
	gboolean null_out;

	/* X stuff */
	Display *display;
	int screen;
	GdkWindow *video_window;

	/* Visual effects */
	char *mrl;
	gboolean show_vfx;
	gboolean using_vfx;

	/* Other stuff */
	int xpos, ypos;
	gboolean logo_mode;
	gboolean auto_resize;

	gint video_width;
	gint video_height;

	/* fullscreen stuff */
	gboolean fullscreen_mode;
};

static void bacon_video_widget_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec);
static void bacon_video_widget_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec);

static void bacon_video_widget_finalize (GObject *object);

static gboolean bacon_video_widget_expose (GtkWidget *widget,
		GdkEventExpose *event);
static gboolean bacon_video_widget_motion_notify (GtkWidget *widget,
		GdkEventMotion *event);
static gboolean bacon_video_widget_button_press (GtkWidget *widget,
		GdkEventButton *event);
static gboolean bacon_video_widget_key_press (GtkWidget *widget,
		GdkEventKey *event);


static GtkWidgetClass *parent_class = NULL;

static int bvw_table_signals[LAST_SIGNAL] = { 0 };

static void
bacon_video_widget_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	BaconVideoWidget *bvw;
	GtkRequisition child_requisition;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(BACON_IS_VIDEO_WIDGET(widget));
	
	bvw = BACON_VIDEO_WIDGET (widget);
	
	gtk_widget_size_request (GTK_WIDGET(bvw->priv->vw), &child_requisition);
	
	requisition->width = child_requisition.width;
	requisition->height = child_requisition.height;
}

static void
bacon_video_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	BaconVideoWidget *bvw;
	GtkAllocation child_allocation;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(BACON_IS_VIDEO_WIDGET(widget));
	
	bvw = BACON_VIDEO_WIDGET (widget);
	
	widget->allocation = *allocation;
	
	child_allocation.x = allocation->x;
	child_allocation.y = allocation->y;
	child_allocation.width = allocation->width;
	child_allocation.height = allocation->height;
	
	gtk_widget_size_allocate (GTK_WIDGET(bvw->priv->vw), &child_allocation);
}

static void
bacon_video_widget_class_init (BaconVideoWidgetClass *klass)
{

	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_vbox_get_type ());

	/* GtkWidget */
	widget_class->size_request = bacon_video_widget_size_request;
	widget_class->size_allocate = bacon_video_widget_size_allocate;
//	widget_class->expose_event = bacon_video_widget_expose;
//	widget_class->motion_notify_event = bacon_video_widget_motion_notify;
//	widget_class->button_press_event = bacon_video_widget_button_press;
//	widget_class->key_press_event = bacon_video_widget_key_press;

	/* GObject */
	object_class->set_property = bacon_video_widget_set_property;
	object_class->get_property = bacon_video_widget_get_property;
	object_class->finalize = bacon_video_widget_finalize;

	/* Properties */
	g_object_class_install_property (object_class, PROP_LOGO_MODE,
			g_param_spec_boolean ("logo_mode", NULL, NULL,
				FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_FULLSCREEN,
			g_param_spec_boolean ("fullscreen", NULL, NULL,
				FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_SPEED,
			g_param_spec_int ("speed", NULL, NULL,
				SPEED_PAUSE, SPEED_NORMAL,
				0, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_POSITION,
			g_param_spec_int ("position", NULL, NULL,
				0, 65535, 0, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_STREAM_LENGTH,
			g_param_spec_int ("stream_length", NULL, NULL,
				0, 65535, 0, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_PLAYING,
			g_param_spec_boolean ("playing", NULL, NULL,
				FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_SEEKABLE,
			g_param_spec_boolean ("seekable", NULL, NULL,
				FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_SHOWCURSOR,
			g_param_spec_boolean ("showcursor", NULL, NULL,
				FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_MEDIADEV,
			g_param_spec_string ("mediadev", NULL, NULL,
				FALSE, G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_SHOW_VISUALS,
			g_param_spec_boolean ("showvisuals", NULL, NULL,
				FALSE, G_PARAM_WRITABLE));

	/* Signals */
	bvw_table_signals[ERROR] =
		g_signal_new ("error",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconVideoWidgetClass, error),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);

	bvw_table_signals[EOS] =
		g_signal_new ("eos",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconVideoWidgetClass, eos),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	bvw_table_signals[TITLE_CHANGE] =
		g_signal_new ("title-change",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconVideoWidgetClass,
					title_change),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);
	bvw_table_signals[TICK] =
		g_signal_new ("tick",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconVideoWidgetClass, tick),
				NULL, NULL,
				baconvideowidget_marshal_VOID__INT_INT_INT,
				G_TYPE_NONE, 3, G_TYPE_INT, G_TYPE_INT,
				G_TYPE_INT);
}

static void
bacon_video_widget_instance_init (BaconVideoWidget *bvw)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	int argc = 1;
	char *argv[] = { "bacon_name", NULL };

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (bvw), GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (bvw), GTK_NO_WINDOW);
	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (bvw), GTK_DOUBLE_BUFFERED);

	/* we could actually change this to have some more flags if some
	 * gconf variable was set
	 * FIXME */
	gst_init (&argc, &argv);

	/* Using opt as default scheduler */
	gst_scheduler_factory_set_default_name ("opt");
	
	bvw->priv = g_new0 (BaconVideoWidgetPrivate, 1);
}

static void
update_xid (GstPlay* play, gint xid, BaconVideoWidget *bvw)
{
	g_return_if_fail(bvw != NULL);
	g_return_if_fail(BACON_IS_VIDEO_WIDGET(bvw));

	g_message ("update_xid");
	
	bvw->priv->media_has_video = TRUE;
	
	/* 0.6.1 
	gst_play_connect_visualisation ( bvw->priv->play, FALSE);*/
	
	if (bvw->priv->vw)
		gst_video_widget_set_xembed_xid(bvw->priv->vw, xid);
}

static void
update_vis_xid (GstPlay* play, gint xid, BaconVideoWidget *bvw)
{
	g_return_if_fail(bvw != NULL);
	g_return_if_fail(BACON_IS_VIDEO_WIDGET(bvw));

	g_message ("update_vis_xid");
	if (bvw->priv->vw)
		gst_video_widget_set_xembed_xid(bvw->priv->vw, xid);
	
}

static void
got_video_size (GstPlay* play, gint width, gint height, BaconVideoWidget *bvw)
{
	g_message ("have_video_size %d, %d", width, height);
	g_return_if_fail(bvw != NULL);
	g_return_if_fail(BACON_IS_VIDEO_WIDGET(bvw));

	bvw->priv->video_width = width;
	bvw->priv->video_height = height;
	
	if (bvw->priv->vw)
		gst_video_widget_set_source_size(bvw->priv->vw, width, height);
}

static void
got_eos (GstPlay* play, BaconVideoWidget *bvw)
{
	g_return_if_fail(bvw != NULL);
	g_return_if_fail(BACON_IS_VIDEO_WIDGET(bvw));

	g_message ("stream_end");
	g_signal_emit (G_OBJECT (bvw), bvw_table_signals[EOS], 0, NULL);
}

static void
bacon_video_widget_finalize (GObject *object)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) object;

	if ( GST_IS_PLAY (bvw->priv->play) )
		gst_play_set_state (bvw->priv->play, GST_STATE_READY);
	
	if (GST_IS_VIDEO_WIDGET(bvw->priv->vw)) {
		gtk_widget_destroy (GTK_WIDGET(bvw->priv->vw));
		bvw->priv->vw = NULL;
	}
	
	if (bvw->priv->play) {
		g_object_unref (bvw->priv->play);
		bvw->priv->play = NULL;
	}
	//FIXME
}

static void
bacon_video_widget_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec)
{
}

static void
bacon_video_widget_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec)
{
}

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

char *
bacon_video_widget_get_backend_name (BaconVideoWidget *bvw)
{
	guint major, minor, micro;
	
	gst_version (&major, &minor, &micro);
	
	
	return g_strdup_printf ("GStreamer version %d.%d.%d", major, minor, micro);
}

gboolean
bacon_video_widget_eject (BaconVideoWidget *bvw)
{
	return FALSE;
}

int
bacon_video_widget_get_subtitle (BaconVideoWidget *bvw)
{
	return -1;
}

void
bacon_video_widget_set_subtitle (BaconVideoWidget *bvw, int subtitle)
{
}

GList
*bacon_video_widget_get_subtitles (BaconVideoWidget *bvw)
{
	return NULL;
}

GList
*bacon_video_widget_get_languages (BaconVideoWidget *bvw)
{
	return NULL;
}

int
bacon_video_widget_get_language (BaconVideoWidget *bvw)
{
	return -1;
}

void
bacon_video_widget_set_language (BaconVideoWidget *bvw, int language)
{
}

int
bacon_video_widget_get_connection_speed (BaconVideoWidget *bvw)
{
	return 0;
}

void
bacon_video_widget_set_connection_speed (BaconVideoWidget *bvw, int speed)
{
}

/* =========================================== */
/*                                             */
/*               Play/Pause, Stop              */
/*                                             */
/* =========================================== */

gboolean
bacon_video_widget_open (BaconVideoWidget *bvw, const gchar *mrl,
		GError **error)
{
	GstElement *datasrc = NULL;
	
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (mrl != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->play != NULL, FALSE);
	g_return_val_if_fail (bvw->priv->mrl == NULL, FALSE);

	g_message ("bacon_video_widget_open: %s", mrl);
	bvw->priv->mrl = g_strdup (mrl);

	/* 0.6.1 
	gst_play_connect_visualisation ( bvw->priv->play, TRUE);*/
	
	gst_play_need_new_video_window ( bvw->priv->play);

	if (g_file_test (mrl,G_FILE_TEST_EXISTS)) {
		datasrc = gst_element_factory_make ("filesrc", "source");
	}
	else {
		datasrc = gst_element_factory_make ("gnomevfssrc", "source");
	}
	
	if (GST_IS_ELEMENT(datasrc))
		gst_play_set_data_src (bvw->priv->play, datasrc);
	
	bvw->priv->media_has_video = FALSE;
	
	return gst_play_set_location (bvw->priv->play, mrl);;
}

/* This is used for seeking:
 * @pos is used for seeking, from 0 (start) to 65535 (end)
 * @start_time is in milliseconds */
gboolean
bacon_video_widget_play	(BaconVideoWidget *bvw,
		guint pos,
		guint start_time,
		GError **error)
{
	//FIXME
	g_message ("bacon_video_widget_play");
	gst_play_set_state (bvw->priv->play, GST_STATE_PLAYING);
	return TRUE;
}

void
bacon_video_widget_stop		(BaconVideoWidget *bvw)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));

	gst_play_set_state (bvw->priv->play, GST_STATE_READY);
}

void
bacon_video_widget_close	(BaconVideoWidget *bvw)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));

	gst_play_set_state (bvw->priv->play, GST_STATE_READY);
	gst_play_set_location (bvw->priv->play, "/dev/null");
	g_free (bvw->priv->mrl);
	bvw->priv->mrl = NULL;
}

void
bacon_video_widget_dvd_event (BaconVideoWidget *bvw,
		BaconVideoWidgetDVDEvent type)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));
}

void
bacon_video_widget_set_logo (BaconVideoWidget *bvw, gchar *filename)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_VIDEO_WIDGET(bvw->priv->vw));
	
	bvw->priv->logo_pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	
	gst_video_widget_set_logo (bvw->priv->vw, bvw->priv->logo_pixbuf);
}

void
bacon_video_widget_set_logo_mode (BaconVideoWidget *bvw, gboolean logo_mode)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_VIDEO_WIDGET(bvw->priv->vw));
	gst_video_widget_set_logo_focus (bvw->priv->vw, TRUE);
}

gboolean
bacon_video_widget_get_logo_mode (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET(bvw->priv->vw), FALSE);
	return gst_video_widget_get_logo_focus (bvw->priv->vw);
}

void
bacon_video_widget_set_speed (BaconVideoWidget *bvw, Speeds speed)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));
	
	switch (speed) {
		case SPEED_PAUSE:
			gst_play_set_state (bvw->priv->play, GST_STATE_PAUSED);
			break;;
		case SPEED_NORMAL:
			gst_play_set_state (bvw->priv->play, GST_STATE_PLAYING);
			break;;
		default:
			g_warning ("unsupported speed %d", speed);
	}
}

int
bacon_video_widget_get_speed (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, -1);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), -1);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), -1);
	
	if (gst_play_get_state (bvw->priv->play) == GST_STATE_PAUSED)
		return SPEED_PAUSE;
	else if (gst_play_get_state (bvw->priv->play) == GST_STATE_PLAYING)
		return SPEED_NORMAL;
	else
		g_message ("pipeline is not in a known Bacon speed");

	return -1;
}

void
bacon_video_widget_set_fullscreen (BaconVideoWidget *bvw, gboolean fullscreen)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));
}

gboolean
bacon_video_widget_is_fullscreen (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), FALSE);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), FALSE);
	return FALSE;
}

gboolean
bacon_video_widget_can_set_volume (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), FALSE);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), FALSE);
	return TRUE;
}

void
bacon_video_widget_set_volume (BaconVideoWidget *bvw, int volume)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));
	gst_play_set_volume (bvw->priv->play, volume / 100);
}

int
bacon_video_widget_get_volume (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, -1);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), -1);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), -1);
	return gst_play_get_volume (bvw->priv->play) * 100;
}

void
bacon_video_widget_set_show_cursor (BaconVideoWidget *bvw, gboolean use_cursor)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_VIDEO_WIDGET(bvw->priv->vw));
	
	gst_video_widget_set_cursor_visible (bvw->priv->vw, use_cursor);
}

gboolean
bacon_video_widget_get_show_cursor (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), FALSE);
	g_return_val_if_fail (GST_IS_VIDEO_WIDGET(bvw->priv->vw), FALSE);

	return gst_video_widget_get_cursor_visible (bvw->priv->vw);
}

void
bacon_video_widget_set_media_device (BaconVideoWidget *bvw, const char *path)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));
}

int
bacon_video_widget_set_show_visuals (BaconVideoWidget *bvw,
		gboolean show_visuals)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));

	if (!bvw->priv->media_has_video) {
		if (!show_visuals) {
			gst_video_widget_set_logo_focus (bvw->priv->vw,TRUE);
		}
		else {
			gst_video_widget_set_logo_focus (bvw->priv->vw,FALSE);
		}
		
		/* 0.6.1 
		gst_play_connect_visualisation (bvw->priv->play, show_visuals); */
		
	}
	
	return TRUE;
}

void
bacon_video_widget_set_auto_resize (BaconVideoWidget *bvw, gboolean auto_resize)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));
}

void
bacon_video_widget_toggle_aspect_ratio (BaconVideoWidget *bvw)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));
}

void
bacon_video_widget_set_scale_ratio (BaconVideoWidget *bvw, gfloat ratio)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));
}

int
bacon_video_widget_get_position (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, -1);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), -1);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), -1);
}

int
bacon_video_widget_get_current_time (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, -1);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), -1);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), -1);
}

int
bacon_video_widget_get_stream_length (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, -1);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), -1);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), -1);

	return bvw->priv->play->length_nanos / 1000;
}

gboolean
bacon_video_widget_is_playing (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), FALSE);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), FALSE);
	
	if (gst_play_get_state(bvw->priv->play) == GST_STATE_PLAYING) {
		return TRUE;
	}

	return FALSE;
}

gboolean
bacon_video_widget_is_seekable (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), FALSE);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), FALSE);

	if (bvw->priv->play->length_nanos)
		return TRUE;
	else
		return FALSE;
}

gboolean
bacon_video_widget_can_play (BaconVideoWidget *bvw, MediaType type)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), FALSE);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), FALSE);
	return FALSE;
}

G_CONST_RETURN gchar **
bacon_video_widget_get_mrls (BaconVideoWidget *bvw, MediaType type)
{
	g_return_val_if_fail (bvw != NULL, NULL);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), NULL);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), NULL);
	return NULL;
}

void
bacon_video_widget_get_metadata (BaconVideoWidget *bvw,
		BaconVideoWidgetMetadataType type,
		GValue *value)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (GST_IS_PLAY(bvw->priv->play));
}

/* Screenshot functions */
gboolean
bacon_video_widget_can_get_frames (BaconVideoWidget *bvw, GError **error)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), FALSE);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), FALSE);
	return FALSE;
}

GdkPixbuf *
bacon_video_widget_get_current_frame (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, NULL);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET(bvw), NULL);
	g_return_val_if_fail (GST_IS_PLAY(bvw->priv->play), NULL);
	return NULL;
}

/* =========================================== */
/*                                             */
/*          Widget typing & Creation           */
/*                                             */
/* =========================================== */

GType
bacon_video_widget_get_type (void)
{
	static GType bacon_video_widget_type = 0;

	if (!bacon_video_widget_type) {
		static const GTypeInfo bacon_video_widget_info = {
			sizeof (BaconVideoWidgetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) bacon_video_widget_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (BaconVideoWidget),
			0 /* n_preallocs */,
			(GInstanceInitFunc) bacon_video_widget_instance_init,
		};

		bacon_video_widget_type = g_type_register_static
			(GTK_TYPE_VBOX, "BaconVideoWidget",
			 &bacon_video_widget_info, (GTypeFlags)0);
	}

	return bacon_video_widget_type;
}

GtkWidget *
bacon_video_widget_new (int width, int height,
		gboolean null_out, GError **err)
{
	BaconVideoWidget *bvw;
	GstElement *audio_sink, *video_sink, *vis_video_sink, *vis_element;

	bvw = BACON_VIDEO_WIDGET (g_object_new
			(bacon_video_widget_get_type (), NULL));

	//FIXME 0.6.1
	//bvw->priv->play = gst_play_new (GST_PLAY_PIPE_VIDEO_VISUALISATION, err);
	bvw->priv->play = gst_play_new (GST_PLAY_PIPE_VIDEO, err);
	//FIXME
	if (*err != NULL)
	{
		g_message ("error: %s", (*err)->message);
		return NULL;
	}

	audio_sink = gst_gconf_get_default_audio_sink ();
	if (!GST_IS_ELEMENT (audio_sink)) {
		g_message ("failed to render default audio sink from gconf");
		return NULL;
	}
	video_sink = gst_gconf_get_default_video_sink ();
	if (!GST_IS_ELEMENT (video_sink)) {
		g_message ("failed to render default video sink from gconf");
		return NULL;
	}
	vis_video_sink = gst_gconf_get_default_video_sink ();
	if (!GST_IS_ELEMENT (vis_video_sink)) {
		g_message ("failed to render default video sink from gconf for vis");
		return NULL;
	}
	/* 0.6.1 
	vis_element = gst_gconf_get_default_visualisation_element ();
	if (!GST_IS_ELEMENT (vis_element)) {
		g_message ("failed to render default visualisation element from gconf");
		return NULL;
	}*/

	gst_play_set_video_sink (bvw->priv->play, video_sink);
	gst_play_set_audio_sink (bvw->priv->play, audio_sink);
	/* 0.6.1 
	gst_play_set_visualisation_video_sink (bvw->priv->play, vis_video_sink);
	gst_play_set_visualisation_element (bvw->priv->play, vis_element);*/

	g_signal_connect (G_OBJECT (bvw->priv->play),
			"have_xid", (GtkSignalFunc) update_xid, (gpointer) bvw);
	/* 0.6.1 
	g_signal_connect (G_OBJECT (bvw->priv->play),
			"have_vis_xid", (GtkSignalFunc) update_vis_xid, (gpointer) bvw);*/
	g_signal_connect (G_OBJECT (bvw->priv->play),
			"have_video_size", (GtkSignalFunc) got_video_size, (gpointer) bvw);
	g_signal_connect (G_OBJECT (bvw->priv->play),
			"stream_end", (GtkSignalFunc) got_eos, (gpointer) bvw);
	
	bvw->priv->vw = GST_VIDEO_WIDGET(gst_video_widget_new ());
	if (!GST_IS_VIDEO_WIDGET(bvw->priv->vw)) {
		g_message ("failed to create video widget");
		return NULL;
	}
	
	gtk_box_pack_end (GTK_BOX(bvw), GTK_WIDGET(bvw->priv->vw), TRUE, TRUE, 0);
	
	gtk_widget_show (GTK_WIDGET(bvw->priv->vw));
	
	return GTK_WIDGET (bvw);
}
