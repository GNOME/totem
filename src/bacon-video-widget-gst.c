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
enum
{
  ERROR,
  EOS,
  TITLE_CHANGE,
  CHANNELS_CHANGE,
  TICK,
  GOT_METADATA,
  BUFFERING,
  SPEED_WARNING,
  LAST_SIGNAL
};

/* Enum for none-signal stuff that needs to go through the AsyncQueue */
enum
{
  RATIO = LAST_SIGNAL
};

/* Arguments */
enum
{
  PROP_0,
  PROP_LOGO_MODE,
  PROP_POSITION,
  PROP_CURRENT_TIME,
  PROP_STREAM_LENGTH,
  PROP_PLAYING,
  PROP_SEEKABLE,
  PROP_SHOWCURSOR,
  PROP_MEDIADEV,
  PROP_SHOW_VISUALS,
};

struct BaconVideoWidgetPrivate
{
  double display_ratio;

  GstPlay *play;
  GstVideoWidget *vw;

  GdkPixbuf *logo_pixbuf;

  gboolean media_has_video;

  gint64 stream_length;
  gint64 current_time_nanos;
  gint64 current_time;
  float current_position;

  GHashTable *metadata_hash;

  char *last_error_message;

  /* X stuff */
  Display *display;
  int screen;
  GdkWindow *video_window;

  /* Visual effects */
  GList *vis_plugins_list;
  char *mrl;
  gboolean show_vfx;
  gboolean using_vfx;
  GstElement *vis_element;

  /* Other stuff */
  int xpos, ypos;
  gboolean logo_mode;
  gboolean auto_resize;

  guint video_width;
  guint video_height;

  guint init_width;
  guint init_height;

  /* Signal handlers we want to keep */
  gulong vis_sig_handler;
  gboolean vis_signal_blocked;
};

static void bacon_video_widget_set_property (GObject * object,
					     guint property_id,
					     const GValue * value,
					     GParamSpec * pspec);
static void bacon_video_widget_get_property (GObject * object,
					     guint property_id,
					     GValue * value,
					     GParamSpec * pspec);

static void bacon_video_widget_finalize (GObject * object);

static GtkWidgetClass *parent_class = NULL;

static int bvw_table_signals[LAST_SIGNAL] = { 0 };

static void
bacon_video_widget_size_request (GtkWidget * widget,
				 GtkRequisition * requisition)
{
  BaconVideoWidget *bvw;
  GtkRequisition child_requisition;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (widget));

  bvw = BACON_VIDEO_WIDGET (widget);

  gtk_widget_size_request (GTK_WIDGET (bvw->priv->vw), &child_requisition);

  requisition->width = child_requisition.width;
  requisition->height = child_requisition.height;
}

static void
bacon_video_widget_size_allocate (GtkWidget * widget,
				  GtkAllocation * allocation)
{
  BaconVideoWidget *bvw;
  GtkAllocation child_allocation;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (widget));

  bvw = BACON_VIDEO_WIDGET (widget);

  widget->allocation = *allocation;

  child_allocation.x = allocation->x;
  child_allocation.y = allocation->y;
  child_allocation.width = allocation->width;
  child_allocation.height = allocation->height;

  gtk_widget_size_allocate (GTK_WIDGET (bvw->priv->vw), &child_allocation);

  if ((bvw->priv->init_width == 0) && (bvw->priv->init_height == 0))
    {
      bvw->priv->init_width = allocation->width;
      bvw->priv->init_height = allocation->height;
      gst_video_widget_set_minimum_size (bvw->priv->vw,
					 bvw->priv->init_width,
					 bvw->priv->init_height);
    }
}

static void
bacon_video_widget_class_init (BaconVideoWidgetClass * klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;

  parent_class = gtk_type_class (gtk_box_get_type ());

  /* GtkWidget */
  widget_class->size_request = bacon_video_widget_size_request;
  widget_class->size_allocate = bacon_video_widget_size_allocate;

  /* GObject */
  object_class->set_property = bacon_video_widget_set_property;
  object_class->get_property = bacon_video_widget_get_property;
  object_class->finalize = bacon_video_widget_finalize;

  /* Properties */
  g_object_class_install_property (object_class, PROP_LOGO_MODE,
				   g_param_spec_boolean ("logo_mode", NULL,
							 NULL, FALSE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_POSITION,
				   g_param_spec_int ("position", NULL, NULL,
						     0, G_MAXINT, 0,
						     G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_STREAM_LENGTH,
				   g_param_spec_int64 ("stream_length", NULL,
						     NULL, 0, G_MAXINT64, 0,
						     G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_PLAYING,
				   g_param_spec_boolean ("playing", NULL,
							 NULL, FALSE,
							 G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_SEEKABLE,
				   g_param_spec_boolean ("seekable", NULL,
							 NULL, FALSE,
							 G_PARAM_READABLE));
  g_object_class_install_property (object_class, PROP_SHOWCURSOR,
				   g_param_spec_boolean ("showcursor", NULL,
							 NULL, FALSE,
							 G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_MEDIADEV,
				   g_param_spec_string ("mediadev", NULL,
							NULL, FALSE,
							G_PARAM_WRITABLE));
  g_object_class_install_property (object_class, PROP_SHOW_VISUALS,
				   g_param_spec_boolean ("showvisuals", NULL,
							 NULL, FALSE,
							 G_PARAM_WRITABLE));

  /* Signals */
  bvw_table_signals[ERROR] =
    g_signal_new ("error",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, error),
		  NULL, NULL,
		  baconvideowidget_marshal_VOID__STRING_BOOLEAN,
		  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  bvw_table_signals[EOS] =
    g_signal_new ("eos",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, eos),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  bvw_table_signals[GOT_METADATA] =
    g_signal_new ("got-metadata",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, got_metadata),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  bvw_table_signals[TITLE_CHANGE] =
    g_signal_new ("title-change",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, title_change),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__STRING,
		  G_TYPE_NONE, 1, G_TYPE_STRING);

  bvw_table_signals[CHANNELS_CHANGE] =
    g_signal_new ("channels-change",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, channels_change),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  bvw_table_signals[TICK] =
    g_signal_new ("tick",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, tick),
		  NULL, NULL,
		  baconvideowidget_marshal_VOID__INT64_INT64_FLOAT,
		  G_TYPE_NONE, 3, G_TYPE_INT64, G_TYPE_INT64, G_TYPE_FLOAT);

  bvw_table_signals[BUFFERING] =
    g_signal_new ("buffering",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, buffering),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  bvw_table_signals[SPEED_WARNING] =
    g_signal_new ("speed-warning",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, speed_warning),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
bacon_video_widget_instance_init (BaconVideoWidget * bvw)
{
  int argc = 1;
  char **argv = NULL;

  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

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
shrink_toplevel (BaconVideoWidget * bvw)
{
  GtkWidget *toplevel;
  GtkRequisition requisition;
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (bvw));
  gtk_widget_size_request (toplevel, &requisition);
  gtk_window_resize (GTK_WINDOW (toplevel), requisition.width,
		     requisition.height);
}

static void
update_vis_xid (GstPlay * play, gint xid, BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  if ((bvw->priv->vw) && (!bvw->priv->media_has_video))
    {
      gst_video_widget_set_xembed_xid (bvw->priv->vw, xid);
      shrink_toplevel (bvw);
    }
}

static void
update_xid (GstPlay * play, gint xid, BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->priv->media_has_video = TRUE;

  gst_play_connect_visualisation (bvw->priv->play, FALSE);

  if (!bvw->priv->vis_signal_blocked)
    {
      g_signal_handler_block (G_OBJECT (bvw->priv->play),
			      bvw->priv->vis_sig_handler);
      bvw->priv->vis_signal_blocked = TRUE;
    }

  if (bvw->priv->vw)
    {
      gst_video_widget_set_xembed_xid (bvw->priv->vw, xid);
      shrink_toplevel (bvw);
    }
}

static void
got_video_size (GstPlay * play, gint width, gint height,
		BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->priv->video_width = width;
  bvw->priv->video_height = height;

  if (bvw->priv->vw)
    gst_video_widget_set_source_size (bvw->priv->vw, width, height);
}

static void
got_vis_video_size (GstPlay * play, gint width, gint height,
		    BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  if (bvw->priv->media_has_video)
    return;
  
  bvw->priv->video_width = width;
  bvw->priv->video_height = height;

  if (bvw->priv->vw)
    gst_video_widget_set_source_size (bvw->priv->vw, width, height);
}

static void
got_eos (GstPlay * play, BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_signal_emit (G_OBJECT (bvw), bvw_table_signals[EOS], 0, NULL);
}

static void
got_stream_length (GstPlay * play, gint64 length_nanos,
		   BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->priv->stream_length = (gint64) length_nanos / GST_MSECOND;
  
  g_signal_emit (G_OBJECT (bvw), bvw_table_signals[GOT_METADATA], 0, NULL);
}

static void
got_time_tick (GstPlay * play, gint64 time_nanos, BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->priv->current_time_nanos = time_nanos;

  bvw->priv->current_time = (gint64) time_nanos / GST_MSECOND;

  if (bvw->priv->stream_length == 0)
    bvw->priv->current_position = 0;
  else
    {
      bvw->priv->current_position =
	(float) bvw->priv->current_time / bvw->priv->stream_length;
    }

  g_message ("current time : %lld stream length : %lld current position : %f",
             bvw->priv->current_time, bvw->priv->stream_length, bvw->priv->current_position);
  g_signal_emit (G_OBJECT (bvw),
		 bvw_table_signals[TICK], 0,
		 bvw->priv->current_time, bvw->priv->stream_length,
		 bvw->priv->current_position);
}

static void
got_error (GstPlay * play, GstElement * orig,
	   char *error_message, BaconVideoWidget * bvw)
{
  gboolean emit = TRUE;
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  if (bvw->priv->last_error_message)
    {
      /* Let's check the latest error message */
      if (g_ascii_strcasecmp (error_message, bvw->priv->last_error_message) == 0)
	emit = FALSE;
    }

  if (emit)
    {
      g_signal_emit (G_OBJECT (bvw),
		     bvw_table_signals[ERROR], 0, error_message, TRUE);
      if (bvw->priv->last_error_message)
	g_free (bvw->priv->last_error_message);
      bvw->priv->last_error_message = g_strdup (error_message);
    }
}

static void
bacon_video_widget_finalize (GObject * object)
{
  BaconVideoWidget *bvw = (BaconVideoWidget *) object;

  if (bvw->priv->metadata_hash)
    {
      g_hash_table_destroy (bvw->priv->metadata_hash);
      bvw->priv->metadata_hash = NULL;
    }

  if (bvw->priv->vis_plugins_list)
    {
      g_list_foreach (bvw->priv->vis_plugins_list, (GFunc) g_free, NULL);
      g_list_free (bvw->priv->vis_plugins_list);
      bvw->priv->vis_plugins_list = NULL;
    }

  if (bvw->priv->play != NULL && GST_IS_PLAY (bvw->priv->play))
    gst_play_set_state (bvw->priv->play, GST_STATE_READY);

  if (bvw->priv->vw != NULL && GST_IS_VIDEO_WIDGET (bvw->priv->vw))
    {
      gtk_widget_destroy (GTK_WIDGET (bvw->priv->vw));
      bvw->priv->vw = NULL;
    }

  if ((bvw->priv->play) && G_IS_OBJECT (bvw->priv->play))
    {
      g_object_unref (bvw->priv->play);
      bvw->priv->play = NULL;
    }
}

static void
bacon_video_widget_set_property (GObject * object, guint property_id,
				 const GValue * value, GParamSpec * pspec)
{
}

static void
bacon_video_widget_get_property (GObject * object, guint property_id,
				 GValue * value, GParamSpec * pspec)
{
}

static gboolean
dummy_true_function (gpointer key, gpointer value, gpointer data)
{
  return TRUE;
}

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

char *
bacon_video_widget_get_backend_name (BaconVideoWidget * bvw)
{
  guint major, minor, micro;

  gst_version (&major, &minor, &micro);

  return g_strdup_printf ("GStreamer version %d.%d.%d", major, minor, micro);
}

int
bacon_video_widget_get_subtitle (BaconVideoWidget * bvw)
{
  return -1;
}

void
bacon_video_widget_set_subtitle (BaconVideoWidget * bvw, int subtitle)
{
}

GList * bacon_video_widget_get_subtitles (BaconVideoWidget * bvw)
{
  return NULL;
}

GList * bacon_video_widget_get_languages (BaconVideoWidget * bvw)
{
  return NULL;
}

int
bacon_video_widget_get_language (BaconVideoWidget * bvw)
{
  return -1;
}

void
bacon_video_widget_set_language (BaconVideoWidget * bvw, int language)
{
}

int
bacon_video_widget_get_connection_speed (BaconVideoWidget * bvw)
{
  return 0;
}

void
bacon_video_widget_set_connection_speed (BaconVideoWidget * bvw, int speed)
{
}

void
bacon_video_widget_set_deinterlacing (BaconVideoWidget * bvw,
				      gboolean deinterlace)
{
}

gboolean
bacon_video_widget_get_deinterlacing (BaconVideoWidget * bvw)
{
  return FALSE;
}

gboolean
bacon_video_widget_set_tv_out (BaconVideoWidget * bvw, TvOutType tvout)
{
  return FALSE;
}

TvOutType
bacon_video_widget_get_tv_out (BaconVideoWidget * bvw)
{
  return TV_OUT_NONE;
}

BaconVideoWidgetAudioOutType
bacon_video_widget_get_audio_out_type (BaconVideoWidget *bvw)
{
  return BVW_AUDIO_SOUND_STEREO;
}

void
bacon_video_widget_set_audio_out_type (BaconVideoWidget *bvw,
                                       BaconVideoWidgetAudioOutType type)
{
  
}

/* =========================================== */
/*                                             */
/*               Play/Pause, Stop              */
/*                                             */
/* =========================================== */

gboolean
bacon_video_widget_open (BaconVideoWidget * bvw, const gchar * mrl,
			 GError ** error)
{
  GstElement * datasrc = NULL;

  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (mrl != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (bvw->priv->play != NULL, FALSE);
  g_return_val_if_fail (bvw->priv->mrl == NULL, FALSE);

  bvw->priv->mrl = g_strdup (mrl);

  /* Resetting last_error_message to NULL */
  if (bvw->priv->last_error_message)
    {
      g_free (bvw->priv->last_error_message);
      bvw->priv->last_error_message = NULL;
    }

  /* Cleaning metadata hash */
  g_hash_table_foreach_remove (bvw->priv->metadata_hash,
			       dummy_true_function, bvw);

  if (bvw->priv->vis_signal_blocked)
    {
      g_signal_handler_unblock (G_OBJECT (bvw->priv->play),
				bvw->priv->vis_sig_handler);
      bvw->priv->vis_signal_blocked = FALSE;
    }

  gst_video_widget_set_source_size (GST_VIDEO_WIDGET (bvw->priv->vw), 1, 1);
  gst_play_need_new_video_window (bvw->priv->play);

  if (g_file_test (mrl, G_FILE_TEST_EXISTS))
    {
      datasrc = gst_element_factory_make ("filesrc", "source");
    }
  else
    {
      datasrc = gst_element_factory_make ("gnomevfssrc", "source");
    }

  if (GST_IS_ELEMENT (datasrc))
    gst_play_set_data_src (bvw->priv->play, datasrc);

  bvw->priv->media_has_video = FALSE;
  bvw->priv->stream_length = 0;

  return gst_play_set_location (bvw->priv->play, mrl);;
}

gboolean
bacon_video_widget_play (BaconVideoWidget * bvw, GError ** error)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);

  /* Resetting last_error_message to NULL */
  if (bvw->priv->last_error_message)
    {
      g_free (bvw->priv->last_error_message);
      bvw->priv->last_error_message = NULL;
    }

  gst_play_set_state (bvw->priv->play, GST_STATE_PLAYING);

  return TRUE;
}

gboolean bacon_video_widget_seek (BaconVideoWidget *bvw, float position,
		GError **gerror)
{
  gint64 seek_time, length_nanos;

  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);

  /* Resetting last_error_message to NULL */
  if (bvw->priv->last_error_message)
    {
      g_free (bvw->priv->last_error_message);
      bvw->priv->last_error_message = NULL;
    }

  length_nanos = (gint64) (bvw->priv->stream_length * GST_MSECOND);
  seek_time = (gint64) (length_nanos * position);
  gst_play_seek_to_time (bvw->priv->play, seek_time);

  return TRUE;
}

gboolean bacon_video_widget_seek_time (BaconVideoWidget *bvw, gint64 time,
		GError **gerror)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);

  /* Resetting last_error_message to NULL */
  if (bvw->priv->last_error_message)
    {
      g_free (bvw->priv->last_error_message);
      bvw->priv->last_error_message = NULL;
    }

  gst_play_seek_to_time (bvw->priv->play, time);

  return TRUE;
}

void
bacon_video_widget_stop (BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));

  gst_play_set_state (bvw->priv->play, GST_STATE_READY);
}

void
bacon_video_widget_close (BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));

  gst_play_set_state (bvw->priv->play, GST_STATE_READY);
  gst_play_set_location (bvw->priv->play, "/dev/null");
  g_free (bvw->priv->mrl);
  bvw->priv->mrl = NULL;
}

void
bacon_video_widget_dvd_event (BaconVideoWidget * bvw,
			      BaconVideoWidgetDVDEvent type)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));
}

void
bacon_video_widget_set_logo (BaconVideoWidget * bvw, gchar * filename)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_VIDEO_WIDGET (bvw->priv->vw));

  bvw->priv->logo_pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

  gst_video_widget_set_logo (bvw->priv->vw, bvw->priv->logo_pixbuf);
}

void
bacon_video_widget_set_logo_mode (BaconVideoWidget * bvw, gboolean logo_mode)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_VIDEO_WIDGET (bvw->priv->vw));
  gst_video_widget_set_logo_focus (bvw->priv->vw, TRUE);
}

gboolean
bacon_video_widget_get_logo_mode (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_VIDEO_WIDGET (bvw->priv->vw), FALSE);
  return gst_video_widget_get_logo_focus (bvw->priv->vw);
}

void
bacon_video_widget_pause (BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));

  gst_play_set_state (bvw->priv->play, GST_STATE_PAUSED);
}

void
bacon_video_widget_set_proprietary_plugins_path (BaconVideoWidget * bvw,
						 const char *path)
{
}

gboolean
bacon_video_widget_can_set_volume (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);
  return TRUE;
}

void
bacon_video_widget_set_volume (BaconVideoWidget * bvw, int volume)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));
  gst_play_set_volume (bvw->priv->play, (gfloat) volume / 100);
}

int
bacon_video_widget_get_volume (BaconVideoWidget * bvw)
{
  int volume;
  g_return_val_if_fail (bvw != NULL, -1);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), -1);
  volume = gst_play_get_volume (bvw->priv->play) * 100;
  return volume;
}

void
bacon_video_widget_set_fullscreen (BaconVideoWidget * bvw,
				   gboolean fullscreen)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  
  if (bvw->priv->vw)
    {
      gst_video_widget_set_scale_override (GST_VIDEO_WIDGET
					   (bvw->priv->vw),
					   FALSE);
    }
}

void
bacon_video_widget_set_show_cursor (BaconVideoWidget * bvw,
				    gboolean use_cursor)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_VIDEO_WIDGET (bvw->priv->vw));
  gst_video_widget_set_cursor_visible (bvw->priv->vw, use_cursor);
}

gboolean
bacon_video_widget_get_show_cursor (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_VIDEO_WIDGET (bvw->priv->vw), FALSE);
  return gst_video_widget_get_cursor_visible (bvw->priv->vw);
}

void
bacon_video_widget_set_media_device (BaconVideoWidget * bvw, const char *path)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));
}

gboolean
bacon_video_widget_set_show_visuals (BaconVideoWidget * bvw,
				     gboolean show_visuals)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);

  if (!bvw->priv->media_has_video)
    {
      if (!show_visuals)
	gst_video_widget_set_logo_focus (bvw->priv->vw, TRUE);
      else
	gst_video_widget_set_logo_focus (bvw->priv->vw, FALSE);

      gst_play_connect_visualisation (bvw->priv->play, show_visuals);
    }

  return TRUE;
}

GList *
bacon_video_widget_get_visuals_list (BaconVideoWidget * bvw)
{
  GList *pool_registries = NULL, *registries = NULL, *vis_plugins_list = NULL;
  GList *plugins = NULL, *features = NULL;

  g_return_val_if_fail (bvw != NULL, NULL);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), NULL);

  /* Cache */
  if (bvw->priv->vis_plugins_list)
    {
      return bvw->priv->vis_plugins_list;
    }

  pool_registries = gst_registry_pool_list ();

  registries = pool_registries;

  while (registries)
    {
      GstRegistry *registry = GST_REGISTRY (registries->data);

      plugins = registry->plugins;

      while (plugins)
	{
	  GstPlugin *plugin = GST_PLUGIN (plugins->data);

	  features = gst_plugin_get_feature_list (plugin);

	  while (features)
	    {
	      GstPluginFeature *feature = GST_PLUGIN_FEATURE (features->data);

	      if (GST_IS_ELEMENT_FACTORY (feature))
		{
		  GstElementFactory *factory = GST_ELEMENT_FACTORY (feature);
		  if (g_strrstr (factory->details->klass, "Visualization"))
		    {
		      vis_plugins_list = g_list_append (vis_plugins_list,
							(char *)
							g_strdup
							(GST_OBJECT_NAME
							 (factory)));
		    }
		}
	      features = g_list_next (features);
	    }

	  plugins = g_list_next (plugins);
	}

      registries = g_list_next (registries);
    }

  g_list_free (pool_registries);
  pool_registries = NULL;

  bvw->priv->vis_plugins_list = vis_plugins_list;

  return vis_plugins_list;
}

gboolean
bacon_video_widget_set_visuals (BaconVideoWidget * bvw, const char *name)
{
  gboolean paused = FALSE;

  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);

  if (gst_play_get_state (bvw->priv->play) == GST_STATE_PLAYING)
    {
      gst_play_set_state (bvw->priv->play, GST_STATE_PAUSED);
      paused = TRUE;
    }

  bvw->priv->vis_element =
    gst_element_factory_make (name, "vis_plugin_element");
  if (GST_IS_ELEMENT (bvw->priv->vis_element))
    {
      gst_play_set_visualisation_element (bvw->priv->play,
					  bvw->priv->vis_element);
      if (paused)
	{
	  gst_play_seek_to_time (bvw->priv->play,
				 bvw->priv->current_time_nanos);
	  gst_play_set_state (bvw->priv->play, GST_STATE_PLAYING);
	}

      return FALSE;
    }
  else
    {
      return FALSE;
    }
}

void
bacon_video_widget_set_visuals_quality (BaconVideoWidget * bvw,
					VisualsQuality quality)
{
  int fps, w, h;
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));

  if (bvw->priv->vis_element == NULL)
    return;

  switch (quality)
    {
    case VISUAL_SMALL:
      fps = 15;
      w = 320;
      h = 240;
      break;
    case VISUAL_NORMAL:
      fps = 25;
      w = 320;
      h = 240;
      break;
    case VISUAL_LARGE:
      fps = 25;
      w = 640;
      h = 480;
      break;
    case VISUAL_EXTRA_LARGE:
      fps = 30;
      w = 800;
      h = 600;
      break;
    default:
      /* shut up warnings */
      fps = w = h = 0;
      g_assert_not_reached ();
    }

  g_object_set (G_OBJECT (bvw->priv->vis_element),
		"width", w, "height", h, "fps", fps, NULL);

}

gboolean
bacon_video_widget_get_auto_resize (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);
  g_return_val_if_fail (GST_IS_VIDEO_WIDGET (bvw->priv->vw), FALSE);

  return gst_video_widget_get_auto_resize (bvw->priv->vw);
}

void
bacon_video_widget_set_auto_resize (BaconVideoWidget * bvw,
				    gboolean auto_resize)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));
  g_return_if_fail (GST_IS_VIDEO_WIDGET (bvw->priv->vw));

  gst_video_widget_set_auto_resize (bvw->priv->vw, auto_resize);
}

void
bacon_video_widget_toggle_aspect_ratio (BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));
}

void
bacon_video_widget_set_scale_ratio (BaconVideoWidget * bvw, gfloat ratio)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));
  g_return_if_fail (GST_IS_VIDEO_WIDGET (bvw->priv->vw));

  gst_video_widget_set_scale (bvw->priv->vw, ratio);
  gst_video_widget_set_scale_override (bvw->priv->vw, TRUE);
  shrink_toplevel (bvw);
}

int
bacon_video_widget_get_video_property (BaconVideoWidget *bvw,
		                BaconVideoWidgetVideoProperty type)
{
	g_return_val_if_fail (bvw != NULL, 65535 / 2);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 65535 / 2);

	//FIXME
	return 65535 / 2;
}

void
bacon_video_widget_set_video_property (BaconVideoWidget *bvw,
		                BaconVideoWidgetVideoProperty type, int value)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail ((value < 65535 && value > 0));

	//FIXME
}

float
bacon_video_widget_get_position (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, -1);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
  return bvw->priv->current_position;
}

gint64
bacon_video_widget_get_current_time (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, -1);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
  return bvw->priv->current_time;
}

gint64
bacon_video_widget_get_stream_length (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, -1);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
  return bvw->priv->stream_length;
}

gboolean
bacon_video_widget_is_playing (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);

  if (gst_play_get_state (bvw->priv->play) == GST_STATE_PLAYING)
    return TRUE;

  return FALSE;
}

gboolean
bacon_video_widget_is_seekable (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);

  if (bvw->priv->stream_length)
    return TRUE;
  else
    return FALSE;
}

gboolean
bacon_video_widget_can_play (BaconVideoWidget * bvw, MediaType type)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);
  
  switch (type)
    {
      case MEDIA_DVD:
        return FALSE;
      case MEDIA_VCD:
        return FALSE;
      case MEDIA_CDDA:
        return FALSE;
      default:
        return FALSE;
    }
}

G_CONST_RETURN gchar **
bacon_video_widget_get_mrls (BaconVideoWidget * bvw, MediaType type)
{
  g_return_val_if_fail (bvw != NULL, NULL);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), NULL);
  return NULL;
}

static void
bacon_video_widget_information (GstPlay * play,
				GstObject * object,
				GParamSpec * param, BaconVideoWidget * bvw)
{
  GValue value = { 0, };
  GstCaps *metadata;
  GstProps *props;
  GList *p;

  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));
  g_return_if_fail (object != NULL);

  if (strcmp (param->name, "metadata") == 0)
    {
      g_value_init (&value, param->value_type);
      g_object_get_property (G_OBJECT (object), param->name, &value);
      metadata = g_value_peek_pointer (&value);
      props = gst_caps_get_props (metadata);
      p = props->properties;
      while (p)
	{
	  const gchar *name;
	  const gchar *val;
	  GstPropsEntry *entry = (GstPropsEntry *) p->data;
	  name = gst_props_entry_get_name (entry);
	  gst_props_entry_get_string (entry, &val);
	  g_hash_table_replace (bvw->priv->metadata_hash,
				g_ascii_strdown (name, -1), (char *) val);
	  p = g_list_next (p);
	}
      g_signal_emit (G_OBJECT (bvw), bvw_table_signals[GOT_METADATA], 0,
		     NULL);
    }
}

static void
bacon_video_widget_get_metadata_string (BaconVideoWidget * bvw,
					BaconVideoWidgetMetadataType type,
					GValue * value)
{
  const char *string = NULL;

  g_value_init (value, G_TYPE_STRING);

  if (bvw->priv->play == NULL)
    {
      g_value_set_string (value, "");
      return;
    }

  switch (type)
    {
    case BVW_INFO_TITLE:
      string = g_hash_table_lookup (bvw->priv->metadata_hash, "title");
      break;
    case BVW_INFO_ARTIST:
      string = g_hash_table_lookup (bvw->priv->metadata_hash, "artist");
      break;
    case BVW_INFO_YEAR:
      string = g_hash_table_lookup (bvw->priv->metadata_hash, "year");
      break;
    case BVW_INFO_VIDEO_CODEC:
      string = g_hash_table_lookup (bvw->priv->metadata_hash, "video-codec");
      break;
    case BVW_INFO_AUDIO_CODEC:
      string = g_hash_table_lookup (bvw->priv->metadata_hash, "audio-codec");
      break;
    default:
      g_assert_not_reached ();
    }

  g_value_set_string (value, string);

  return;
}

static void
bacon_video_widget_get_metadata_int (BaconVideoWidget * bvw,
				     BaconVideoWidgetMetadataType type,
				     GValue * value)
{
  int integer = 0;

  g_value_init (value, G_TYPE_INT);

  if (bvw->priv->play == NULL)
    {
      g_value_set_int (value, 0);
      return;
    }

  switch (type)
    {
    case BVW_INFO_DURATION:
      integer = bacon_video_widget_get_stream_length (bvw) / 1000;
      break;
    case BVW_INFO_DIMENSION_X:
      integer = bvw->priv->video_width;
      break;
    case BVW_INFO_DIMENSION_Y:
      integer = bvw->priv->video_height;
      break;
    case BVW_INFO_FPS:
      integer = 0;
      break;
    case BVW_INFO_BITRATE:
      integer = 0;
      break;
    default:
      g_assert_not_reached ();
    }

  g_value_set_int (value, integer);

  return;
}

static void
bacon_video_widget_get_metadata_bool (BaconVideoWidget * bvw,
				      BaconVideoWidgetMetadataType type,
				      GValue * value)
{
  gboolean boolean = FALSE;

  g_value_init (value, G_TYPE_BOOLEAN);

  if (bvw->priv->play == NULL)
    {
      g_value_set_boolean (value, FALSE);
      return;
    }

  switch (type)
    {
    case BVW_INFO_HAS_VIDEO:
      boolean = bvw->priv->media_has_video;
      break;
    case BVW_INFO_HAS_AUDIO:
      boolean = TRUE;
      break;
    default:
      g_assert_not_reached ();
    }

  g_value_set_boolean (value, boolean);

  return;
}

void
bacon_video_widget_get_metadata (BaconVideoWidget * bvw,
				 BaconVideoWidgetMetadataType type,
				 GValue * value)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_PLAY (bvw->priv->play));

  switch (type)
    {
    case BVW_INFO_TITLE:
    case BVW_INFO_ARTIST:
    case BVW_INFO_YEAR:
    case BVW_INFO_VIDEO_CODEC:
    case BVW_INFO_AUDIO_CODEC:
      bacon_video_widget_get_metadata_string (bvw, type, value);
      break;
    case BVW_INFO_DURATION:
    case BVW_INFO_DIMENSION_X:
    case BVW_INFO_DIMENSION_Y:
    case BVW_INFO_FPS:
    case BVW_INFO_BITRATE:
      bacon_video_widget_get_metadata_int (bvw, type, value);
      break;
    case BVW_INFO_HAS_VIDEO:
    case BVW_INFO_HAS_AUDIO:
      bacon_video_widget_get_metadata_bool (bvw, type, value);
      break;
    default:
      g_assert_not_reached ();
    }

  return;
}

/* Screenshot functions */
gboolean
bacon_video_widget_can_get_frames (BaconVideoWidget * bvw, GError ** error)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), FALSE);
  return FALSE;
}

GdkPixbuf *
bacon_video_widget_get_current_frame (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, NULL);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (GST_IS_PLAY (bvw->priv->play), NULL);
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

  if (!bacon_video_widget_type)
    {
      static const GTypeInfo bacon_video_widget_info = {
	sizeof (BaconVideoWidgetClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
	(GClassInitFunc) bacon_video_widget_class_init,
	(GClassFinalizeFunc) NULL,
	NULL /* class_data */ ,
	sizeof (BaconVideoWidget),
	0 /* n_preallocs */ ,
	(GInstanceInitFunc) bacon_video_widget_instance_init,
      };

      bacon_video_widget_type = g_type_register_static
	(GTK_TYPE_BOX, "BaconVideoWidget",
	 &bacon_video_widget_info, (GTypeFlags) 0);
    }

  return bacon_video_widget_type;
}

GtkWidget *
bacon_video_widget_new (int width, int height,
			gboolean null_out, GError ** err)
{
  BaconVideoWidget *bvw;
  GstElement *audio_sink, *video_sink, *vis_video_sink;

  bvw = BACON_VIDEO_WIDGET (g_object_new
			    (bacon_video_widget_get_type (), NULL));

  bvw->priv->metadata_hash =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  bvw->priv->play = gst_play_new (GST_PLAY_PIPE_VIDEO_VISUALISATION, err);

  bvw->priv->init_width = bvw->priv->init_height = 0;

  //FIXME
  if (*err != NULL)
    {
      g_message ("error: %s", (*err)->message);
      return NULL;
    }

  audio_sink = gst_gconf_get_default_audio_sink ();
  if (!GST_IS_ELEMENT (audio_sink))
    {
      g_message ("failed to render default audio sink from gconf");
      return NULL;
    }
  video_sink = gst_gconf_get_default_video_sink ();
  if (!GST_IS_ELEMENT (video_sink))
    {
      g_message ("failed to render default video sink from gconf");
      return NULL;
    }
  vis_video_sink = gst_gconf_get_default_video_sink ();
  if (!GST_IS_ELEMENT (vis_video_sink))
    {
      g_message ("failed to render default video sink from gconf for vis");
      return NULL;
    }

  gst_play_set_video_sink (bvw->priv->play, video_sink);
  gst_play_set_audio_sink (bvw->priv->play, audio_sink);
  gst_play_set_visualisation_video_sink (bvw->priv->play, vis_video_sink);

  bvw->priv->vis_plugins_list = NULL;

  bvw->priv->vis_sig_handler = g_signal_connect (G_OBJECT (bvw->priv->play),
						 "have_vis_xid",
						 (GtkSignalFunc)
						 update_vis_xid,
						 (gpointer) bvw);
  bvw->priv->vis_signal_blocked = FALSE;


  g_signal_connect (G_OBJECT (bvw->priv->play),
		    "have_xid", (GtkSignalFunc) update_xid, (gpointer) bvw);
  g_signal_connect (G_OBJECT (bvw->priv->play),
		    "have_video_size", (GtkSignalFunc) got_video_size,
		    (gpointer) bvw);
  g_signal_connect (G_OBJECT (bvw->priv->play),
		    "have_vis_size", (GtkSignalFunc) got_vis_video_size,
		    (gpointer) bvw);
  g_signal_connect (G_OBJECT (bvw->priv->play), "stream_end",
		    (GtkSignalFunc) got_eos, (gpointer) bvw);
  g_signal_connect (G_OBJECT (bvw->priv->play), "stream_length",
		    (GtkSignalFunc) got_stream_length, (gpointer) bvw);
  g_signal_connect (G_OBJECT (bvw->priv->play), "information",
		    (GtkSignalFunc) bacon_video_widget_information,
		    (gpointer) bvw);
  g_signal_connect (G_OBJECT (bvw->priv->play), "time_tick",
		    (GtkSignalFunc) got_time_tick, (gpointer) bvw);
  g_signal_connect (G_OBJECT (bvw->priv->play), "pipeline_error",
		    (GtkSignalFunc) got_error, (gpointer) bvw);

  bvw->priv->vw = GST_VIDEO_WIDGET (gst_video_widget_new ());
  if (!GST_IS_VIDEO_WIDGET (bvw->priv->vw))
    {
      g_message ("failed to create video widget");
      return NULL;
    }

  gtk_box_pack_end (GTK_BOX (bvw), GTK_WIDGET (bvw->priv->vw), TRUE, TRUE, 0);

  gtk_widget_show (GTK_WIDGET (bvw->priv->vw));

  return GTK_WIDGET (bvw);
}
