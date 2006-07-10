/* 
 * Copyright (C) 2003-2006 the GStreamer project
 *      Julien Moutte <julien@moutte.net>
 *      Ronald Bultje <rbultje@ronald.bitfreak.net>
 *      Tim-Philipp Müller <tim centricular net>
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
 * permission is above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exemption clause.
 * See license_change file for details.
 *
 */

#include <config.h>

#ifdef HAVE_NVTV
#include <nvtv_simple.h>
#endif 

#include <gst/gst.h>

/* GStreamer Interfaces */
#include <gst/interfaces/xoverlay.h>
#include <gst/interfaces/navigation.h>
#include <gst/interfaces/colorbalance.h>
/* for detecting sources of errors */
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>
#include <gst/audio/gstbaseaudiosink.h>

/* system */
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

/* gtk+/gnome */
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

#include "bacon-video-widget.h"
#include "baconvideowidget-marshal.h"
#include "video-utils.h"
#include "gstscreenshot.h"

#define DEFAULT_HEIGHT 420
#define DEFAULT_WIDTH  315

/* Signals */
enum
{
  SIGNAL_ERROR,
  SIGNAL_EOS,
  SIGNAL_REDIRECT,
  SIGNAL_TITLE_CHANGE,
  SIGNAL_CHANNELS_CHANGE,
  SIGNAL_TICK,
  SIGNAL_GOT_METADATA,
  SIGNAL_BUFFERING,
  SIGNAL_SPEED_WARNING,
  LAST_SIGNAL
};

/* Properties */
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
  PROP_SHOW_VISUALS
};

static const gchar *video_props_str[4] = {
  GCONF_PREFIX "/brightness",
  GCONF_PREFIX "/contrast",
  GCONF_PREFIX "/saturation",
  GCONF_PREFIX "/hue"
};

struct BaconVideoWidgetPrivate
{
  BaconVideoWidgetAspectRatio  ratio_type;

  GstElement                  *play;
  GstXOverlay                 *xoverlay; /* protect with lock */
  GstColorBalance             *balance;  /* protect with lock */
  GMutex                      *lock;

  guint                        update_id;

  GdkPixbuf                   *logo_pixbuf;

  gboolean                     media_has_video;
  gboolean                     media_has_audio;
  gint64                       stream_length;
  gint64                       current_time_nanos;
  gint64                       current_time;
  gfloat                       current_position;

  GstTagList                  *tagcache;
  GstTagList                  *audiotags;
  GstTagList                  *videotags;

  gboolean                     got_redirect;

  GdkWindow                   *video_window;
  GtkAllocation                video_window_allocation;

  /* Visual effects */
  GList                       *vis_plugins_list;
  gboolean                     show_vfx;
  gboolean                     vis_changed;
  VisualsQuality               visq;
  gchar                       *vis_element_name;
  GstElement                  *audio_capsfilter;

  /* Other stuff */
  gint                         xpos, ypos;
  gboolean                     logo_mode;
  gboolean                     cursor_shown;
  gboolean                     fullscreen_mode;
  gboolean                     auto_resize;
  
  gint                         video_width; /* Movie width */
  gint                         video_height; /* Movie height */
  const GValue                *movie_par; /* Movie pixel aspect ratio */
  gint                         video_width_pixels; /* Scaled movie width */
  gint                         video_height_pixels; /* Scaled movie height */
  gint                         video_fps_n;
  gint                         video_fps_d;

  guint                        init_width;
  guint                        init_height;
  
  gchar                       *mrl;
  gchar                       *media_device;

  BaconVideoWidgetAudioOutType speakersetup;
  TvOutType                    tv_out_type;
  gint                         connection_speed;

  GstMessageType               ignore_messages_mask;

  GConfClient                 *gc;

  GstBus                      *bus;
  gulong                       sig_bus_sync;
  gulong                       sig_bus_async;

  BvwUseType                   use_type;

  gint                         eos_id;
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

static void bvw_update_interface_implementations (BaconVideoWidget *bvw);
static void setup_vis (BaconVideoWidget * bvw);
static GList * get_visualization_features (void);
static gboolean bacon_video_widget_configure_event (GtkWidget *widget,
    GdkEventConfigure *event, BaconVideoWidget *bvw);
static void bvw_process_pending_tag_messages (BaconVideoWidget * bvw);
static void bvw_stop_play_pipeline (BaconVideoWidget * bvw);

static GtkWidgetClass *parent_class = NULL;

static int bvw_signals[LAST_SIGNAL] = { 0 };

GST_DEBUG_CATEGORY (_totem_gst_debug_cat);
#define GST_CAT_DEFAULT _totem_gst_debug_cat

static void
get_media_size (BaconVideoWidget *bvw, gint *width, gint *height)
{
  if (bvw->priv->logo_mode) {
    if (bvw->priv->logo_pixbuf) {
      *width = gdk_pixbuf_get_width (bvw->priv->logo_pixbuf);
      *height = gdk_pixbuf_get_height (bvw->priv->logo_pixbuf);
    } else {
      *width = 0;
      *height = 0;
    }
  } else {
    if (bvw->priv->media_has_video) {
      GValue * disp_par = NULL;
      guint movie_par_n, movie_par_d, disp_par_n, disp_par_d, num, den;
      
      /* Create and init the fraction value */
      disp_par = g_new0 (GValue, 1);
      g_value_init (disp_par, GST_TYPE_FRACTION);
    
      /* Now try getting display's pixel aspect ratio */
      if (bvw->priv->xoverlay) {
        GValue disp_par_string = { 0, };
      
        /* Init the string value */
        g_value_init (&disp_par_string, G_TYPE_STRING);
        /* Get the string GValue from the video sink */
        g_object_get_property (G_OBJECT (bvw->priv->xoverlay),
            "pixel-aspect-ratio", &disp_par_string);
        
        /* Try transforming */
        if (!g_value_transform (&disp_par_string, disp_par)) {
          gst_value_set_fraction (disp_par, 1, 1);
        }
        
        /* Free the string PAR */
        g_value_unset (&disp_par_string);
      }
      else {
        /* Square pixel is our default */
        gst_value_set_fraction (disp_par, 1, 1);
      }
      
      disp_par_n = gst_value_get_fraction_numerator (disp_par);
      disp_par_d = gst_value_get_fraction_denominator (disp_par);
      
      GST_DEBUG ("display PAR is %d/%d", disp_par_n, disp_par_d);
      
      /* If movie pixel aspect ratio is enforced, use that */
      if (bvw->priv->ratio_type != BVW_RATIO_AUTO) {
        switch (bvw->priv->ratio_type) {
          case BVW_RATIO_SQUARE:
            movie_par_n = 1;
            movie_par_d = 1;
            break;
          case BVW_RATIO_FOURBYTHREE:
            movie_par_n = 4 * bvw->priv->video_height;
            movie_par_d = 3 * bvw->priv->video_width;
            break;
          case BVW_RATIO_ANAMORPHIC:
            movie_par_n = 16 * bvw->priv->video_height;
            movie_par_d = 9 * bvw->priv->video_width;
            break;
          case BVW_RATIO_DVB:
            movie_par_n = 20 * bvw->priv->video_height;
            movie_par_d = 9 * bvw->priv->video_width;
            break;
          /* handle these to avoid compiler warnings */
          case BVW_RATIO_AUTO:
          default:
            movie_par_n = 0;
            movie_par_d = 0;
            g_assert_not_reached ();
        }
      }
      else {
        /* Use the movie pixel aspect ratio if any */
        if (bvw->priv->movie_par) {
          movie_par_n = gst_value_get_fraction_numerator (bvw->priv->movie_par);
          movie_par_d =
              gst_value_get_fraction_denominator (bvw->priv->movie_par);
        }
        else {
          /* Square pixels */
          movie_par_n = 1;
          movie_par_d = 1;
        }
      }
      
      GST_DEBUG ("movie PAR is %d/%d", movie_par_n, movie_par_d);

      if (!gst_video_calculate_display_ratio (&num, &den,
          bvw->priv->video_width, bvw->priv->video_height,
          movie_par_n, movie_par_d, disp_par_n, disp_par_d)) {
        GST_WARNING ("overflow calculating display aspect ratio!");
        num = 1;   /* FIXME: what values to use here? */
        den = 1;
      }

      GST_DEBUG ("calculated scaling ratio %d/%d for video %dx%d", num, den,
          bvw->priv->video_width, bvw->priv->video_height);
      
      /* now find a width x height that respects this display ratio.
       * prefer those that have one of w/h the same as the incoming video
       * using wd / hd = num / den */
    
      /* start with same height, because of interlaced video */
      /* check hd / den is an integer scale factor, and scale wd with the PAR */
      if (bvw->priv->video_height % den == 0) {
        GST_DEBUG ("keeping video height");
        bvw->priv->video_width_pixels =
            (guint) gst_util_uint64_scale (bvw->priv->video_height, num, den);
        bvw->priv->video_height_pixels = bvw->priv->video_height;
      } else if (bvw->priv->video_width % num == 0) {
        GST_DEBUG ("keeping video width");
        bvw->priv->video_width_pixels = bvw->priv->video_width;
        bvw->priv->video_height_pixels =
            (guint) gst_util_uint64_scale (bvw->priv->video_width, den, num);
      } else {
        GST_DEBUG ("approximating while keeping video height");
        bvw->priv->video_width_pixels =
            (guint) gst_util_uint64_scale (bvw->priv->video_height, num, den);
        bvw->priv->video_height_pixels = bvw->priv->video_height;
      }
      GST_DEBUG ("scaling to %dx%d", bvw->priv->video_width_pixels,
          bvw->priv->video_height_pixels);
      
      *width = bvw->priv->video_width_pixels;
      *height = bvw->priv->video_height_pixels;
      
      /* Free the PAR fraction */
      g_value_unset (disp_par);
      g_free (disp_par);
    }
    else {
      *width = 0;
      *height = 0;
    }
  }
}

static void
bacon_video_widget_realize (GtkWidget * widget)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);
  GdkWindowAttr attributes;
  gint attributes_mask, w, h;
  GdkColor colour;

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
  attributes.event_mask |= GDK_EXPOSURE_MASK |
                           GDK_POINTER_MOTION_MASK |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_KEY_PRESS_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
      &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  /* Creating our video window */
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = 0;
  attributes.y = 0;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK |
                           GDK_POINTER_MOTION_MASK |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_KEY_PRESS_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  bvw->priv->video_window = gdk_window_new (widget->window,
      &attributes, attributes_mask);
  gdk_window_set_user_data (bvw->priv->video_window, widget);

  gdk_color_parse ("black", &colour);
  gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &colour);
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gdk_window_set_background (bvw->priv->video_window, &colour);
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  
  /* Connect to configure event on the top level window */
  g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)),
      "configure-event", G_CALLBACK (bacon_video_widget_configure_event), bvw);

  /* nice hack to show the logo fullsize, while still being resizable */
  get_media_size (BACON_VIDEO_WIDGET (widget), &w, &h);
  totem_widget_set_preferred_size (widget, w, h);

#ifdef HAVE_NVTV
  if (!(nvtv_simple_init() && nvtv_enable_autoresize(TRUE))) {
    nvtv_simple_enable(FALSE);
  } 
#endif

}

static void
bacon_video_widget_unrealize (GtkWidget *widget)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);

#ifdef HAVE_NVTV
  /* Kill the TV out */
  nvtv_simple_exit();
#endif

  gdk_window_set_user_data (bvw->priv->video_window, NULL);
  gdk_window_destroy (bvw->priv->video_window);
  bvw->priv->video_window = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
bacon_video_widget_show (GtkWidget *widget)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);

  if (widget->window)
    gdk_window_show (widget->window);
  if (bvw->priv->video_window)
    gdk_window_show (bvw->priv->video_window);

  if (GTK_WIDGET_CLASS (parent_class)->show)
    GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
bacon_video_widget_hide (GtkWidget *widget)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);

  if (widget->window)
    gdk_window_hide (widget->window);
  if (bvw->priv->video_window)
    gdk_window_hide (bvw->priv->video_window);

  if (GTK_WIDGET_CLASS (parent_class)->hide)
    GTK_WIDGET_CLASS (parent_class)->hide (widget);
}

static gboolean
bacon_video_widget_configure_event (GtkWidget *widget, GdkEventConfigure *event,
    BaconVideoWidget *bvw)
{
  GstXOverlay *xoverlay = NULL;
  
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  
  xoverlay = bvw->priv->xoverlay;
  
  if (xoverlay != NULL && GST_IS_X_OVERLAY (xoverlay)) {
    gst_x_overlay_expose (xoverlay);
  }
  
  return FALSE;
}

static gboolean
bacon_video_widget_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);
  GstXOverlay *xoverlay;
  gboolean draw_logo;
  XID window;

  if (event && event->count > 0)
    return TRUE;

  g_mutex_lock (bvw->priv->lock);
  xoverlay = bvw->priv->xoverlay;
  if (xoverlay == NULL) {
    bvw_update_interface_implementations (bvw);
    xoverlay = bvw->priv->xoverlay;
  }
  if (xoverlay != NULL)
    gst_object_ref (xoverlay);

  g_mutex_unlock (bvw->priv->lock);

  window = GDK_WINDOW_XWINDOW (bvw->priv->video_window);

  if (xoverlay != NULL && GST_IS_X_OVERLAY (xoverlay))
    gst_x_overlay_set_xwindow_id (xoverlay, window);

  /* Start with a nice black canvas */
  gdk_draw_rectangle (widget->window, widget->style->black_gc, TRUE, 0, 0,
      widget->allocation.width, widget->allocation.height);

  /* if there's only audio and no visualisation, draw the logo as well */
  draw_logo = bvw->priv->media_has_audio &&
      !bvw->priv->media_has_video && !bvw->priv->show_vfx;

  if (bvw->priv->logo_mode || draw_logo) {
    if (bvw->priv->logo_pixbuf != NULL) {
      /* draw logo here */
      GdkPixbuf *logo = NULL;
      gint s_width, s_height, w_width, w_height;
      gfloat ratio;
      
      s_width = gdk_pixbuf_get_width (bvw->priv->logo_pixbuf);
      s_height = gdk_pixbuf_get_height (bvw->priv->logo_pixbuf);
      w_width = widget->allocation.width;
      w_height = widget->allocation.height;
      
      if ((gfloat) w_width / s_width > (gfloat) w_height / s_height) {
        ratio = (gfloat) w_height / s_height;
      } else {
        ratio = (gfloat) w_width / s_width;
      }
      
      s_width *= ratio;
      s_height *= ratio;
      
      logo = gdk_pixbuf_scale_simple (bvw->priv->logo_pixbuf,
          s_width, s_height, GDK_INTERP_BILINEAR);

      gdk_draw_pixbuf (widget->window, widget->style->fg_gc[0], logo,
          0, 0, (w_width - s_width) / 2, (w_height - s_height) / 2,
          s_width, s_height, GDK_RGB_DITHER_NONE, 0, 0);

      gdk_pixbuf_unref (logo);
    } else if (widget->window) {
      /* No pixbuf, just draw a black background then */
      gdk_draw_rectangle (widget->window, widget->style->black_gc, 
                         TRUE, 0, 0,
                         widget->allocation.width,
                         widget->allocation.height);
    }
  } else {
    /* no logo, pass the expose to gst */
    if (xoverlay != NULL && GST_IS_X_OVERLAY (xoverlay))
      gst_x_overlay_expose (xoverlay);
    else {
      /* No xoverlay to expose yet */
      gdk_draw_rectangle (bvw->priv->video_window, widget->style->black_gc, 
                         TRUE, 0, 0,
                         bvw->priv->video_window_allocation.width,
                         bvw->priv->video_window_allocation.height);
    }
  }
  if (xoverlay != NULL)
    gst_object_unref (xoverlay);

  return TRUE;
}

/* need to use gstnavigation interface for these vmethods, to allow for the sink
   to map screen coordinates to video coordinates in the presence of e.g.
   hardware scaling */

static gboolean
bacon_video_widget_motion_notify (GtkWidget *widget,
				  GdkEventMotion *event)
{
  gboolean res = FALSE;
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);

  g_return_val_if_fail (bvw->priv->play != NULL, FALSE);

  if (!bvw->priv->logo_mode) {
    GstElement *videosink = NULL;

    g_object_get (bvw->priv->play, "video-sink", &videosink, NULL);

    if (videosink && GST_IS_BIN (videosink)) {
      GstElement *newvideosink;
      newvideosink = gst_bin_get_by_interface (GST_BIN (videosink),
					       GST_TYPE_NAVIGATION);
      gst_object_unref (videosink);
      videosink = newvideosink;
    }

    if (videosink && GST_IS_NAVIGATION (videosink)) {
      GstNavigation *nav = GST_NAVIGATION (videosink);

      gst_navigation_send_mouse_event (nav,
				       "mouse-move", 0, event->x, event->y);

      res = TRUE;
    }

    if (videosink)
      gst_object_unref (videosink);
  }

  if (GTK_WIDGET_CLASS (parent_class)->motion_notify_event)
    res |= GTK_WIDGET_CLASS (parent_class)->motion_notify_event (widget, event);

  return res;
}

static gboolean
bacon_video_widget_button_press (GtkWidget *widget,
				 GdkEventButton *event)
{
  gboolean res = FALSE;
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);

  g_return_val_if_fail (bvw->priv->play != NULL, FALSE);

  if (!bvw->priv->logo_mode) {
    GstElement *videosink = NULL;

    g_object_get (bvw->priv->play, "video-sink", &videosink, NULL);

    if (videosink && GST_IS_BIN (videosink)) {
      GstElement *newvideosink;
      newvideosink = gst_bin_get_by_interface (GST_BIN (videosink),
					       GST_TYPE_NAVIGATION);
      gst_object_unref (videosink);
      videosink = newvideosink;
    }

    if (videosink && GST_IS_NAVIGATION (videosink)) {
      GstNavigation *nav = GST_NAVIGATION (videosink);

      gst_navigation_send_mouse_event (nav,
          "mouse-button-press", event->button, event->x, event->y);

      res = TRUE;
    }

    if (videosink)
      gst_object_unref (videosink);
  }

  if (GTK_WIDGET_CLASS (parent_class)->button_press_event)
    res |= GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);

  return res;
}

static gboolean
bacon_video_widget_button_release (GtkWidget *widget,
				   GdkEventButton *event)
{
  gboolean res = FALSE;
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);

  g_return_val_if_fail (bvw->priv->play != NULL, FALSE);

  if (!bvw->priv->logo_mode) {
    GstElement *videosink = NULL;

    g_object_get (bvw->priv->play, "video-sink", &videosink, NULL);

    if (videosink && GST_IS_BIN (videosink)) {
      GstElement *newvideosink;
      newvideosink = gst_bin_get_by_interface (GST_BIN (videosink),
					       GST_TYPE_NAVIGATION);
      gst_object_unref (videosink);
      videosink = newvideosink;
    }

    if (videosink && GST_IS_NAVIGATION (videosink)) {
      GstNavigation *nav = GST_NAVIGATION (videosink);

      gst_navigation_send_mouse_event (nav,
          "mouse-button-release", event->button, event->x, event->y);

      res = TRUE;
    }

    if (videosink)
      gst_object_unref (videosink);
  }

  if (GTK_WIDGET_CLASS (parent_class)->button_release_event)
    res |= GTK_WIDGET_CLASS (parent_class)->button_release_event (widget, event);

  return res;
}

static void
bacon_video_widget_size_request (GtkWidget * widget,
    GtkRequisition * requisition)
{
  requisition->width = 240;
  requisition->height = 180;
}

static void
bacon_video_widget_size_allocate (GtkWidget * widget,
				  GtkAllocation * allocation)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);

  g_return_if_fail (widget != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (widget));

  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget)) {
    gfloat width, height, ratio;
    int w, h;

    gdk_window_move_resize (widget->window,
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

    /* resize video_window */
    get_media_size (bvw, &w, &h);
    if (!w || !h) {
      w = allocation->width;
      h = allocation->height;
    }
    width = w;
    height = h;

    if ((gfloat) allocation->width / width >
        (gfloat) allocation->height / height) {
      ratio = (gfloat) allocation->height / height;
    } else {
      ratio = (gfloat) allocation->width / width;
    }

    width *= ratio;
    height *= ratio;

    bvw->priv->video_window_allocation.width = width;
    bvw->priv->video_window_allocation.height = height;
    bvw->priv->video_window_allocation.x = (allocation->width - width) / 2;
    bvw->priv->video_window_allocation.y = (allocation->height - height) / 2;
    gdk_window_move_resize (bvw->priv->video_window,
                            (allocation->width - width) / 2,
                            (allocation->height - height) / 2,
                            width, height);
    gtk_widget_queue_draw (widget);
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
  widget_class->realize = bacon_video_widget_realize;
  widget_class->unrealize = bacon_video_widget_unrealize;
  widget_class->show = bacon_video_widget_show;
  widget_class->hide = bacon_video_widget_hide;
  widget_class->expose_event = bacon_video_widget_expose_event;
  widget_class->motion_notify_event = bacon_video_widget_motion_notify;
  widget_class->button_press_event = bacon_video_widget_button_press;
  widget_class->button_release_event = bacon_video_widget_button_release;

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
							G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_SHOW_VISUALS,
				   g_param_spec_boolean ("showvisuals", NULL,
							 NULL, FALSE,
							 G_PARAM_WRITABLE));

  /* Signals */
  bvw_signals[SIGNAL_ERROR] =
    g_signal_new ("error",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, error),
		  NULL, NULL,
		  baconvideowidget_marshal_VOID__STRING_BOOLEAN_BOOLEAN,
		  G_TYPE_NONE, 3, G_TYPE_STRING,
		  G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

  bvw_signals[SIGNAL_EOS] =
    g_signal_new ("eos",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, eos),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  bvw_signals[SIGNAL_GOT_METADATA] =
    g_signal_new ("got-metadata",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, got_metadata),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  bvw_signals[SIGNAL_REDIRECT] =
    g_signal_new ("got-redirect",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, got_redirect),
		  NULL, NULL, g_cclosure_marshal_VOID__STRING,
		  G_TYPE_NONE, 1, G_TYPE_STRING);

  bvw_signals[SIGNAL_TITLE_CHANGE] =
    g_signal_new ("title-change",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, title_change),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__STRING,
		  G_TYPE_NONE, 1, G_TYPE_STRING);

  bvw_signals[SIGNAL_CHANNELS_CHANGE] =
    g_signal_new ("channels-change",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, channels_change),
		  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  bvw_signals[SIGNAL_TICK] =
    g_signal_new ("tick",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, tick),
		  NULL, NULL,
		  baconvideowidget_marshal_VOID__INT64_INT64_FLOAT_BOOLEAN,
		  G_TYPE_NONE, 4, G_TYPE_INT64, G_TYPE_INT64, G_TYPE_FLOAT,
                  G_TYPE_BOOLEAN);

  bvw_signals[SIGNAL_BUFFERING] =
    g_signal_new ("buffering",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, buffering),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  bvw_signals[SIGNAL_SPEED_WARNING] =
    g_signal_new ("speed-warning",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (BaconVideoWidgetClass, speed_warning),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
bacon_video_widget_init (BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (bvw), GTK_CAN_FOCUS);
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (bvw), GTK_NO_WINDOW);
  GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (bvw), GTK_DOUBLE_BUFFERED);

  bvw->priv = g_new0 (BaconVideoWidgetPrivate, 1);
  
  bvw->priv->update_id = 0;
  bvw->priv->tagcache = NULL;
  bvw->priv->audiotags = NULL;
  bvw->priv->videotags = NULL;

  bvw->priv->lock = g_mutex_new ();
}

static void
shrink_toplevel (BaconVideoWidget * bvw)
{
  GtkWidget *toplevel, *widget;
  widget = GTK_WIDGET (bvw);
  toplevel = gtk_widget_get_toplevel (widget);
  if (toplevel != widget && GTK_IS_WINDOW (toplevel) != FALSE)
    gtk_window_resize (GTK_WINDOW (toplevel), 1, 1);
}

static gboolean bvw_query_timeout (BaconVideoWidget *bvw);
static void parse_stream_info (BaconVideoWidget *bvw);

static void
bvw_handle_application_message (BaconVideoWidget *bvw, GstMessage *msg)
{
  const gchar *msg_name;

  msg_name = gst_structure_get_name (msg->structure);
  g_return_if_fail (msg_name != NULL);

  GST_DEBUG ("Handling application message: %" GST_PTR_FORMAT, msg->structure);

  if (strcmp (msg_name, "notify-streaminfo") == 0) {
    /* if we're not interactive, we want to announce metadata
     * only later when we can be sure we got it all */
    if (bvw->priv->use_type == BVW_USE_TYPE_VIDEO ||
        bvw->priv->use_type == BVW_USE_TYPE_AUDIO) {
      g_signal_emit (bvw, bvw_signals[SIGNAL_GOT_METADATA], 0, NULL);
      g_signal_emit (bvw, bvw_signals[SIGNAL_CHANNELS_CHANGE], 0);
    }
  } 
  else if (strcmp (msg_name, "video-size") == 0) {
    /* if we're not interactive, we want to announce metadata
     * only later when we can be sure we got it all */
    if (bvw->priv->use_type == BVW_USE_TYPE_VIDEO ||
        bvw->priv->use_type == BVW_USE_TYPE_AUDIO) {
      g_signal_emit (bvw, bvw_signals[SIGNAL_GOT_METADATA], 0, NULL);
    }

    if (bvw->priv->auto_resize && !bvw->priv->fullscreen_mode) {
      gint w, h;

      shrink_toplevel (bvw);
      get_media_size (bvw, &w, &h);
      totem_widget_set_preferred_size (GTK_WIDGET (bvw), w, h);
    } else {
      bacon_video_widget_size_allocate (GTK_WIDGET (bvw),
                                        &GTK_WIDGET (bvw)->allocation);

      /* Uhm, so this ugly hack here makes media loading work for
       * weird laptops with NVIDIA graphics cards... Dunno what the
       * bug is really, but hey, it works. :). */
      if (GTK_WIDGET (bvw)->window) {
        gdk_window_hide (GTK_WIDGET (bvw)->window);
        gdk_window_show (GTK_WIDGET (bvw)->window);
        
        bacon_video_widget_expose_event (GTK_WIDGET (bvw), NULL);
      }
    }
  } else {
    g_message ("Unhandled application message %s", msg_name);
  }
}

static void
bvw_handle_element_message (BaconVideoWidget *bvw, GstMessage *msg)
{
  const gchar *type_name = NULL;
  gchar *src_name;

  src_name = gst_object_get_name (msg->src);
  if (msg->structure)
    type_name = gst_structure_get_name (msg->structure);

  GST_DEBUG ("Element message from element %s: %" GST_PTR_FORMAT, src_name,
      msg->structure);

  if (type_name == NULL)
    goto unhandled;

  if (strcmp (type_name, "redirect") == 0) {
    const gchar *new_location;

    new_location = gst_structure_get_string (msg->structure, "new-location");
    GST_DEBUG ("Got redirect to '%s'", GST_STR_NULL (new_location));

    if (new_location && *new_location) {
      g_signal_emit (bvw, bvw_signals[SIGNAL_REDIRECT], 0, new_location);
      goto done;
    }
  } else if (strcmp (type_name, "prepare-xwindow-id") == 0 ||
      strcmp (type_name, "have-xwindow-id") == 0) {
    /* we handle these synchroneously or want to ignore them */
    goto done;
  }

unhandled:
  g_message ("Unhandled element message '%s' from element '%s'",
             GST_STR_NULL (type_name), src_name);

done:
  g_free (src_name);
}

/* This is a hack to avoid doing poll_for_state_change() indirectly
 * from the bus message callback (via EOS => totem => close => wait for READY)
 * and deadlocking there. We need something like a
 * gst_bus_set_auto_flushing(bus, FALSE) ... */
static gboolean
bvw_signal_eos_delayed (gpointer user_data)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (user_data);

  g_signal_emit (bvw, bvw_signals[SIGNAL_EOS], 0, NULL);
  bvw->priv->eos_id = 0;
  return FALSE;
}

static void
bvw_reconfigure_tick_timeout (BaconVideoWidget *bvw, guint msecs)
{
  if (bvw->priv->update_id != 0) {
    GST_DEBUG ("removing tick timeout");
    g_source_remove (bvw->priv->update_id);
    bvw->priv->update_id = 0;
  }
  if (msecs > 0) {
    GST_DEBUG ("adding tick timeout (at %ums)", msecs);
    bvw->priv->update_id =
      g_timeout_add (msecs, (GSourceFunc) bvw_query_timeout, bvw);
  }
}

static void
bvw_bus_message_cb (GstBus * bus, GstMessage * message, gpointer data)
{
  BaconVideoWidget *bvw = (BaconVideoWidget *) data;
  GstMessageType msg_type;

  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  msg_type = GST_MESSAGE_TYPE (message);

  /* somebody else is handling the message, probably in poll_for_state_change */
  if (bvw->priv->ignore_messages_mask & msg_type) {
    gchar *src_name = gst_object_get_name (message->src);
    GST_LOG ("Ignoring %s message from element %s as requested",
        gst_message_type_get_name (msg_type), src_name);
    g_free (src_name);
    return;
  }

  if (msg_type != GST_MESSAGE_STATE_CHANGED) {
    gchar *src_name = gst_object_get_name (message->src);
    GST_LOG ("Handling %s message from element %s",
        gst_message_type_get_name (msg_type), src_name);
    g_free (src_name);
  }

  switch (msg_type) {
    case GST_MESSAGE_ERROR: {
      GError *error = NULL;
      gchar *debug = NULL;

      gst_message_parse_error (message, &error, &debug);

      GST_DEBUG ("Error message: %s [%s]", GST_STR_NULL (error->message),
          GST_STR_NULL (debug));

      g_signal_emit (bvw, bvw_signals[SIGNAL_ERROR], 0,
		     error->message, TRUE, FALSE);
      g_error_free (error);

      if (bvw->priv->play)
        gst_element_set_state (bvw->priv->play, GST_STATE_NULL);

      g_free (debug);
      break;
    }
    case GST_MESSAGE_WARNING: {
      GError *error = NULL;
      gchar *debug = NULL;

      gst_message_parse_warning (message, &error, &debug);

      GST_DEBUG ("Warning message: %s [%s]", GST_STR_NULL (error->message),
          GST_STR_NULL (debug));

      g_warning ("%s [%s]", GST_STR_NULL (error->message), GST_STR_NULL (debug));

      g_error_free (error);
      g_free (debug);
      break;
    }
    case GST_MESSAGE_TAG: {
      GstTagList *tag_list, *result;
      GstElementFactory *f;

      gst_message_parse_tag (message, &tag_list);

      GST_DEBUG ("Tags: %" GST_PTR_FORMAT, tag_list);

      /* all tags */
      result = gst_tag_list_merge (bvw->priv->tagcache, tag_list,
				   GST_TAG_MERGE_KEEP);
      if (bvw->priv->tagcache)
        gst_tag_list_free (bvw->priv->tagcache);
      bvw->priv->tagcache = result;

      /* media-type-specific tags */
      if (GST_IS_ELEMENT (message->src) &&
	  (f = gst_element_get_factory (GST_ELEMENT (message->src)))) {
        const gchar *klass = gst_element_factory_get_klass (f);
        GstTagList **cache = NULL;

        if (g_strrstr (klass, "Video")) {
          cache = &bvw->priv->videotags;
        } else if (g_strrstr (klass, "Audio")) {
          cache = &bvw->priv->audiotags;
        }

        if (cache) {
          result = gst_tag_list_merge (*cache, tag_list, GST_TAG_MERGE_KEEP);
          if (*cache)
            gst_tag_list_free (*cache);
          *cache = result;
        }
      }

      /* clean up */
      gst_tag_list_free (tag_list);

      /* if we're not interactive, we want to announce metadata
       * only later when we can be sure we got it all */
      if (bvw->priv->use_type == BVW_USE_TYPE_VIDEO ||
          bvw->priv->use_type == BVW_USE_TYPE_AUDIO) {
        g_signal_emit (bvw, bvw_signals[SIGNAL_GOT_METADATA], 0);
      }
      break;
    }
    case GST_MESSAGE_EOS:
      GST_DEBUG ("EOS message");
      /* update slider one last time */
      bvw_query_timeout (bvw);
      if (bvw->priv->eos_id == 0)
        bvw->priv->eos_id = g_idle_add (bvw_signal_eos_delayed, bvw);
      break;
    case GST_MESSAGE_BUFFERING: {
      gint percent = 0;
      gst_structure_get_int (message->structure, "buffer-percent", &percent);
      GST_DEBUG ("Buffering message (%u%%)", percent);
      g_signal_emit (bvw, bvw_signals[SIGNAL_BUFFERING], 0, percent);
      break;
    }
    case GST_MESSAGE_APPLICATION: {
      bvw_handle_application_message (bvw, message);
      break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
      GstState old_state, new_state;
      gchar *src_name;

      gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

      if (old_state == new_state)
        break;

      /* we only care about playbin (pipeline) state changes */
      if (GST_MESSAGE_SRC (message) != GST_OBJECT (bvw->priv->play))
        break;

      src_name = gst_object_get_name (message->src);
      GST_DEBUG ("%s changed state from %s to %s", src_name,
          gst_element_state_get_name (old_state),
          gst_element_state_get_name (new_state));
      g_free (src_name);

      /* now do stuff */
      if (new_state < GST_STATE_PAUSED) {
        bvw_reconfigure_tick_timeout (bvw, 0);
      } else if (new_state == GST_STATE_PAUSED) {
        /* yes, we need to keep the tick timeout running in PAUSED state
         * as well, totem depends on that (use lower frequency though) */
        bvw_reconfigure_tick_timeout (bvw, 500);
      } else if (new_state > GST_STATE_PAUSED) {
        bvw_reconfigure_tick_timeout (bvw, 200);
      }

      if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
        parse_stream_info (bvw);
      } else if (old_state == GST_STATE_PAUSED && new_state == GST_STATE_READY) {
        bvw->priv->media_has_video = FALSE;
        bvw->priv->media_has_audio = FALSE;

        /* clean metadata cache */
        if (bvw->priv->tagcache) {
          gst_tag_list_free (bvw->priv->tagcache);
          bvw->priv->tagcache = NULL;
        }
        if (bvw->priv->audiotags) {
          gst_tag_list_free (bvw->priv->audiotags);
          bvw->priv->audiotags = NULL;
        }
        if (bvw->priv->videotags) {
          gst_tag_list_free (bvw->priv->videotags);
          bvw->priv->videotags = NULL;
        }

        bvw->priv->video_width = 0;
        bvw->priv->video_height = 0;
      }
      break;
    }
    case GST_MESSAGE_ELEMENT:{
      bvw_handle_element_message (bvw, message);
      break;
    }

    case GST_MESSAGE_DURATION: {
      /* force _get_stream_length() to do new duration query */
      bvw->priv->stream_length = 0;
      if (bacon_video_widget_get_stream_length (bvw) == 0) {
        GST_DEBUG ("Failed to query duration after DURATION message?!");
      }
      break;
    }

    case GST_MESSAGE_CLOCK_PROVIDE:
    case GST_MESSAGE_CLOCK_LOST:
    case GST_MESSAGE_NEW_CLOCK:
    case GST_MESSAGE_STATE_DIRTY:
      break;

    default:
      g_message ("Unhandled message of type '%s' (0x%x)", 
          gst_message_type_get_name (msg_type), msg_type);
      break;
  }
}

/* FIXME: how to recognise this in 0.9? */
#if 0
static void
group_switch (GstElement *play, BaconVideoWidget *bvw)
{
  GstMessage *msg;

  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  if (bvw->priv->tagcache) {
    gst_tag_list_free (bvw->priv->tagcache);
    bvw->priv->tagcache = NULL;
  }
  if (bvw->priv->audiotags) {
    gst_tag_list_free (bvw->priv->audiotags);
    bvw->priv->audiotags = NULL;
  }
  if (bvw->priv->videotags) {
    gst_tag_list_free (bvw->priv->videotags);
    bvw->priv->videotags = NULL;
  }

  msg = gst_message_new_application (GST_OBJECT (bvw->priv->play),
      gst_structure_new ("notify-streaminfo", NULL));
  gst_element_post_message (bvw->priv->play, msg);
}
#endif

static void
got_video_size (BaconVideoWidget * bvw)
{
  GstMessage *msg;

  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  msg = gst_message_new_application (GST_OBJECT (bvw->priv->play),
      gst_structure_new ("video-size", "width", G_TYPE_INT,
          bvw->priv->video_width, "height", G_TYPE_INT,
          bvw->priv->video_height, NULL));
  gst_element_post_message (bvw->priv->play, msg);
}

static void
got_time_tick (GstElement * play, gint64 time_nanos, BaconVideoWidget * bvw)
{
  gboolean seekable;

  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  if (bvw->priv->logo_mode != FALSE)
    return;

  bvw->priv->current_time_nanos = time_nanos;

  bvw->priv->current_time = (gint64) time_nanos / GST_MSECOND;

  if (bvw->priv->stream_length == 0) {
    bvw->priv->current_position = 0;
  } else {
    bvw->priv->current_position =
      (gfloat) bvw->priv->current_time / bvw->priv->stream_length;
  }


  if (bvw->priv->stream_length == 0) {
    seekable = bacon_video_widget_is_seekable (bvw);
  } else {
    seekable = TRUE;
  }

/*
  GST_DEBUG ("%" GST_TIME_FORMAT ",%" GST_TIME_FORMAT " %s",
      GST_TIME_ARGS (bvw->priv->current_time),
      GST_TIME_ARGS (bvw->priv->stream_length),
      (seekable) ? "TRUE" : "FALSE");
*/
  
  g_signal_emit (bvw, bvw_signals[SIGNAL_TICK], 0,
                 bvw->priv->current_time, bvw->priv->stream_length,
                 bvw->priv->current_position,
                 seekable);
}

static void
got_source (GObject    *play,
	    GParamSpec *pspec,
	    BaconVideoWidget *bvw)
{
  GObject *source = NULL;
  GObjectClass *klass;

  if (bvw->priv->tagcache) {
    gst_tag_list_free (bvw->priv->tagcache);
    bvw->priv->tagcache = NULL;
  }
  if (bvw->priv->audiotags) {
    gst_tag_list_free (bvw->priv->audiotags);
    bvw->priv->audiotags = NULL;
  }
  if (bvw->priv->videotags) {
    gst_tag_list_free (bvw->priv->videotags);
    bvw->priv->videotags = NULL;
  }

  if (!bvw->priv->media_device)
    return;

  g_object_get (play, "source", &source, NULL);
  if (!source)
    return;

  klass = G_OBJECT_GET_CLASS (source);
  if (g_object_class_find_property (klass, "device")) {
    g_object_set (source, "device", bvw->priv->media_device, NULL);
  }
  g_object_unref (source);
}

static gboolean
bvw_query_timeout (BaconVideoWidget *bvw)
{
  GstFormat fmt = GST_FORMAT_TIME;
  gint64 prev_len = -1;
  gint64 pos = -1, len = -1;
  
  /* check length/pos of stream */
  prev_len = bvw->priv->stream_length;
  if (gst_element_query_duration (bvw->priv->play, &fmt, &len)) {
    if (len != -1 && fmt == GST_FORMAT_TIME) {
      bvw->priv->stream_length = len / GST_MSECOND;
      if (bvw->priv->stream_length != prev_len) {
        g_signal_emit (bvw, bvw_signals[SIGNAL_GOT_METADATA], 0, NULL);
      }
    }
  } else {
    GST_DEBUG ("could not get duration");
  }

  if (gst_element_query_position (bvw->priv->play, &fmt, &pos)) {
    if (pos != -1 && fmt == GST_FORMAT_TIME) {
      got_time_tick (GST_ELEMENT (bvw->priv->play), pos, bvw);
    }
  } else {
    GST_DEBUG ("could not get position");
  }

  return TRUE;
}

static void
caps_set (GObject * obj,
    GParamSpec * pspec, BaconVideoWidget * bvw)
{
  GstPad *pad = GST_PAD (obj);
  GstStructure *s;
  GstCaps *caps;

  if (!(caps = gst_pad_get_negotiated_caps (pad)))
    return;

  /* Get video decoder caps */
  s = gst_caps_get_structure (caps, 0);
  if (s) {
    /* We need at least width/height and framerate */
    if (!(gst_structure_get_fraction (s, "framerate", &bvw->priv->video_fps_n, 
          &bvw->priv->video_fps_d) &&
          gst_structure_get_int (s, "width", &bvw->priv->video_width) &&
          gst_structure_get_int (s, "height", &bvw->priv->video_height)))
      return;
    
    /* Get the movie PAR if available */
    bvw->priv->movie_par = gst_structure_get_value (s, "pixel-aspect-ratio");
    
    /* Now set for real */
    bacon_video_widget_set_aspect_ratio (bvw, bvw->priv->ratio_type);
  }

  gst_caps_unref (caps);
}

static void get_visualization_size (BaconVideoWidget *bvw,
				    int *w, int *h, gint *fps_n, gint *fps_d);

static void
parse_stream_info (BaconVideoWidget *bvw)
{
  GList *streaminfo = NULL;
  GstPad *videopad = NULL;

  g_object_get (bvw->priv->play, "stream-info", &streaminfo, NULL);
  streaminfo = g_list_copy (streaminfo);
  g_list_foreach (streaminfo, (GFunc) g_object_ref, NULL);
  for ( ; streaminfo != NULL; streaminfo = streaminfo->next) {
    GObject *info = streaminfo->data;
    gint type;
    GParamSpec *pspec;
    GEnumValue *val;

    if (!info)
      continue;
    g_object_get (info, "type", &type, NULL);
    pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (info), "type");
    val = g_enum_get_value (G_PARAM_SPEC_ENUM (pspec)->enum_class, type);

    if (!g_strcasecmp (val->value_nick, "audio")) {
      bvw->priv->media_has_audio = TRUE;
      if (!bvw->priv->media_has_video && bvw->priv->video_window) {
        if (bvw->priv->show_vfx) {
          gdk_window_show (bvw->priv->video_window);
        } else {
          gdk_window_hide (bvw->priv->video_window);
        }
      }
    } else if (!g_strcasecmp (val->value_nick, "video")) {
      bvw->priv->media_has_video = TRUE;
      if (bvw->priv->video_window)
        gdk_window_show (bvw->priv->video_window);
      if (!videopad) {
        g_object_get (info, "object", &videopad, NULL);
      }
    }
  }

  if (videopad) {
    GstCaps *caps;

    if ((caps = gst_pad_get_negotiated_caps (videopad))) {
      caps_set (G_OBJECT (videopad), NULL, bvw);
      gst_caps_unref (caps);
    }
    g_signal_connect (videopad, "notify::caps",
        G_CALLBACK (caps_set), bvw);
    /* FIXME: don't we need to unref the video pad? (tpm) */
  } else if (bvw->priv->show_vfx) {
    get_visualization_size (bvw, &bvw->priv->video_width,
			    &bvw->priv->video_height, NULL, NULL);
  }

  g_list_foreach (streaminfo, (GFunc) g_object_unref, NULL);
  g_list_free (streaminfo);
}

static void
stream_info_set (GObject * obj, GParamSpec * pspec, BaconVideoWidget * bvw)
{
  GstMessage *msg;

  parse_stream_info (bvw);

  msg = gst_message_new_application (GST_OBJECT (bvw->priv->play),
      gst_structure_new ("notify-streaminfo", NULL));
  gst_element_post_message (bvw->priv->play, msg);
}

static void
bacon_video_widget_finalize (GObject * object)
{
  BaconVideoWidget *bvw = (BaconVideoWidget *) object;

  GST_DEBUG ("finalizing");

  if (bvw->priv->bus) {
    /* make bus drop all messages to make sure none of our callbacks is ever
     * called again (main loop might be run again to display error dialog) */
    gst_bus_set_flushing (bvw->priv->bus, TRUE);

    if (bvw->priv->sig_bus_sync)
      g_signal_handler_disconnect (bvw->priv->bus, bvw->priv->sig_bus_sync);

    if (bvw->priv->sig_bus_async)
      g_signal_handler_disconnect (bvw->priv->bus, bvw->priv->sig_bus_async);

    gst_object_unref (bvw->priv->bus);
    bvw->priv->bus = NULL;
  }

  g_free (bvw->priv->media_device);
  bvw->priv->media_device = NULL;
    
  g_free (bvw->priv->mrl);
  bvw->priv->mrl = NULL;
  
  if (bvw->priv->vis_element_name) {
    g_free (bvw->priv->vis_element_name);
    bvw->priv->vis_element_name = NULL;
  }
 
  if (bvw->priv->vis_plugins_list) {
    g_list_free (bvw->priv->vis_plugins_list);
    bvw->priv->vis_plugins_list = NULL;
  }

  if (bvw->priv->play != NULL && GST_IS_ELEMENT (bvw->priv->play)) {
    gst_element_set_state (bvw->priv->play, GST_STATE_NULL);
    gst_object_unref (bvw->priv->play);
    bvw->priv->play = NULL;
  }

  if (bvw->priv->update_id) {
    g_source_remove (bvw->priv->update_id);
    bvw->priv->update_id = 0;
  }

  if (bvw->priv->tagcache) {
    gst_tag_list_free (bvw->priv->tagcache);
    bvw->priv->tagcache = NULL;
  }
  if (bvw->priv->audiotags) {
    gst_tag_list_free (bvw->priv->audiotags);
    bvw->priv->audiotags = NULL;
  }
  if (bvw->priv->videotags) {
    gst_tag_list_free (bvw->priv->videotags);
    bvw->priv->videotags = NULL;
  }

  if (bvw->priv->eos_id != 0)
    g_source_remove (bvw->priv->eos_id);

  g_mutex_free (bvw->priv->lock);

  g_free (bvw->priv);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
bacon_video_widget_set_property (GObject * object, guint property_id,
                                 const GValue * value, GParamSpec * pspec)
{
  BaconVideoWidget *bvw;

  bvw = BACON_VIDEO_WIDGET (object);

  switch (property_id)
    {
    case PROP_LOGO_MODE:
      bacon_video_widget_set_logo_mode (bvw,
	  g_value_get_boolean (value));
      break;
    case PROP_SHOWCURSOR:
      bacon_video_widget_set_show_cursor (bvw,
	  g_value_get_boolean (value));
      break;
    case PROP_MEDIADEV:
      bacon_video_widget_set_media_device (bvw,
	  g_value_get_string (value));
      break;
    case PROP_SHOW_VISUALS:
      bacon_video_widget_set_show_visuals (bvw,
	  g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
bacon_video_widget_get_property (GObject * object, guint property_id,
                                 GValue * value, GParamSpec * pspec)
{
  BaconVideoWidget *bvw;

  bvw = BACON_VIDEO_WIDGET (object);

  switch (property_id)
    {
      case PROP_LOGO_MODE:
	g_value_set_boolean (value,
	    bacon_video_widget_get_logo_mode (bvw));
	break;
      case PROP_POSITION:
	g_value_set_int64 (value, bacon_video_widget_get_position (bvw));
	break;
      case PROP_STREAM_LENGTH:
	g_value_set_int64 (value,
	    bacon_video_widget_get_stream_length (bvw));
	break;
      case PROP_PLAYING:
	g_value_set_boolean (value,
	    bacon_video_widget_is_playing (bvw));
	break;
      case PROP_SEEKABLE:
	g_value_set_boolean (value,
	    bacon_video_widget_is_seekable (bvw));
	break;
      case PROP_SHOWCURSOR:
	g_value_set_boolean (value,
	    bacon_video_widget_get_show_cursor (bvw));
	break;
      case PROP_MEDIADEV:
	g_value_set_string (value, bvw->priv->media_device);
	break;
      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

char *
bacon_video_widget_get_backend_name (BaconVideoWidget * bvw)
{
  return gst_version_string ();
}

static gboolean
has_subp (BaconVideoWidget * bvw)
{
  GList *streaminfo = NULL;
  gboolean res = FALSE;

  if (bvw->priv->play == NULL || bvw->priv->mrl == NULL)
    return FALSE;

  g_object_get (G_OBJECT (bvw->priv->play), "stream-info", &streaminfo, NULL);
  streaminfo = g_list_copy (streaminfo);
  g_list_foreach (streaminfo, (GFunc) g_object_ref, NULL);
  for ( ; streaminfo != NULL; streaminfo = streaminfo->next) {
    GObject *info = streaminfo->data;
    gint type;
    GParamSpec *pspec;
    GEnumValue *val;

    if (!info)
      continue;
    g_object_get (info, "type", &type, NULL);
    pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (info), "type");
    val = g_enum_get_value (G_PARAM_SPEC_ENUM (pspec)->enum_class, type);

    if (strstr (val->value_name, "SUBPICTURE")) {
      res = TRUE;
      break;
    }
  }
  g_list_foreach (streaminfo, (GFunc) g_object_unref, NULL);
  g_list_free (streaminfo);

  return res;
}

int
bacon_video_widget_get_subtitle (BaconVideoWidget * bvw)
{
  int subtitle = -1;

  g_return_val_if_fail (bvw != NULL, -2);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -2);
  g_return_val_if_fail (bvw->priv->play != NULL, -2);

  if (has_subp (bvw))
    g_object_get (G_OBJECT (bvw->priv->play), "current-subpicture", &subtitle, NULL);
  else
    g_object_get (G_OBJECT (bvw->priv->play), "current-text", &subtitle, NULL);

  if (subtitle == -1)
    subtitle = -2;

  return subtitle;
}

void
bacon_video_widget_set_subtitle (BaconVideoWidget * bvw, int subtitle)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (bvw->priv->play != NULL);

  if (subtitle == -1)
    subtitle = 0;
  else if (subtitle == -2)
    subtitle = -1;

  if (has_subp (bvw))
    g_object_set (bvw->priv->play, "current-subpicture", subtitle, NULL);
  else
    g_object_set (bvw->priv->play, "current-text", subtitle, NULL);
}

static GList *
get_list_of_type (BaconVideoWidget * bvw, const gchar * type_name)
{
  GList *streaminfo = NULL, *ret = NULL;
  gint num = 0;

  if (bvw->priv->play == NULL || bvw->priv->mrl == NULL)
    return NULL;

  g_object_get (G_OBJECT (bvw->priv->play), "stream-info", &streaminfo, NULL);
  streaminfo = g_list_copy (streaminfo);
  g_list_foreach (streaminfo, (GFunc) g_object_ref, NULL);
  for ( ; streaminfo != NULL; streaminfo = streaminfo->next) {
    GObject *info = streaminfo->data;
    gint type;
    GParamSpec *pspec;
    GEnumValue *val;
    gchar *lc = NULL, *cd = NULL;

    if (!info)
      continue;
    g_object_get (info, "type", &type, NULL);
    pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (info), "type");
    val = g_enum_get_value (G_PARAM_SPEC_ENUM (pspec)->enum_class, type);

    if (g_ascii_strcasecmp (val->value_nick, type_name) == 0) {
      g_object_get (info, "codec", &cd, "language-code", &lc, NULL);

      if (lc) {
        ret = g_list_prepend (ret, lc);
        g_free (cd);
      } else if (cd) {
        ret = g_list_prepend (ret, cd);
      } else {
        ret = g_list_prepend (ret, g_strdup_printf ("%s %d", type_name, num++));
      }
    }
  }
  g_list_foreach (streaminfo, (GFunc) g_object_unref, NULL);
  g_list_free (streaminfo);

  return g_list_reverse (ret);
}

GList * bacon_video_widget_get_subtitles (BaconVideoWidget * bvw)
{
  GList *list;

  g_return_val_if_fail (bvw != NULL, NULL);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (bvw->priv->play != NULL, NULL);

  if (!(list =  get_list_of_type (bvw, "SUBPICTURE")))
    list = get_list_of_type (bvw, "TEXT");

  return list;
}

GList * bacon_video_widget_get_languages (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, NULL);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (bvw->priv->play != NULL, NULL);

  return get_list_of_type (bvw, "AUDIO");
}

int
bacon_video_widget_get_language (BaconVideoWidget * bvw)
{
  int language = -1;

  g_return_val_if_fail (bvw != NULL, -2);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -2);
  g_return_val_if_fail (bvw->priv->play != NULL, -2);

  g_object_get (G_OBJECT (bvw->priv->play), "current-audio", &language, NULL);

  if (language == -1)
    language = -2;

  return language;
}

void
bacon_video_widget_set_language (BaconVideoWidget * bvw, int language)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (bvw->priv->play != NULL);

  if (language == -1)
    language = 0;
  else if (language == -2)
    language = -1;

  g_object_set (bvw->priv->play, "current-audio", language, NULL);
}

int
bacon_video_widget_get_connection_speed (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, 0);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);

  return bvw->priv->connection_speed;
}

void
bacon_video_widget_set_connection_speed (BaconVideoWidget * bvw, int speed)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->priv->connection_speed = speed;
  gconf_client_set_int (bvw->priv->gc,
       GCONF_PREFIX"/connection_speed", speed, NULL);
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
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

  bvw->priv->tv_out_type = tvout;
  gconf_client_set_int (bvw->priv->gc,
      GCONF_PREFIX"/tv_out_type", tvout, NULL);

#ifdef HAVE_NVTV
  if (tvout == TV_OUT_NVTV_PAL) {
    nvtv_simple_set_tvsystem(NVTV_SIMPLE_TVSYSTEM_PAL);
  } else if (tvout == TV_OUT_NVTV_NTSC) {
    nvtv_simple_set_tvsystem(NVTV_SIMPLE_TVSYSTEM_NTSC);
  }
#endif

  return FALSE;
}

TvOutType
bacon_video_widget_get_tv_out (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, 0);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);

  return bvw->priv->tv_out_type;
}

static gint
get_num_audio_channels (BaconVideoWidget * bvw)
{
  gint channels;

  switch (bvw->priv->speakersetup) {
    case BVW_AUDIO_SOUND_STEREO:
      channels = 2;
      break;
    case BVW_AUDIO_SOUND_4CHANNEL:
      channels = 4;
      break;
    case BVW_AUDIO_SOUND_5CHANNEL:
      channels = 5;
      break;
    case BVW_AUDIO_SOUND_41CHANNEL:
      /* so alsa has this as 5.1, but empty center speaker. We don't really
       * do that yet. ;-). So we'll take the placebo approach. */
    case BVW_AUDIO_SOUND_51CHANNEL:
      channels = 6;
      break;
    case BVW_AUDIO_SOUND_AC3PASSTHRU:
    default:
      g_return_val_if_reached (-1);
  }

  return channels;
}

static GstCaps *
fixate_to_num (const GstCaps * in_caps, gint channels)
{
  gint n, count;
  GstStructure *s;
  const GValue *v;
  GstCaps *out_caps;

  out_caps = gst_caps_copy (in_caps);

  count = gst_caps_get_size (out_caps);
  for (n = 0; n < count; n++) {
    s = gst_caps_get_structure (out_caps, n);
    v = gst_structure_get_value (s, "channels");
    if (!v)
      continue;

    /* get channel count (or list of ~) */
    gst_structure_fixate_field_nearest_int (s, "channels", channels);
  }

  return out_caps;
}

static void
set_audio_filter (BaconVideoWidget *bvw)
{
  gint channels;
  GstCaps *caps, *res;
  GstPad *pad;

  /* reset old */
  g_object_set (bvw->priv->audio_capsfilter, "caps", NULL, NULL);

  /* construct possible caps to filter down to our chosen caps */
  /* Start with what the audio sink supports, but limit the allowed
   * channel count to our speaker output configuration */
  pad = gst_element_get_pad (bvw->priv->audio_capsfilter, "src");
  caps = gst_pad_peer_get_caps (pad);        
  gst_object_unref (pad);

  if ((channels = get_num_audio_channels (bvw)) == -1)
    return;

  res = fixate_to_num (caps, channels);
  gst_caps_unref (caps);

  /* set */
  if (res && gst_caps_is_empty (res)) {
    gst_caps_unref (res);
    res = NULL;
  }
  g_object_set (bvw->priv->audio_capsfilter, "caps", res, NULL);

  if (res) {
    gst_caps_unref (res);
  }

  /* reset */
  pad = gst_element_get_pad (bvw->priv->audio_capsfilter, "src");
  gst_pad_set_caps (pad, NULL);
  gst_object_unref (pad);
}

BaconVideoWidgetAudioOutType
bacon_video_widget_get_audio_out_type (BaconVideoWidget *bvw)
{
  g_return_val_if_fail (bvw != NULL, -1);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);

  return bvw->priv->speakersetup;
}

gboolean
bacon_video_widget_set_audio_out_type (BaconVideoWidget *bvw,
                                       BaconVideoWidgetAudioOutType type)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

  if (type == bvw->priv->speakersetup)
    return FALSE;
  else if (type == BVW_AUDIO_SOUND_AC3PASSTHRU)
    return FALSE;

  bvw->priv->speakersetup = type;
  gconf_client_set_int (bvw->priv->gc,
      GCONF_PREFIX"/audio_output_type", type, NULL);

  set_audio_filter (bvw);

  return FALSE;
}

/* =========================================== */
/*                                             */
/*               Play/Pause, Stop              */
/*                                             */
/* =========================================== */

static GError*
bvw_error_from_gst_error (BaconVideoWidget *bvw, GstElement *src, GError *e,
    char *debug)
{
  const gchar *src_typename;
  GError *ret = NULL;

  src_typename = (src) ? G_OBJECT_TYPE_NAME (src) : NULL;

  GST_DEBUG ("e->message = '%s'", GST_STR_NULL (e->message));
  GST_DEBUG ("e->domain  = %d", e->domain);
  GST_DEBUG ("e->code    = %d", e->code);
  GST_DEBUG ("debug      = %s", GST_STR_NULL (debug));
  GST_DEBUG ("source     = %s (%s)", (src) ? GST_OBJECT_NAME (src) : "(null)",
      GST_STR_NULL (src_typename));

#define is_error(e, d, c) \
  (e->domain == GST_##d##_ERROR && \
   e->code == GST_##d##_ERROR_##c)

  /* FIXME: we aren't doing anything with debug right now */

  if (is_error (e, RESOURCE, NOT_FOUND) ||
      is_error (e, RESOURCE, OPEN_READ)) {
#if 0
    if (strchr (mrl, ':') &&
	(g_str_has_prefix (mrl, "dvd") ||
	 g_str_has_prefix (mrl, "cd") ||
	 g_str_has_prefix (mrl, "vcd"))) {
      ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_INVALID_DEVICE,
				 e->message);
    } else {
#endif
      if (e->code == GST_RESOURCE_ERROR_NOT_FOUND) {
	if (GST_IS_BASE_AUDIO_SINK (src)) {
	  ret = g_error_new_literal
	    (BVW_ERROR, BVW_ERROR_AUDIO_PLUGIN,
	     _("The requested audio output was not found. "
	       "Please select another audio output in the Multimedia "
	       "Systems Selector."));
	} else {
	  ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_FILE_NOT_FOUND,
				     _("Location not found."));
	}
      } else {
	ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_FILE_PERMISSION,
		     _("Could not open location; "
		       "You may not have permission to open the file."));
      }
#if 0
    }
#endif
  } else if (is_error (e, RESOURCE, BUSY)) {
    if (GST_IS_VIDEO_SINK (src)) {
      /* a somewhat evil check, but hey.. */
      ret = g_error_new_literal
	(BVW_ERROR, BVW_ERROR_VIDEO_PLUGIN,
	 _("The video output is in use by another application. "
	   "Please close other video applications, or select "
	   "another video output in the Multimedia Systems Selector."));
    } else if (GST_IS_BASE_AUDIO_SINK (src)) {
      ret = g_error_new_literal
	(BVW_ERROR, BVW_ERROR_AUDIO_BUSY,
	 _("The audio output is in use by another application. "
	   "Please select another audio output in the Multimedia Systems Selector. "
	   "You may want to consider using a sound server."));
    }
  } else if (e->domain == GST_RESOURCE_ERROR) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_FILE_GENERIC,
			       e->message);
  } else if (is_error (e, STREAM, WRONG_TYPE) ||
	     is_error (e, STREAM, CODEC_NOT_FOUND) ||
	     is_error (e, STREAM, NOT_IMPLEMENTED)) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_CODEC_NOT_HANDLED,
			       e->message);
  } else if (is_error (e, STREAM, FAILED) &&
             src_typename && strncmp (src_typename, "GstTypeFind", 11) == 0) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_READ_ERROR,
        _("Cannot play this file over the network. "
          "Try downloading it to disk first."));
  } else {
    /* generic error, no code; take message */
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_GENERIC,
			       e->message);
  }

  return ret;
}

static gboolean
poll_for_state_change_full (BaconVideoWidget *bvw, GstElement *element,
    GstState state, GError **error, gint64 timeout)
{
  GstBus *bus;
  GstMessageType events, saved_events;
  
  bus = gst_element_get_bus (element);

  events = GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS;

  saved_events = bvw->priv->ignore_messages_mask;

  if (element != NULL && element == bvw->priv->play) {
    /* we do want the main handler to process state changed messages for
     * playbin as well, otherwise it won't hook up the timeout etc. */
    bvw->priv->ignore_messages_mask |= (events ^ GST_MESSAGE_STATE_CHANGED);
  } else {
    bvw->priv->ignore_messages_mask |= events;
  }

  while (TRUE) {
    GstMessage *message;
    GstElement *src;

    message = gst_bus_poll (bus, events, timeout);
    
    if (!message)
      goto timed_out;
    
    src = (GstElement*)GST_MESSAGE_SRC (message);

    switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_STATE_CHANGED:
      {
	GstState old, new, pending;

	if (src == element) {
	  gst_message_parse_state_changed (message, &old, &new, &pending);
	  if (new == state) {
	    gst_message_unref (message);
	    goto success;
	  }
	}
      }
      break;
    case GST_MESSAGE_ERROR:
      {
	gchar *debug = NULL;
	GError *gsterror = NULL;

	gst_message_parse_error (message, &gsterror, &debug);
	if (error) {
	  *error = bvw_error_from_gst_error (bvw, src, gsterror, debug);
	} else {
	  g_warning ("Error: %s (%s)", gsterror->message, debug);
	}

	gst_message_unref (message);
	g_error_free (gsterror);
	g_free (debug);
	goto error;
      }
      break;
    case GST_MESSAGE_EOS:
      {
	g_set_error (error, BVW_ERROR, BVW_ERROR_FILE_GENERIC,
		     _("Media file could not be played."));
	gst_message_unref (message);
	goto error;
      }
      break;
    default:
      g_assert_not_reached ();
      break;
    }

    gst_message_unref (message);
  }
    
  g_assert_not_reached ();

success:
  /* state change succeeded */
  GST_DEBUG ("state change to %s succeeded", gst_element_state_get_name (state));
  bvw->priv->ignore_messages_mask = saved_events;
  return TRUE;

timed_out:
  /* it's taking a long time to open -- just tell totem it was ok, this allows
   * the user to stop the loading process with the normal stop button */
  GST_DEBUG ("state change to %s timed out, returning success and handling "
      "errors asynchroneously", gst_element_state_get_name (state));
  bvw->priv->ignore_messages_mask = saved_events;
  return TRUE;

error:
  GST_DEBUG ("error while waiting for state change to %s: %s",
      gst_element_state_get_name (state),
      (error && *error) ? (*error)->message : "unknown");
  /* already set *error */
  bvw->priv->ignore_messages_mask = saved_events;
  return FALSE;
}

static gboolean
poll_for_state_change (BaconVideoWidget *bvw, GstElement *element,
    GstState state, GError **error)
{
  return poll_for_state_change_full (bvw,element, state, error, GST_SECOND/4);
}

gboolean
bacon_video_widget_open_with_subtitle (BaconVideoWidget * bvw,
		const gchar * mrl, const gchar *subtitle_uri, GError ** error)
{
  gboolean ret;

  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (mrl != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (bvw->priv->play != NULL, FALSE);
  
  /* So we aren't closed yet... */
  if (bvw->priv->mrl) {
    bacon_video_widget_close (bvw);
  }
  
  GST_DEBUG ("mrl = %s", GST_STR_NULL (mrl));
  GST_DEBUG ("subtitle_uri = %s", GST_STR_NULL (subtitle_uri));
  
  /* hmm... */
  if (bvw->priv->mrl && strcmp (bvw->priv->mrl, mrl) == 0) {
    GST_DEBUG ("same as current mrl");
    /* FIXME: shouldn't we ensure playing state here? */
    return TRUE;
  }

  /* this allows non-URI type of files in the thumbnailer and so on */
  g_free (bvw->priv->mrl);
  if (mrl[0] == '/') {
    bvw->priv->mrl = g_strdup_printf ("file://%s", mrl);
  } else {
    if (strchr (mrl, ':')) {
      bvw->priv->mrl = g_strdup (mrl);
    } else {
      gchar *cur_dir = g_get_current_dir ();
      if (!cur_dir) {
	g_set_error (error, BVW_ERROR, BVW_ERROR_GENERIC,
	     _("Failed to retrieve working directory"));
	return FALSE;
      }
      bvw->priv->mrl = g_strdup_printf ("file://%s/%s", cur_dir, mrl);
      g_free (cur_dir);
    }
  }

  /* this allows to play backups of dvds */
  if (g_str_has_prefix (mrl, "dvd:///")) {
    g_free (bvw->priv->mrl);
    bvw->priv->mrl = g_strdup ("dvd://");
    bacon_video_widget_set_media_device (bvw, mrl + strlen ("dvd://"));
  }

  bvw->priv->got_redirect = FALSE;
  bvw->priv->media_has_video = FALSE;
  bvw->priv->media_has_audio = FALSE;
  bvw->priv->stream_length = 0;
  bvw->priv->ignore_messages_mask = 0;
  
  /* We hide the video window for now. Will show when video of vfx comes up */
  if (bvw->priv->video_window) {
    gdk_window_hide (bvw->priv->video_window);
    /* We also take the whole widget until we know video size */
    gdk_window_move_resize (bvw->priv->video_window, 0, 0,
        GTK_WIDGET (bvw)->allocation.width,
        GTK_WIDGET (bvw)->allocation.height);
  }
  
  /* Visualization settings changed */
  if (bvw->priv->vis_changed) {
    setup_vis (bvw);
  }

  if (g_strrstr (bvw->priv->mrl, "#subtitle:")) {
    gchar **uris;
    gchar *subtitle_uri;

    uris = g_strsplit (bvw->priv->mrl, "#subtitle:", 2);
    /* Try to fix subtitle uri if needed */
    if (uris[1][0] == '/') {
      subtitle_uri = g_strdup_printf ("file://%s", uris[1]);
    }
    else {
      if (strchr (uris[1], ':')) {
        subtitle_uri = g_strdup (uris[1]);
      } else {
        gchar *cur_dir = g_get_current_dir ();
        if (!cur_dir) {
          g_set_error (error, BVW_ERROR, BVW_ERROR_GENERIC,
              _("Failed to retrieve working directory"));
          return FALSE;
        }
        subtitle_uri = g_strdup_printf ("file://%s/%s", cur_dir, uris[1]);
        g_free (cur_dir);
      }
    }
    g_object_set (bvw->priv->play, "uri", bvw->priv->mrl,
                  "suburi", subtitle_uri, NULL);
    g_free (subtitle_uri);
    g_strfreev (uris);
  } else {
    g_object_set (bvw->priv->play, "uri", bvw->priv->mrl,
		  "suburi", subtitle_uri, NULL);
  }

  gst_element_set_state (bvw->priv->play, GST_STATE_PAUSED);

  if (bvw->priv->use_type == BVW_USE_TYPE_AUDIO ||
      bvw->priv->use_type == BVW_USE_TYPE_VIDEO) {
    /* normal interactive usage */
    ret = poll_for_state_change (bvw, bvw->priv->play, GST_STATE_PAUSED, error);
  } else {
    /* used as thumbnailer or metadata extractor for properties dialog. In
     * this case, wait for any state change to really finish and process any
     * pending tag messages, so that the information is available right away */
    GST_DEBUG ("waiting for state changed to PAUSED to complete");
    ret = poll_for_state_change_full (bvw, bvw->priv->play,
        GST_STATE_PAUSED, error, -1);

    bvw_process_pending_tag_messages (bvw);
    bacon_video_widget_get_stream_length (bvw);
    GST_DEBUG ("stream length = %u", bvw->priv->stream_length);

    /* even in case of an error (e.g. no decoders installed) we might still
     * have useful metadata (like codec types, duration, etc.) */
    g_signal_emit (bvw, bvw_signals[SIGNAL_GOT_METADATA], 0, NULL);
  }
  
  if (ret) {
    g_signal_emit (bvw, bvw_signals[SIGNAL_CHANNELS_CHANGE], 0);
  } else {
    GST_DEBUG ("Error on open: %s", (error) ? (*error)->message : "(unknown)");
    bvw->priv->ignore_messages_mask |= GST_MESSAGE_ERROR;
    bvw_stop_play_pipeline (bvw);
    g_free (bvw->priv->mrl);
    bvw->priv->mrl = NULL;
  }
  
  /* When opening a new media we want to redraw ourselves */
  gtk_widget_queue_draw (GTK_WIDGET (bvw));

  return ret;
}

gboolean
bacon_video_widget_play (BaconVideoWidget * bvw, GError ** error)
{
  gboolean ret;

  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);

  /* no need to actually go into PLAYING in capture/metadata mode (esp.
   * not with sinks that don't sync to the clock), we'll get everything
   * we need by prerolling the pipeline, and that is done in _open() */
  if (bvw->priv->use_type == BVW_USE_TYPE_CAPTURE ||
      bvw->priv->use_type == BVW_USE_TYPE_METADATA) {
    return TRUE;
  }

  GST_DEBUG ("play");
  gst_element_set_state (bvw->priv->play, GST_STATE_PLAYING);

  ret = poll_for_state_change (bvw, bvw->priv->play, GST_STATE_PLAYING, error);

  return ret;
}

gboolean
bacon_video_widget_can_direct_seek (BaconVideoWidget *bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);

  if (!bvw->priv->mrl)
    return FALSE;

  /* (instant seeking only make sense with video, hence no cdda:// here) */
  if (g_str_has_prefix (bvw->priv->mrl, "file://") ||
      g_str_has_prefix (bvw->priv->mrl, "dvd://") ||
      g_str_has_prefix (bvw->priv->mrl, "vcd://"))
    return TRUE;

  return FALSE;
}

gboolean
bacon_video_widget_seek_time (BaconVideoWidget *bvw, gint64 time, GError **gerror)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);

  GST_LOG ("Seeking to %" GST_TIME_FORMAT, GST_TIME_ARGS (time * GST_MSECOND));

  /* Emit a time tick of where we are going, we are paused */
  got_time_tick (bvw->priv->play, time * GST_MSECOND, bvw);
  
  gst_element_seek (bvw->priv->play, 1.0,
      GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
      GST_SEEK_TYPE_SET, time * GST_MSECOND,
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

  gst_element_get_state (bvw->priv->play, NULL, NULL, 100 * GST_MSECOND);

  return TRUE;
}

gboolean
bacon_video_widget_seek (BaconVideoWidget *bvw, float position, GError **error)
{
  gint64 seek_time, length_nanos;

  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);

  length_nanos = (gint64) (bvw->priv->stream_length * GST_MSECOND);
  seek_time = (gint64) (length_nanos * position);

  GST_LOG ("Seeking to %3.2f%% %" GST_TIME_FORMAT, position,
      GST_TIME_ARGS (seek_time));

  return bacon_video_widget_seek_time (bvw, seek_time / GST_MSECOND, error);
}

static void
bvw_stop_play_pipeline (BaconVideoWidget * bvw)
{
  GstElement *playbin = bvw->priv->play;
  GstState current_state;

  /* first go to ready, that way our state change handler gets to see
   * our state change messages (before the bus goes to flushing) and
   * cleans up */
  GST_DEBUG ("stopping");
  gst_element_get_state (playbin, &current_state, NULL, 0);
  if (current_state > GST_STATE_READY) {
    GError *err = NULL;

    gst_element_set_state (playbin, GST_STATE_READY);
    poll_for_state_change_full (bvw, playbin, GST_STATE_READY, &err, -1);
    if (err)
      g_error_free (err);
  }

  /* now finally go to null state */
  GST_DEBUG ("almost stopped, setting to NULL");
  gst_element_set_state (playbin, GST_STATE_NULL);
  GST_DEBUG ("stopped");
}

void
bacon_video_widget_stop (BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));

  GST_LOG ("Stopping");
  bvw_stop_play_pipeline (bvw);
  
  /* Reset position to 0 when stopping */
  got_time_tick (GST_ELEMENT (bvw->priv->play), 0, bvw);
}

void
bacon_video_widget_close (BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));
  
  GST_LOG ("Closing");
  bvw_stop_play_pipeline (bvw);

  if (bvw->priv->mrl) {
    g_free (bvw->priv->mrl);
    bvw->priv->mrl = NULL;
  }

  g_signal_emit (bvw, bvw_signals[SIGNAL_CHANNELS_CHANGE], 0);
}

void
bacon_video_widget_dvd_event (BaconVideoWidget * bvw,
                              BaconVideoWidgetDVDEvent type)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));

  switch (type) {
    case BVW_DVD_ROOT_MENU:
    case BVW_DVD_TITLE_MENU:
    case BVW_DVD_SUBPICTURE_MENU:
    case BVW_DVD_AUDIO_MENU:
    case BVW_DVD_ANGLE_MENU:
    case BVW_DVD_CHAPTER_MENU:
      /* FIXME */
      GST_WARNING ("FIXME: implement type %d", type);
      break;
    case BVW_DVD_NEXT_CHAPTER:
    case BVW_DVD_PREV_CHAPTER:
    case BVW_DVD_NEXT_TITLE:
    case BVW_DVD_PREV_TITLE:
    case BVW_DVD_NEXT_ANGLE:
    case BVW_DVD_PREV_ANGLE: {
      const gchar *fmt_name;
      GstFormat fmt;
      gint64 val;
      gint dir;

      if (type == BVW_DVD_NEXT_CHAPTER ||
          type == BVW_DVD_NEXT_TITLE ||
          type == BVW_DVD_NEXT_ANGLE)
        dir = 1;
      else
        dir = -1;

      if (type == BVW_DVD_NEXT_CHAPTER || type == BVW_DVD_PREV_CHAPTER)
        fmt_name = "chapter"; 
      else if (type == BVW_DVD_NEXT_TITLE || type == BVW_DVD_PREV_TITLE)
        fmt_name = "title";
      else
        fmt_name = "angle";

      fmt = gst_format_get_by_nick (fmt_name);
      if (gst_element_query_position (bvw->priv->play, &fmt, &val)) {
        GST_DEBUG ("current %s is: %" G_GINT64_FORMAT, fmt_name, val);
        val += dir;
        GST_DEBUG ("seeking to %s: %" G_GINT64_FORMAT, val);
        gst_element_seek (bvw->priv->play, 1.0, fmt, GST_SEEK_FLAG_FLUSH,
            GST_SEEK_TYPE_SET, val, GST_SEEK_TYPE_NONE, 0);
      } else {
        GST_DEBUG ("failed to query position (%s)", fmt_name);
      }
      break;
    }
    default:
      GST_WARNING ("unhandled type %d", type);
      break;
  }
}

void
bacon_video_widget_set_logo (BaconVideoWidget * bvw, gchar * filename)
{
  GError *error = NULL;
  
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (bvw->priv->logo_pixbuf == NULL);

  bvw->priv->logo_pixbuf = gdk_pixbuf_new_from_file (filename, &error);

  if (error) {
    g_warning ("An error occurred trying to open logo %s: %s",
               filename, error->message);
    g_error_free (error);
  }
}

void
bacon_video_widget_set_logo_mode (BaconVideoWidget * bvw, gboolean logo_mode)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->priv->logo_mode = logo_mode;

  if (bvw->priv->video_window) {
    if (logo_mode) {
      gdk_window_hide (bvw->priv->video_window);
    } else {
      gdk_window_show (bvw->priv->video_window);
    }
  }
  
  /* Queue a redraw of the widget */
  gtk_widget_queue_draw (GTK_WIDGET (bvw));
}

gboolean
bacon_video_widget_get_logo_mode (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

  return bvw->priv->logo_mode;
}

void
bacon_video_widget_pause (BaconVideoWidget * bvw)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));

  GST_LOG ("Pausing");
  gst_element_set_state (GST_ELEMENT (bvw->priv->play), GST_STATE_PAUSED);
}

void
bacon_video_widget_set_subtitle_font (BaconVideoWidget * bvw,
    				      const gchar * font)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));

  if (!g_object_class_find_property (G_OBJECT_GET_CLASS (bvw->priv->play), "subtitle-font-desc"))
    return;
  g_object_set (bvw->priv->play, "subtitle-font-desc", font, NULL);
}

void
bacon_video_widget_set_subtitle_encoding (BaconVideoWidget *bvw,
					  const char *encoding)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));

  if (!g_object_class_find_property (G_OBJECT_GET_CLASS (bvw->priv->play), "subtitle-encoding"))
    return;
  g_object_set (bvw->priv->play, "subtitle-encoding", encoding, NULL);
}

gboolean
bacon_video_widget_can_set_volume (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);
  
  return TRUE;
}

void
bacon_video_widget_set_volume (BaconVideoWidget * bvw, int volume)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));

  if (bacon_video_widget_can_set_volume (bvw) != FALSE)
  {
    volume = CLAMP (volume, 0, 100);
    g_object_set (bvw->priv->play, "volume",
	(gdouble) (1. * volume / 100), NULL);
  }
}

int
bacon_video_widget_get_volume (BaconVideoWidget * bvw)
{
  gdouble vol;

  g_return_val_if_fail (bvw != NULL, -1);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), -1);

  g_object_get (G_OBJECT (bvw->priv->play), "volume", &vol, NULL);

  return (gint) (vol * 100 + 0.5);
}

gboolean
bacon_video_widget_fullscreen_mode_available (BaconVideoWidget *bvw,
		TvOutType tvout) 
{
	switch(tvout) {
	case TV_OUT_NONE:
		/* Asume that ordinary fullscreen always works */
		return TRUE;
	case TV_OUT_NVTV_NTSC:
	case TV_OUT_NVTV_PAL:
#ifdef HAVE_NVTV
		/* Make sure nvtv is initialized, it will not do any harm 
		 * if it is done twice any way */
		if (!(nvtv_simple_init() && nvtv_enable_autoresize(TRUE))) {
			nvtv_simple_enable(FALSE);
		}
		return (nvtv_simple_is_available());
#else
		return FALSE;
#endif
	}
	return FALSE;
}

void
bacon_video_widget_set_fullscreen (BaconVideoWidget * bvw,
				   gboolean fullscreen)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->priv->fullscreen_mode = fullscreen;

#ifdef HAVE_NVTV
  if (bvw->priv->tv_out_type != TV_OUT_NVTV_NTSC &&
      bvw->priv->tv_out_type != TV_OUT_NVTV_PAL)
    return;

  if (fullscreen == FALSE)
  {
    /* If NVTV is used */
    if (nvtv_simple_get_state() == NVTV_SIMPLE_TV_ON) {
      nvtv_simple_switch(NVTV_SIMPLE_TV_OFF,0,0);

    }
    /* Turn fullscreen on with NVTV if that option is on */
  } else if ((bvw->priv->tv_out_type == TV_OUT_NVTV_NTSC) ||
      (bvw->priv->tv_out_type == TV_OUT_NVTV_PAL)) {
    nvtv_simple_switch(NVTV_SIMPLE_TV_ON,
	bvw->priv->video_width,
	bvw->priv->video_height);
  }
#endif
}

void
bacon_video_widget_set_show_cursor (BaconVideoWidget * bvw,
				    gboolean show_cursor)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  
  bvw->priv->cursor_shown = show_cursor;
  
  if (!GTK_WIDGET (bvw)->window) {
    return;
  }

  if (show_cursor == FALSE) {
    totem_gdk_window_set_invisible_cursor (GTK_WIDGET (bvw)->window);
  } else {
    gdk_window_set_cursor (GTK_WIDGET (bvw)->window, NULL);
  }
}

gboolean
bacon_video_widget_get_show_cursor (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

  return bvw->priv->cursor_shown;
}

void
bacon_video_widget_set_media_device (BaconVideoWidget * bvw, const char *path)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));
  
  g_free (bvw->priv->media_device);
  bvw->priv->media_device = g_strdup (path);
}

static void
get_visualization_size (BaconVideoWidget *bvw,
			int *w, int *h, gint *fps_n, gint *fps_d)
{
  gint new_fps_n, new_fps_d;

  /* now see how close we can go */
  switch (bvw->priv->visq) {
    case VISUAL_SMALL:
      new_fps_n = 10;
      new_fps_d = 1;
      *w = 200;
      *h = 150;
      break;
    case VISUAL_NORMAL:
      new_fps_n = 20;
      new_fps_d = 1;
      *w = 320;
      *h = 240;
      break;
    case VISUAL_LARGE:
      new_fps_n = 25;
      new_fps_d = 1;
      *w = 640;
      *h = 480;
      break;
    case VISUAL_EXTRA_LARGE:
      new_fps_n = 30;
      new_fps_d = 1;
      *w = 800;
      *h = 600;
      break;
    default:
      /* shut up warnings */
      *w = *h = 0;
      new_fps_n = 0;
      new_fps_d = 1;
      g_assert_not_reached ();
  }
  if (fps_n) 
    *fps_n = new_fps_n;
  if (fps_d) 
    *fps_d = new_fps_d;
}

static GstElementFactory *
setup_vis_find_factory (BaconVideoWidget * bvw, const gchar * vis_name)
{
  GstElementFactory *fac = NULL;
  GList *l, *features;

  features = get_visualization_features ();

  /* find element factory using long name */
  for (l = features; l != NULL; l = l->next) {
    GstElementFactory *f = GST_ELEMENT_FACTORY (l->data);
       
    if (f && strcmp (vis_name, gst_element_factory_get_longname (f)) == 0) {
      fac = f;
      goto done;
    }
  }

  /* if nothing was found, try the short name (the default schema uses this) */
  for (l = features; l != NULL; l = l->next) {
    GstElementFactory *f = GST_ELEMENT_FACTORY (l->data);

    /* set to long name as key so that the preferences dialog gets it right */
    if (f && strcmp (vis_name, GST_PLUGIN_FEATURE_NAME (f)) == 0) {
      gconf_client_set_string (bvw->priv->gc, GCONF_PREFIX "/visual",
          gst_element_factory_get_longname (f), NULL);
      fac = f;
      goto done;
    }
  }

done:
  g_list_free (features);
  return fac;
}

static void
setup_vis (BaconVideoWidget * bvw)
{
  GstElement *vis_bin = NULL;

  GST_DEBUG ("setup_vis called, show_vfx %d, vis element %s",
      bvw->priv->show_vfx, bvw->priv->vis_element_name);
  
  if (bvw->priv->show_vfx && bvw->priv->vis_element_name) {
    GstElement *vis_element = NULL, *vis_capsfilter = NULL;
    GstPad *pad = NULL;
    GstCaps *caps = NULL;
    GstElementFactory *fac = NULL;
    
    fac = setup_vis_find_factory (bvw, bvw->priv->vis_element_name);
    if (!fac) {
      GST_DEBUG ("Could not find element factory for visualisation '%s'",
          GST_STR_NULL (bvw->priv->vis_element_name));
      /* use goom as fallback, better than nothing */
      fac = setup_vis_find_factory (bvw, "goom");
      if (fac == NULL) {
        goto beach;
      } else {
        GST_DEBUG ("Falling back on 'goom' for visualisation");
      }     
    }
    
    vis_element = gst_element_factory_create (fac, "vis_element");
    if (!GST_IS_ELEMENT (vis_element)) {
      GST_DEBUG ("failed creating visualisation element");
      goto beach;
    }
    
    vis_capsfilter = gst_element_factory_make ("capsfilter",
        "vis_capsfilter");
    if (!GST_IS_ELEMENT (vis_capsfilter)) {
      GST_DEBUG ("failed creating visualisation capsfilter element");
      gst_object_unref (vis_element);
      goto beach;
    }
    
    vis_bin = gst_bin_new ("vis_bin");
    if (!GST_IS_ELEMENT (vis_bin)) {
      GST_DEBUG ("failed creating visualisation bin");
      gst_object_unref (vis_element);
      gst_object_unref (vis_capsfilter);
      goto beach;
    }
    
    gst_bin_add_many (GST_BIN (vis_bin), vis_element, vis_capsfilter, NULL);
    
    /* Sink ghostpad */
    pad = gst_element_get_pad (vis_element, "sink");
    gst_element_add_pad (vis_bin, gst_ghost_pad_new ("sink", pad));
    gst_object_unref (pad);

    /* Source ghostpad, link with vis_element */
    pad = gst_element_get_pad (vis_capsfilter, "src");
    gst_element_add_pad (vis_bin, gst_ghost_pad_new ("src", pad));
    gst_element_link_pads (vis_element, "src", vis_capsfilter, "sink");
    gst_object_unref (pad);

    /* Get allowed output caps from visualisation element */
    pad = gst_element_get_pad (vis_element, "src");
    caps = gst_pad_get_allowed_caps (pad);
    gst_object_unref (pad);
    
    GST_DEBUG ("allowed caps: %" GST_PTR_FORMAT, caps);
    
    /* Can we fixate ? */
    if (caps && !gst_caps_is_fixed (caps)) {
      guint i;
      gint w, h, fps_n, fps_d;

      caps = gst_caps_make_writable (caps);

      /* Get visualization size */
      get_visualization_size (bvw, &w, &h, &fps_n, &fps_d);

      for (i = 0; i < gst_caps_get_size (caps); ++i) {
        GstStructure *s = gst_caps_get_structure (caps, i);
      
        /* Fixate */
        gst_structure_fixate_field_nearest_int (s, "width", w);
        gst_structure_fixate_field_nearest_int (s, "height", h);
        gst_structure_fixate_field_nearest_fraction (s, "framerate", fps_n,
            fps_d);
      }

      /* set this */
      g_object_set (vis_capsfilter, "caps", caps, NULL);
    }

    GST_DEBUG ("visualisation caps: %" GST_PTR_FORMAT, caps);
    
    if (GST_IS_CAPS (caps)) {
      gst_caps_unref (caps);
    }
  }
  
  bvw->priv->vis_changed = FALSE;
  
beach:
  g_object_set (bvw->priv->play, "vis-plugin", vis_bin, NULL);
  
  return;
}

gboolean
bacon_video_widget_set_show_visuals (BaconVideoWidget * bvw,
				     gboolean show_visuals)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);

  bvw->priv->show_vfx = show_visuals;
  bvw->priv->vis_changed = TRUE;
  
  return FALSE;
}

static gboolean
filter_features (GstPluginFeature * feature, gpointer data)
{
  GstElementFactory *f;

  if (!GST_IS_ELEMENT_FACTORY (feature))
    return FALSE;
  f = GST_ELEMENT_FACTORY (feature);
  if (!g_strrstr (gst_element_factory_get_klass (f), "Visualization"))
    return FALSE;

  return TRUE;
}

static GList *
get_visualization_features (void)
{
  return gst_registry_feature_filter (gst_registry_get_default (),
      filter_features, FALSE, NULL);
}

static void
add_longname (GstElementFactory *f, GList ** to)
{
  *to = g_list_append (*to, (gchar *) gst_element_factory_get_longname (f));
}

GList *
bacon_video_widget_get_visuals_list (BaconVideoWidget * bvw)
{
  GList *features, *names = NULL;

  g_return_val_if_fail (bvw != NULL, NULL);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), NULL);

  if (bvw->priv->vis_plugins_list) {
    return bvw->priv->vis_plugins_list;
  }

  features = get_visualization_features ();
  g_list_foreach (features, (GFunc) add_longname, &names);
  g_list_free (features);
  bvw->priv->vis_plugins_list = names;

  return names;
}

gboolean
bacon_video_widget_set_visuals (BaconVideoWidget * bvw, const char *name)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);
  
  if (bvw->priv->vis_element_name) {
    if (strcmp (bvw->priv->vis_element_name, name) == 0) {
      return FALSE;
    }
    else {
      g_free (bvw->priv->vis_element_name);
    }
  }
  
  bvw->priv->vis_element_name = g_strdup (name);

  GST_DEBUG ("new visualisation element name = '%s'", GST_STR_NULL (name));
  
  setup_vis (bvw);
  
  return FALSE;
}

void
bacon_video_widget_set_visuals_quality (BaconVideoWidget * bvw,
					VisualsQuality quality)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));

  if (bvw->priv->visq == quality)
    return;

  bvw->priv->visq = quality;
  
  setup_vis (bvw);
}

gboolean
bacon_video_widget_get_auto_resize (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

  return bvw->priv->auto_resize;
}

void
bacon_video_widget_set_auto_resize (BaconVideoWidget * bvw,
				    gboolean auto_resize)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->priv->auto_resize = auto_resize;

  /* this will take effect when the next media file loads */
}

void
bacon_video_widget_set_aspect_ratio (BaconVideoWidget *bvw,
				BaconVideoWidgetAspectRatio ratio)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->priv->ratio_type = ratio;
  got_video_size (bvw);
}

BaconVideoWidgetAspectRatio
bacon_video_widget_get_aspect_ratio (BaconVideoWidget *bvw)
{
  g_return_val_if_fail (bvw != NULL, 0);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);

  return bvw->priv->ratio_type;
}

void
bacon_video_widget_set_scale_ratio (BaconVideoWidget * bvw, gfloat ratio)
{
  gint w, h;

  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));

  get_media_size (bvw, &w, &h);
  if (ratio == 0) {
    if (totem_ratio_fits_screen (bvw->priv->video_window, w, h, 2.0))
      ratio = 2.0;
    else if (totem_ratio_fits_screen (bvw->priv->video_window, w, h, 1.0))
      ratio = 1.0;
    else if (totem_ratio_fits_screen (bvw->priv->video_window, w, h, 0.5))
      ratio = 0.5;
    else
      return;
  } else {
    if (!totem_ratio_fits_screen (bvw->priv->video_window, w, h, ratio))
      return;
  }
  w = (gfloat) w * ratio;
  h = (gfloat) h * ratio;
  shrink_toplevel (bvw);
  totem_widget_set_preferred_size (GTK_WIDGET (bvw), w, h);
}

gboolean
bacon_video_widget_can_set_zoom (BaconVideoWidget *bvw)
{
  return FALSE;
}

void
bacon_video_widget_set_zoom (BaconVideoWidget *bvw,
			     int               zoom)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  /* implement me */
}

int
bacon_video_widget_get_zoom (BaconVideoWidget *bvw)
{
  g_return_val_if_fail (bvw != NULL, 100);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 100);

  return 100;
}

int
bacon_video_widget_get_video_property (BaconVideoWidget *bvw,
                                       BaconVideoWidgetVideoProperty type)
{
  int ret;

  g_return_val_if_fail (bvw != NULL, 65535/2);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 65535/2);
  
  g_mutex_lock (bvw->priv->lock);
  
  if (bvw->priv->balance && GST_IS_COLOR_BALANCE (bvw->priv->balance))
    {
      const GList *channels_list = NULL;
      GstColorBalanceChannel *found_channel = NULL;
      
      channels_list = gst_color_balance_list_channels (bvw->priv->balance);
      
      while (channels_list != NULL && found_channel == NULL)
        { /* We search for the right channel corresponding to type */
          GstColorBalanceChannel *channel = channels_list->data;

          if (type == BVW_VIDEO_BRIGHTNESS && channel &&
              g_strrstr (channel->label, "BRIGHTNESS"))
            {
              g_object_ref (channel);
              found_channel = channel;
            }
          else if (type == BVW_VIDEO_CONTRAST && channel &&
              g_strrstr (channel->label, "CONTRAST"))
            {
              g_object_ref (channel);
              found_channel = channel;
            }
          else if (type == BVW_VIDEO_SATURATION && channel &&
              g_strrstr (channel->label, "SATURATION"))
            {
              g_object_ref (channel);
              found_channel = channel;
            }
          else if (type == BVW_VIDEO_HUE && channel &&
              g_strrstr (channel->label, "HUE"))
            {
              g_object_ref (channel);
              found_channel = channel;
            }
          channels_list = g_list_next (channels_list);
        }
        
      if (found_channel && GST_IS_COLOR_BALANCE_CHANNEL (found_channel)) {
        gint cur;

        cur = gst_color_balance_get_value (bvw->priv->balance,
                                           found_channel);

        GST_DEBUG ("channel %s: cur=%d, min=%d, max=%d", found_channel->label,
            cur, found_channel->min_value, found_channel->max_value);

        ret = ((double) cur - found_channel->min_value) * 65535 /
              ((double) found_channel->max_value - found_channel->min_value);

        GST_DEBUG ("channel %s: returning value %d", found_channel->label, ret);
        g_object_unref (found_channel);
        goto done;
      }
    }

  /* value wasn't found, get from gconf */
  ret = gconf_client_get_int (bvw->priv->gc, video_props_str[type], NULL);

  GST_DEBUG ("nothing found for type %d, returning value %d from gconf key %s",
      type, ret, video_props_str[type]);

done:

  g_mutex_unlock (bvw->priv->lock);
  return ret;
}

void
bacon_video_widget_set_video_property (BaconVideoWidget *bvw,
                                       BaconVideoWidgetVideoProperty type,
                                       int value)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  
  GST_DEBUG ("set video property type %d to value %d", type, value);
  
  if ( !(value < 65535 && value > 0) )
    return;

  if (bvw->priv->balance && GST_IS_COLOR_BALANCE (bvw->priv->balance))
    {
      const GList *channels_list = NULL;
      GstColorBalanceChannel *found_channel = NULL;
      
      channels_list = gst_color_balance_list_channels (bvw->priv->balance);

      while (found_channel == NULL && channels_list != NULL) {
          /* We search for the right channel corresponding to type */
          GstColorBalanceChannel *channel = channels_list->data;

          if (type == BVW_VIDEO_BRIGHTNESS && channel &&
              g_strrstr (channel->label, "BRIGHTNESS"))
            {
              g_object_ref (channel);
              found_channel = channel;
            }
          else if (type == BVW_VIDEO_CONTRAST && channel &&
              g_strrstr (channel->label, "CONTRAST"))
            {
              g_object_ref (channel);
              found_channel = channel;
            }
          else if (type == BVW_VIDEO_SATURATION && channel &&
              g_strrstr (channel->label, "SATURATION"))
            {
              g_object_ref (channel);
              found_channel = channel;
            }
          else if (type == BVW_VIDEO_HUE && channel &&
              g_strrstr (channel->label, "HUE"))
            {
              g_object_ref (channel);
              found_channel = channel;
            }
          channels_list = g_list_next (channels_list);
        }

      if (found_channel && GST_IS_COLOR_BALANCE_CHANNEL (found_channel))
        {
          int i_value = value * ((double) found_channel->max_value -
              found_channel->min_value) / 65535 + found_channel->min_value;

          GST_DEBUG ("channel %s: set to %d/65535", found_channel->label, value);

          gst_color_balance_set_value (bvw->priv->balance, found_channel,
                                       i_value);

          GST_DEBUG ("channel %s: val=%d, min=%d, max=%d", found_channel->label,
              i_value, found_channel->min_value, found_channel->max_value);

          g_object_unref (found_channel);
        }
    }

  /* save in gconf */
  gconf_client_set_int (bvw->priv->gc, video_props_str[type], value, NULL);

  GST_DEBUG ("setting value %d on gconf key %s", value, video_props_str[type]);
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

  if (bvw->priv->stream_length == 0 && bvw->priv->play != NULL) {
    GstFormat fmt = GST_FORMAT_TIME;
    gint64 len = -1;

    if (gst_element_query_duration (bvw->priv->play, &fmt, &len) && len != -1) {
      bvw->priv->stream_length = len / GST_MSECOND;
    }
  }

  return bvw->priv->stream_length;
}

gboolean
bacon_video_widget_is_playing (BaconVideoWidget * bvw)
{
  GstState cur, pending;

  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);

  gst_element_get_state (bvw->priv->play, &cur, &pending, 0);

  if (cur == GST_STATE_PLAYING || pending == GST_STATE_PLAYING)
    return TRUE;

  /* just lie (see _play() above) */
  if ((bvw->priv->use_type == BVW_USE_TYPE_CAPTURE
       || bvw->priv->use_type == BVW_USE_TYPE_METADATA) &&
       (cur >= GST_STATE_PAUSED || pending >= GST_STATE_PAUSED)) {
    return TRUE;
  }

  return FALSE;
}

gboolean
bacon_video_widget_is_seekable (BaconVideoWidget * bvw)
{
  gboolean res;

  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);

  if (bvw->priv->stream_length == 0) {
    res = (bacon_video_widget_get_stream_length (bvw) > 0);
  } else {
    res = (bvw->priv->stream_length > 0);
  }

  GST_DEBUG ("stream is%s seekable", (res) ? "" : " not");
  return res;
}

gboolean
bacon_video_widget_can_play (BaconVideoWidget * bvw, MediaType type)
{
  gboolean res;

  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);

  switch (type) {
    case MEDIA_TYPE_CDDA:
    case MEDIA_TYPE_VCD:
      res = TRUE;
      break;
    case MEDIA_TYPE_DVD:
    default:
      res = FALSE;
      break;
  }

  GST_DEBUG ("type=%d, can_play=%s", type, (res) ? "TRUE" : "FALSE");
  return res;
}

gchar **
bacon_video_widget_get_mrls (BaconVideoWidget * bvw, MediaType type)
{
  gchar **mrls;

  g_return_val_if_fail (bvw != NULL, NULL);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), NULL);

  GST_DEBUG ("type = %d", type);

  switch (type) {
    case MEDIA_TYPE_CDDA: {
      GstStateChangeReturn ret;
      GstElement *cddasrc;
      GstFormat fmt;
      GstPad *pad;
      gint64 num_tracks = 0;
      gchar *uri[] = { "cdda://", NULL };
      gint i;

      GST_DEBUG ("Checking for Audio CD sources (cdda://) ...");
      cddasrc = gst_element_make_from_uri (GST_URI_SRC, "cdda://1", NULL);
      if (!cddasrc) {
        GST_DEBUG ("No Audio CD source plugins found");
        return NULL;
      }

      fmt = gst_format_get_by_nick ("track");
      if (!fmt) {
        gst_object_unref (cddasrc);
        return NULL;
      }

      /* FIXME: what about setting the device? */

      GST_DEBUG ("Opening CD and getting number of tracks ...");

      /* wait for state change to complete or fail */
      gst_element_set_state (cddasrc, GST_STATE_PAUSED);
      ret = gst_element_get_state (cddasrc, NULL, NULL, -1);
      if (ret == GST_STATE_CHANGE_FAILURE) {
        GST_DEBUG ("Couldn't set cdda source to PAUSED");
        gst_element_set_state (cddasrc, GST_STATE_NULL);
        gst_object_unref (cddasrc);
        return NULL;
      }

      pad = gst_element_get_pad (cddasrc, "src");
      if (gst_pad_query_duration (pad, &fmt, &num_tracks) && num_tracks > 0) {
        GST_DEBUG ("%" G_GINT64_FORMAT " tracks", num_tracks);
        mrls = g_new0 (gchar *, num_tracks + 1);
        for (i = 1; i <= num_tracks; ++i) {
          mrls[i-1] = g_strdup_printf ("cdda://%d", i);
        }
      } else {
        GST_DEBUG ("could not query track number");
        mrls = g_strdupv (uri);
      }
      gst_object_unref (pad);

      gst_element_set_state (cddasrc, GST_STATE_NULL);
      gst_object_unref (cddasrc);
      break;
    }
    case MEDIA_TYPE_VCD: {
      gchar *uri[] = { "vcd://", NULL };
      mrls = g_strdupv (uri);
      break;
    }
/*
    case MEDIA_TYPE_DVD: {
      gchar *uri[] = { "dvd://", NULL };
      mrls = g_strdupv (uri);
      break;
    }
*/
    default:
      mrls = NULL;
      break;
  }

  return mrls;
}

static struct _metadata_map_info {
  BaconVideoWidgetMetadataType type;
  const gchar *str;
} metadata_str_map[] = {
  { BVW_INFO_TITLE, "title" },
  { BVW_INFO_ARTIST, "artist" },
  { BVW_INFO_YEAR, "year" },
  { BVW_INFO_ALBUM, "album" },
  { BVW_INFO_DURATION, "duration" },
  { BVW_INFO_TRACK_NUMBER, "track-number" },
  { BVW_INFO_HAS_VIDEO, "has-video" },
  { BVW_INFO_DIMENSION_X, "dimension-x" },
  { BVW_INFO_DIMENSION_Y, "dimension-y" },
  { BVW_INFO_VIDEO_BITRATE, "video-bitrate" },
  { BVW_INFO_VIDEO_CODEC, "video-codec" },
  { BVW_INFO_FPS, "fps" },
  { BVW_INFO_HAS_AUDIO, "has-audio" },
  { BVW_INFO_AUDIO_BITRATE, "audio-bitrate" },
  { BVW_INFO_AUDIO_CODEC, "audio-codec" }
};

static const gchar *
get_metadata_type_name (BaconVideoWidgetMetadataType type)
{
  guint i;
  for (i = 0; i < G_N_ELEMENTS (metadata_str_map); ++i) {
    if (metadata_str_map[i].type == type)
      return metadata_str_map[i].str;
  }
  return "unknown";
}

static void
bacon_video_widget_get_metadata_string (BaconVideoWidget * bvw,
                                        BaconVideoWidgetMetadataType type,
                                        GValue * value)
{
  char *string = NULL;
  gboolean res = FALSE;

  g_value_init (value, G_TYPE_STRING);

  if (bvw->priv->play == NULL || bvw->priv->tagcache == NULL)
    {
      g_value_set_string (value, NULL);
      return;
    }

  switch (type)
    {
    case BVW_INFO_TITLE:
      res = gst_tag_list_get_string_index (bvw->priv->tagcache,
					   GST_TAG_TITLE, 0, &string);
      break;
    case BVW_INFO_ARTIST:
      res = gst_tag_list_get_string_index (bvw->priv->tagcache,
					   GST_TAG_ARTIST, 0, &string);
      break;
    case BVW_INFO_YEAR: {
      GDate *date;
      if ((res = gst_tag_list_get_date (bvw->priv->tagcache,
					GST_TAG_DATE, &date))) {
        string = g_strdup_printf ("%d", g_date_get_year (date));
        g_date_free (date);
      }
      break;
    }
    case BVW_INFO_ALBUM:
      res = gst_tag_list_get_string_index (bvw->priv->tagcache,
					   GST_TAG_ALBUM, 0, &string);
      break;
    case BVW_INFO_VIDEO_CODEC:
      res = gst_tag_list_get_string (bvw->priv->tagcache,
				     GST_TAG_VIDEO_CODEC, &string);
      break;
    case BVW_INFO_AUDIO_CODEC:
      res = gst_tag_list_get_string (bvw->priv->tagcache,
				     GST_TAG_AUDIO_CODEC, &string);
      break;
    default:
      g_assert_not_reached ();
    }

  if (res) {
    g_value_take_string (value, string);
    GST_DEBUG ("%s = '%s'", get_metadata_type_name (type), string);
  } else {
    g_value_set_string (value, NULL);
  }

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
    case BVW_INFO_TRACK_NUMBER:
      if (!gst_tag_list_get_uint (bvw->priv->tagcache,
              GST_TAG_TRACK_NUMBER, (guint *) &integer))
        integer = 0;
      break;
    case BVW_INFO_DIMENSION_X:
      integer = bvw->priv->video_width;
      break;
    case BVW_INFO_DIMENSION_Y:
      integer = bvw->priv->video_height;
      break;
    case BVW_INFO_FPS:
      if (bvw->priv->video_fps_d > 0) {
        /* Round up/down to the nearest integer framerate */
        integer = (bvw->priv->video_fps_n + bvw->priv->video_fps_d/2) /
                  bvw->priv->video_fps_d;
      }
      else
        integer = 0;
      break;
    case BVW_INFO_AUDIO_BITRATE:
      if (bvw->priv->audiotags == NULL)
        break;
      if (gst_tag_list_get_uint (bvw->priv->audiotags, GST_TAG_BITRATE,
          (guint *)&integer) ||
          gst_tag_list_get_uint (bvw->priv->audiotags, GST_TAG_NOMINAL_BITRATE,
          (guint *)&integer)) {
        integer /= 1000;
      }
      break;
    case BVW_INFO_VIDEO_BITRATE:
      if (bvw->priv->videotags == NULL)
        break;
      if (gst_tag_list_get_uint (bvw->priv->videotags, GST_TAG_BITRATE,
          (guint *)&integer) ||
          gst_tag_list_get_uint (bvw->priv->videotags, GST_TAG_NOMINAL_BITRATE,
          (guint *)&integer)) {
        integer /= 1000;
      }
      break;
    default:
      g_assert_not_reached ();
    }

  g_value_set_int (value, integer);
  GST_DEBUG ("%s = %d", get_metadata_type_name (type), integer);

  return;
}

static void
bacon_video_widget_get_metadata_bool (BaconVideoWidget * bvw,
                                      BaconVideoWidgetMetadataType type,
                                      GValue * value)
{
  gboolean boolean = FALSE;

  g_value_init (value, G_TYPE_BOOLEAN);

  if (bvw->priv->play == NULL) {
    g_value_set_boolean (value, FALSE);
    return;
  }

  GST_DEBUG ("tagcache  = %" GST_PTR_FORMAT, bvw->priv->tagcache);
  GST_DEBUG ("videotags = %" GST_PTR_FORMAT, bvw->priv->videotags);
  GST_DEBUG ("audiotags = %" GST_PTR_FORMAT, bvw->priv->audiotags);

  switch (type)
  {
    case BVW_INFO_HAS_VIDEO:
      boolean = bvw->priv->media_has_video;
      /* if properties dialog, show the metadata we
       * have even if we cannot decode the stream */
      if (!boolean && bvw->priv->use_type == BVW_USE_TYPE_METADATA &&
          bvw->priv->tagcache != NULL &&
          gst_structure_has_field ((GstStructure *) bvw->priv->tagcache,
                                   GST_TAG_VIDEO_CODEC)) {
        boolean = TRUE;
      }
      break;
    case BVW_INFO_HAS_AUDIO:
      boolean = bvw->priv->media_has_audio;
      /* if properties dialog, show the metadata we
       * have even if we cannot decode the stream */
      if (!boolean && bvw->priv->use_type == BVW_USE_TYPE_METADATA &&
          bvw->priv->tagcache != NULL &&
          gst_structure_has_field ((GstStructure *) bvw->priv->tagcache,
                                   GST_TAG_AUDIO_CODEC)) {
        boolean = TRUE;
      }
      break;
    default:
      g_assert_not_reached ();
  }

  g_value_set_boolean (value, boolean);
  GST_DEBUG ("%s = %s", get_metadata_type_name (type), (boolean) ? "yes" : "no");

  return;
}

static void
bvw_process_pending_tag_messages (BaconVideoWidget * bvw)
{
  GstMessageType events;
  GstMessage *msg;
  GstBus *bus;
    
  /* process any pending tag messages on the bus NOW, so we can get to
   * the information without/before giving control back to the main loop */

  /* application message is for stream-info */
  events = GST_MESSAGE_TAG | GST_MESSAGE_DURATION | GST_MESSAGE_APPLICATION;
  bus = gst_element_get_bus (bvw->priv->play);
  while ((msg = gst_bus_poll (bus, events, 0))) {
    gst_bus_async_signal_func (bus, msg, NULL);
  }
  gst_object_unref (bus);
}

void
bacon_video_widget_get_metadata (BaconVideoWidget * bvw,
                                 BaconVideoWidgetMetadataType type,
                                 GValue * value)
{
  g_return_if_fail (bvw != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->priv->play));

  switch (type)
    {
    case BVW_INFO_TITLE:
    case BVW_INFO_ARTIST:
    case BVW_INFO_YEAR:
    case BVW_INFO_ALBUM:
    case BVW_INFO_VIDEO_CODEC:
    case BVW_INFO_AUDIO_CODEC:
      bacon_video_widget_get_metadata_string (bvw, type, value);
      break;
    case BVW_INFO_DURATION:
    case BVW_INFO_DIMENSION_X:
    case BVW_INFO_DIMENSION_Y:
    case BVW_INFO_FPS:
    case BVW_INFO_AUDIO_BITRATE:
    case BVW_INFO_VIDEO_BITRATE:
    case BVW_INFO_TRACK_NUMBER:
      bacon_video_widget_get_metadata_int (bvw, type, value);
      break;
    case BVW_INFO_HAS_VIDEO:
    case BVW_INFO_HAS_AUDIO:
      bacon_video_widget_get_metadata_bool (bvw, type, value);
      break;
    default:
      g_return_if_reached ();
    }

  return;
}

/* Screenshot functions */
gboolean
bacon_video_widget_can_get_frames (BaconVideoWidget * bvw, GError ** error)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), FALSE);

  /* check for version */
  if (!g_object_class_find_property (
           G_OBJECT_GET_CLASS (bvw->priv->play), "frame")) {
    g_set_error (error, BVW_ERROR, BVW_ERROR_GENERIC,
        _("Too old version of GStreamer installed."));
    return FALSE;
  }

  /* check for video */
  if (!bvw->priv->media_has_video) {
    g_set_error (error, BVW_ERROR, BVW_ERROR_GENERIC,
        _("Media contains no supported video streams."));
  }

  return bvw->priv->media_has_video;
}

static void
destroy_pixbuf (guchar *pix, gpointer data)
{
  gst_buffer_unref (GST_BUFFER (data));
}

GdkPixbuf *
bacon_video_widget_get_current_frame (BaconVideoWidget * bvw)
{
  GstStructure *s;
  GstBuffer *buf = NULL;
  GdkPixbuf *pixbuf;
  GstCaps *to_caps;
  gint outwidth = 0;
  gint outheight = 0;

  g_return_val_if_fail (bvw != NULL, NULL);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->priv->play), NULL);

  /* when used as thumbnailer, wait for pending seeks to complete */
  if (bvw->priv->use_type == BVW_USE_TYPE_CAPTURE) {
    gst_element_get_state (bvw->priv->play, NULL, NULL, -1);
  }

  /* no video info */
  if (!bvw->priv->video_width || !bvw->priv->video_height) {
    GST_DEBUG ("Could not take screenshot: %s", "no video info");
    g_warning ("Could not take screenshot: %s", "no video info");
    return NULL;
  }

  /* get frame */
  g_object_get (bvw->priv->play, "frame", &buf, NULL);

  if (!buf) {
    GST_DEBUG ("Could not take screenshot: %s", "no last video frame");
    g_warning ("Could not take screenshot: %s", "no last video frame");
    return NULL;
  }

  if (GST_BUFFER_CAPS (buf) == NULL) {
    GST_DEBUG ("Could not take screenshot: %s", "no caps on buffer");
    g_warning ("Could not take screenshot: %s", "no caps on buffer");
    return NULL;
  }

  /* convert to our desired format (RGB24) */
  to_caps = gst_caps_new_simple ("video/x-raw-rgb",
      "bpp", G_TYPE_INT, 24,
      "depth", G_TYPE_INT, 24,
      /* Note: we don't ask for a specific width/height here, so that
       * videoscale can adjust dimensions from a non-1/1 pixel aspect
       * ratio to a 1/1 pixel-aspect-ratio */
      "framerate", GST_TYPE_FRACTION, 
      bvw->priv->video_fps_n, bvw->priv->video_fps_d,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
      "endianness", G_TYPE_INT, G_BIG_ENDIAN,
      "red_mask", G_TYPE_INT, 0xff0000,
      "green_mask", G_TYPE_INT, 0x00ff00,
      "blue_mask", G_TYPE_INT, 0x0000ff,
      NULL);

  GST_DEBUG ("frame caps: %" GST_PTR_FORMAT, GST_BUFFER_CAPS (buf));
  GST_DEBUG ("pixbuf caps: %" GST_PTR_FORMAT, to_caps);

  /* bvw_frame_conv_convert () takes ownership of the buffer passed */
  buf = bvw_frame_conv_convert (buf, to_caps);

  gst_caps_unref (to_caps);

  if (!buf) {
    GST_DEBUG ("Could not take screenshot: %s", "conversion failed");
    g_warning ("Could not take screenshot: %s", "conversion failed");
    return NULL;
  }

  if (!GST_BUFFER_CAPS (buf)) {
    GST_DEBUG ("Could not take screenshot: %s", "no caps on output buffer");
    g_warning ("Could not take screenshot: %s", "no caps on output buffer");
    return NULL;
  }

  s = gst_caps_get_structure (GST_BUFFER_CAPS (buf), 0);
  gst_structure_get_int (s, "width", &outwidth);
  gst_structure_get_int (s, "height", &outheight);
  g_return_val_if_fail (outwidth > 0 && outheight > 0, FALSE);

  /* create pixbuf from that - use our own destroy function */
  pixbuf = gdk_pixbuf_new_from_data (GST_BUFFER_DATA (buf),
			GDK_COLORSPACE_RGB, FALSE,
			8, outwidth, outheight,
			GST_ROUND_UP_4 (outwidth * 3),
			destroy_pixbuf, buf);

  if (!pixbuf) {
    GST_DEBUG ("Could not take screenshot: %s", "could not create pixbuf");
    g_warning ("Could not take screenshot: %s", "could not create pixbuf");
    gst_buffer_unref (buf);
  }

  return pixbuf;
}

static void
cb_gconf (GConfClient * client,
	  guint connection_id,
	  GConfEntry * entry,
	  gpointer data)
{
  BaconVideoWidget *bvw = data;

  if (!strcmp (entry->key, "/apps/totem/network-buffer-threshold")) {
    g_object_set (bvw->priv->play, "queue-threshold",
        (guint64) (GST_SECOND * gconf_value_get_float (entry->value)), NULL);
  } else if (!strcmp (entry->key, "/apps/totem/buffer-size")) {
    g_object_set (bvw->priv->play, "queue-threshold",
        (guint64) (GST_SECOND * gconf_value_get_float (entry->value)), NULL);
  }
}

/* =========================================== */
/*                                             */
/*          Widget typing & Creation           */
/*                                             */
/* =========================================== */

G_DEFINE_TYPE(BaconVideoWidget, bacon_video_widget, GTK_TYPE_BOX)

static void
bacon_video_widget_init_gst (gpointer notused)
{
  gchar *version_str;

  gst_init (NULL, NULL);

  GST_DEBUG_CATEGORY_INIT (_totem_gst_debug_cat, "totem", 0,
      "Totem GStreamer Backend");

  version_str = gst_version_string ();
  GST_DEBUG ("Initialised %s", version_str);
  g_free (version_str);
}

static GOnce bvw_init_once = G_ONCE_INIT;

struct poptOption *
bacon_video_widget_get_popt_table (void)
{
  static struct poptOption gstreamer_options[] = {
    POPT_TABLEEND
  };

  g_once (&bvw_init_once, (GThreadFunc) bacon_video_widget_init_gst, NULL);

  return gstreamer_options;
}

void
bacon_video_widget_init_backend (int *argc, char ***argv)
{
  /* FIXME: Is this ever called? what about the args? */
  g_once (&bvw_init_once, (GThreadFunc) bacon_video_widget_init_gst, NULL);
}

GQuark
bacon_video_widget_error_quark (void)
{
  static GQuark q; /* 0 */

  if (q == 0) {
    q = g_quark_from_static_string ("bvw-error-quark");
  }
  return q;
}

/* fold function to pick the best colorspace element */
static gboolean
find_colorbalance_element (GstElement *element, GValue * ret, GstElement **cb)
{
  GstColorBalanceClass *cb_class;

  GST_DEBUG ("Checking element %s ...", GST_OBJECT_NAME (element));

  if (!GST_IS_COLOR_BALANCE (element))
    return TRUE;

  GST_DEBUG ("Element %s is a color balance", GST_OBJECT_NAME (element));

  cb_class = GST_COLOR_BALANCE_GET_CLASS (element);
  if (GST_COLOR_BALANCE_TYPE (cb_class) == GST_COLOR_BALANCE_HARDWARE) {
    gst_object_replace ((GstObject **) cb, (GstObject *) element);
    /* shortcuts the fold */
    return FALSE;
  } else if (*cb == NULL) {
    gst_object_replace ((GstObject **) cb, (GstObject *) element);
    return TRUE;
  } else {
    return TRUE;
  }
}

static void
bvw_update_interface_implementations (BaconVideoWidget *bvw)
{
  GstColorBalance *old_balance = bvw->priv->balance;
  GstXOverlay *old_xoverlay = bvw->priv->xoverlay;
  GConfValue *confvalue;
  GstElement *video_sink = NULL;
  GstElement *element = NULL;
  GstIteratorResult ires;
  GstIterator *iter;
  gint i;

  g_object_get (bvw->priv->play, "video-sink", &video_sink, NULL);
  g_assert (video_sink != NULL);

  /* We try to get an element supporting XOverlay interface */
  if (GST_IS_BIN (video_sink)) {
    GST_DEBUG ("Retrieving xoverlay from bin ...");
    element = gst_bin_get_by_interface (GST_BIN (video_sink),
                                        GST_TYPE_X_OVERLAY);
  } else {
    element = video_sink;
  }

  if (GST_IS_X_OVERLAY (element)) {
    GST_DEBUG ("Found xoverlay: %s", GST_OBJECT_NAME (element));
    bvw->priv->xoverlay = GST_X_OVERLAY (element);
  } else {
    GST_DEBUG ("No xoverlay found");
    bvw->priv->xoverlay = NULL;
  }

  /* Find best color balance element (using custom iterator so
   * we can prefer hardware implementations to software ones) */

  /* FIXME: this doesn't work reliably yet, most of the time
   * the fold function doesn't even get called, while sometimes
   * it does ... */
  iter = gst_bin_iterate_all_by_interface (GST_BIN (bvw->priv->play),
                                           GST_TYPE_COLOR_BALANCE);
  /* naively assume no resync */
  element = NULL;
  ires = gst_iterator_fold (iter,
      (GstIteratorFoldFunction) find_colorbalance_element, NULL, &element);
  gst_iterator_free (iter);

  if (element) {
    bvw->priv->balance = GST_COLOR_BALANCE (element);
    GST_DEBUG ("Best colorbalance found: %s",
        GST_OBJECT_NAME (bvw->priv->balance));
  } else if (GST_IS_COLOR_BALANCE (bvw->priv->xoverlay)) {
    bvw->priv->balance = GST_COLOR_BALANCE (bvw->priv->xoverlay);
    gst_object_ref (bvw->priv->balance);
    GST_DEBUG ("Colorbalance backup found: %s",
        GST_OBJECT_NAME (bvw->priv->balance));
  } else {
    GST_DEBUG ("No colorbalance found");
    bvw->priv->balance = NULL;
  }

  /* Setup brightness and contrast */
  for (i = 0; i < 4; i++) {
    confvalue = gconf_client_get_without_default (bvw->priv->gc,
        video_props_str[i], NULL);
    if (confvalue != NULL) {
      bacon_video_widget_set_video_property (bvw, i,
        gconf_value_get_int (confvalue));
      gconf_value_free (confvalue);
    }
  }

  if (old_xoverlay)
    gst_object_unref (GST_OBJECT (old_xoverlay));

  if (old_balance)
    gst_object_unref (GST_OBJECT (old_balance));

  gst_object_unref (video_sink);
}

static void
bvw_element_msg_sync (GstBus *bus, GstMessage *msg, gpointer data)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (data);

  g_assert (msg->type == GST_MESSAGE_ELEMENT);

  if (msg->structure == NULL)
    return;

  /* This only gets sent if we haven't set an ID yet. This is our last
   * chance to set it before the video sink will create its own window */
  if (gst_structure_has_name (msg->structure, "prepare-xwindow-id")) {
    XID window;

    GST_DEBUG ("Handling sync prepare-xwindow-id message");

    g_mutex_lock (bvw->priv->lock);
    bvw_update_interface_implementations (bvw);
    g_mutex_unlock (bvw->priv->lock);

    g_return_if_fail (bvw->priv->xoverlay != NULL);
    g_return_if_fail (bvw->priv->video_window != NULL);

    window = GDK_WINDOW_XWINDOW (bvw->priv->video_window);
    gst_x_overlay_set_xwindow_id (bvw->priv->xoverlay, window);
  }
}

static void
got_new_video_sink_bin_element (GstBin *video_sink, GstElement *element,
                                gpointer data)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (data);

  g_mutex_lock (bvw->priv->lock);
  bvw_update_interface_implementations (bvw);
  g_mutex_unlock (bvw->priv->lock);
}

GtkWidget *
bacon_video_widget_new (int width, int height,
			BvwUseType type, GError ** err)
{
  GConfValue *confvalue;
  BaconVideoWidget *bvw;
  GstElement *audio_sink = NULL, *video_sink = NULL;

  bvw = BACON_VIDEO_WIDGET (g_object_new
                            (bacon_video_widget_get_type (), NULL));

  bvw->priv->use_type = type;
  GST_DEBUG ("use_type = %d", type);

  bvw->priv->play = gst_element_factory_make ("playbin", "play");
  if (!bvw->priv->play) {
    g_set_error (err, BVW_ERROR, BVW_ERROR_PLUGIN_LOAD,
		 _("Failed to create a GStreamer play object. "
		   "Please check your GStreamer installation."));
    g_object_unref (bvw);
    return NULL;
  }

  bvw->priv->bus = gst_element_get_bus (bvw->priv->play);
  
  gst_bus_add_signal_watch (bvw->priv->bus);

  bvw->priv->sig_bus_async = 
      g_signal_connect (bvw->priv->bus, "message", 
                        G_CALLBACK (bvw_bus_message_cb),
                        bvw);

  bvw->priv->speakersetup = BVW_AUDIO_SOUND_STEREO;
  bvw->priv->media_device = g_strdup ("/dev/dvd");
  bvw->priv->init_width = 240;
  bvw->priv->init_height = 180;
  bvw->priv->visq = VISUAL_SMALL;
  bvw->priv->show_vfx = FALSE;
  bvw->priv->vis_element_name = NULL;
  bvw->priv->tv_out_type = TV_OUT_NONE;
  bvw->priv->connection_speed = 0;
  bvw->priv->ratio_type = BVW_RATIO_AUTO;

  bvw->priv->cursor_shown = TRUE;
  bvw->priv->logo_mode = FALSE;
  bvw->priv->auto_resize = TRUE;

  /* gconf setting in backend */
  bvw->priv->gc = gconf_client_get_default ();
  gconf_client_notify_add (bvw->priv->gc, "/apps/totem",
      cb_gconf, bvw, NULL, NULL);

  if (type == BVW_USE_TYPE_VIDEO || type == BVW_USE_TYPE_AUDIO) {
    audio_sink = gst_element_factory_make ("gconfaudiosink", "audio-sink");
    if (audio_sink == NULL) {
      g_warning ("Could not create element 'gconfaudiosink'");
      /* Try to fallback on autoaudiosink */
      audio_sink = gst_element_factory_make ("autoaudiosink", "audio-sink");
    }
  } else {
    audio_sink = gst_element_factory_make ("fakesink", "audio-fake-sink");
  }

  if (type == BVW_USE_TYPE_VIDEO) {
    video_sink = gst_element_factory_make ("gconfvideosink", "video-sink");
    if (video_sink == NULL) {
      g_warning ("Could not create element 'gconfvideosink'");
      /* Try to fallback on ximagesink */
      video_sink = gst_element_factory_make ("ximagesink", "video-sink");
    }
/* FIXME: April fool's day puzzle */
#if 0
    if (video_sink) {
      GDate d;

      g_date_clear (&d, 1);
      g_date_set_time (&d, time (NULL));
      if (g_date_day (&d) == 1 && g_date_month (&d) == G_DATE_APRIL) {
        confvalue = gconf_client_get_without_default (bvw->priv->gc,
            GCONF_PREFIX"/puzzle_year", NULL);

        if (!confvalue ||
            gconf_value_get_int (confvalue) != g_date_year (&d)) {
          GstElement *puzzle;

          gconf_client_set_int (bvw->priv->gc, GCONF_PREFIX"/puzzle_year",
              g_date_year (&d), NULL);

          puzzle = gst_element_factory_make ("puzzle", NULL);
          if (puzzle) {
            GstElement *bin = gst_bin_new ("videosinkbin");
            GstPad *pad;

            gst_bin_add_many (GST_BIN (bin), puzzle, video_sink, NULL);
            gst_element_link_pads (puzzle, "src", video_sink, "sink");
            pad = gst_element_get_pad (puzzle, "sink");
            gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
            gst_object_unref (pad);
            video_sink = bin;
          }
        }

        if (confvalue)
          gconf_value_free (confvalue);
      }
    }
#endif
  } else {
    video_sink = gst_element_factory_make ("fakesink", "video-fake-sink");
  }

  if (video_sink) {
    gboolean success;

    /* need to set bus explicitly as it's not in a bin yet and
     * poll_for_state_change() needs one to catch error messages */
    gst_element_set_bus (video_sink, bvw->priv->bus);
    gst_element_set_state (video_sink, GST_STATE_READY);
    success = poll_for_state_change (bvw, video_sink, GST_STATE_READY, err);
    if (!success) {
      /* Drop this video sink */
      gst_element_set_state (video_sink, GST_STATE_NULL);
      gst_object_unref (video_sink);
      /* Try again with ximagesink */
      video_sink = gst_element_factory_make ("ximagesink", "video-sink");
      gst_element_set_bus (video_sink, bvw->priv->bus);
      gst_element_set_state (video_sink, GST_STATE_READY);
      success = poll_for_state_change (bvw, video_sink, GST_STATE_READY, err);
      if (!success) {
      if (err && !*err) {
        g_warning ("Should have gotten an error message, please file a bug.");
        g_set_error (err, BVW_ERROR, BVW_ERROR_VIDEO_PLUGIN,
             _("Failed to open video output. It may not be available. "
               "Please select another video output in the Multimedia "
               "Systems Selector."));
      }
        goto sink_error;
      }
    }
  } else {
    g_set_error (err, BVW_ERROR, BVW_ERROR_VIDEO_PLUGIN,
		 _("Could not find the video output. "
		   "You may need to install additional GStreamer plugins, "
		   "or select another video output in the Multimedia Systems "
		   "Selector."));
    goto sink_error;
  }

  if (audio_sink) {
    gboolean success;

    /* need to set bus explicitly as it's not in a bin yet and
     * poll_for_state_change() needs one to catch error messages */
    gst_element_set_bus (audio_sink, bvw->priv->bus);
    gst_element_set_state (audio_sink, GST_STATE_READY);
    success = poll_for_state_change (bvw, audio_sink, GST_STATE_READY, err);
    if (!success) {
      if (err && !*err) {
	g_warning ("Should have gotten an error message, please file a bug.");
	g_set_error (err, BVW_ERROR, BVW_ERROR_VIDEO_PLUGIN,
		     _("Failed to open audio output. You may not have "
		       "permission to open the sound device, or the sound "
		       "server may not be running. "
		       "Please select another audio output in the Multimedia "
		       "Systems Selector."));
      }
      goto sink_error;
    }
  } else {
    g_set_error (err, BVW_ERROR, BVW_ERROR_VIDEO_PLUGIN,
		 _("Could not find the audio output. "
		   "You may need to install additional GStreamer plugins, or "
		   "select another audio output in the Multimedia Systems "
		   "Selector."));
    goto sink_error;
  }

  do {
    GstElement *bin;
    GstPad *pad;

    bvw->priv->audio_capsfilter =
        gst_element_factory_make ("capsfilter", "audiofilter");
    bin = gst_bin_new ("audiosinkbin");
    gst_bin_add_many (GST_BIN (bin),
	bvw->priv->audio_capsfilter, audio_sink, NULL);
    gst_element_link_pads (bvw->priv->audio_capsfilter, "src",
	audio_sink, "sink");

    pad = gst_element_get_pad (bvw->priv->audio_capsfilter, "sink");
    gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
    gst_object_unref (pad);

    audio_sink = bin;
  } while (0);

  /* now tell playbin */
  g_object_set (bvw->priv->play, "video-sink", video_sink, NULL);
  g_object_set (bvw->priv->play, "audio-sink", audio_sink, NULL);

  bvw->priv->vis_plugins_list = NULL;

  g_signal_connect (bvw->priv->play, "notify::source",
		    G_CALLBACK (got_source), bvw);
  g_signal_connect (bvw->priv->play, "notify::stream-info",
		    G_CALLBACK (stream_info_set), bvw);

  if (type == BVW_USE_TYPE_VIDEO) {
    GstStateChangeReturn ret;

    /* wait for video sink to finish changing to READY state, 
     * otherwise we won't be able to detect the colorbalance interface */
    ret = gst_element_get_state (video_sink, NULL, NULL, 5 * GST_SECOND);
    if (ret != GST_STATE_CHANGE_SUCCESS) {
      GST_WARNING ("Timeout setting videosink to READY");
      g_set_error (err, BVW_ERROR, BVW_ERROR_VIDEO_PLUGIN,
          _("Failed to open video output. It may not be available. "
          "Please select another video output in the Multimedia Systems Selector."));
      return NULL;
    }
    bvw_update_interface_implementations (bvw);
  }

  /* we want to catch "prepare-xwindow-id" element messages synchroneously */
  gst_bus_set_sync_handler (bvw->priv->bus, gst_bus_sync_signal_handler, bvw);

  bvw->priv->sig_bus_sync = 
      g_signal_connect (bvw->priv->bus, "sync-message::element",
                        G_CALLBACK (bvw_element_msg_sync), bvw);

  if (GST_IS_BIN (video_sink)) {
    /* video sink bins like gconfvideosink might remove their children and
     * create new ones when set to NULL state, and they are currently set
     * to NULL state whenever playbin re-creates its internal video bin
     * (it sets all elements to NULL state before gst_bin_remove()ing them) */
    g_signal_connect (video_sink, "element-added",
                      G_CALLBACK (got_new_video_sink_bin_element), bvw);
  }

  /* audio out, if any */
  confvalue = gconf_client_get_without_default (bvw->priv->gc,
      GCONF_PREFIX"/audio_output_type", NULL);
  if (confvalue != NULL &&
      (type != BVW_USE_TYPE_METADATA && type != BVW_USE_TYPE_CAPTURE)) {
    bvw->priv->speakersetup = gconf_value_get_int (confvalue);
    bvw->priv->speakersetup = gconf_value_get_int (confvalue) - 1;
    bacon_video_widget_set_audio_out_type (bvw, bvw->priv->speakersetup + 1);
    gconf_value_free (confvalue);
  } else {
    bvw->priv->speakersetup = -1;
    bacon_video_widget_set_audio_out_type (bvw, BVW_AUDIO_SOUND_STEREO);
  }

  /* visualization */
  confvalue = gconf_client_get_without_default (bvw->priv->gc,
      GCONF_PREFIX "/show_vfx", NULL);
  if (confvalue != NULL) {
    bvw->priv->show_vfx = gconf_value_get_bool (confvalue);
    gconf_value_free (confvalue);
  }
  confvalue = gconf_client_get_without_default (bvw->priv->gc,
      GCONF_PREFIX "/visual_quality", NULL);
  if (confvalue != NULL) {
    bvw->priv->visq = gconf_value_get_int (confvalue);
    gconf_value_free (confvalue);
  }
#if 0
  confvalue = gconf_client_get_without_default (bvw->priv->gc,
      GCONF_PREFIX "/visual", NULL);
  if (confvalue != NULL) {
    bvw->priv->vis_element = 
        gst_element_factory_make (gconf_value_get_string (confvalue), NULL);
    gconf_value_free (confvalue);
  }
  setup_vis ();
#endif

  /* tv/conn (not used yet) */
  confvalue = gconf_client_get_without_default (bvw->priv->gc,
      GCONF_PREFIX "/tv_out_type", NULL);
  if (confvalue != NULL) {
    bvw->priv->tv_out_type = gconf_value_get_int (confvalue);
    gconf_value_free (confvalue);
  }
  confvalue = gconf_client_get_without_default (bvw->priv->gc,
      GCONF_PREFIX "/connection_speed", NULL);
  if (confvalue != NULL) {
    bvw->priv->connection_speed = gconf_value_get_int (confvalue);
    gconf_value_free (confvalue);
  }

  /* those are private to us, i.e. not Xine-compatible */
  confvalue = gconf_client_get_without_default (bvw->priv->gc,
      GCONF_PREFIX "/buffer-size", NULL);
  if (confvalue != NULL) {
    g_object_set (bvw->priv->play, "queue-size",
        (guint64) (GST_SECOND * gconf_value_get_float (confvalue)), NULL);
    gconf_value_free (confvalue);
  }
  confvalue = gconf_client_get_without_default (bvw->priv->gc,
      GCONF_PREFIX "/network-buffer-threshold", NULL);
  if (confvalue != NULL) {
    g_object_set (bvw->priv->play, "queue-threshold",
        (guint64) (GST_SECOND * gconf_value_get_float (confvalue)), NULL);
    gconf_value_free (confvalue);
  }

  return GTK_WIDGET (bvw);

  /* errors */
sink_error:
  {
    if (video_sink) {
      gst_element_set_state (video_sink, GST_STATE_NULL);
      gst_object_unref (video_sink);
    }
    if (audio_sink) {
      gst_element_set_state (audio_sink, GST_STATE_NULL);
      gst_object_unref (audio_sink);
    }

    g_object_ref (bvw);
    gtk_object_sink (GTK_OBJECT (bvw));
    g_object_unref (bvw);

    return NULL;
  }
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 *
 * Local Variables:
 * indent-tabs-mode:t
 * c-basic-offset:2
 * End:
 */
