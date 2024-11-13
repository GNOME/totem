/*
 * Copyright (C) 2003-2007 the GStreamer project
 *      Julien Moutte <julien@moutte.net>
 *      Ronald Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2003-2022 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2005-2008 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright © 2009 Christian Persch
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

/**
 * SECTION:bacon-video-widget
 * @short_description: video playing widget and abstraction
 * @stability: Unstable
 * @include: bacon-video-widget.h
 *
 * #BaconVideoWidget is a widget to play audio or video streams It has a GStreamer
 * backend, and abstracts away the differences to provide a simple interface to the functionality required by Totem. It handles all the low-level
 * audio and video work for Totem (or passes the work off to the backend).
 **/

#include <config.h>

#define GST_USE_UNSTABLE_API 1

#include <gst/gst.h>

/* GStreamer Interfaces */
#include <gst/video/navigation.h>
#include <gst/video/colorbalance.h>
/* for detecting sources of errors */
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <gst/audio/streamvolume.h>

/* for missing decoder/demuxer detection */
#include <gst/pbutils/pbutils.h>

/* for the cover metadata info */
#include <gst/tag/tag.h>

/* system */
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* gtk+/gnome */
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gdesktop-enums.h>

#include "totem-gst-helpers.h"
#include "totem-gst-pixbuf-helpers.h"
#include "bacon-video-widget.h"
#include "bacon-video-widget-enums.h"
#include "bacon-video-widget-resources.h"

#define DEFAULT_USER_AGENT "Videos/"VERSION

#define DEFAULT_CONTROLS_WIDTH 600             /* In pixels */
#define LOGO_SIZE 64                           /* Maximum size of the logo */
#define REWIND_OR_PREVIOUS 4000

#define MAX_NETWORK_SPEED 10752
#define BUFFERING_LEFT_RATIO 1.1

/* Helper constants */
#define NANOSECS_IN_SEC 1000000000
#define SEEK_TIMEOUT NANOSECS_IN_SEC / 10
#define FORWARD_RATE 1.0
#define REVERSE_RATE -1.0
#define DIRECTION_STR (forward == FALSE ? "reverse" : "forward")

#define BVW_TRACK_NONE -2
#define BVW_TRACK_AUTO -1

#define is_error(e, d, c) \
  (e->domain == GST_##d##_ERROR && \
   e->code == GST_##d##_ERROR_##c)

#define I_(string) (g_intern_static_string (string))

/* Signals */
enum
{
  SIGNAL_ERROR,
  SIGNAL_EOS,
  SIGNAL_REDIRECT,
  SIGNAL_CHANNELS_CHANGE,
  SIGNAL_TICK,
  SIGNAL_GOT_METADATA,
  SIGNAL_BUFFERING,
  SIGNAL_MISSING_PLUGINS,
  SIGNAL_DOWNLOAD_BUFFERING,
  SIGNAL_PLAY_STARTING,
  SIGNAL_SUBTITLES_CHANGED,
  SIGNAL_LANGUAGES_CHANGED,
  LAST_SIGNAL
};

/* Properties */
enum
{
  PROP_0,
  PROP_POSITION,
  PROP_CURRENT_TIME,
  PROP_STREAM_LENGTH,
  PROP_PLAYING,
  PROP_REFERRER,
  PROP_SEEKABLE,
  PROP_USER_AGENT,
  PROP_VOLUME,
  PROP_DOWNLOAD_FILENAME,
  PROP_DEINTERLACING,
  PROP_BRIGHTNESS,
  PROP_CONTRAST,
  PROP_SATURATION,
  PROP_HUE,
  PROP_AUDIO_OUTPUT_TYPE,
  PROP_AV_OFFSET,
  PROP_SHOW_CURSOR,
};

static const gchar *video_props_str[4] = {
  "brightness",
  "contrast",
  "saturation",
  "hue"
};

struct _BaconVideoWidget
{
  GtkBin                       parent;

  /* widgets */
  GtkWidget                   *stack;
  GtkWidget                   *audio_only;
  GtkWidget                   *broken_video;
  GtkWidget                   *video_widget;

  GtkWindow                   *parent_toplevel;

  GError                      *init_error;

  char                        *user_agent;

  char                        *referrer;
  char                        *mrl;
  char                        *subtitle_uri;
  BvwAspectRatio               ratio_type;

  GstElement                  *play;
  GstElement                  *video_sink;
  GstNavigation               *navigation;

  guint                        update_id;
  guint                        fill_id;

  gboolean                     media_has_video;
  gboolean                     media_has_unsupported_video;
  gboolean                     media_has_audio;
  gint                         seekable; /* -1 = don't know, FALSE = no */
  gint64                       stream_length;
  gint64                       current_time;
  gdouble                      current_position;
  gboolean                     is_live;

  GstTagList                  *tagcache;
  GstTagList                  *audiotags;
  GstTagList                  *videotags;

  GAsyncQueue                 *tag_update_queue;
  guint                        tag_update_id;

  gboolean                     got_redirect;

  GdkCursor                   *blank_cursor;
  GdkCursor                   *hand_cursor;
  gboolean                     cursor_shown;
  gboolean                     hovering_menu;

  /* Visual effects */
  GstElement                  *audio_capsfilter;
  GstElement                  *audio_pitchcontrol;

  /* Other stuff */
  gboolean                     uses_audio_fakesink;
  gdouble                      volume;
  gboolean                     is_menu;
  gboolean                     has_angles;
  GList                       *chapters;
  GList                       *subtitles; /* GList of BvwLangInfo */
  GList                       *languages; /* GList of BvwLangInfo */

  BvwRotation                  rotation;
  
  gint                         video_width; /* Movie width */
  gint                         video_height; /* Movie height */
  gint                         video_fps_n;
  gint                         video_fps_d;

  BvwAudioOutputType           speakersetup;

  GstBus                      *bus;
  gulong                       sig_bus_async;

  gint                         eos_id;

  /* When seeking, queue up the seeks if they happen before
   * the previous one finished */
  GMutex                       seek_mutex;
  GstClock                    *clock;
  GstClockTime                 seek_req_time;
  gint64                       seek_time;
  /* state we want to be in, as opposed to actual pipeline state
   * which may change asynchronously or during buffering */
  GstState                     target_state;
  gboolean                     buffering;
  gboolean                     download_buffering;
  char                        *download_filename;
  /* used to compute when the download buffer has gone far
   * enough to start playback, not "amount of buffering time left
   * to reach 100% fill-level" */
  gint64                       buffering_left;

  /* for missing codecs handling */
  GList                       *missing_plugins;   /* GList of GstMessages */

  /* for mounting locations if necessary */
  GCancellable                *mount_cancellable;
  gboolean                     mount_in_progress;

  /* for auth */
  GMountOperation             *auth_dialog;
  GMountOperationResult        auth_last_result;
  char                        *user_id, *user_pw;

  /* for stepping */
  float                        rate;
};

G_DEFINE_TYPE (BaconVideoWidget, bacon_video_widget, GTK_TYPE_BIN)

static void bacon_video_widget_set_property (GObject * object,
                                             guint property_id,
                                             const GValue * value,
                                             GParamSpec * pspec);
static void bacon_video_widget_get_property (GObject * object,
                                             guint property_id,
                                             GValue * value,
                                             GParamSpec * pspec);

static void bacon_video_widget_finalize (GObject * object);

static void bvw_reconfigure_fill_timeout (BaconVideoWidget *bvw, guint msecs);
static void bvw_stop_play_pipeline (BaconVideoWidget * bvw);
static GError* bvw_error_from_gst_error (BaconVideoWidget *bvw, GstMessage *m);
static gboolean bvw_set_playback_direction (BaconVideoWidget *bvw, gboolean forward);
static gboolean bacon_video_widget_seek_time_no_lock (BaconVideoWidget *bvw,
						      gint64 _time,
						      GstSeekFlags flag,
						      GError **error);
static gboolean update_subtitles_tracks (BaconVideoWidget *bvw);
static gboolean update_languages_tracks (BaconVideoWidget *bvw);

typedef struct {
  GstTagList *tags;
  const gchar *type;
} UpdateTagsDelayedData;

static void update_tags_delayed_data_destroy (UpdateTagsDelayedData *data);

static GtkWidgetClass *parent_class = NULL;

static int bvw_signals[LAST_SIGNAL] = { 0 };

GST_DEBUG_CATEGORY (_totem_gst_debug_cat);
#define GST_CAT_DEFAULT _totem_gst_debug_cat

typedef gchar * (* MsgToStrFunc) (GstMessage * msg);

static gchar **
bvw_get_missing_plugins_foo (const GList * missing_plugins, MsgToStrFunc func)
{
  GPtrArray *arr = g_ptr_array_new ();
  GHashTable *ht;

  ht = g_hash_table_new (g_str_hash, g_str_equal);
  while (missing_plugins != NULL) {
    char *tmp;
    tmp = func (GST_MESSAGE (missing_plugins->data));
    if (!g_hash_table_lookup (ht, tmp)) {
      g_ptr_array_add (arr, tmp);
      g_hash_table_insert (ht, tmp, GINT_TO_POINTER (1));
    } else {
      g_free (tmp);
    }
    missing_plugins = missing_plugins->next;
  }
  g_ptr_array_add (arr, NULL);
  g_hash_table_destroy (ht);
  return (gchar **) g_ptr_array_free (arr, FALSE);
}

static gchar **
bvw_get_missing_plugins_descriptions (const GList * missing_plugins)
{
  return bvw_get_missing_plugins_foo (missing_plugins,
      gst_missing_plugin_message_get_description);
}

static void
bvw_clear_missing_plugins_messages (BaconVideoWidget * bvw)
{
  g_list_free_full (bvw->missing_plugins, (GDestroyNotify) gst_mini_object_unref);
  bvw->missing_plugins = NULL;
}

static void
bvw_check_if_video_decoder_is_missing (BaconVideoWidget * bvw)
{
  GList *l;

  for (l = bvw->missing_plugins; l != NULL; l = l->next) {
    GstMessage *msg = GST_MESSAGE (l->data);
    g_autofree char *d = NULL;
    char *f;

    if ((d = gst_missing_plugin_message_get_installer_detail (msg))) {
      if ((f = strstr (d, "|decoder-")) && strstr (f, "video")) {
	bvw->media_has_unsupported_video = TRUE;
	break;
      }
    }
  }
}

static void
bvw_show_error_if_video_decoder_is_missing (BaconVideoWidget * bvw)
{
  GList *l;

  if (bvw->media_has_video || bvw->missing_plugins == NULL)
    return;

  for (l = bvw->missing_plugins; l != NULL; l = l->next) {
    GstMessage *msg = GST_MESSAGE (l->data);
    gchar *d, *f;

    if ((d = gst_missing_plugin_message_get_installer_detail (msg))) {
      if ((f = strstr (d, "|decoder-")) && strstr (f, "video")) {
        GError *err;

        /* create a fake GStreamer error so we get a nice warning message */
        err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN, "x");
        msg = gst_message_new_error (GST_OBJECT (bvw->play), err, NULL);
        g_error_free (err);
        err = bvw_error_from_gst_error (bvw, msg);
        gst_message_unref (msg);
        g_signal_emit (bvw, bvw_signals[SIGNAL_ERROR], 0, err->message, FALSE);
        g_error_free (err);
        g_free (d);
        break;
      }
      g_free (d);
    }
  }
}

static void
update_cursor (BaconVideoWidget *bvw)
{
  GdkWindow *window;

  window = gtk_widget_get_window (GTK_WIDGET (bvw));

  if (!gtk_window_is_active (bvw->parent_toplevel)) {
    gdk_window_set_cursor (window, NULL);
    return;
  }

  if (bvw->hovering_menu)
    gdk_window_set_cursor (window, bvw->hand_cursor);
  else if (bvw->cursor_shown)
    gdk_window_set_cursor (window, NULL);
  else
    gdk_window_set_cursor (window, bvw->blank_cursor);
}

static void
bacon_video_widget_realize (GtkWidget * widget)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);
  GdkWindow *window;
  GdkDisplay *display;

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  window = gtk_widget_get_window (widget);
  display = gdk_window_get_display (window);
  bvw->hand_cursor = gdk_cursor_new_for_display (display, GDK_HAND2);
  bvw->blank_cursor = gdk_cursor_new_for_display (display, GDK_BLANK_CURSOR);

  bvw->parent_toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (bvw)));
  g_signal_connect_swapped (G_OBJECT (bvw->parent_toplevel), "notify::is-active",
			    G_CALLBACK (update_cursor), bvw);
}

static void
bacon_video_widget_unrealize (GtkWidget *widget)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);

  if (bvw->parent_toplevel != NULL) {
    g_signal_handlers_disconnect_by_func (bvw->parent_toplevel, update_cursor, bvw);
    bvw->parent_toplevel = NULL;
  }
  g_clear_object (&bvw->blank_cursor);
  g_clear_object (&bvw->hand_cursor);
}

static void
set_current_actor (BaconVideoWidget *bvw)
{
  const char *page;

  if (bvw->media_has_audio && !bvw->media_has_video)
    page = "audio-only";
  else if (bvw->media_has_unsupported_video)
    page = "broken-video";
  else
    page = "video";

  gtk_stack_set_visible_child_name (GTK_STACK (bvw->stack), page);
}

static void
translate_coords (GtkWidget   *widget,
		  GdkWindow   *window,
		  int          x,
		  int          y,
		  int         *out_x,
		  int         *out_y)
{
  GtkWidget *src;

  gdk_window_get_user_data (window, (gpointer *)&src);
  if (src && src != widget) {
    gtk_widget_translate_coordinates (src, widget, x, y, out_x, out_y);
  } else {
    *out_x = x;
    *out_y = y;
  }
}

/* need to use gstnavigation interface for these vmethods, to allow for the sink
   to map screen coordinates to video coordinates in the presence of e.g.
   hardware scaling */

static gboolean
bacon_video_widget_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
  gboolean res = GDK_EVENT_PROPAGATE;
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);

  g_return_val_if_fail (bvw->play != NULL, FALSE);

  if (bvw->navigation)
    gst_navigation_send_mouse_event (bvw->navigation, "mouse-move", 0, event->x, event->y);

  if (GTK_WIDGET_CLASS (parent_class)->motion_notify_event)
    res |= GTK_WIDGET_CLASS (parent_class)->motion_notify_event (widget, event);

  return res;
}

static gboolean
bacon_video_widget_button_press_or_release (GtkWidget *widget, GdkEventButton *event)
{
  gboolean res = FALSE;
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (widget);
  GdkDevice *device;

  device = gdk_event_get_source_device ((GdkEvent *) event);
  if (gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN)
    return FALSE;

  g_return_val_if_fail (bvw->play != NULL, FALSE);

  if (event->type != GDK_BUTTON_PRESS &&
      event->type != GDK_BUTTON_RELEASE)
    goto bail;

  if (bvw->navigation &&
      event->button == 1 &&
      bvw->is_menu != FALSE) {
    int x, y;
    const char *event_str;
    event_str = (event->type == GDK_BUTTON_PRESS) ? "mouse-button-press" : "mouse-button-release";
    translate_coords (widget, event->window, event->x, event->y, &x, &y);
    gst_navigation_send_mouse_event (bvw->navigation,
				     event_str, event->button, x, y);

    /* FIXME need to check whether the backend will have handled
     * the button press
     res = TRUE; */
  }

bail:
  if (event->type == GDK_BUTTON_PRESS && GTK_WIDGET_CLASS (parent_class)->button_press_event)
    res |= GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
  if (event->type == GDK_BUTTON_RELEASE && GTK_WIDGET_CLASS (parent_class)->button_release_event)
    res |= GTK_WIDGET_CLASS (parent_class)->button_release_event (widget, event);

  return res;
}

static void
bacon_video_widget_get_preferred_width (GtkWidget *widget,
                                        gint      *minimum,
                                        gint      *natural)
{
  /* We could also make the actor a minimum width, based on its contents */
  *minimum = *natural = DEFAULT_CONTROLS_WIDTH;
}

static void
bacon_video_widget_get_preferred_height (GtkWidget *widget,
                                         gint      *minimum,
                                         gint      *natural)
{
  *minimum = *natural = DEFAULT_CONTROLS_WIDTH / 16 * 9;
}

static gboolean
bvw_boolean_handled_accumulator (GSignalInvocationHint * ihint,
    GValue * return_accu, const GValue * handler_return, gpointer foobar)
{
  gboolean continue_emission;
  gboolean signal_handled;
  
  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;
  
  return continue_emission;
}

static void
bacon_video_widget_class_init (BaconVideoWidgetClass * klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkIconTheme *default_theme;

  object_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  /* GtkWidget */
  widget_class->get_preferred_width = bacon_video_widget_get_preferred_width;
  widget_class->get_preferred_height = bacon_video_widget_get_preferred_height;
  widget_class->realize = bacon_video_widget_realize;
  widget_class->unrealize = bacon_video_widget_unrealize;

  widget_class->motion_notify_event = bacon_video_widget_motion_notify;
  widget_class->button_press_event = bacon_video_widget_button_press_or_release;
  widget_class->button_release_event = bacon_video_widget_button_press_or_release;

  /* GObject */
  object_class->set_property = bacon_video_widget_set_property;
  object_class->get_property = bacon_video_widget_get_property;
  object_class->finalize = bacon_video_widget_finalize;

  /* Properties */
  /**
   * BaconVideoWidget:position:
   *
   * The current position in the stream, as a percentage between <code class="literal">0</code> and <code class="literal">1</code>.
   **/
  g_object_class_install_property (object_class, PROP_POSITION,
                                   g_param_spec_double ("position", "Position", "The current position in the stream.",
							0, 1.0, 0,
							G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:stream-length:
   *
   * The length of the current stream, in milliseconds.
   **/
  g_object_class_install_property (object_class, PROP_STREAM_LENGTH,
	                           g_param_spec_int64 ("stream-length", "Stream length",
                                                     "The length of the current stream, in milliseconds.", 0, G_MAXINT64, 0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:playing:
   *
   * Whether a stream is currently playing.
   **/
  g_object_class_install_property (object_class, PROP_PLAYING,
                                   g_param_spec_boolean ("playing", "Playing?",
                                                         "Whether a stream is currently playing.", FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:seekable:
   *
   * Whether the current stream can be seeked.
   **/
  g_object_class_install_property (object_class, PROP_SEEKABLE,
                                   g_param_spec_boolean ("seekable", "Seekable?",
                                                         "Whether the current stream can be seeked.", FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:volume:
   *
   * The current volume level, as a percentage between <code class="literal">0</code> and <code class="literal">1</code>.
   **/
  g_object_class_install_property (object_class, PROP_VOLUME,
	                           g_param_spec_double ("volume", "Volume", "The current volume level.",
	                                                0.0, 1.0, 0.0,
	                                                G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:referrer:
   *
   * The HTTP referrer URI.
   **/
  g_object_class_install_property (object_class, PROP_REFERRER,
                                   g_param_spec_string ("referrer", "Referrer URI", "The HTTP referrer URI.",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:user-agent:
   *
   * The HTTP user agent string to use.
   **/
  g_object_class_install_property (object_class, PROP_USER_AGENT,
                                   g_param_spec_string ("user-agent", "User agent", "The HTTP user agent string to use.",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:download-filename:
   *
   * The filename of the fully downloaded stream when using
   * download buffering.
   **/
  g_object_class_install_property (object_class, PROP_DOWNLOAD_FILENAME,
                                   g_param_spec_string ("download-filename", "Download filename.", "The filename of the fully downloaded stream.",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:deinterlacing:
   *
   * Whether to automatically deinterlace videos.
   **/
  g_object_class_install_property (object_class, PROP_DEINTERLACING,
                                   g_param_spec_boolean ("deinterlacing", "Deinterlacing?",
                                                         "Whether to automatically deinterlace videos.", FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:brightness:
   *
   * The brightness of the video display.
   **/
  g_object_class_install_property (object_class, PROP_BRIGHTNESS,
                                   g_param_spec_int ("brightness", "Brightness",
                                                      "The brightness of the video display.", 0, 65535, 32768,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:contrast:
   *
   * The contrast of the video display.
   **/
  g_object_class_install_property (object_class, PROP_CONTRAST,
                                   g_param_spec_int ("contrast", "Contrast",
                                                      "The contrast of the video display.", 0, 65535, 32768,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:saturation:
   *
   * The saturation of the video display.
   **/
  g_object_class_install_property (object_class, PROP_SATURATION,
                                   g_param_spec_int ("saturation", "Saturation",
                                                      "The saturation of the video display.", 0, 65535, 32768,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:hue:
   *
   * The hue of the video display.
   **/
  g_object_class_install_property (object_class, PROP_HUE,
                                   g_param_spec_int ("hue", "Hue",
                                                      "The hue of the video display.", 0, 65535, 32768,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:audio-output-type:
   *
   * The type of audio output to use (e.g. the number of channels).
   **/
  g_object_class_install_property (object_class, PROP_AUDIO_OUTPUT_TYPE,
                                   g_param_spec_enum ("audio-output-type", "Audio output type",
                                                      "The type of audio output to use.", BVW_TYPE_AUDIO_OUTPUT_TYPE,
                                                      BVW_AUDIO_SOUND_STEREO,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:av-offset:
   *
   * Control the synchronisation offset between the audio and video streams.
   * Positive values make the audio ahead of the video and negative values
   * make the audio go behind the video.
   **/
  g_object_class_install_property (object_class, PROP_AV_OFFSET,
				   g_param_spec_int64 ("av-offset", "Audio/Video offset",
						       "The synchronisation offset between audio and video in nanoseconds.",
						       G_MININT64, G_MAXINT64,
						       0, G_PARAM_READWRITE |
						       G_PARAM_STATIC_STRINGS));

  /**
   * BaconVideoWidget:show-cursor:
   *
   * Whether the mouse cursor is shown.
   **/
  g_object_class_install_property (object_class, PROP_SHOW_CURSOR,
                                   g_param_spec_boolean ("show-cursor", "Show cursor",
                                                         "Whether the mouse cursor is shown.", FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /* Signals */
  /**
   * BaconVideoWidget::error:
   * @bvw: the #BaconVideoWidget which received the signal
   * @message: the error message
   * @playback_stopped: %TRUE if playback has stopped due to the error, %FALSE otherwise
   * @fatal: %TRUE if the error was fatal to playback, %FALSE otherwise
   *
   * Emitted when the backend wishes to asynchronously report an error. If @fatal is %TRUE,
   * playback of this stream cannot be restarted.
   **/
  bvw_signals[SIGNAL_ERROR] =
    g_signal_new (I_("error"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  /**
   * BaconVideoWidget::eos:
   * @bvw: the #BaconVideoWidget which received the signal
   *
   * Emitted when the end of the current stream is reached.
   **/
  bvw_signals[SIGNAL_EOS] =
    g_signal_new (I_("eos"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * BaconVideoWidget::got-metadata:
   * @bvw: the #BaconVideoWidget which received the signal
   *
   * Emitted when the widget has updated the metadata of the current stream. This
   * will typically happen just after opening a stream.
   *
   * Call bacon_video_widget_get_metadata() to query the updated metadata.
   **/
  bvw_signals[SIGNAL_GOT_METADATA] =
    g_signal_new (I_("got-metadata"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * BaconVideoWidget::got-redirect:
   * @bvw: the #BaconVideoWidget which received the signal
   * @new_mrl: the new MRL
   *
   * Emitted when a redirect response is received from a stream's server.
   **/
  bvw_signals[SIGNAL_REDIRECT] =
    g_signal_new (I_("got-redirect"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * BaconVideoWidget::channels-change:
   * @bvw: the #BaconVideoWidget which received the signal
   *
   * Emitted when the number of audio languages available changes, or when the
   * selected audio language is changed.
   *
   * Query the new list of audio languages with bacon_video_widget_get_languages().
   **/
  bvw_signals[SIGNAL_CHANNELS_CHANGE] =
    g_signal_new (I_("channels-change"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * BaconVideoWidget::tick:
   * @bvw: the #BaconVideoWidget which received the signal
   * @current_time: the current position in the stream, in milliseconds since the beginning of the stream
   * @stream_length: the length of the stream, in milliseconds
   * @current_position: the current position in the stream, as a percentage between <code class="literal">0</code> and <code class="literal">1</code>
   * @seekable: %TRUE if the stream can be seeked, %FALSE otherwise
   *
   * Emitted every time an important time event happens, or at regular intervals when playing a stream.
   **/
  bvw_signals[SIGNAL_TICK] =
    g_signal_new (I_("tick"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 4, G_TYPE_INT64, G_TYPE_INT64, G_TYPE_DOUBLE,
                  G_TYPE_BOOLEAN);

  /**
   * BaconVideoWidget::buffering:
   * @bvw: the #BaconVideoWidget which received the signal
   * @percentage: the percentage of buffering completed, between <code class="literal">0</code> and <code class="literal">1</code>
   *
   * Emitted regularly when a network stream is being buffered, to provide status updates on the buffering
   * progress.
   **/
  bvw_signals[SIGNAL_BUFFERING] =
    g_signal_new (I_("buffering"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__DOUBLE, G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  /**
   * BaconVideoWidget::missing-plugins:
   * @bvw: the #BaconVideoWidget which received the signal
   * @details: a %NULL-terminated array of missing plugin details for use when installing the plugins with libgimme-codec
   * @descriptions: a %NULL-terminated array of missing plugin descriptions for display to the user
   * @playing: %TRUE if the stream could be played even without these plugins, %FALSE otherwise
   *
   * Emitted when plugins required to play the current stream are not found. This allows the application
   * to request the user install them before proceeding to try and play the stream again.
   *
   * Note that this signal is only available for the GStreamer backend.
   *
   * Return value: %TRUE if the signal was handled and some action was taken, %FALSE otherwise
   **/
  bvw_signals[SIGNAL_MISSING_PLUGINS] =
    g_signal_new (I_("missing-plugins"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  bvw_boolean_handled_accumulator, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_BOOLEAN, 3, G_TYPE_STRV, G_TYPE_STRV, G_TYPE_BOOLEAN);

  /**
   * BaconVideoWidget::download-buffering:
   * @bvw: the #BaconVideoWidget which received the signal
   * @percentage: the percentage of download buffering completed, between <code class="literal">0</code> and <code class="literal">1</code>
   *
   * Emitted regularly when a network stream is being cached on disk, to provide status
   *  updates on the buffering level of the stream.
   **/
  bvw_signals[SIGNAL_DOWNLOAD_BUFFERING] =
    g_signal_new ("download-buffering",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__DOUBLE, G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  /**
   * BaconVideoWidget::play-starting:
   * @bvw: the #BaconVideoWidget which received the signal
   *
   * Emitted when a movie will start playing, meaning it's not buffering, or paused
   *  waiting for plugins to be installed, drives to be mounted or authentication
   *  to succeed.
   *
   * This usually means that OSD popups can be hidden.
   *
   **/
  bvw_signals[SIGNAL_PLAY_STARTING] =
    g_signal_new ("play-starting",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * BaconVideoWidget::subtitles-changed:
   * @bvw: the #BaconVideoWidget which received the signal
   *
   * Emitted when the list of subtitle tracks has changed.
   **/
  bvw_signals[SIGNAL_SUBTITLES_CHANGED] =
    g_signal_new ("subtitles-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * BaconVideoWidget::languages-changed:
   * @bvw: the #BaconVideoWidget which received the signal
   *
   * Emitted when the list of languages/audio tracks has changed.
   **/
  bvw_signals[SIGNAL_LANGUAGES_CHANGED] =
    g_signal_new ("languages-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  g_resources_register (_bvw_get_resource ());

  default_theme = gtk_icon_theme_get_default ();
  gtk_icon_theme_add_resource_path (default_theme, "/org/gnome/totem/bvw");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/bvw/bacon-video-widget.ui");
  gtk_widget_class_bind_template_child (widget_class, BaconVideoWidget, stack);
  gtk_widget_class_bind_template_child (widget_class, BaconVideoWidget, audio_only);
  gtk_widget_class_bind_template_child (widget_class, BaconVideoWidget, broken_video);
}

static gboolean bvw_query_timeout (BaconVideoWidget *bvw);
static gboolean bvw_query_buffering_timeout (BaconVideoWidget *bvw);
static void parse_stream_info (BaconVideoWidget *bvw);

static void
bvw_update_stream_info (BaconVideoWidget *bvw)
{
  parse_stream_info (bvw);

  g_signal_emit (bvw, bvw_signals[SIGNAL_GOT_METADATA], 0, NULL);
  if (update_subtitles_tracks (bvw))
    g_signal_emit (bvw, bvw_signals[SIGNAL_SUBTITLES_CHANGED], 0);
  if (update_languages_tracks (bvw))
    g_signal_emit (bvw, bvw_signals[SIGNAL_LANGUAGES_CHANGED], 0);
  g_signal_emit (bvw, bvw_signals[SIGNAL_CHANNELS_CHANGE], 0);
}

static void
bvw_handle_application_message (BaconVideoWidget *bvw, GstMessage *msg)
{
  const GstStructure *structure;
  const gchar *msg_name;

  structure = gst_message_get_structure (msg);
  msg_name = gst_structure_get_name (structure);
  g_return_if_fail (msg_name != NULL);

  GST_DEBUG ("Handling application message: %" GST_PTR_FORMAT, structure);

  if (strcmp (msg_name, "stream-changed") == 0) {
    bvw_update_stream_info (bvw);
  }
  else if (strcmp (msg_name, "video-size") == 0) {
    g_signal_emit (bvw, bvw_signals[SIGNAL_GOT_METADATA], 0, NULL);
    set_current_actor (bvw);
  } else {
    g_debug ("Unhandled application message %s", msg_name);
  }
}

static gboolean
bvw_do_navigation_query (BaconVideoWidget * bvw, GstQuery *query)
{
  if (!bvw->navigation)
    return FALSE;

  return gst_element_query (GST_ELEMENT_CAST (bvw->navigation), query);
}

static void
mount_cb (GObject *obj, GAsyncResult *res, gpointer user_data)
{
  BaconVideoWidget * bvw = user_data;
  gboolean ret;
  gchar *uri;
  GError *error = NULL;
  GError *err = NULL;
  GstMessage *msg;

  ret = g_file_mount_enclosing_volume_finish (G_FILE (obj), res, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  g_clear_object (&bvw->mount_cancellable);
  bvw->mount_in_progress = FALSE;

  uri = g_strdup (bvw->mrl);

  if (ret) {
    GstState target_state;

    GST_DEBUG ("Mounting location '%s' successful", GST_STR_NULL (uri));
    /* Save the expected pipeline state */
    target_state = bvw->target_state;
    bacon_video_widget_open (bvw, uri);
    if (target_state == GST_STATE_PLAYING)
      bacon_video_widget_play (bvw, NULL);
    g_free (uri);
    return;
  }

  if (!ret)
    GST_DEBUG ("Mounting location '%s' failed: %s", GST_STR_NULL (uri), error->message);
  else
    GST_DEBUG ("Failed to set '%s' back to playing: %s", GST_STR_NULL (uri), error->message);

  /* create a fake GStreamer error so we get a nice warning message */
  err = g_error_new_literal (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_OPEN_READ, error->message);
  msg = gst_message_new_error (GST_OBJECT (bvw->play), err, error->message);
  g_error_free (err);
  g_error_free (error);
  err = bvw_error_from_gst_error (bvw, msg);
  gst_message_unref (msg);
  g_signal_emit (bvw, bvw_signals[SIGNAL_ERROR], 0, err->message, FALSE);
  g_error_free (err);

  g_free (uri);
}

static void
bvw_handle_element_message (BaconVideoWidget *bvw, GstMessage *msg)
{
  const GstStructure *structure;
  const gchar *type_name = NULL;
  gchar *src_name;

  src_name = gst_object_get_name (msg->src);

  structure = gst_message_get_structure (msg);
  if (structure)
    type_name = gst_structure_get_name (structure);

  GST_DEBUG ("from %s: %" GST_PTR_FORMAT, src_name, structure);

  if (type_name == NULL)
    goto unhandled;

  if (strcmp (type_name, "redirect") == 0) {
    const gchar *new_location;

    new_location = gst_structure_get_string (structure, "new-location");
    GST_DEBUG ("Got redirect to '%s'", GST_STR_NULL (new_location));

    if (new_location && *new_location) {
      g_signal_emit (bvw, bvw_signals[SIGNAL_REDIRECT], 0, new_location);
      goto done;
    }
  } else if (strcmp (type_name, "progress") == 0) {
    /* this is similar to buffering messages, but shouldn't affect pipeline
     * state; qtdemux emits those when headers are after movie data and
     * it is in streaming mode and has to receive all the movie data first */
    if (!bvw->buffering) {
      gint percent = 0;

      if (gst_structure_get_int (structure, "percent", &percent)) {
	gdouble fraction = (gdouble) percent / 100.0;
        g_signal_emit (bvw, bvw_signals[SIGNAL_BUFFERING], 0, fraction);
      }
    }
    goto done;
  } else if (gst_is_missing_plugin_message (msg)) {
    bvw->missing_plugins =
      g_list_prepend (bvw->missing_plugins, gst_message_ref (msg));
    goto done;
  } else if (strcmp (type_name, "not-mounted") == 0) {
    const GValue *val;
    GFile *file;
    GMountOperation *mount_op;
    GstState target_state;
    const char *uri;

    val = gst_structure_get_value (structure, "uri");
    uri = g_value_get_string (val);

    if (bvw->mount_in_progress) {
      g_cancellable_cancel (bvw->mount_cancellable);
      g_clear_object (&bvw->mount_cancellable);
      bvw->mount_in_progress = FALSE;
    }

    GST_DEBUG ("Trying to mount location '%s'", GST_STR_NULL (uri));

    val = gst_structure_get_value (structure, "file");
    if (val == NULL)
      goto done;

    file = G_FILE (g_value_get_object (val));
    if (file == NULL)
      goto done;

    /* Save and restore the expected pipeline state */
    target_state = bvw->target_state;
    bacon_video_widget_stop (bvw);
    bvw->target_state = target_state;

    mount_op = gtk_mount_operation_new (bvw->parent_toplevel);
    bvw->mount_in_progress = TRUE;
    bvw->mount_cancellable = g_cancellable_new ();
    g_file_mount_enclosing_volume (file, G_MOUNT_MOUNT_NONE,
        mount_op, bvw->mount_cancellable, mount_cb, bvw);

    g_object_unref (mount_op);
    goto done;
  } else if (strcmp (type_name, "GstCacheDownloadComplete") == 0) {
    const gchar *location;

    /* do query for the last time */
    bvw_query_buffering_timeout (bvw);
    /* Finished buffering the whole file, so don't run the timeout anymore */
    bvw_reconfigure_fill_timeout (bvw, 0);

    /* Tell the front-end about the downloaded file */
    g_object_notify (G_OBJECT (bvw), "download-filename");

    location = gst_structure_get_string (structure, "location");
    GST_DEBUG ("Finished download of '%s'", GST_STR_NULL (location));
    goto done;
  } else {
    GstNavigationMessageType nav_msg_type =
        gst_navigation_message_get_type (msg);

    switch (nav_msg_type) {
      case GST_NAVIGATION_MESSAGE_MOUSE_OVER: {
        gint active;
        if (!gst_navigation_message_parse_mouse_over (msg, &active))
          break;
        bvw->hovering_menu = active;
        update_cursor (bvw);
        goto done;
      }
      case GST_NAVIGATION_MESSAGE_COMMANDS_CHANGED: {
        GstQuery *cmds_q = gst_navigation_query_new_commands();
        gboolean res = bvw_do_navigation_query (bvw, cmds_q);

        if (res) {
          gboolean is_menu = FALSE;
	  gboolean has_angles = FALSE;
          guint i, n;

          if (gst_navigation_query_parse_commands_length (cmds_q, &n)) {
            for (i = 0; i < n; i++) {
              GstNavigationCommand cmd;
              if (!gst_navigation_query_parse_commands_nth (cmds_q, i, &cmd))
                break;
              is_menu |= (cmd == GST_NAVIGATION_COMMAND_ACTIVATE);
              is_menu |= (cmd == GST_NAVIGATION_COMMAND_LEFT);
              is_menu |= (cmd == GST_NAVIGATION_COMMAND_RIGHT);
              is_menu |= (cmd == GST_NAVIGATION_COMMAND_UP);
              is_menu |= (cmd == GST_NAVIGATION_COMMAND_DOWN);

	      has_angles |= (cmd == GST_NAVIGATION_COMMAND_PREV_ANGLE);
	      has_angles |= (cmd == GST_NAVIGATION_COMMAND_NEXT_ANGLE);
            }
          }
	  /* Are we in a menu now? */
	  if (bvw->is_menu != is_menu) {
	    bvw->is_menu = is_menu;
	    g_object_notify (G_OBJECT (bvw), "seekable");
	  }
	  /* Do we have angle switching now? */
	  if (bvw->has_angles != has_angles) {
	    bvw->has_angles = has_angles;
	    g_signal_emit (bvw, bvw_signals[SIGNAL_CHANNELS_CHANGE], 0);
	  }
        }

        gst_query_unref (cmds_q);
        goto done;
      }
      case GST_NAVIGATION_MESSAGE_ANGLES_CHANGED:
      case GST_NAVIGATION_MESSAGE_INVALID:
        goto unhandled;
      default:
        break;
    }
  }

unhandled:
  GST_WARNING ("Unhandled element message %s from %s: %" GST_PTR_FORMAT,
      GST_STR_NULL (type_name), GST_STR_NULL (src_name), msg);

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
  bvw->eos_id = 0;
  return FALSE;
}

static void
bvw_reconfigure_tick_timeout (BaconVideoWidget *bvw, guint msecs)
{
  if (bvw->update_id != 0) {
    GST_DEBUG ("removing tick timeout");
    g_source_remove (bvw->update_id);
    bvw->update_id = 0;
  }
  if (msecs > 0) {
    GST_DEBUG ("adding tick timeout (at %ums)", msecs);
    bvw->update_id =
      g_timeout_add (msecs, (GSourceFunc) bvw_query_timeout, bvw);
    g_source_set_name_by_id (bvw->update_id, "[totem] bvw_query_timeout");
  }
}

static void
bvw_reconfigure_fill_timeout (BaconVideoWidget *bvw, guint msecs)
{
  if (bvw->fill_id != 0) {
    GST_DEBUG ("removing fill timeout");
    g_source_remove (bvw->fill_id);
    bvw->fill_id = 0;
  }
  if (msecs > 0) {
    GST_DEBUG ("adding fill timeout (at %ums)", msecs);
    bvw->fill_id =
      g_timeout_add (msecs, (GSourceFunc) bvw_query_buffering_timeout, bvw);
    g_source_set_name_by_id (bvw->fill_id, "[totem] bvw_query_buffering_timeout");
  }
}

static void
bvw_auth_reply_cb (GMountOperation      *op,
		   GMountOperationResult result,
		   BaconVideoWidget     *bvw)
{
  GST_DEBUG ("Got authentication reply %d", result);
  bvw->auth_last_result = result;

  if (result == G_MOUNT_OPERATION_HANDLED) {
    bvw->user_id = g_strdup (g_mount_operation_get_username (op));
    bvw->user_pw = g_strdup (g_mount_operation_get_password (op));
  }

  g_clear_object (&bvw->auth_dialog);

  if (bvw->target_state == GST_STATE_PLAYING) {
    GST_DEBUG ("Starting deferred playback after authentication");
    bacon_video_widget_play (bvw, NULL);
  }
}

static int
bvw_get_http_error_code (GstMessage *err_msg)
{
  GError *err = NULL;
  gchar *dbg = NULL;
  int code = -1;

  if (g_strcmp0 ("GstRTSPSrc", G_OBJECT_TYPE_NAME (err_msg->src)) != 0 &&
      g_strcmp0 ("GstSoupHTTPSrc", G_OBJECT_TYPE_NAME (err_msg->src)) != 0)
    return code;

  gst_message_parse_error (err_msg, &err, &dbg);

  /* Urgh! Check whether this is an auth error */
  if (err == NULL || dbg == NULL)
    goto done;
  if (!is_error (err, RESOURCE, READ) &&
      !is_error (err, RESOURCE, OPEN_READ))
    goto done;

  /* FIXME: Need to find a better way than parsing the plain text */
  /* Keep in sync with bvw_error_from_gst_error() */
  if (strstr (dbg, "401") != NULL)
    code = 401;
  else if (strstr (dbg, "404") != NULL)
    code = 404;
  else if (strstr (dbg, "403") != NULL)
    code = 403;
  else if (strstr (dbg, "install glib-networking") != NULL)
    code = 495;

done:
  if (err != NULL)
    g_error_free (err);
  g_free (dbg);
  return code;
}

/* returns TRUE if the error should be ignored */
static gboolean
bvw_check_missing_auth (BaconVideoWidget * bvw, GstMessage * err_msg)
{
  GMountOperationClass *klass;
  int code;

  if (gtk_widget_get_realized (GTK_WIDGET (bvw)) == FALSE)
    return FALSE;

  /* The user already tried, and we aborted */
  if (bvw->auth_last_result == G_MOUNT_OPERATION_ABORTED) {
    GST_DEBUG ("Not authenticating, the user aborted the last auth attempt");
    return FALSE;
  }
  /* There's already an auth on-going, ignore */
  if (bvw->auth_dialog != NULL) {
    GST_DEBUG ("Ignoring error, we're doing authentication");
    return TRUE;
  }

  /* RTSP or HTTP source with user-id property ? */
  code = bvw_get_http_error_code (err_msg);
  if (code != 401)
    return FALSE;

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (err_msg->src), "user-id") == NULL) {
    GST_DEBUG ("HTTP error is 401, but don't have \"user-id\" property, exiting");
    return FALSE;
  }

  GST_DEBUG ("Trying to get auth for location '%s'", GST_STR_NULL (bvw->mrl));

  if (bvw->auth_dialog == NULL) {
    bvw->auth_dialog = gtk_mount_operation_new (bvw->parent_toplevel);
    g_signal_connect (G_OBJECT (bvw->auth_dialog), "reply",
		      G_CALLBACK (bvw_auth_reply_cb), bvw);
  }

  /* And popup the dialogue! */
  klass = (GMountOperationClass *) G_OBJECT_GET_CLASS (bvw->auth_dialog);
  klass->ask_password (bvw->auth_dialog,
		       _("Password requested for RTSP server"),
		       g_get_user_name (),
		       NULL,
		       G_ASK_PASSWORD_NEED_PASSWORD | G_ASK_PASSWORD_NEED_USERNAME);
  return TRUE;
}

/* returns TRUE if the error has been handled and should be ignored */
static gboolean
bvw_check_missing_plugins_error (BaconVideoWidget * bvw, GstMessage * err_msg)
{
  gboolean error_src_is_playbin;
  GError *err = NULL;

  if (bvw->missing_plugins == NULL) {
    GST_DEBUG ("no missing-plugin messages");
    return FALSE;
  }

  gst_message_parse_error (err_msg, &err, NULL);

  error_src_is_playbin = (err_msg->src == GST_OBJECT_CAST (bvw->play));

  /* If we get a WRONG_TYPE error from playbin itself it's most likely because
   * there is a subtitle stream we can decode, but no video stream to overlay
   * it on. Since there were missing-plugins messages, we'll assume this is
   * because we cannot decode the video stream (this should probably be fixed
   * in playbin, but for now we'll work around it here) */
  if (is_error (err, CORE, MISSING_PLUGIN) ||
      is_error (err, STREAM, CODEC_NOT_FOUND) ||
      (is_error (err, STREAM, WRONG_TYPE) && error_src_is_playbin)) {
    bvw_check_if_video_decoder_is_missing (bvw);
    set_current_actor (bvw);
  } else {
    GST_DEBUG ("not an error code we are looking for, doing nothing");
  }

  g_error_free (err);
  return FALSE;
}

static gboolean
bvw_check_mpeg_eos (BaconVideoWidget *bvw, GstMessage *err_msg)
{
  gboolean ret = FALSE;
  g_autoptr(GError) err = NULL;
  g_autofree char *dbg = NULL;

  gst_message_parse_error (err_msg, &err, &dbg);

  /* Error from gst-libs/gst/video/gstvideodecoder.c
   * thrown by mpeg2dec */

  if (err != NULL &&
      dbg != NULL &&
      is_error (err, STREAM, DECODE) &&
      strstr (dbg, "no valid frames found")) {
    if (bvw->eos_id == 0) {
      bvw->eos_id = g_idle_add (bvw_signal_eos_delayed, bvw);
      g_source_set_name_by_id (bvw->eos_id, "[totem] bvw_signal_eos_delayed");
      GST_DEBUG ("Throwing EOS instead of an error when seeking to the end of an MPEG file");
    } else {
      GST_DEBUG ("Not throwing EOS instead of an error when seeking to the end of an MPEG file, EOS already planned");
    }
    ret = TRUE;
  }

  return ret;
}

static void
bvw_update_tags (BaconVideoWidget * bvw, GstTagList *tag_list, const gchar *type)
{
  GstTagList **cache = NULL;
  GstTagList *result;

  /* all tags (replace previous tags, title/artist/etc. might change
   * in the middle of a stream, e.g. with radio streams) */
  result = gst_tag_list_merge (bvw->tagcache, tag_list,
                                   GST_TAG_MERGE_REPLACE);
  if (bvw->tagcache &&
      result &&
      gst_tag_list_is_equal (result, bvw->tagcache)) {
    gst_tag_list_unref (result);
    GST_WARNING ("Pipeline sent %s tags update with no changes", type);
    return;
  }
  g_clear_pointer (&bvw->tagcache, gst_tag_list_unref);
  bvw->tagcache = result;
  GST_DEBUG ("Tags: %" GST_PTR_FORMAT, tag_list);

  /* media-type-specific tags */
  if (!strcmp (type, "video")) {
    cache = &bvw->videotags;
  } else if (!strcmp (type, "audio")) {
    cache = &bvw->audiotags;
  }

  if (cache) {
    result = gst_tag_list_merge (*cache, tag_list, GST_TAG_MERGE_REPLACE);
    if (*cache)
      gst_tag_list_unref (*cache);
    *cache = result;
  }

  /* clean up */
  if (tag_list)
    gst_tag_list_unref (tag_list);

  g_signal_emit (bvw, bvw_signals[SIGNAL_GOT_METADATA], 0);

  set_current_actor (bvw);
}

static void
update_tags_delayed_data_destroy (UpdateTagsDelayedData *data)
{
  g_slice_free (UpdateTagsDelayedData, data);
}

static gboolean
bvw_update_tags_dispatcher (BaconVideoWidget *bvw)
{
  UpdateTagsDelayedData *data;

  /* If we take the queue's lock for the entire function call, we can use it to protect tag_update_id too */
  g_async_queue_lock (bvw->tag_update_queue);

  while ((data = g_async_queue_try_pop_unlocked (bvw->tag_update_queue)) != NULL) {
    bvw_update_tags (bvw, data->tags, data->type);
    update_tags_delayed_data_destroy (data);
  }

  bvw->tag_update_id = 0;
  g_async_queue_unlock (bvw->tag_update_queue);

  return FALSE;
}

/* Marshal the changed tags to the main thread for updating the GUI
 * and sending the BVW signals */
static void
bvw_update_tags_delayed (BaconVideoWidget *bvw, GstTagList *tags, const gchar *type) {
  UpdateTagsDelayedData *data = g_slice_new0 (UpdateTagsDelayedData);

  data->tags = tags;
  data->type = type;

  g_async_queue_lock (bvw->tag_update_queue);
  g_async_queue_push_unlocked (bvw->tag_update_queue, data);

  if (bvw->tag_update_id == 0) {
    bvw->tag_update_id = g_idle_add ((GSourceFunc) bvw_update_tags_dispatcher, bvw);
    g_source_set_name_by_id (bvw->tag_update_id, "[totem] bvw_update_tags_dispatcher");
  }

  g_async_queue_unlock (bvw->tag_update_queue);
}

static void
video_tags_changed_cb (GstElement *playbin2, gint stream_id, gpointer user_data)
{
  BaconVideoWidget *bvw = (BaconVideoWidget *) user_data;
  GstTagList *tags = NULL;
  gint current_stream_id = 0;

  g_object_get (G_OBJECT (bvw->play), "current-video", &current_stream_id, NULL);

  /* Only get the updated tags if it's for our current stream id */
  if (current_stream_id != stream_id)
    return;

  g_signal_emit_by_name (G_OBJECT (bvw->play), "get-video-tags", stream_id, &tags);

  if (tags)
    bvw_update_tags_delayed (bvw, tags, "video");
}

static void
audio_tags_changed_cb (GstElement *playbin2, gint stream_id, gpointer user_data)
{
  BaconVideoWidget *bvw = (BaconVideoWidget *) user_data;
  GstTagList *tags = NULL;
  gint current_stream_id = 0;

  g_object_get (G_OBJECT (bvw->play), "current-audio", &current_stream_id, NULL);

  /* Only get the updated tags if it's for our current stream id */
  if (current_stream_id != stream_id)
    return;

  g_signal_emit_by_name (G_OBJECT (bvw->play), "get-audio-tags", stream_id, &tags);

  if (tags)
    bvw_update_tags_delayed (bvw, tags, "audio");
}

static void
text_tags_changed_cb (GstElement *playbin2, gint stream_id, gpointer user_data)
{
  BaconVideoWidget *bvw = (BaconVideoWidget *) user_data;
  GstTagList *tags = NULL;
  gint current_stream_id = 0;

  g_object_get (G_OBJECT (bvw->play), "current-text", &current_stream_id, NULL);

  /* Only get the updated tags if it's for our current stream id */
  if (current_stream_id != stream_id)
    return;

  g_signal_emit_by_name (G_OBJECT (bvw->play), "get-text-tags", stream_id, &tags);

  if (tags)
    bvw_update_tags_delayed (bvw, tags, "text");
}

static gboolean
bvw_download_buffering_done (BaconVideoWidget *bvw)
{
  /* When we set buffering left to 0, that means it's ready to play */
  if (bvw->buffering_left == 0) {
    GST_DEBUG ("Buffering left is 0, so buffering done");
    return TRUE;
  }
  if (bvw->stream_length <= 0)
    return FALSE;
  /* When queue2 doesn't implement buffering-left, always think
   * it's ready to go */
  if (bvw->buffering_left < 0) {
    GST_DEBUG ("Buffering left not implemented, so buffering done");
    return TRUE;
  }

  if (bvw->buffering_left * BUFFERING_LEFT_RATIO < bvw->stream_length - bvw->current_time) {
    GST_DEBUG ("Buffering left: %" G_GINT64_FORMAT " * %f, = %f < %" G_GUINT64_FORMAT,
	       bvw->buffering_left, BUFFERING_LEFT_RATIO,
	       bvw->buffering_left * BUFFERING_LEFT_RATIO,
	       bvw->stream_length - bvw->current_time);
    return TRUE;
  }
  return FALSE;
}

static void
bvw_handle_buffering_message (GstMessage * message, BaconVideoWidget *bvw)
{
  GstBufferingMode mode;
  gint percent = 0;

   gst_message_parse_buffering_stats (message, &mode, NULL, NULL, NULL);
   if (mode == GST_BUFFERING_DOWNLOAD) {
     if (bvw->download_buffering == FALSE) {
       bvw->download_buffering = TRUE;

       /* We're not ready to play yet, so pause the stream */
       GST_DEBUG ("Pausing because we're not ready to play the buffer yet");
       gst_element_set_state (GST_ELEMENT (bvw->play), GST_STATE_PAUSED);

       bvw_reconfigure_fill_timeout (bvw, 200);
     }

     return;
   }

   /* We switched from download mode to normal buffering */
   if (bvw->download_buffering != FALSE) {
     bvw_reconfigure_fill_timeout (bvw, 0);
     bvw->download_buffering = FALSE;
     g_clear_pointer (&bvw->download_filename, g_free);
   }

   /* Live, timeshift and stream buffering modes */
  gst_message_parse_buffering (message, &percent);
  g_signal_emit (bvw, bvw_signals[SIGNAL_BUFFERING], 0, (gdouble) percent / 100.0);

  if (percent >= 100) {
    /* a 100% message means buffering is done */
    bvw->buffering = FALSE;
    /* if the desired state is playing, go back */
    if (bvw->target_state == GST_STATE_PLAYING) {
      GST_DEBUG ("Buffering done, setting pipeline back to PLAYING");
      bacon_video_widget_play (bvw, NULL);
    } else {
      GST_DEBUG ("Buffering done, keeping pipeline PAUSED");
    }
  } else if (bvw->target_state == GST_STATE_PLAYING) {
    GstState cur_state;

    gst_element_get_state (bvw->play, &cur_state, NULL, 0);
    if (cur_state != GST_STATE_PAUSED) {
      GST_DEBUG ("Buffering ... temporarily pausing playback %d%%", percent);
      gst_element_set_state (bvw->play, GST_STATE_PAUSED);
    } else {
      GST_LOG ("Buffering (already paused) ... %d%%", percent);
    }
    bvw->buffering = TRUE;
  } else {
    GST_LOG ("Buffering ... %d", percent);
    bvw->buffering = TRUE;
  }
}

static inline void
bvw_get_navigation_if_available (BaconVideoWidget *bvw)
{
  GstElement * nav;
  nav = gst_bin_get_by_interface (GST_BIN (bvw->play),
                                        GST_TYPE_NAVIGATION);
  g_clear_pointer (&bvw->navigation, gst_object_unref);

  if (nav)
    bvw->navigation = GST_NAVIGATION (nav);
}

static void
bvw_handle_toc_message (GstMessage       *message,
			BaconVideoWidget *bvw)
{
  GstToc *toc;
  GList *entries, *l;
  guint i;

  gst_message_parse_toc (message, &toc, NULL);
  if (gst_toc_get_scope (toc) != GST_TOC_SCOPE_GLOBAL)
    goto out;

  entries = gst_toc_get_entries (toc);

parse:
  if (entries == NULL)
    goto out;
  if (gst_toc_entry_get_entry_type (entries->data) != GST_TOC_ENTRY_TYPE_CHAPTER) {
    if (g_list_length (entries) == 1) {
      entries = gst_toc_entry_get_sub_entries (entries->data);
      goto parse;
    }
    goto out;
  }

  GST_DEBUG ("Found %d chapters", g_list_length (entries));

  if (bvw->chapters)
    g_list_free_full (bvw->chapters, (GDestroyNotify) gst_mini_object_unref);

  for (l = entries, i = 0; l != NULL; l = l->next, i++) {
    GstTocEntry *entry = l->data;
    gint64 start, stop;

    if (!gst_toc_entry_get_start_stop_times (entry, &start, &stop)) {
      GST_DEBUG ("Chapter #%d (couldn't get times)", i);
    } else {
      GST_DEBUG ("Chapter #%d (start: %" G_GINT64_FORMAT " stop: %" G_GINT64_FORMAT ")", i, start, stop);
    }
  }

  bvw->chapters = g_list_copy_deep (entries, (GCopyFunc) gst_mini_object_ref, NULL);

out:
  gst_toc_unref (toc);
}

static void
bvw_bus_message_cb (GstBus * bus, GstMessage * message, BaconVideoWidget *bvw)
{
  GstMessageType msg_type;

  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  msg_type = GST_MESSAGE_TYPE (message);

  if (msg_type != GST_MESSAGE_STATE_CHANGED) {
    gchar *src_name = gst_object_get_name (message->src);
    GST_LOG ("Handling %s message from element %s",
        gst_message_type_get_name (msg_type), src_name);
    g_free (src_name);
  }

  switch (msg_type) {
    case GST_MESSAGE_ERROR: {
      totem_gst_message_print (message, bvw->play, "totem-error");

      if (!bvw_check_missing_plugins_error (bvw, message) &&
	  !bvw_check_missing_auth (bvw, message) &&
	  !bvw_check_mpeg_eos (bvw, message)) {
        GError *error;

        error = bvw_error_from_gst_error (bvw, message);

        bvw->target_state = GST_STATE_NULL;
        if (bvw->play)
          gst_element_set_state (bvw->play, GST_STATE_NULL);

        bvw->buffering = FALSE;

        g_signal_emit (bvw, bvw_signals[SIGNAL_ERROR], 0,
                       error->message, TRUE);

        g_error_free (error);
      }
      break;
    }
    case GST_MESSAGE_WARNING: {
      GST_WARNING ("Warning message: %" GST_PTR_FORMAT, message);
      break;
    }
    case GST_MESSAGE_TAG: 
      /* Ignore TAG messages, we get updated tags from the
       * {audio,video,text}-tags-changed signals of playbin2
       */
      break;
    case GST_MESSAGE_EOS:
      GST_DEBUG ("EOS message");
      /* update slider one last time */
      bvw_query_timeout (bvw);
      if (bvw->eos_id == 0) {
        bvw->eos_id = g_idle_add (bvw_signal_eos_delayed, bvw);
        g_source_set_name_by_id (bvw->eos_id, "[totem] bvw_signal_eos_delayed");
      }
      break;
    case GST_MESSAGE_BUFFERING:
      bvw_handle_buffering_message (message, bvw);
      break;
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
      if (GST_MESSAGE_SRC (message) != GST_OBJECT (bvw->play))
        break;

      src_name = gst_object_get_name (message->src);
      GST_DEBUG ("%s changed state from %s to %s", src_name,
          gst_element_state_get_name (old_state),
          gst_element_state_get_name (new_state));
      g_free (src_name);

      if (new_state <= GST_STATE_READY) {
        if (bvw->navigation)
          g_clear_object (&bvw->navigation);
      }

      /* now do stuff */
      if (new_state <= GST_STATE_PAUSED) {
        bvw_query_timeout (bvw);
        bvw_reconfigure_tick_timeout (bvw, 0);
      } else if (new_state > GST_STATE_PAUSED) {
        bvw_reconfigure_tick_timeout (bvw, 200);
      }

      if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
        GST_DEBUG_BIN_TO_DOT_FILE (GST_BIN_CAST (bvw->play),
            GST_DEBUG_GRAPH_SHOW_ALL ^ GST_DEBUG_GRAPH_SHOW_NON_DEFAULT_PARAMS,
            "totem-prerolled");
	bacon_video_widget_get_stream_length (bvw);
        bvw_update_stream_info (bvw);
        /* show a non-fatal warning message if we can't decode the video */
        bvw_show_error_if_video_decoder_is_missing (bvw);
	/* Now that we have the length, check whether we wanted
	 * to pause or to stop the pipeline */
        if (bvw->target_state == GST_STATE_PAUSED)
	  bacon_video_widget_pause (bvw);
      } else if (old_state == GST_STATE_PAUSED && new_state == GST_STATE_READY) {
        bvw->media_has_video = FALSE;
        bvw->media_has_audio = FALSE;
	bvw->media_has_unsupported_video = FALSE;

        /* clean metadata cache */
	g_clear_pointer (&bvw->tagcache, gst_tag_list_unref);
	g_clear_pointer (&bvw->audiotags, gst_tag_list_unref);
	g_clear_pointer (&bvw->videotags, gst_tag_list_unref);

        bvw->video_width = 0;
        bvw->video_height = 0;
      }
      break;
    }
    case GST_MESSAGE_ELEMENT: {
      bvw_handle_element_message (bvw, message);
      break;
    }

    case GST_MESSAGE_DURATION_CHANGED: {
      gint64 len = -1;
      if (gst_element_query_duration (bvw->play, GST_FORMAT_TIME, &len) && len != -1) {
        bvw->stream_length = len / GST_MSECOND;
	GST_DEBUG ("got new stream length (through duration message) %" G_GINT64_FORMAT, bvw->stream_length);
      }
      break;
    }

    case GST_MESSAGE_ASYNC_DONE: {
	gint64 _time;
	/* When a seek has finished, set the playing state again */
	g_mutex_lock (&bvw->seek_mutex);

	bvw->seek_req_time = gst_clock_get_internal_time (bvw->clock);
	_time = bvw->seek_time;
	bvw->seek_time = -1;

	g_mutex_unlock (&bvw->seek_mutex);

	if (_time >= 0) {
	  GST_DEBUG ("Have an old seek to schedule, doing it now");
	  bacon_video_widget_seek_time_no_lock (bvw, _time, 0, NULL);
	} else if (bvw->target_state == GST_STATE_PLAYING) {
	  GST_DEBUG ("Maybe starting deferred playback after seek");
	  bacon_video_widget_play (bvw, NULL);
	}
	bvw_get_navigation_if_available (bvw);
	bacon_video_widget_get_stream_length (bvw);
	bacon_video_widget_is_seekable (bvw);
      break;
    }

    case GST_MESSAGE_TOC: {
	bvw_handle_toc_message (message, bvw);
	break;
    }

    /* FIXME: at some point we might want to handle CLOCK_LOST and set the
     * pipeline back to PAUSED and then PLAYING again to select a different
     * clock (this seems to trip up rtspsrc though so has to wait until
     * rtspsrc gets fixed) */
    case GST_MESSAGE_CLOCK_PROVIDE:
    case GST_MESSAGE_CLOCK_LOST:
    case GST_MESSAGE_NEW_CLOCK:
    case GST_MESSAGE_STATE_DIRTY:
    case GST_MESSAGE_STREAM_STATUS:
      break;

    case GST_MESSAGE_UNKNOWN:
    case GST_MESSAGE_INFO:
    case GST_MESSAGE_STEP_DONE:
    case GST_MESSAGE_STRUCTURE_CHANGE:
    case GST_MESSAGE_SEGMENT_START:
    case GST_MESSAGE_SEGMENT_DONE:
    case GST_MESSAGE_LATENCY:
    case GST_MESSAGE_ASYNC_START:
    case GST_MESSAGE_REQUEST_STATE:
    case GST_MESSAGE_STEP_START:
    case GST_MESSAGE_QOS:
    case GST_MESSAGE_PROGRESS:
    case GST_MESSAGE_ANY:
    case GST_MESSAGE_RESET_TIME:
    case GST_MESSAGE_STREAM_START:
    case GST_MESSAGE_NEED_CONTEXT:
    case GST_MESSAGE_HAVE_CONTEXT:
    default:
      GST_LOG ("Unhandled message: %" GST_PTR_FORMAT, message);
      break;
  }
}

static void
got_time_tick (GstElement * play, gint64 time_nanos, BaconVideoWidget * bvw)
{
  gboolean seekable;

  bvw->current_time = (gint64) time_nanos / GST_MSECOND;

  if (bvw->stream_length == 0) {
    bvw->current_position = 0;
  } else {
    bvw->current_position =
      (gdouble) bvw->current_time / bvw->stream_length;
  }

  if (bvw->stream_length == 0) {
    seekable = bacon_video_widget_is_seekable (bvw);
  } else {
    if (bvw->seekable == -1)
      g_object_notify (G_OBJECT (bvw), "seekable");
    seekable = TRUE;
  }

  bvw->is_live = (bvw->stream_length == 0);

/*
  GST_DEBUG ("current time: %" GST_TIME_FORMAT ", stream length: %" GST_TIME_FORMAT ", seekable: %s",
      GST_TIME_ARGS (bvw->current_time * GST_MSECOND),
      GST_TIME_ARGS (bvw->stream_length * GST_MSECOND),
      (seekable) ? "TRUE" : "FALSE");
*/

  g_signal_emit (bvw, bvw_signals[SIGNAL_TICK], 0,
                 bvw->current_time, bvw->stream_length,
                 bvw->current_position,
                 seekable);
}

static void
bvw_set_user_agent_on_element (BaconVideoWidget * bvw, GstElement * element)
{
  const char *user_agent;

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (element), "user-agent") == NULL)
    return;

  user_agent = bvw->user_agent ? bvw->user_agent : DEFAULT_USER_AGENT;
  GST_DEBUG ("Setting HTTP user-agent to '%s'", user_agent);
  g_object_set (element, "user-agent", user_agent, NULL);
}

static void
bvw_set_auth_on_element (BaconVideoWidget * bvw, GstElement * element)
{
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (element), "user-id") == NULL)
    return;
  if (bvw->auth_last_result != G_MOUNT_OPERATION_HANDLED)
    return;
  if (bvw->user_id == NULL || bvw->user_pw == NULL)
    return;

  GST_DEBUG ("Setting username and password");
  g_object_set (element,
		"user-id", bvw->user_id,
		"user-pw", bvw->user_pw,
		NULL);

  g_clear_pointer (&bvw->user_id, g_free);
  g_clear_pointer (&bvw->user_pw, g_free);
}

static void
bvw_set_http_proxy_on_element (BaconVideoWidget *bvw,
			       GstElement       *element,
			       const char       *uri_str)
{
  GstUri *uri;
  char *protocol, *proxy_url;
  const char *host, *userinfo;
  guint port;
  char **user_strv;
  g_autofree char *user = NULL;
  g_autofree char *password = NULL;

  uri = gst_uri_from_string (uri_str);
  if (!uri) {
    GST_DEBUG ("Failed to parse URI '%s'", uri_str);
    return;
  }

  protocol = gst_uri_get_protocol (uri_str);
  host = gst_uri_get_host (uri);
  port = gst_uri_get_port (uri);

  proxy_url = g_strdup_printf ("%s://%s:%d", protocol, host, port);
  g_object_set (element, "proxy", proxy_url, NULL);
  g_free (proxy_url);

  /* https doesn't handle authentication yet */
  if (gst_uri_has_protocol (uri_str, "https"))
    goto finish;

  userinfo = gst_uri_get_userinfo (uri);
  if (userinfo == NULL)
    goto finish;

  user_strv = g_strsplit (userinfo, ":", 2);
  user = g_uri_unescape_string (user_strv[0], NULL);
  password = g_uri_unescape_string (user_strv[1], NULL);

  g_object_set (element,
		"proxy-id", user,
		"proxy-pw", password,
		NULL);
  g_strfreev (user_strv);

finish:
  gst_uri_unref (uri);
}

static void
bvw_set_proxy_on_element (BaconVideoWidget * bvw, GstElement * element)
{
  GError *error = NULL;
  char **uris;

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (element), "proxy") == NULL)
    return;

  uris = g_proxy_resolver_lookup (g_proxy_resolver_get_default (),
				  bvw->mrl,
				  NULL,
				  &error);
  if (!uris) {
    if (error != NULL) {
      GST_DEBUG ("Failed to look up proxy for MRL '%s': %s",
                 bvw->mrl,
                 error->message);
      g_clear_error (&error);
    }
    return;
  }

  if (!g_str_equal (uris[0], "direct://"))
    bvw_set_http_proxy_on_element (bvw, element, uris[0]);
  g_strfreev (uris);
}

static void
bvw_set_referrer_on_element (BaconVideoWidget * bvw, GstElement * element)
{
  GstStructure *extra_headers = NULL;

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (element), "extra-headers") == NULL)
    return;

  GST_DEBUG ("Setting HTTP referrer to '%s'", bvw->referrer ? bvw->referrer : "none");

  g_object_get (element, "extra-headers", &extra_headers, NULL);
  if (extra_headers == NULL) {
    extra_headers = gst_structure_new_empty ("extra-headers");
  }
  g_assert (GST_IS_STRUCTURE (extra_headers));

  if (bvw->referrer != NULL) {
    gst_structure_set (extra_headers,
                       "Referer" /* not a typo! */,
                       G_TYPE_STRING,
                       bvw->referrer,
                       NULL);
  } else {
    gst_structure_remove_field (extra_headers,
                                "Referer" /* not a typo! */);
  }

  g_object_set (element, "extra-headers", extra_headers, NULL);
  gst_structure_free (extra_headers);
}

static void
playbin_source_setup_cb (GstElement       *playbin,
			 GstElement       *source,
			 BaconVideoWidget *bvw)
{
  GST_DEBUG ("Got source of type '%s'", G_OBJECT_TYPE_NAME (source));
  if (g_strcmp0 (G_OBJECT_TYPE_NAME (source), "GstCurlHttpSrc") == 0)
    g_warning ("Download buffering not supported with GstCurlHttpSrc, see https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/issues/551");
  bvw_set_user_agent_on_element (bvw, source);
  bvw_set_referrer_on_element (bvw, source);
  bvw_set_auth_on_element (bvw, source);
  bvw_set_proxy_on_element (bvw, source);
}

static void
playbin_element_setup_cb (GstElement *playbin,
			  GstElement *element,
			  BaconVideoWidget *bvw)
{
  char *template;

  if (g_strcmp0 (G_OBJECT_TYPE_NAME (element), "GstDownloadBuffer") != 0)
    return;

  /* See also bacon_video_widget_init() */
  template = g_build_filename (g_get_user_cache_dir (), "totem", "stream-buffer", "XXXXXX", NULL);
  g_object_set (element, "temp-template", template, NULL);
  GST_DEBUG ("Reconfigured file download template to '%s'", template);
  g_free (template);
}

static void
playbin_deep_notify_cb (GstObject  *gstobject,
			GstObject  *prop_object,
			GParamSpec *prop,
			BaconVideoWidget *bvw)
{
  if (g_str_equal (prop->name, "temp-location") == FALSE)
    return;

  g_clear_pointer (&bvw->download_filename, g_free);
  g_object_get (G_OBJECT (prop_object),
		"temp-location", &bvw->download_filename,
		NULL);
}

static gboolean
bvw_query_timeout (BaconVideoWidget *bvw)
{
  gint64 pos = -1;

  /* check pos of stream */
  if (gst_element_query_position (bvw->play, GST_FORMAT_TIME, &pos)) {
    if (pos != -1) {
      got_time_tick (GST_ELEMENT (bvw->play), pos, bvw);
    }
  } else {
    GST_DEBUG ("could not get position");
  }

  return TRUE;
}

static gboolean
bvw_query_buffering_timeout (BaconVideoWidget *bvw)
{
  GstQuery *query;
  GstElement *element;

  element = bvw->play;

  query = gst_query_new_buffering (GST_FORMAT_PERCENT);
  if (gst_element_query (element, query)) {
    gint64 stop, estimated_total;
    gdouble fill;
    guint n_ranges, i, pos;

    gst_query_parse_buffering_range (query, NULL, NULL, &stop, &estimated_total);

    /* stop expresses the last bit of data that we have from the currently downloading
     * region and is a good value to use for the fill level if it is after our
     * current position. */
    pos = bvw->current_position * GST_FORMAT_PERCENT_MAX;
    if (stop < pos)
      stop = -1;

    n_ranges = gst_query_get_n_buffering_ranges (query);

    for (i = 0; i < n_ranges; i++) {
      gint64 n_start, n_stop;
      gst_query_parse_nth_buffering_range (query, i, &n_start, &n_stop);

      /* take first stop after current offset if not known */
      if (stop == -1 && n_stop > pos)
        stop = n_stop;

      GST_DEBUG ("%s range %d: start %" G_GINT64_FORMAT " stop %" G_GINT64_FORMAT,
		 n_stop == stop ? "*" : " ",
		 i, n_start, n_stop);
    }
    /* if no fill level, just take the current position */
    if (stop == -1)
      stop = pos;

    /* estimated_total is the amount of time it will take to download the
     * remaining part of the file, from the current position to the end. */
    bvw->buffering_left = estimated_total;
    GST_DEBUG ("stop %" G_GINT64_FORMAT ", buffering left %" G_GINT64_FORMAT,
               stop, bvw->buffering_left);

    fill = (gdouble) stop / GST_FORMAT_PERCENT_MAX;
    GST_DEBUG ("download buffer filled up to %f%% (element: %s)", fill * 100.0,
	       G_OBJECT_TYPE_NAME (element));

    g_signal_emit (bvw, bvw_signals[SIGNAL_DOWNLOAD_BUFFERING], 0, fill);

    /* Start playing when we've downloaded enough */
    if (bvw_download_buffering_done (bvw) != FALSE &&
	bvw->target_state == GST_STATE_PLAYING) {
      GST_DEBUG ("Starting playback because the download buffer is filled enough");
      bacon_video_widget_play (bvw, NULL);
    }
  } else {
    g_debug ("Failed to query the source element for buffering info in percent");
  }
  gst_query_unref (query);

  return TRUE;
}

static void
caps_set (GObject * obj,
    GParamSpec * pspec, BaconVideoWidget * bvw)
{
  GstPad *pad = GST_PAD (obj);
  GstStructure *s;
  GstCaps *caps;

  if (!(caps = gst_pad_get_current_caps (pad)))
    return;

  /* Get video decoder caps */
  s = gst_caps_get_structure (caps, 0);
  if (s) {
    /* We need at least width/height and framerate */
    if (!(gst_structure_get_fraction (s, "framerate", &bvw->video_fps_n,
          &bvw->video_fps_d) &&
          gst_structure_get_int (s, "width", &bvw->video_width) &&
          gst_structure_get_int (s, "height", &bvw->video_height)))
      return;
  }

  gst_caps_unref (caps);
}

static void
parse_stream_info (BaconVideoWidget *bvw)
{
  GstPad *videopad = NULL;
  gint n_audio, n_video;

  g_object_get (G_OBJECT (bvw->play), "n-audio", &n_audio,
      "n-video", &n_video, NULL);

  bvw->media_has_video = FALSE;
  bvw->media_has_unsupported_video = FALSE;
  if (n_video > 0) {
    gint i;

    bvw->media_has_video = TRUE;

    for (i = 0; i < n_video && videopad == NULL; i++)
      g_signal_emit_by_name (bvw->play, "get-video-pad", i, &videopad);
  }

  bvw->media_has_audio = (n_audio > 0);

  if (videopad) {
    GstCaps *caps;

    if ((caps = gst_pad_get_current_caps (videopad))) {
      caps_set (G_OBJECT (videopad), NULL, bvw);
      gst_caps_unref (caps);
    }
    g_signal_connect (videopad, "notify::caps",
        G_CALLBACK (caps_set), bvw);
    gst_object_unref (videopad);
  }

  set_current_actor (bvw);
}

static void
playbin_stream_changed_cb (GstElement * obj, gpointer data)
{
  BaconVideoWidget *bvw = BACON_VIDEO_WIDGET (data);
  GstMessage *msg;

  /* we're being called from the streaming thread, so don't do anything here */
  GST_LOG ("streams have changed");
  msg = gst_message_new_application (GST_OBJECT (bvw->play),
				     gst_structure_new_empty ("stream-changed"));
  gst_element_post_message (bvw->play, msg);
}

static void
bacon_video_widget_finalize (GObject * object)
{
  BaconVideoWidget *bvw = (BaconVideoWidget *) object;

  GST_DEBUG ("finalizing");

  g_type_class_unref (g_type_class_peek (BVW_TYPE_METADATA_TYPE));
  g_type_class_unref (g_type_class_peek (BVW_TYPE_DVD_EVENT));
  g_type_class_unref (g_type_class_peek (BVW_TYPE_ROTATION));

  if (bvw->bus) {
    /* make bus drop all messages to make sure none of our callbacks is ever
     * called again (main loop might be run again to display error dialog) */
    gst_bus_set_flushing (bvw->bus, TRUE);

    if (bvw->sig_bus_async)
      g_signal_handler_disconnect (bvw->bus, bvw->sig_bus_async);

    g_clear_pointer (&bvw->bus, gst_object_unref);
  }

  g_clear_error (&bvw->init_error);
  g_clear_pointer (&bvw->user_agent, g_free);
  g_clear_pointer (&bvw->referrer, g_free);
  g_clear_pointer (&bvw->mrl, g_free);
  g_clear_pointer (&bvw->subtitle_uri, g_free);
  g_clear_pointer (&bvw->user_id, g_free);
  g_clear_pointer (&bvw->user_pw, g_free);

  g_clear_object (&bvw->clock);

  if (bvw->play != NULL)
    gst_element_set_state (bvw->play, GST_STATE_NULL);

  g_clear_object (&bvw->play);

  if (bvw->update_id) {
    g_source_remove (bvw->update_id);
    bvw->update_id = 0;
  }

  if (bvw->chapters) {
    g_list_free_full (bvw->chapters, (GDestroyNotify) gst_mini_object_unref);
    bvw->chapters = NULL;
  }
  if (bvw->subtitles) {
    g_list_free_full (bvw->subtitles, (GDestroyNotify) bacon_video_widget_lang_info_free);
    bvw->subtitles = NULL;
  }
  if (bvw->languages) {
    g_list_free_full (bvw->languages, (GDestroyNotify) bacon_video_widget_lang_info_free);
    bvw->languages = NULL;
  }

  g_clear_pointer (&bvw->tagcache, gst_tag_list_unref);
  g_clear_pointer (&bvw->audiotags, gst_tag_list_unref);
  g_clear_pointer (&bvw->videotags, gst_tag_list_unref);

  if (bvw->tag_update_id != 0)
    g_source_remove (bvw->tag_update_id);
  g_async_queue_unref (bvw->tag_update_queue);

  if (bvw->eos_id != 0) {
    g_source_remove (bvw->eos_id);
    bvw->eos_id = 0;
  }

  if (bvw->mount_cancellable)
    g_cancellable_cancel (bvw->mount_cancellable);
  g_clear_object (&bvw->mount_cancellable);

  g_mutex_clear (&bvw->seek_mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
bacon_video_widget_set_property (GObject * object, guint property_id,
                                 const GValue * value, GParamSpec * pspec)
{
  BaconVideoWidget *bvw;

  bvw = BACON_VIDEO_WIDGET (object);

  switch (property_id) {
    case PROP_REFERRER:
      bacon_video_widget_set_referrer (bvw, g_value_get_string (value));
      break;
    case PROP_USER_AGENT:
      bacon_video_widget_set_user_agent (bvw, g_value_get_string (value));
      break;
    case PROP_VOLUME:
      bacon_video_widget_set_volume (bvw, g_value_get_double (value));
      break;
    case PROP_DEINTERLACING:
      bacon_video_widget_set_deinterlacing (bvw, g_value_get_boolean (value));
      break;
    case PROP_BRIGHTNESS:
      bacon_video_widget_set_video_property (bvw, BVW_VIDEO_BRIGHTNESS, g_value_get_int (value));
      break;
    case PROP_CONTRAST:
      bacon_video_widget_set_video_property (bvw, BVW_VIDEO_CONTRAST, g_value_get_int (value));
      break;
    case PROP_SATURATION:
      bacon_video_widget_set_video_property (bvw, BVW_VIDEO_SATURATION, g_value_get_int (value));
      break;
    case PROP_HUE:
      bacon_video_widget_set_video_property (bvw, BVW_VIDEO_HUE, g_value_get_int (value));
      break;
    case PROP_AUDIO_OUTPUT_TYPE:
      bacon_video_widget_set_audio_output_type (bvw, g_value_get_enum (value));
      break;
    case PROP_AV_OFFSET:
      g_object_set_property (G_OBJECT (bvw->play), "av-offset", value);
      break;
    case PROP_SHOW_CURSOR:
      bacon_video_widget_set_show_cursor (bvw, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
bacon_video_widget_get_property (GObject * object, guint property_id,
                                 GValue * value, GParamSpec * pspec)
{
  BaconVideoWidget *bvw;

  bvw = BACON_VIDEO_WIDGET (object);

  switch (property_id) {
    case PROP_POSITION:
      g_value_set_double (value, bacon_video_widget_get_position (bvw));
      break;
    case PROP_STREAM_LENGTH:
      g_value_set_int64 (value, bacon_video_widget_get_stream_length (bvw));
      break;
    case PROP_PLAYING:
      g_value_set_boolean (value, bacon_video_widget_is_playing (bvw));
      break;
    case PROP_REFERRER:
      g_value_set_string (value, bvw->referrer);
      break;
    case PROP_SEEKABLE:
      g_value_set_boolean (value, bacon_video_widget_is_seekable (bvw));
      break;
    case PROP_USER_AGENT:
      g_value_set_string (value, bvw->user_agent);
      break;
    case PROP_VOLUME:
      g_value_set_double (value, bvw->volume);
      break;
    case PROP_DOWNLOAD_FILENAME:
      g_value_set_string (value, bvw->download_filename);
      break;
    case PROP_DEINTERLACING:
      g_value_set_boolean (value, bacon_video_widget_get_deinterlacing (bvw));
      break;
    case PROP_BRIGHTNESS:
      g_value_set_int (value, bacon_video_widget_get_video_property (bvw, BVW_VIDEO_BRIGHTNESS));
      break;
    case PROP_CONTRAST:
      g_value_set_int (value, bacon_video_widget_get_video_property (bvw, BVW_VIDEO_CONTRAST));
      break;
    case PROP_SATURATION:
      g_value_set_int (value, bacon_video_widget_get_video_property (bvw, BVW_VIDEO_SATURATION));
      break;
    case PROP_HUE:
      g_value_set_int (value, bacon_video_widget_get_video_property (bvw, BVW_VIDEO_HUE));
      break;
    case PROP_AUDIO_OUTPUT_TYPE:
      g_value_set_enum (value, bacon_video_widget_get_audio_output_type (bvw));
      break;
    case PROP_AV_OFFSET:
      g_object_get_property (G_OBJECT (bvw->play), "av-offset", value);
      break;
    case PROP_SHOW_CURSOR:
      g_value_set_boolean (value, bvw->cursor_shown);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

/**
 * bacon_video_widget_get_subtitle:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the id of the current subtitles.
 *
 * Return value: the subtitle id
 **/
int
bacon_video_widget_get_subtitle (BaconVideoWidget * bvw)
{
  int subtitle = -1;
  gint flags;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), BVW_TRACK_NONE);
  g_return_val_if_fail (bvw->play != NULL, BVW_TRACK_NONE);

  if (g_list_length (bvw->subtitles) == 1)
    return BVW_TRACK_NONE;

  g_object_get (bvw->play, "flags", &flags, NULL);

  if ((flags & GST_PLAY_FLAG_TEXT) == 0)
    return BVW_TRACK_NONE;

  g_object_get (G_OBJECT (bvw->play), "current-text", &subtitle, NULL);

  return subtitle;
}

static BvwLangInfo *
find_info_for_id (GList *list,
		  int    id)
{
  GList *l;

  if (list == NULL)
    return NULL;
  for (l = list; l != NULL; l = l->next) {
    BvwLangInfo *info = l->data;
    if (info->id == id)
        return info;
  }
  return NULL;
}

/**
 * bacon_video_widget_set_subtitle:
 * @bvw: a #BaconVideoWidget
 * @subtitle: a subtitle id
 *
 * Sets the subtitle id for @bvw.
 **/
void
bacon_video_widget_set_subtitle (BaconVideoWidget * bvw, int subtitle)
{
  GstTagList *tags;
  int flags;

  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (bvw->play != NULL);
  g_return_if_fail (find_info_for_id (bvw->subtitles, subtitle) != NULL);

  g_object_get (bvw->play, "flags", &flags, NULL);

  if (subtitle == BVW_TRACK_NONE) {
    flags &= ~GST_PLAY_FLAG_TEXT;
    g_object_set (bvw->play, "flags", flags, NULL);
  } else {
    flags |= GST_PLAY_FLAG_TEXT;
    g_object_set (bvw->play, "flags", flags, "current-text", subtitle, NULL);
    g_signal_emit_by_name (G_OBJECT (bvw->play), "get-text-tags", subtitle, &tags);
    bvw_update_tags (bvw, tags, "text");
  }
}

/**
 * bacon_video_toggle_subtitles:
 * @bvw: a #BaconVideoWidget
 *
 * Toggles the visibility of subtitles.
 **/
void
bacon_video_toggle_subtitles (BaconVideoWidget *bvw)
{
  guint flags;

  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (bvw->play != NULL);

  g_object_get (bvw->play, "flags", &flags, NULL);
  if (flags & GST_PLAY_FLAG_TEXT)
    flags &= ~GST_PLAY_FLAG_TEXT;
  else
    flags |= GST_PLAY_FLAG_TEXT;
  g_object_set (bvw->play, "flags", flags, NULL);
  g_signal_emit (bvw, bvw_signals[SIGNAL_SUBTITLES_CHANGED], 0);
}

static BvwLangInfo *
find_next_info_for_id (GList *list,
		       int    current)
{
  GList *l;

  if (list == NULL)
    return NULL;
  for (l = list; l != NULL; l = l->next) {
    BvwLangInfo *info = l->data;
    if (info->id == current) {
      if (l->next == NULL)
        return list->data;
      return l->next->data;
    }
  }
  return NULL;
}

/**
 * bacon_video_widget_set_next_subtitle:
 * @bvw: a #BaconVideoWidget
 *
 * Switch to the next text subtitle for the current video. See
 * bacon_video_widget_set_subtitle().
 *
 * Since: 3.12
 */
void
bacon_video_widget_set_next_subtitle (BaconVideoWidget *bvw)
{
  BvwLangInfo *info;
  int current_text;

  current_text = bacon_video_widget_get_subtitle (bvw);
  info = find_next_info_for_id (bvw->subtitles, current_text);
  if (!info) {
    GST_DEBUG ("Could not find next subtitle id (current = %d)", current_text);
    return;
  }
  GST_DEBUG ("Switching from subtitle %d to next %d", current_text, info->id);
  bacon_video_widget_set_subtitle (bvw, info->id);
  g_signal_emit (bvw, bvw_signals[SIGNAL_SUBTITLES_CHANGED], 0);
}

static gboolean
bvw_chapter_compare_func (GstTocEntry      *entry,
			  BaconVideoWidget *bvw)
{
  gint64 start, stop;

  if (!gst_toc_entry_get_start_stop_times (entry, &start, &stop))
    return -1;

  if (bvw->current_time >= start / GST_MSECOND &&
      bvw->current_time < stop / GST_MSECOND)
    return 0;

  return -1;
}

static GList *
bvw_get_current_chapter (BaconVideoWidget *bvw)
{
  return g_list_find_custom (bvw->chapters, bvw, (GCompareFunc) bvw_chapter_compare_func);
}

/**
 * bacon_video_widget_has_next_track:
 * @bvw: a #BaconVideoWidget
 *
 * Determines whether there is another track after the current one, typically
 * as a chapter on a DVD.
 *
 * Return value: %TRUE if there is another track, %FALSE otherwise
 **/
gboolean
bacon_video_widget_has_next_track (BaconVideoWidget *bvw)
{
  GList *l;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

  if (bvw->mrl == NULL)
    return FALSE;

  if (g_str_has_prefix (bvw->mrl, "dvd:/"))
    return TRUE;

  l = bvw_get_current_chapter (bvw);
  if (l != NULL && l->next != NULL)
    return TRUE;

  return FALSE;
}

/**
 * bacon_video_widget_has_previous_track:
 * @bvw: a #BaconVideoWidget
 *
 * Determines whether there is another track before the current one, typically
 * as a chapter on a DVD.
 *
 * Return value: %TRUE if there is another track, %FALSE otherwise
 **/
gboolean
bacon_video_widget_has_previous_track (BaconVideoWidget *bvw)
{
  GstFormat fmt;
  gint64 val;
  GList *l;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

  if (bvw->mrl == NULL)
    return FALSE;

  if (g_str_has_prefix (bvw->mrl, "dvd:/"))
    return TRUE;

  /* Look in the chapters first */
  l = bvw_get_current_chapter (bvw);
  if (l != NULL && l->prev != NULL)
    return TRUE;

  fmt = gst_format_get_by_nick ("chapter");
  /* If chapter isn't registered, then there's no chapters support */
  if (fmt == GST_FORMAT_UNDEFINED)
    return FALSE;

  if (gst_element_query_position (bvw->play, fmt, &val))
    return (val > 0);

  return FALSE;
}

static gboolean
bvw_lang_infos_equal (GList *orig, GList *new)
{
  GList *o, *n;
  gboolean retval;

  if ((orig == NULL && new != NULL) || (orig != NULL && new == NULL))
    return FALSE;
  if (orig == NULL && new == NULL)
    return TRUE;

  if (g_list_length (orig) != g_list_length (new))
    return FALSE;

  retval = TRUE;
  o = orig;
  n = new;
  while (o != NULL && n != NULL && retval != FALSE) {
    BvwLangInfo *info_o, *info_n;

    info_o = o->data;
    info_n = n->data;
    if (g_strcmp0 (info_o->title, info_n->title) != 0)
      retval = FALSE;
    if (g_strcmp0 (info_o->language, info_n->language) != 0)
      retval = FALSE;
    if (g_strcmp0 (info_o->codec, info_n->codec) != 0)
      retval = FALSE;
    o = g_list_next (o);
    n = g_list_next (n);
  }

  return retval;
}

static GList *
get_lang_list_for_type (BaconVideoWidget * bvw, const gchar * type_name)
{
  GList *ret = NULL;
  gint i, n;
  const char *prop;
  const char *signal;

  if (g_str_equal (type_name, "AUDIO")) {
    prop = "n-audio";
    signal = "get-audio-tags";
  } else if (g_str_equal (type_name, "TEXT")) {
    prop = "n-text";
    signal = "get-text-tags";
  } else {
    g_critical ("Invalid stream type '%s'", type_name);
    return NULL;
  }

  n = 0;
  g_object_get (G_OBJECT (bvw->play), prop, &n, NULL);
  if (n == 0)
    return NULL;

  for (i = 0; i < n; i++) {
    GstTagList *tags = NULL;
    BvwLangInfo *info;

    g_signal_emit_by_name (G_OBJECT (bvw->play), signal, i, &tags);

    info = g_new0 (BvwLangInfo, 1);
    info->id = i;

    if (tags) {
      gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &info->language);
      gst_tag_list_get_string (tags, GST_TAG_TITLE, &info->title);
      if (g_str_equal (type_name, "AUDIO"))
	gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &info->codec);

      gst_tag_list_unref (tags);
    }

    if (info->language == NULL)
      info->language = g_strdup ("und");
    ret = g_list_prepend (ret, info);
  }

  return g_list_reverse (ret);
}

static void
print_lang_list (GList *list)
{
  GList *l;

  if (list == NULL) {
    GST_DEBUG ("  Empty list");
    return;
  }

  for (l = list; l != NULL; l = l->next) {
    BvwLangInfo *info = l->data;
    GST_DEBUG ("  %d: %s / %s / %s", info->id,
	       GST_STR_NULL (info->title),
	       GST_STR_NULL (info->language),
	       GST_STR_NULL (info->codec));
  }
}

static gboolean
update_subtitles_tracks (BaconVideoWidget *bvw)
{
  g_autolist(BvwLangInfo) list;
  BvwLangInfo *info;

  list = get_lang_list_for_type (bvw, "TEXT");

  /* Add "None" */
  info = g_new0 (BvwLangInfo, 1);
  info->id = BVW_TRACK_NONE;
  info->codec = g_strdup ("none");
  list = g_list_prepend (list, info);

  if (bvw_lang_infos_equal (list, bvw->subtitles))
    return FALSE;
  if (bvw->subtitles)
    g_list_free_full (bvw->subtitles, (GDestroyNotify) bacon_video_widget_lang_info_free);
  GST_DEBUG ("subtitles changed:");
  print_lang_list (list);
  bvw->subtitles = g_steal_pointer (&list);
  return TRUE;
}

static gboolean
update_languages_tracks (BaconVideoWidget *bvw)
{
  g_autolist(BvwLangInfo) list;

  list = get_lang_list_for_type (bvw, "AUDIO");

  /* Add "auto" if we have a DVD */
  if (g_str_has_prefix (bvw->mrl, "dvd:")) {
    BvwLangInfo *info;
    info = g_new0 (BvwLangInfo, 1);
    info->id = 0;
    info->codec = g_strdup ("auto");
    list = g_list_prepend (list, info);
  }
  if (bvw_lang_infos_equal (list, bvw->languages))
    return FALSE;
  if (bvw->languages)
    g_list_free_full (bvw->languages, (GDestroyNotify) bacon_video_widget_lang_info_free);
  GST_DEBUG ("languages changed:");
  print_lang_list (list);
  bvw->languages = g_steal_pointer (&list);
  return TRUE;
}

/**
 * bacon_video_widget_lang_info_free:
 * @info: a #BvwLangInfo
 *
 * Frees a #BvwLangInfo structure.
 */
void
bacon_video_widget_lang_info_free (BvwLangInfo *info)
{
  if (info == NULL)
    return;
  g_free (info->title);
  g_free (info->language);
  g_free (info->codec);
  g_free (info);
}

/**
 * bacon_video_widget_get_subtitles:
 * @bvw: a #BaconVideoWidget
 *
 * Returns a list of #BvwLangInfo for each subtitle track.
 *
 * Return value: a #GList of #BvwLangInfo, or %NULL; this list is owned by the @bvw, do not free.
 **/
GList *
bacon_video_widget_get_subtitles (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (bvw->play != NULL, NULL);

  return bvw->subtitles;
}

/**
 * bacon_video_widget_get_languages:
 * @bvw: a #BaconVideoWidget
 *
 * Returns a list of #BvwLangInfo for each audio track.
 *
 * Return value: a #GList of #BvwLangInfo, or %NULL; this list is owned by the @bvw, do not free.
 **/
GList *
bacon_video_widget_get_languages (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (bvw->play != NULL, NULL);

  return bvw->languages;
}

/**
 * bacon_video_widget_get_language:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the id of the current audio language.
 *
 * If the widget is not playing, or the default language is in use, <code class="literal">-1</code> will be returned.
 *
 * Return value: the audio language index
 **/
int
bacon_video_widget_get_language (BaconVideoWidget * bvw)
{
  int language = -1;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
  g_return_val_if_fail (bvw->play != NULL, -1);

  g_object_get (G_OBJECT (bvw->play), "current-audio", &language, NULL);

  return language;
}

/**
 * bacon_video_widget_set_language:
 * @bvw: a #BaconVideoWidget
 * @language: an audio language index
 *
 * Sets the audio language id for @bvw.
 **/
void
bacon_video_widget_set_language (BaconVideoWidget * bvw, int language)
{
  GstTagList *tags;

  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (bvw->play != NULL);
  g_return_if_fail (find_info_for_id (bvw->languages, language) != NULL);

  GST_DEBUG ("setting language to %d", language);

  g_object_set (bvw->play, "current-audio", language, NULL);

  g_signal_emit_by_name (G_OBJECT (bvw->play), "get-audio-tags", language, &tags);
  bvw_update_tags (bvw, tags, "audio");
  if (update_languages_tracks (bvw))
    g_signal_emit (bvw, bvw_signals[SIGNAL_LANGUAGES_CHANGED], 0);

  /* so it updates its metadata for the newly-selected stream */
  g_signal_emit (bvw, bvw_signals[SIGNAL_GOT_METADATA], 0, NULL);
}

/**
 * bacon_video_widget_set_next_language:
 * @bvw: a #BaconVideoWidget
 *
 * Switch to the next audio language for the current video. See
 * bacon_video_widget_set_language().
 *
 * Since: 3.12
 */
void
bacon_video_widget_set_next_language (BaconVideoWidget *bvw)
{
  BvwLangInfo *info;
  int current_audio;

  g_object_get (bvw->play, "current-audio", &current_audio, NULL);
  info = find_next_info_for_id (bvw->languages, current_audio);
  if (!info) {
    GST_DEBUG ("Could not find next language id (current = %d)", current_audio);
    return;
  }
  GST_DEBUG ("Switching from audio track %d to next %d", current_audio, info->id);
  bacon_video_widget_set_language (bvw, info->id);
  g_signal_emit (bvw, bvw_signals[SIGNAL_LANGUAGES_CHANGED], 0);
}

/**
 * bacon_video_widget_set_deinterlacing:
 * @bvw: a #BaconVideoWidget
 * @deinterlace: %TRUE if videos should be automatically deinterlaced, %FALSE otherwise
 *
 * Sets whether the widget should deinterlace videos.
 **/
void
bacon_video_widget_set_deinterlacing (BaconVideoWidget * bvw,
                                      gboolean deinterlace)
{
  gint flags;

  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));

  g_object_get (bvw->play, "flags", &flags, NULL);
  if (deinterlace)
    flags |= GST_PLAY_FLAG_DEINTERLACE;
  else
    flags &= ~GST_PLAY_FLAG_DEINTERLACE;
  g_object_set (bvw->play, "flags", flags, NULL);

  g_object_notify (G_OBJECT (bvw), "deinterlacing");
}

/**
 * bacon_video_widget_get_deinterlacing:
 * @bvw: a #BaconVideoWidget
 *
 * Returns whether deinterlacing of videos is enabled for this widget.
 *
 * Return value: %TRUE if automatic deinterlacing is enabled, %FALSE otherwise
 **/
gboolean
bacon_video_widget_get_deinterlacing (BaconVideoWidget * bvw)
{
  gint flags;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);

  g_object_get (bvw->play, "flags", &flags, NULL);

  return !!(flags & GST_PLAY_FLAG_DEINTERLACE);
}

static gint
get_num_audio_channels (BaconVideoWidget * bvw)
{
  gint channels;

  switch (bvw->speakersetup) {
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
  GstPad *pad, *peer_pad;

  /* reset old */
  g_object_set (bvw->audio_capsfilter, "caps", NULL, NULL);

  /* construct possible caps to filter down to our chosen caps */
  /* Start with what the audio sink supports, but limit the allowed
   * channel count to our speaker output configuration */
  pad = gst_element_get_static_pad (bvw->audio_capsfilter, "src");
  peer_pad = gst_pad_get_peer (pad);
  gst_object_unref (pad);

  caps = gst_pad_get_current_caps (peer_pad);
  gst_object_unref (peer_pad);

  if ((channels = get_num_audio_channels (bvw)) == -1)
    return;

  res = fixate_to_num (caps, channels);
  gst_caps_unref (caps);

  /* set */
  if (res && gst_caps_is_empty (res)) {
    gst_caps_unref (res);
    res = NULL;
  }
  g_object_set (bvw->audio_capsfilter, "caps", res, NULL);

  if (res) {
    gst_caps_unref (res);
  }

  /* reset */
  pad = gst_element_get_static_pad (bvw->audio_capsfilter, "src");
  gst_pad_set_caps (pad, NULL);
  gst_object_unref (pad);
}

/**
 * bacon_video_widget_get_audio_output_type:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the current audio output type (e.g. how many speaker channels)
 * from #BvwAudioOutputType.
 *
 * Return value: the audio output type, or <code class="literal">-1</code>
 **/
BvwAudioOutputType
bacon_video_widget_get_audio_output_type (BaconVideoWidget *bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);

  return bvw->speakersetup;
}

/**
 * bacon_video_widget_set_audio_output_type:
 * @bvw: a #BaconVideoWidget
 * @type: the new audio output type
 *
 * Sets the audio output type (number of speaker channels) in the video widget.
 **/
void
bacon_video_widget_set_audio_output_type (BaconVideoWidget *bvw,
                                          BvwAudioOutputType type)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  if (type == bvw->speakersetup)
    return;
  else if (type == BVW_AUDIO_SOUND_AC3PASSTHRU)
    return;

  bvw->speakersetup = type;
  g_object_notify (G_OBJECT (bvw), "audio-output-type");

  set_audio_filter (bvw);
}

/* =========================================== */
/*                                             */
/*               Play/Pause, Stop              */
/*                                             */
/* =========================================== */

static GError*
bvw_error_from_gst_error (BaconVideoWidget *bvw, GstMessage * err_msg)
{
  const gchar *src_typename;
  GError *ret = NULL;
  GError *e = NULL;
  char *dbg = NULL;
  int http_error_code;

  GST_LOG ("resolving %" GST_PTR_FORMAT, err_msg);

  src_typename = (err_msg->src) ? G_OBJECT_TYPE_NAME (err_msg->src) : NULL;

  gst_message_parse_error (err_msg, &e, &dbg);

  /* FIXME:
   * Unemitted errors:
   * BVW_ERROR_BROKEN_FILE
   */

  if (src_typename &&
      g_str_equal (src_typename, "GstGtkGLSink") &&
      is_error (e, RESOURCE, NOT_FOUND)) {
    bvw->media_has_unsupported_video = TRUE;
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_GENERIC,
			       _("Could not initialize OpenGL support."));
    set_current_actor (bvw);
    goto done;
  }

  /* Can't open optical disc? */
  if (is_error (e, RESOURCE, NOT_FOUND) ||
      is_error (e, RESOURCE, OPEN_READ)) {
    if (g_str_has_prefix (bvw->mrl, "dvd:")) {
      ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_INVALID_DEVICE,
				 _("The DVD device you specified seems to be invalid."));
      goto done;
    } else if (g_str_has_prefix (bvw->mrl, "vcd:")) {
      ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_INVALID_DEVICE,
				 _("The VCD device you specified seems to be invalid."));
      goto done;
    }
  }

  /* Check for encrypted DVD */
  if (is_error (e, RESOURCE, READ) &&
      g_str_has_prefix (bvw->mrl, "dvd:")) {
    GModule *module;
    gpointer sym;

    module = g_module_open ("libdvdcss", 0);
    if (module == NULL ||
	g_module_symbol (module, "dvdcss_open", &sym)) {
      g_clear_pointer (&module, g_module_close);
      ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_DVD_ENCRYPTED,
				 _("The source seems encrypted and can’t be read. Are you trying to play an encrypted DVD without libdvdcss?"));
      goto done;
    }
    g_clear_pointer (&module, g_module_close);
  }

  /* HTTP error codes */
  /* FIXME: bvw_get_http_error_code() calls gst_message_parse_error too */
  http_error_code = bvw_get_http_error_code (err_msg);

  if (is_error (e, RESOURCE, NOT_FOUND) ||
      http_error_code == 404) {
    if (strstr (e->message, "Cannot resolve hostname") != NULL) {
      ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_UNKNOWN_HOST,
				 _("The server you are trying to connect to is not known."));
    } else if (strstr (e->message, "Cannot connect to destination") != NULL) {
      ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_CONNECTION_REFUSED,
				 _("The connection to this server was refused."));
    } else {
      ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_FILE_NOT_FOUND,
				 _("The specified movie could not be found."));
    }
    goto done;
  }

  if (http_error_code == 403) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_FILE_PERMISSION,
			       _("The server refused access to this file or stream."));
    goto done;
  }

  if (http_error_code == 401) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_FILE_PERMISSION,
			       _("Authentication is required to access this file or stream."));
    goto done;
  }

  if (http_error_code == 495) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_READ_ERROR,
			       _("SSL/TLS support is missing. Check your installation."));
    goto done;
  }

  if (is_error (e, RESOURCE, OPEN_READ)) {
    if (strstr (dbg, g_strerror (EACCES)) != NULL) {
      ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_FILE_PERMISSION,
				 _("You are not allowed to open this file."));
      goto done;
    }
    if (strstr (dbg, "Error parsing URL.") != NULL) {
      ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_INVALID_LOCATION,
				 _("This location is not a valid one."));
      goto done;
    }
  }

  if (is_error (e, RESOURCE, OPEN_READ) ||
      is_error (e, RESOURCE, READ)) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_READ_ERROR,
			       _("The movie could not be read."));
    goto done;
  }

  if (is_error (e, STREAM, DECRYPT)) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_FILE_ENCRYPTED,
			       _("This file is encrypted and cannot be played back."));
    goto done;
  }

  if (is_error (e, STREAM, TYPE_NOT_FOUND)) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_EMPTY_FILE,
			       _("The file you tried to play is an empty file."));
    goto done;
  }

  if (e->domain == GST_RESOURCE_ERROR) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_FILE_GENERIC,
                               e->message);
    goto done;
  }

  if (is_error (e, CORE, MISSING_PLUGIN) ||
      is_error (e, STREAM, CODEC_NOT_FOUND) ||
      is_error (e, STREAM, WRONG_TYPE) ||
      is_error (e, STREAM, NOT_IMPLEMENTED) ||
      (is_error (e, STREAM, FORMAT) && strstr (dbg, "no video pad or visualizations"))) {
    if (bvw->missing_plugins != NULL) {
      gchar **descs, *msg = NULL;
      guint num;

      descs = bvw_get_missing_plugins_descriptions (bvw->missing_plugins);
      num = g_list_length (bvw->missing_plugins);

      if (is_error (e, CORE, MISSING_PLUGIN)) {
        /* should be exactly one missing thing (source or converter) */
        msg = g_strdup_printf (_("The playback of this movie requires a %s "
				 "plugin which is not installed."), descs[0]);
	ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_NO_PLUGIN_FOR_FILE, msg);
	g_free (msg);
      } else {
        gchar *desc_list;

        desc_list = g_strjoinv ("\n", descs);
        msg = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "The playback of this movie "
            "requires a %s plugin which is not installed.", "The playback "
            "of this movie requires the following plugins which are not "
            "installed:\n\n%s", num), (num == 1) ? descs[0] : desc_list);
        g_free (desc_list);
	ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_CODEC_NOT_HANDLED, msg);
	g_free (msg);
      }
      g_strfreev (descs);
    } else {
      if (g_str_has_prefix (bvw->mrl, "rtsp:")) {
	ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_NETWORK_UNREACHABLE,
				   _("This stream cannot be played. It’s possible that a firewall is blocking it."));
      } else {
	ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_CODEC_NOT_HANDLED,
				   _("An audio or video stream is not handled due to missing codecs. "
				     "You might need to install additional plugins to be able to play some types of movies"));
      }
    }
    goto done;
  }

  if (is_error (e, STREAM, FAILED) &&
	     src_typename &&
	     strncmp (src_typename, "GstTypeFind", 11) == 0) {
    ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_READ_ERROR,
			       _("This file cannot be played over the network. Try downloading it locally first."));
    goto done;
  }

  /* generic error, no code; take message */
  ret = g_error_new_literal (BVW_ERROR, BVW_ERROR_GENERIC,
			     e->message);

done:
  g_error_free (e);
  g_free (dbg);
  bvw_clear_missing_plugins_messages (bvw);

  return ret;
}

static char *
get_target_uri (GFile *file)
{
  GFileInfo *info;
  char *target;

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI, G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (info == NULL)
    return NULL;
  target = g_strdup (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI));
  g_object_unref (info);

  return target;
}

/**
 * bacon_video_widget_open:
 * @bvw: a #BaconVideoWidget
 * @mrl: an MRL
 *
 * Opens the given @mrl in @bvw for playing.
 *
 * The MRL is loaded and waiting to be played with bacon_video_widget_play().
 **/
void
bacon_video_widget_open (BaconVideoWidget *bvw,
                         const char       *mrl)
{
  GFile *file;

  g_return_if_fail (mrl != NULL);
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (bvw->play != NULL);
  
  /* So we aren't closed yet... */
  if (bvw->mrl) {
    bacon_video_widget_close (bvw);
  }
  
  GST_DEBUG ("mrl = %s", GST_STR_NULL (mrl));

  /* this allows non-URI type of files in the thumbnailer and so on */
  file = g_file_new_for_commandline_arg (mrl);

  if (g_file_has_uri_scheme (file, "trash") != FALSE ||
      g_file_has_uri_scheme (file, "recent") != FALSE) {
    bvw->mrl = get_target_uri (file);
    GST_DEBUG ("Found target location '%s' for original MRL '%s'",
	       GST_STR_NULL (bvw->mrl), mrl);
  } else if (g_file_has_uri_scheme (file, "cdda") != FALSE) {
    char *path;
    path = g_file_get_path (file);
    bvw->mrl = g_filename_to_uri (path, NULL, NULL);
    g_free (path);
  } else {
    bvw->mrl = g_strdup (mrl);
  }

  g_object_unref (file);

  bvw->got_redirect = FALSE;
  bvw->media_has_video = FALSE;
  bvw->media_has_unsupported_video = FALSE;
  bvw->media_has_audio = FALSE;

  /* Flush the bus to make sure we don't get any messages
   * from the previous URI, see bug #607224.
   */
  gst_bus_set_flushing (bvw->bus, TRUE);
  bvw->target_state = GST_STATE_READY;
  gst_element_set_state (bvw->play, GST_STATE_READY);
  gst_bus_set_flushing (bvw->bus, FALSE);

  g_object_set (bvw->play, "uri", bvw->mrl, NULL);

  bvw->seekable = -1;
  bvw->target_state = GST_STATE_PAUSED;
  bvw_clear_missing_plugins_messages (bvw);

  gst_element_set_state (bvw->play, GST_STATE_PAUSED);

  if (update_subtitles_tracks (bvw))
    g_signal_emit (bvw, bvw_signals[SIGNAL_SUBTITLES_CHANGED], 0);
  if (update_languages_tracks (bvw))
    g_signal_emit (bvw, bvw_signals[SIGNAL_LANGUAGES_CHANGED], 0);
  g_signal_emit (bvw, bvw_signals[SIGNAL_CHANNELS_CHANGE], 0);
}

/**
 * bacon_video_widget_play:
 * @bvw: a #BaconVideoWidget
 * @error: a #GError, or %NULL
 *
 * Plays the currently-loaded video in @bvw.
 *
 * Errors from the GStreamer backend will be returned asynchronously via the
 * #BaconVideoWidget::error signal, even if this function returns %TRUE.
 *
 * Return value: %TRUE on success, %FALSE otherwise
 **/
gboolean
bacon_video_widget_play (BaconVideoWidget * bvw, GError ** error)
{
  GstState cur_state;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);
  g_return_val_if_fail (bvw->mrl != NULL, FALSE);

  bvw->target_state = GST_STATE_PLAYING;

  /* Don't try to play if we're already doing that */
  gst_element_get_state (bvw->play, &cur_state, NULL, 0);
  if (cur_state == GST_STATE_PLAYING)
    return TRUE;

  /* Lie when trying to play a file whilst we're download buffering */
  if (bvw->download_buffering != FALSE &&
      bvw_download_buffering_done (bvw) == FALSE) {
    GST_DEBUG ("download buffering in progress, not playing");
    return TRUE;
  }

  /* Or when we're buffering */
  if (bvw->buffering != FALSE) {
    GST_DEBUG ("buffering in progress, not playing");
    return TRUE;
  }

  /* just lie and do nothing in this case */
  if (bvw->mount_in_progress) {
    GST_DEBUG ("Mounting in progress, not playing");
    return TRUE;
  } else if (bvw->auth_dialog != NULL) {
    GST_DEBUG ("Authentication in progress, not playing");
    return TRUE;
  }

  /* Set direction to forward */
  if (bvw_set_playback_direction (bvw, TRUE) == FALSE) {
    GST_DEBUG ("Failed to reset direction back to forward to play");
    g_set_error_literal (error, BVW_ERROR, BVW_ERROR_GENERIC,
        _("This file could not be played. Try restarting playback."));
    return FALSE;
  }

  g_signal_emit (bvw, bvw_signals[SIGNAL_PLAY_STARTING], 0);

  GST_DEBUG ("play");
  gst_element_set_state (bvw->play, GST_STATE_PLAYING);

  /* will handle all errors asynchroneously */
  return TRUE;
}

/**
 * bacon_video_widget_can_direct_seek:
 * @bvw: a #BaconVideoWidget
 *
 * Determines whether direct seeking is possible for the current stream.
 *
 * Return value: %TRUE if direct seeking is possible, %FALSE otherwise
 **/
gboolean
bacon_video_widget_can_direct_seek (BaconVideoWidget *bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);

  if (bvw->mrl == NULL)
    return FALSE;

  if (bvw->download_buffering != FALSE)
    return TRUE;

  /* (instant seeking only make sense with video,
   * hence no cdda:// here) */
  if (g_str_has_prefix (bvw->mrl, "file://") ||
      g_str_has_prefix (bvw->mrl, "dvd:/") ||
      g_str_has_prefix (bvw->mrl, "vcd:/") ||
      g_str_has_prefix (bvw->mrl, "trash:/"))
    return TRUE;

  return FALSE;
}

static gboolean
bacon_video_widget_seek_time_no_lock (BaconVideoWidget *bvw,
				      gint64 _time,
				      GstSeekFlags flag,
				      GError **error)
{
  if (bvw_set_playback_direction (bvw, TRUE) == FALSE)
    return FALSE;

  bvw->seek_time = -1;

  gst_element_set_state (bvw->play, GST_STATE_PAUSED);

  gst_element_seek (bvw->play, bvw->rate,
		    GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | flag,
		    GST_SEEK_TYPE_SET, _time * GST_MSECOND,
		    GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

  return TRUE;
}

/**
 * bacon_video_widget_seek_time:
 * @bvw: a #BaconVideoWidget
 * @_time: the time to which to seek, in milliseconds
 * @accurate: whether to use accurate seek, an accurate seek might be slower for some formats (see GStreamer docs)
 * @error: a #GError, or %NULL
 *
 * Seeks the currently-playing stream to the absolute position @time, in milliseconds.
 *
 * Return value: %TRUE on success, %FALSE otherwise
 **/
gboolean
bacon_video_widget_seek_time (BaconVideoWidget *bvw, gint64 _time, gboolean accurate, GError **error)
{
  GstClockTime cur_time;
  GstSeekFlags  flag;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);

  GST_LOG ("Seeking to %" GST_TIME_FORMAT, GST_TIME_ARGS (_time * GST_MSECOND));

  /* Don't say we'll seek past the end */
  _time = MIN (_time, bvw->stream_length);

  /* Emit a time tick of where we are going, we are paused */
  got_time_tick (bvw->play, _time * GST_MSECOND, bvw);

  /* Is there a pending seek? */
  g_mutex_lock (&bvw->seek_mutex);

  /* If there's no pending seek, or
   * it's been too long since the seek,
   * or we don't have an accurate seek requested */
  cur_time = gst_clock_get_internal_time (bvw->clock);
  if (bvw->seek_req_time == GST_CLOCK_TIME_NONE ||
      cur_time > bvw->seek_req_time + SEEK_TIMEOUT ||
      accurate) {
    bvw->seek_time = -1;
    bvw->seek_req_time = cur_time;
    g_mutex_unlock (&bvw->seek_mutex);
  } else {
    GST_LOG ("Not long enough since last seek, queuing it");
    bvw->seek_time = _time;
    g_mutex_unlock (&bvw->seek_mutex);
    return TRUE;
  }

  flag = (accurate ? GST_SEEK_FLAG_ACCURATE : GST_SEEK_FLAG_NONE);
  bacon_video_widget_seek_time_no_lock (bvw, _time, flag, error);

  return TRUE;
}

/**
 * bacon_video_widget_seek:
 * @bvw: a #BaconVideoWidget
 * @position: the percentage of the way through the stream to which to seek
 * @error: a #GError, or %NULL
 *
 * Seeks the currently-playing stream to @position as a percentage of the total
 * stream length.
 *
 * Return value: %TRUE on success, %FALSE otherwise
 **/
gboolean
bacon_video_widget_seek (BaconVideoWidget *bvw, double position, GError **error)
{
  gint64 seek_time, length_nanos;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);

  length_nanos = (gint64) (bvw->stream_length * GST_MSECOND);
  seek_time = (gint64) (length_nanos * position);

  GST_LOG ("Seeking to %3.2f%% %" GST_TIME_FORMAT, position,
      GST_TIME_ARGS (seek_time));

  return bacon_video_widget_seek_time (bvw, seek_time / GST_MSECOND, FALSE, error);
}

/**
 * bacon_video_widget_step:
 * @bvw: a #BaconVideoWidget
 * @forward: the direction of the frame step
 * @error: a #GError, or %NULL
 *
 * Step one frame forward, if @forward is %TRUE, or backwards, if @forward is %FALSE
 *
 * Return value: %TRUE on success, %FALSE otherwise
 **/
gboolean
bacon_video_widget_step (BaconVideoWidget *bvw, gboolean forward, GError **error)
{
  GstEvent *event;
  gboolean retval;

  if (bvw_set_playback_direction (bvw, forward) == FALSE)
    return FALSE;

  event = gst_event_new_step (GST_FORMAT_BUFFERS, 1, 1.0, TRUE, FALSE);

  retval = gst_element_send_event (bvw->play, event);

  if (retval != FALSE)
    bvw_query_timeout (bvw);
  else
    GST_WARNING ("Failed to step %s", DIRECTION_STR);

  return retval;
}

static void
bvw_stop_play_pipeline (BaconVideoWidget * bvw)
{
  GstState cur_state;

  gst_element_get_state (bvw->play, &cur_state, NULL, 0);
  if (cur_state > GST_STATE_READY) {
    GstMessage *msg;

    GST_DEBUG ("stopping");
    gst_element_set_state (bvw->play, GST_STATE_READY);

    /* process all remaining state-change messages so everything gets
     * cleaned up properly (before the state change to NULL flushes them) */
    GST_DEBUG ("processing pending state-change messages");
    while ((msg = gst_bus_pop_filtered (bvw->bus, GST_MESSAGE_STATE_CHANGED))) {
      gst_bus_async_signal_func (bvw->bus, msg, NULL);
      gst_message_unref (msg);
    }
  }

  /* and now drop all following messages until we start again. The
   * bus is set to flush=false again in bacon_video_widget_open()
   */
  if (bvw->bus)
    gst_bus_set_flushing (bvw->bus, TRUE);

  /* Now in READY or lower */
  bvw->target_state = GST_STATE_READY;

  bvw->buffering = FALSE;
  bvw->download_buffering = FALSE;
  g_clear_pointer (&bvw->download_filename, g_free);
  bvw->buffering_left = -1;
  bvw_reconfigure_fill_timeout (bvw, 0);
  g_signal_emit (bvw, bvw_signals[SIGNAL_BUFFERING], 0, 100.0);
  g_object_set (bvw->video_sink,
                "rotate-method", GST_VIDEO_ORIENTATION_AUTO,
                NULL);
  GST_DEBUG ("stopped");
}

/**
 * bacon_video_widget_stop:
 * @bvw: a #BaconVideoWidget
 *
 * Stops playing the current stream and resets to the first position in the stream.
 **/
void
bacon_video_widget_stop (BaconVideoWidget * bvw)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));

  GST_LOG ("Stopping");
  bvw_stop_play_pipeline (bvw);

  /* Reset position to 0 when stopping */
  got_time_tick (GST_ELEMENT (bvw->play), 0, bvw);
}

/**
 * bacon_video_widget_close:
 * @bvw: a #BaconVideoWidget
 *
 * Closes the current stream and frees the resources associated with it.
 **/
void
bacon_video_widget_close (BaconVideoWidget * bvw)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));
  
  GST_LOG ("Closing");
  bvw_stop_play_pipeline (bvw);

  g_clear_pointer (&bvw->mrl, g_free);
  g_clear_pointer (&bvw->subtitle_uri, g_free);
  g_object_set (G_OBJECT (bvw->play), "suburi", NULL, NULL);
  g_clear_pointer (&bvw->user_id, g_free);
  g_clear_pointer (&bvw->user_pw, g_free);

  bvw->is_live = FALSE;
  bvw->is_menu = FALSE;
  bvw->has_angles = FALSE;
  bvw->rate = FORWARD_RATE;

  bvw->current_time = 0;
  bvw->seek_req_time = GST_CLOCK_TIME_NONE;
  bvw->seek_time = -1;
  bvw->stream_length = 0;

  if (bvw->eos_id != 0)
    g_source_remove (bvw->eos_id);

  if (bvw->chapters) {
    g_list_free_full (bvw->chapters, (GDestroyNotify) gst_mini_object_unref);
    bvw->chapters = NULL;
  }
  if (bvw->subtitles) {
    g_list_free_full (bvw->subtitles, (GDestroyNotify) bacon_video_widget_lang_info_free);
    bvw->subtitles = NULL;
  }
  if (bvw->languages) {
    g_list_free_full (bvw->languages, (GDestroyNotify) bacon_video_widget_lang_info_free);
    bvw->languages = NULL;
  }

  g_clear_pointer (&bvw->tagcache, gst_tag_list_unref);
  g_clear_pointer (&bvw->audiotags, gst_tag_list_unref);
  g_clear_pointer (&bvw->videotags, gst_tag_list_unref);

  g_object_notify (G_OBJECT (bvw), "seekable");
  g_signal_emit (bvw, bvw_signals[SIGNAL_SUBTITLES_CHANGED], 0);
  g_signal_emit (bvw, bvw_signals[SIGNAL_LANGUAGES_CHANGED], 0);
  g_signal_emit (bvw, bvw_signals[SIGNAL_CHANNELS_CHANGE], 0);
  got_time_tick (GST_ELEMENT (bvw->play), 0, bvw);
}

static void
bvw_do_navigation_command (BaconVideoWidget * bvw, GstNavigationCommand command)
{
  if (bvw->navigation)
    gst_navigation_send_command (bvw->navigation, command);
}

/**
 * bacon_video_widget_set_text_subtitle:
 * @bvw: a #BaconVideoWidget
 * @subtitle_uri: (allow-none): the URI of a subtitle file, or %NULL
 *
 * Sets the URI for the text subtitle file to be displayed alongside
 * the current video. Use %NULL if you want to unload the current text subtitle
 * file being used.
 */
void
bacon_video_widget_set_text_subtitle (BaconVideoWidget * bvw,
				      const gchar * subtitle_uri)
{
  GstState cur_state;
  int lang;

  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));
  g_return_if_fail (bvw->mrl != NULL);

  GST_LOG ("Setting subtitle as %s", GST_STR_NULL (subtitle_uri));

  if (subtitle_uri == NULL &&
      bvw->subtitle_uri == NULL)
    return;

  /* Save current audio track */
  lang = bacon_video_widget_get_language (bvw);

  /* Wait for the previous state change to finish */
  gst_element_get_state (bvw->play, NULL, NULL, GST_CLOCK_TIME_NONE);

  /* -> READY */
  gst_element_get_state (bvw->play, &cur_state, NULL, 0);
  if (cur_state > GST_STATE_READY) {
    gst_element_set_state (bvw->play, GST_STATE_READY);
    /* Block for new state */
    gst_element_get_state (bvw->play, NULL, NULL, GST_CLOCK_TIME_NONE);
  }

  g_free (bvw->subtitle_uri);
  bvw->subtitle_uri = g_strdup (subtitle_uri);
  g_object_set (G_OBJECT (bvw->play), "suburi", subtitle_uri, NULL);

  /* And back to the original state */
  if (cur_state > GST_STATE_READY) {
    gst_element_set_state (bvw->play, cur_state);
    /* Block for new state */
    gst_element_get_state (bvw->play, NULL, NULL, GST_CLOCK_TIME_NONE);
  }

  if (bvw->current_time > 0) {
    bacon_video_widget_seek_time_no_lock (bvw, bvw->current_time,
					  GST_SEEK_FLAG_ACCURATE, NULL);
    bacon_video_widget_set_language (bvw, lang);
  }
}

static void
handle_dvd_seek (BaconVideoWidget *bvw,
		 int               offset,
		 const char       *fmt_name)
{
  GstFormat fmt;
  gint64 val;

  fmt = gst_format_get_by_nick (fmt_name);
  if (!fmt)
    return;

  bvw_set_playback_direction (bvw, TRUE);

  if (gst_element_query_position (bvw->play, fmt, &val)) {
    GST_DEBUG ("current %s is: %" G_GINT64_FORMAT, fmt_name, val);
    val += offset;
    GST_DEBUG ("seeking to %s: %" G_GINT64_FORMAT, fmt_name, val);
    gst_element_seek (bvw->play, FORWARD_RATE, fmt, GST_SEEK_FLAG_FLUSH,
		      GST_SEEK_TYPE_SET, val, GST_SEEK_TYPE_NONE, G_GINT64_CONSTANT (0));
    bvw->rate = FORWARD_RATE;
  } else {
    GST_DEBUG ("failed to query position (%s)", fmt_name);
  }
}

static gboolean
handle_chapters_seek (BaconVideoWidget *bvw,
		      gboolean          forward)
{
  GList *l;
  GstTocEntry *entry;
  gint64 start;

  l = bvw_get_current_chapter (bvw);
  if (!l)
    return FALSE;

  entry = NULL;
  if (forward && l->next)
    entry = l->next->data;
  else if (!forward) {
    gint64 current_start;
    if (gst_toc_entry_get_start_stop_times (l->data, &current_start, NULL)) {
      if (bvw->current_time - current_start / GST_MSECOND < REWIND_OR_PREVIOUS &&
	  bvw->current_time - current_start / GST_MSECOND > 0 &&
	  l->prev) {
	entry = l->prev->data;
      } else {
	entry = l->data;
      }
    }
  }

  if (!entry)
    return FALSE;

  if (!gst_toc_entry_get_start_stop_times (entry, &start, NULL))
    return FALSE;

  GST_DEBUG ("Found chapter and seeking to %" G_GINT64_FORMAT, start / GST_MSECOND);

  return bacon_video_widget_seek_time (bvw, start / GST_MSECOND, FALSE, NULL);
}

/**
 * bacon_video_widget_dvd_event:
 * @bvw: a #BaconVideoWidget
 * @type: the type of DVD event to issue
 *
 * Issues a DVD navigation event to the video widget, such as one to skip to the
 * next chapter, or navigate to the DVD title menu.
 *
 * This is a no-op if the current stream is not navigable.
 **/
void
bacon_video_widget_dvd_event (BaconVideoWidget * bvw,
                              BvwDVDEvent type)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));

  GST_DEBUG ("Sending event '%s'", g_enum_to_string (BVW_TYPE_DVD_EVENT, type));

  switch (type) {
    case BVW_DVD_ROOT_MENU:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_DVD_MENU);
      break;
    case BVW_DVD_TITLE_MENU:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_DVD_TITLE_MENU);
      break;
    case BVW_DVD_SUBPICTURE_MENU:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_DVD_SUBPICTURE_MENU);
      break;
    case BVW_DVD_AUDIO_MENU:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_DVD_AUDIO_MENU);
      break;
    case BVW_DVD_ANGLE_MENU:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_DVD_ANGLE_MENU);
      break;
    case BVW_DVD_CHAPTER_MENU:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_DVD_CHAPTER_MENU);
      break;
    case BVW_DVD_ROOT_MENU_UP:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_UP);
      break;
    case BVW_DVD_ROOT_MENU_DOWN:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_DOWN);
      break;
    case BVW_DVD_ROOT_MENU_LEFT:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_LEFT);
      break;
    case BVW_DVD_ROOT_MENU_RIGHT:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_RIGHT);
      break;
    case BVW_DVD_ROOT_MENU_SELECT:
      bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_ACTIVATE);
      break;
    case BVW_DVD_NEXT_CHAPTER:
      if (!handle_chapters_seek (bvw, TRUE))
	handle_dvd_seek (bvw, 1, "chapter");
      break;
    case BVW_DVD_PREV_CHAPTER:
      if (!handle_chapters_seek (bvw, FALSE))
	handle_dvd_seek (bvw, -1, "chapter");
      break;
    case BVW_DVD_NEXT_TITLE:
      handle_dvd_seek (bvw, 1, "title");
      break;
    case BVW_DVD_PREV_TITLE:
      handle_dvd_seek (bvw, -1, "title");
      break;
    default:
      GST_WARNING ("unhandled type %d", type);
      break;
  }
}

/**
 * bacon_video_widget_pause:
 * @bvw: a #BaconVideoWidget
 *
 * Pauses the current stream in the video widget.
 *
 * If a live stream is being played, playback is stopped entirely.
 **/
void
bacon_video_widget_pause (BaconVideoWidget * bvw)
{
  GstStateChangeReturn ret;
  GstState state;

  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));
  g_return_if_fail (bvw->mrl != NULL);

  /* Get the current state */
  ret = gst_element_get_state (GST_ELEMENT (bvw->play), &state, NULL, 0);

  if (bvw->is_live != FALSE &&
      ret != GST_STATE_CHANGE_NO_PREROLL &&
      ret != GST_STATE_CHANGE_SUCCESS &&
      state > GST_STATE_READY) {
    GST_LOG ("Stopping because we have a live stream");
    bacon_video_widget_stop (bvw);
    return;
  }

  GST_LOG ("Pausing");
  bvw->target_state = GST_STATE_PAUSED;
  gst_element_set_state (GST_ELEMENT (bvw->play), GST_STATE_PAUSED);
}

/**
 * bacon_video_widget_set_subtitle_font:
 * @bvw: a #BaconVideoWidget
 * @font: a font description string
 *
 * Sets the font size and style in which to display subtitles.
 *
 * @font is a Pango font description string, as understood by
 * pango_font_description_from_string().
 **/
void
bacon_video_widget_set_subtitle_font (BaconVideoWidget * bvw,
                                          const gchar * font)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));

  if (!g_object_class_find_property (G_OBJECT_GET_CLASS (bvw->play), "subtitle-font-desc"))
    return;
  g_object_set (bvw->play, "subtitle-font-desc", font, NULL);
}

/**
 * bacon_video_widget_set_subtitle_encoding:
 * @bvw: a #BaconVideoWidget
 * @encoding: an encoding system
 *
 * Sets the encoding system for the subtitles, so that they can be decoded
 * properly.
 **/
void
bacon_video_widget_set_subtitle_encoding (BaconVideoWidget *bvw,
                                          const char *encoding)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));

  if (!g_object_class_find_property (G_OBJECT_GET_CLASS (bvw->play), "subtitle-encoding"))
    return;
  g_object_set (bvw->play, "subtitle-encoding", encoding, NULL);
}

/**
 * bacon_video_widget_set_user_agent:
 * @bvw: a #BaconVideoWidget
 * @user_agent: a HTTP user agent string, or %NULL to use the default
 *
 * Sets the HTTP user agent string to use when fetching HTTP ressources.
 **/
void
bacon_video_widget_set_user_agent (BaconVideoWidget *bvw,
                                   const char *user_agent)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  if (g_strcmp0 (user_agent, bvw->user_agent) == 0)
    return;

  g_free (bvw->user_agent);
  bvw->user_agent = g_strdup (user_agent);

  g_object_notify (G_OBJECT (bvw), "user-agent");
}

/**
 * bacon_video_widget_set_referrer:
 * @bvw: a #BaconVideoWidget
 * @referrer: a HTTP referrer URI, or %NULL
 *
 * Sets the HTTP referrer URI to use when fetching HTTP ressources.
 **/
void
bacon_video_widget_set_referrer (BaconVideoWidget *bvw,
                                 const char *referrer)
{
  char *frag;

  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  if (g_strcmp0 (referrer, bvw->referrer) == 0)
    return;

  g_free (bvw->referrer);
  bvw->referrer = g_strdup (referrer);

  /* Referrer URIs must not have a fragment */
  if ((frag = strchr (bvw->referrer, '#')) != NULL)
    *frag = '\0';

  g_object_notify (G_OBJECT (bvw), "referrer");
}

/**
 * bacon_video_widget_can_set_volume:
 * @bvw: a #BaconVideoWidget
 *
 * Returns whether the volume level can be set, given the current settings.
 *
 * The volume cannot be set if the audio output type is set to
 * %BVW_AUDIO_SOUND_AC3PASSTHRU.
 *
 * Return value: %TRUE if the volume can be set, %FALSE otherwise
 **/
gboolean
bacon_video_widget_can_set_volume (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);

  if (bvw->speakersetup == BVW_AUDIO_SOUND_AC3PASSTHRU)
    return FALSE;

  return !bvw->uses_audio_fakesink;
}

/**
 * bacon_video_widget_set_volume:
 * @bvw: a #BaconVideoWidget
 * @volume: the new volume level, as a percentage between <code class="literal">0</code> and <code class="literal">1</code>
 *
 * Sets the volume level of the stream as a percentage between <code class="literal">0</code> and <code class="literal">1</code>.
 *
 * If bacon_video_widget_can_set_volume() returns %FALSE, this is a no-op.
 **/
void
bacon_video_widget_set_volume (BaconVideoWidget * bvw, double volume)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));

  if (bacon_video_widget_can_set_volume (bvw) != FALSE) {
    volume = CLAMP (volume, 0.0, 1.0);
    gst_stream_volume_set_volume (GST_STREAM_VOLUME (bvw->play),
                                  GST_STREAM_VOLUME_FORMAT_CUBIC,
                                  volume);

    bvw->volume = volume;
    g_object_notify (G_OBJECT (bvw), "volume");
  }
}

/**
 * bacon_video_widget_get_volume:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the current volume level, as a percentage between <code class="literal">0</code> and <code class="literal">1</code>.
 *
 * Return value: the volume as a percentage between <code class="literal">0</code> and <code class="literal">1</code>
 **/
double
bacon_video_widget_get_volume (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0.0);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), 0.0);

  return bvw->volume;
}

/**
 * bacon_video_widget_set_show_cursor:
 * @bvw: a #BaconVideoWidget
 * @show_cursor: %TRUE to show the cursor, %FALSE otherwise
 *
 * Sets whether the cursor should be shown when it is over the video
 * widget. If @show_cursor is %FALSE, the cursor will be invisible
 * when it is moved over the video widget.
 **/
void
bacon_video_widget_set_show_cursor (BaconVideoWidget * bvw,
                                    gboolean show_cursor)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  if (bvw->cursor_shown == show_cursor)
    return;
  bvw->cursor_shown = show_cursor;
  update_cursor (bvw);
}

/**
 * bacon_video_widget_set_aspect_ratio:
 * @bvw: a #BaconVideoWidget
 * @ratio: the new aspect ratio
 *
 * Sets the aspect ratio used by the widget, from #BvwAspectRatio.
 *
 * Changes to this take effect immediately.
 **/
void
bacon_video_widget_set_aspect_ratio (BaconVideoWidget *bvw,
                                     BvwAspectRatio ratio)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  bvw->ratio_type = ratio;

  switch (bvw->ratio_type) {
  case BVW_RATIO_SQUARE:
    g_object_set (bvw->video_sink,
		  "video-aspect-ratio-override", 1, 1,
		  NULL);
    break;
  case BVW_RATIO_FOURBYTHREE:
    g_object_set (bvw->video_sink,
		  "video-aspect-ratio-override", 4, 3,
		  NULL);
    break;
  case BVW_RATIO_ANAMORPHIC:
    g_object_set (bvw->video_sink,
		  "video-aspect-ratio-override", 16, 9,
		  NULL);
    break;
  case BVW_RATIO_DVB:
    g_object_set (bvw->video_sink,
		  "video-aspect-ratio-override", 20, 9,
		  NULL);
    break;
    /* handle these to avoid compiler warnings */
  case BVW_RATIO_AUTO:
  default:
    g_object_set (bvw->video_sink,
		  "video-aspect-ratio-override", 0, 1,
		  NULL);
    break;
  }
}

/**
 * bacon_video_widget_get_aspect_ratio:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the current aspect ratio used by the widget, from
 * #BvwAspectRatio.
 *
 * Return value: the aspect ratio
 **/
BvwAspectRatio
bacon_video_widget_get_aspect_ratio (BaconVideoWidget *bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);

  return bvw->ratio_type;
}

/**
 * bacon_video_widget_set_zoom:
 * @bvw: a #BaconVideoWidget
 * @mode: the #BvwZoomMode
 *
 * Sets the zoom type applied to the video when it is displayed.
 **/
void
bacon_video_widget_set_zoom (BaconVideoWidget *bvw,
                             BvwZoomMode       mode)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  g_debug ("%s not implemented", G_STRFUNC);
}

/**
 * bacon_video_widget_get_zoom:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the zoom mode applied to videos displayed by the widget.
 *
 * Return value: a #BvwZoomMode
 **/
BvwZoomMode
bacon_video_widget_get_zoom (BaconVideoWidget *bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), BVW_ZOOM_NONE);

  g_debug ("%s not implemented", G_STRFUNC);
  return BVW_ZOOM_NONE;
}

/**
 * bacon_video_widget_set_rotation:
 * @bvw: a #BaconVideoWidget
 * @rotation: the #BvwRotation of the video in degrees
 *
 * Sets the rotation to be applied to the video when it is displayed.
 **/
void
bacon_video_widget_set_rotation (BaconVideoWidget *bvw,
				 BvwRotation       rotation)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

  GST_DEBUG ("Rotating to %s (%f degrees) from %s",
	     g_enum_to_string (BVW_TYPE_ROTATION, rotation),
	     rotation * 90.0,
	     g_enum_to_string (BVW_TYPE_ROTATION, bvw->rotation));

  bvw->rotation = rotation;
  g_object_set (bvw->video_sink, "rotate-method", rotation, NULL);
}

/**
 * bacon_video_widget_get_rotation:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the angle of rotation of the video, in degrees.
 *
 * Return value: a #BvwRotation.
 **/
BvwRotation
bacon_video_widget_get_rotation (BaconVideoWidget *bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), BVW_ROTATION_R_ZERO);

  return bvw->rotation;
}

/* Search for the color balance channel corresponding to type and return it. */
static GstColorBalanceChannel *
bvw_get_color_balance_channel (GstColorBalance * color_balance,
    BvwVideoProperty type)
{
  const GList *channels;

  channels = gst_color_balance_list_channels (color_balance);

  for (; channels != NULL; channels = channels->next) {
    GstColorBalanceChannel *c = channels->data;

    if (type == BVW_VIDEO_BRIGHTNESS && g_strrstr (c->label, "BRIGHTNESS"))
      return g_object_ref (c);
    else if (type == BVW_VIDEO_CONTRAST && g_strrstr (c->label, "CONTRAST"))
      return g_object_ref (c);
    else if (type == BVW_VIDEO_SATURATION && g_strrstr (c->label, "SATURATION"))
      return g_object_ref (c);
    else if (type == BVW_VIDEO_HUE && g_strrstr (c->label, "HUE"))
      return g_object_ref (c);
  }

  return NULL;
}

/**
 * bacon_video_widget_get_video_property:
 * @bvw: a #BaconVideoWidget
 * @type: the type of property
 *
 * Returns the given property of the video display, such as its brightness or saturation.
 *
 * It is returned as a percentage in the full range of integer values; from <code class="literal">0</code>
 * to <code class="literal">65535</code> (inclusive), where <code class="literal">32768</code> is the default.
 *
 * Return value: the property's value, in the range <code class="literal">0</code> to <code class="literal">65535</code>
 **/
int
bacon_video_widget_get_video_property (BaconVideoWidget *bvw,
                                       BvwVideoProperty type)
{
  GstColorBalanceChannel *found_channel = NULL;
  int ret, cur;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 65535/2);
  g_return_val_if_fail (bvw->play != NULL, 65535/2);

  ret = 0;


  found_channel = bvw_get_color_balance_channel (GST_COLOR_BALANCE (bvw->play), type);
  cur = gst_color_balance_get_value (GST_COLOR_BALANCE (bvw->play), found_channel);

  GST_DEBUG ("channel %s: cur=%d, min=%d, max=%d", found_channel->label,
	     cur, found_channel->min_value, found_channel->max_value);

  ret = floor (0.5 +
	       ((double) cur - found_channel->min_value) * 65535 /
	       ((double) found_channel->max_value - found_channel->min_value));

  GST_DEBUG ("channel %s: returning value %d", found_channel->label, ret);
  g_object_unref (found_channel);
  return ret;
}

/**
 * bacon_video_widget_has_menus:
 * @bvw: a #BaconVideoWidget
 *
 * Returns whether the widget is currently displaying a menu,
 * such as a DVD menu.
 *
 * Return value: %TRUE if a menu is displayed, %FALSE otherwise
 **/
gboolean
bacon_video_widget_has_menus (BaconVideoWidget *bvw)
{
    g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

    if (bacon_video_widget_is_playing (bvw) == FALSE)
        return FALSE;

    return bvw->is_menu;
}

/**
 * bacon_video_widget_has_angles:
 * @bvw: a #BaconVideoWidget
 *
 * Returns whether the widget is currently playing a stream with
 * multiple angles.
 *
 * Return value: %TRUE if the current video stream has multiple
 * angles, %FALSE otherwise
 **/
gboolean
bacon_video_widget_has_angles (BaconVideoWidget *bvw)
{
    guint n_video;

    g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

    if (bacon_video_widget_is_playing (bvw) == FALSE)
        return FALSE;

    if (bvw->has_angles)
        return TRUE;

    g_object_get (G_OBJECT (bvw->play), "n-video", &n_video, NULL);

    return n_video > 1;
}

/**
 * bacon_video_widget_set_next_angle:
 * @bvw: a #BaconVideoWidget
 *
 * Select the next angle, or video track in the playing stream.
 **/
void
bacon_video_widget_set_next_angle (BaconVideoWidget *bvw)
{
    guint n_video, current_video;

    g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

    if (bacon_video_widget_is_playing (bvw) == FALSE)
        return;

    if (bvw->has_angles) {
        GST_DEBUG ("Sending event 'next-angle'");
        bvw_do_navigation_command (bvw, GST_NAVIGATION_COMMAND_NEXT_ANGLE);
        return;
    }

    g_object_get (G_OBJECT (bvw->play),
		  "current-video", &current_video,
		  "n-video", &n_video,
		  NULL);

    if (n_video <= 1) {
        GST_DEBUG ("Not setting next video stream, we have %d video streams", n_video);
	return;
    }

    current_video++;
    if (current_video == n_video)
      current_video = 0;

    GST_DEBUG ("Setting current-video to %d/%d", current_video, n_video);
    g_object_set (G_OBJECT (bvw->play), "current-video", current_video, NULL);
}

static gboolean
notify_volume_idle_cb (BaconVideoWidget *bvw)
{
  gdouble vol;

  vol = gst_stream_volume_get_volume (GST_STREAM_VOLUME (bvw->play),
                                      GST_STREAM_VOLUME_FORMAT_CUBIC);

  bvw->volume = vol;

  g_object_notify (G_OBJECT (bvw), "volume");

  return FALSE;
}

static void
notify_volume_cb (GObject             *object,
		  GParamSpec          *pspec,
		  BaconVideoWidget    *bvw)
{
  guint id;

  id = g_idle_add ((GSourceFunc) notify_volume_idle_cb, bvw);
  g_source_set_name_by_id (id, "[totem] notify_volume_idle_cb");
}

/**
 * bacon_video_widget_set_video_property:
 * @bvw: a #BaconVideoWidget
 * @type: the type of property
 * @value: the property's value, in the range <code class="literal">0</code> to <code class="literal">65535</code>
 *
 * Sets the given property of the video display, such as its brightness or saturation.
 *
 * It should be given as a percentage in the full range of integer values; from <code class="literal">0</code>
 * to <code class="literal">65535</code> (inclusive), where <code class="literal">32768</code> is the default.
 **/
void
bacon_video_widget_set_video_property (BaconVideoWidget *bvw,
                                       BvwVideoProperty type,
                                       int value)
{
  GstColorBalanceChannel *found_channel = NULL;
  int i_value;

  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (bvw->play != NULL);

  GST_DEBUG ("set video property type %d to value %d", type, value);

  if ( !(value <= 65535 && value >= 0) )
    return;

  found_channel = bvw_get_color_balance_channel (GST_COLOR_BALANCE (bvw->play), type);
  i_value = floor (0.5 + value * ((double) found_channel->max_value -
				  found_channel->min_value) / 65535 + found_channel->min_value);

  GST_DEBUG ("channel %s: set to %d/65535", found_channel->label, value);

  gst_color_balance_set_value (GST_COLOR_BALANCE (bvw->play), found_channel, i_value);

  GST_DEBUG ("channel %s: val=%d, min=%d, max=%d", found_channel->label,
	     i_value, found_channel->min_value, found_channel->max_value);

  g_object_unref (found_channel);

  /* Notify of the property change */
  g_object_notify (G_OBJECT (bvw), video_props_str[type]);

  GST_DEBUG ("setting value %d", value);
}

/**
 * bacon_video_widget_get_position:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the current position in the stream, as a value between
 * <code class="literal">0</code> and <code class="literal">1</code>.
 *
 * Return value: the current position, or <code class="literal">-1</code>
 **/
double
bacon_video_widget_get_position (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
  return bvw->current_position;
}

/**
 * bacon_video_widget_get_current_time:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the current position in the stream, as the time (in milliseconds)
 * since the beginning of the stream.
 *
 * Return value: time since the beginning of the stream, in milliseconds, or <code class="literal">-1</code>
 **/
gint64
bacon_video_widget_get_current_time (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
  return bvw->current_time;
}

/**
 * bacon_video_widget_get_stream_length:
 * @bvw: a #BaconVideoWidget
 *
 * Returns the total length of the stream, in milliseconds.
 *
 * Return value: the stream length, in milliseconds, or <code class="literal">-1</code>
 **/
gint64
bacon_video_widget_get_stream_length (BaconVideoWidget * bvw)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);

  if (bvw->stream_length == 0 && bvw->play != NULL) {
    gint64 len = -1;

    if (gst_element_query_duration (bvw->play, GST_FORMAT_TIME, &len) && len != -1) {
      bvw->stream_length = len / GST_MSECOND;
    }
  }

  return bvw->stream_length;
}

/**
 * bacon_video_widget_is_playing:
 * @bvw: a #BaconVideoWidget
 *
 * Returns whether the widget is currently playing a stream.
 *
 * Return value: %TRUE if a stream is playing, %FALSE otherwise
 **/
gboolean
bacon_video_widget_is_playing (BaconVideoWidget * bvw)
{
  gboolean ret;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);

  ret = (bvw->target_state == GST_STATE_PLAYING);
  GST_LOG ("%splaying", (ret) ? "" : "not ");

  return ret;
}

/**
 * bacon_video_widget_is_seekable:
 * @bvw: a #BaconVideoWidget
 *
 * Returns whether seeking is possible in the current stream.
 *
 * If no stream is loaded, %FALSE is returned.
 *
 * Return value: %TRUE if the stream is seekable, %FALSE otherwise
 **/
gboolean
bacon_video_widget_is_seekable (BaconVideoWidget * bvw)
{
  gboolean res;
  gint old_seekable;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);

  if (bvw->mrl == NULL)
    return FALSE;

  old_seekable = bvw->seekable;

  if (bvw->is_menu != FALSE)
    return FALSE;

  if (bvw->seekable == -1) {
    GstQuery *query;

    query = gst_query_new_seeking (GST_FORMAT_TIME);
    if (gst_element_query (bvw->play, query)) {
      gst_query_parse_seeking (query, NULL, &res, NULL, NULL);
      GST_DEBUG ("seeking query says the stream is%s seekable", (res) ? "" : " not");
      bvw->seekable = (res) ? 1 : 0;
    } else {
      GST_DEBUG ("seeking query failed");
    }
    gst_query_unref (query);
  }

  if (bvw->seekable != -1) {
    res = (bvw->seekable != 0);
    goto done;
  }

  /* Try to guess from duration. This is very unreliable
   * though so don't save it */
  if (bvw->stream_length == 0) {
    res = (bacon_video_widget_get_stream_length (bvw) > 0);
  } else {
    res = (bvw->stream_length > 0);
  }

done:

  if (old_seekable != bvw->seekable)
    g_object_notify (G_OBJECT (bvw), "seekable");

  GST_DEBUG ("stream is%s seekable", (res) ? "" : " not");
  return res;
}

static gint
bvw_get_current_stream_num (BaconVideoWidget * bvw,
    const gchar *stream_type)
{
  gchar *lower, *cur_prop_str;
  gint stream_num = -1;

  if (bvw->play == NULL)
    return stream_num;

  lower = g_ascii_strdown (stream_type, -1);
  cur_prop_str = g_strconcat ("current-", lower, NULL);
  g_object_get (bvw->play, cur_prop_str, &stream_num, NULL);
  g_free (cur_prop_str);
  g_free (lower);

  GST_LOG ("current %s stream: %d", stream_type, stream_num);
  return stream_num;
}

static GstTagList *
bvw_get_tags_of_current_stream (BaconVideoWidget * bvw,
    const gchar *stream_type)
{
  GstTagList *tags = NULL;
  gint stream_num = -1;
  gchar *lower, *cur_sig_str;

  stream_num = bvw_get_current_stream_num (bvw, stream_type);
  if (stream_num < 0)
    return NULL;

  lower = g_ascii_strdown (stream_type, -1);
  cur_sig_str = g_strconcat ("get-", lower, "-tags", NULL);
  g_signal_emit_by_name (bvw->play, cur_sig_str, stream_num, &tags);
  g_free (cur_sig_str);
  g_free (lower);

  GST_LOG ("current %s stream tags %" GST_PTR_FORMAT, stream_type, tags);
  return tags;
}

static GstCaps *
bvw_get_caps_of_current_stream (BaconVideoWidget * bvw,
    const gchar *stream_type)
{
  GstCaps *caps = NULL;
  gint stream_num = -1;
  GstPad *current;
  gchar *lower, *cur_sig_str;

  stream_num = bvw_get_current_stream_num (bvw, stream_type);
  if (stream_num < 0)
    return NULL;

  lower = g_ascii_strdown (stream_type, -1);
  cur_sig_str = g_strconcat ("get-", lower, "-pad", NULL);
  g_signal_emit_by_name (bvw->play, cur_sig_str, stream_num, &current);
  g_free (cur_sig_str);
  g_free (lower);

  if (current != NULL) {
    caps = gst_pad_get_current_caps (current);
    gst_object_unref (current);
  }
  GST_LOG ("current %s stream caps: %" GST_PTR_FORMAT, stream_type, caps);
  return caps;
}

static gboolean
audio_caps_have_LFE (GstStructure * s)
{
  guint64 mask;
  int channels;

  if (!gst_structure_get_int (s, "channels", &channels) ||
      channels == 0)
    return FALSE;

  if (!gst_structure_get (s, "channel-mask", GST_TYPE_BITMASK, &mask, NULL))
    return FALSE;

  if (mask & GST_AUDIO_CHANNEL_POSITION_LFE1 ||
      mask & GST_AUDIO_CHANNEL_POSITION_LFE2)
    return TRUE;

  return FALSE;
}

static void
bacon_video_widget_get_metadata_string (BaconVideoWidget * bvw,
                                        BvwMetadataType type,
                                        GValue * value)
{
  char *string = NULL;
  gboolean res = FALSE;

  g_value_init (value, G_TYPE_STRING);

  if (bvw->play == NULL) {
    g_value_set_string (value, NULL);
    return;
  }

  switch (type) {
    case BVW_INFO_TITLE:
      if (bvw->tagcache != NULL) {
        res = gst_tag_list_get_string_index (bvw->tagcache,
                                             GST_TAG_TITLE, 0, &string);
      }
      break;
    case BVW_INFO_ARTIST:
      if (bvw->tagcache != NULL) {
        res = gst_tag_list_get_string_index (bvw->tagcache,
                                             GST_TAG_ARTIST, 0, &string);
      }
      break;
    case BVW_INFO_YEAR:
      if (bvw->tagcache != NULL) {
        GDate *date;
        GstDateTime *datetime;

        if ((res = gst_tag_list_get_date (bvw->tagcache,
                                          GST_TAG_DATE, &date))) {
          string = g_strdup_printf ("%d", g_date_get_year (date));
          g_date_free (date);
        } else if ((res = gst_tag_list_get_date_time (bvw->tagcache,
                                                      GST_TAG_DATE_TIME, &datetime))) {
          string = g_strdup_printf ("%d", gst_date_time_get_year (datetime));
          gst_date_time_unref (datetime);
        }
      }
      break;
    case BVW_INFO_COMMENT:
      if (bvw->tagcache != NULL) {
        res = gst_tag_list_get_string_index (bvw->tagcache,
                                             GST_TAG_COMMENT, 0, &string);

        /* Use the Comment; if that fails, use Description as specified by:
         * http://xiph.org/vorbis/doc/v-comment.html */
        if (!res) {
          res = gst_tag_list_get_string_index (bvw->tagcache,
                                               GST_TAG_DESCRIPTION, 0, &string);
        }
      }
      break;
    case BVW_INFO_ALBUM:
      if (bvw->tagcache != NULL) {
        res = gst_tag_list_get_string_index (bvw->tagcache,
                                             GST_TAG_ALBUM, 0, &string);
      }
      break;
    case BVW_INFO_CONTAINER:
      if (bvw->tagcache != NULL) {
        res = gst_tag_list_get_string_index (bvw->tagcache,
                                             GST_TAG_CONTAINER_FORMAT, 0, &string);
      }
      break;
    case BVW_INFO_VIDEO_CODEC: {
      GstTagList *tags;

      /* try to get this from the stream info first */
      if ((tags = bvw_get_tags_of_current_stream (bvw, "video"))) {
        res = gst_tag_list_get_string (tags, GST_TAG_CODEC, &string);
	gst_tag_list_unref (tags);
      }

      /* if that didn't work, try the aggregated tags */
      if (!res && bvw->tagcache != NULL) {
        res = gst_tag_list_get_string (bvw->tagcache,
            GST_TAG_VIDEO_CODEC, &string);
      }
      break;
    }
    case BVW_INFO_AUDIO_CODEC: {
      GstTagList *tags;

      /* try to get this from the stream info first */
      if ((tags = bvw_get_tags_of_current_stream (bvw, "audio"))) {
        res = gst_tag_list_get_string (tags, GST_TAG_CODEC, &string);
	gst_tag_list_unref (tags);
      }

      /* if that didn't work, try the aggregated tags */
      if (!res && bvw->tagcache != NULL) {
        res = gst_tag_list_get_string (bvw->tagcache,
            GST_TAG_AUDIO_CODEC, &string);
      }
      break;
    }
    case BVW_INFO_AUDIO_CHANNELS: {
      GstStructure *s;
      GstCaps *caps;

      caps = bvw_get_caps_of_current_stream (bvw, "audio");
      if (caps) {
        gint channels = 0;

        s = gst_caps_get_structure (caps, 0);
        if ((res = gst_structure_get_int (s, "channels", &channels))) {
          /* FIXME: do something more sophisticated - but what? */
          if (channels > 2 && audio_caps_have_LFE (s)) {
            string = g_strdup_printf ("%s %d.1", _("Surround"), channels - 1);
          } else if (channels == 1) {
            string = g_strdup (_("Mono"));
          } else if (channels == 2) {
            string = g_strdup (_("Stereo"));
          } else {
            string = g_strdup_printf ("%d", channels);
          }
        }
        gst_caps_unref (caps);
      }
      break;
    }

    case BVW_INFO_DURATION:
    case BVW_INFO_TRACK_NUMBER:
    case BVW_INFO_HAS_VIDEO:
    case BVW_INFO_DIMENSION_X:
    case BVW_INFO_DIMENSION_Y:
    case BVW_INFO_VIDEO_BITRATE:
    case BVW_INFO_FPS:
    case BVW_INFO_HAS_AUDIO:
    case BVW_INFO_AUDIO_BITRATE:
    case BVW_INFO_AUDIO_SAMPLE_RATE:
      /* Not strings */
    default:
      g_assert_not_reached ();
    }

  /* Remove line feeds */
  if (string && strstr (string, "\n") != NULL)
    g_strdelimit (string, "\n", ' ');
  if (string != NULL)
    string = g_strstrip (string);

  if (res && string && *string != '\0' && g_utf8_validate (string, -1, NULL)) {
    g_value_take_string (value, string);
    GST_DEBUG ("%s = '%s'", g_enum_to_string (BVW_TYPE_METADATA_TYPE, type), string);
  } else {
    g_value_set_string (value, NULL);
    g_free (string);
  }

  return;
}

static void
bacon_video_widget_get_metadata_int (BaconVideoWidget * bvw,
                                     BvwMetadataType type,
                                     GValue * value)
{
  int integer = 0;

  g_value_init (value, G_TYPE_INT);

  if (bvw->play == NULL) {
    g_value_set_int (value, 0);
    return;
  }

  switch (type) {
    case BVW_INFO_DURATION:
      integer = bacon_video_widget_get_stream_length (bvw) / 1000;
      break;
    case BVW_INFO_TRACK_NUMBER:
      if (bvw->tagcache == NULL)
        break;
      if (!gst_tag_list_get_uint (bvw->tagcache,
                                  GST_TAG_TRACK_NUMBER, (guint *) &integer))
        integer = 0;
      break;
    case BVW_INFO_DIMENSION_X:
      integer = bvw->video_width;
      break;
    case BVW_INFO_DIMENSION_Y:
      integer = bvw->video_height;
      break;
    case BVW_INFO_AUDIO_BITRATE:
      if (bvw->audiotags == NULL)
        break;
      if (gst_tag_list_get_uint (bvw->audiotags, GST_TAG_BITRATE,
          (guint *)&integer) ||
          gst_tag_list_get_uint (bvw->audiotags, GST_TAG_NOMINAL_BITRATE,
          (guint *)&integer)) {
        integer /= 1000;
      }
      break;
    case BVW_INFO_VIDEO_BITRATE:
      if (bvw->videotags == NULL)
        break;
      if (gst_tag_list_get_uint (bvw->videotags, GST_TAG_BITRATE,
          (guint *)&integer) ||
          gst_tag_list_get_uint (bvw->videotags, GST_TAG_NOMINAL_BITRATE,
          (guint *)&integer)) {
        integer /= 1000;
      }
      break;
    case BVW_INFO_AUDIO_SAMPLE_RATE: {
      GstStructure *s;
      GstCaps *caps;

      caps = bvw_get_caps_of_current_stream (bvw, "audio");
      if (caps) {
        s = gst_caps_get_structure (caps, 0);
        gst_structure_get_int (s, "rate", &integer);
        gst_caps_unref (caps);
      }
      break;
    }

    case BVW_INFO_TITLE:
    case BVW_INFO_ARTIST:
    case BVW_INFO_YEAR:
    case BVW_INFO_COMMENT:
    case BVW_INFO_ALBUM:
    case BVW_INFO_CONTAINER:
    case BVW_INFO_HAS_VIDEO:
    case BVW_INFO_VIDEO_CODEC:
    case BVW_INFO_HAS_AUDIO:
    case BVW_INFO_AUDIO_CODEC:
    case BVW_INFO_AUDIO_CHANNELS:
      /* Not ints */
    default:
      g_assert_not_reached ();
    }

  g_value_set_int (value, integer);
  GST_DEBUG ("%s = %d", g_enum_to_string (BVW_TYPE_METADATA_TYPE, type), integer);

  return;
}

static void
bacon_video_widget_get_metadata_bool (BaconVideoWidget * bvw,
                                      BvwMetadataType type,
                                      GValue * value)
{
  gboolean boolean = FALSE;

  g_value_init (value, G_TYPE_BOOLEAN);

  if (bvw->play == NULL) {
    g_value_set_boolean (value, FALSE);
    return;
  }

  GST_DEBUG ("tagcache  = %" GST_PTR_FORMAT, bvw->tagcache);
  GST_DEBUG ("videotags = %" GST_PTR_FORMAT, bvw->videotags);
  GST_DEBUG ("audiotags = %" GST_PTR_FORMAT, bvw->audiotags);

  switch (type)
  {
    case BVW_INFO_HAS_VIDEO:
      boolean = bvw->media_has_video;
      break;
    case BVW_INFO_HAS_AUDIO:
      boolean = bvw->media_has_audio;
      break;

    case BVW_INFO_TITLE:
    case BVW_INFO_ARTIST:
    case BVW_INFO_YEAR:
    case BVW_INFO_COMMENT:
    case BVW_INFO_ALBUM:
    case BVW_INFO_DURATION:
    case BVW_INFO_TRACK_NUMBER:
    case BVW_INFO_CONTAINER:
    case BVW_INFO_DIMENSION_X:
    case BVW_INFO_DIMENSION_Y:
    case BVW_INFO_VIDEO_BITRATE:
    case BVW_INFO_VIDEO_CODEC:
    case BVW_INFO_FPS:
    case BVW_INFO_AUDIO_BITRATE:
    case BVW_INFO_AUDIO_CODEC:
    case BVW_INFO_AUDIO_SAMPLE_RATE:
    case BVW_INFO_AUDIO_CHANNELS:
      /* Not bools */
    default:
      g_assert_not_reached ();
  }

  g_value_set_boolean (value, boolean);
  GST_DEBUG ("%s = %s", g_enum_to_string (BVW_TYPE_METADATA_TYPE, type), (boolean) ? "yes" : "no");

  return;
}

/**
 * bacon_video_widget_get_metadata:
 * @bvw: a #BaconVideoWidget
 * @type: the type of metadata to return
 * @value: a #GValue
 *
 * Provides metadata of the given @type about the current stream in @value.
 *
 * Free the #GValue with g_value_unset().
 **/
void
bacon_video_widget_get_metadata (BaconVideoWidget * bvw,
                                 BvwMetadataType type,
                                 GValue * value)
{
  g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
  g_return_if_fail (GST_IS_ELEMENT (bvw->play));

  switch (type)
    {
    case BVW_INFO_TITLE:
    case BVW_INFO_ARTIST:
    case BVW_INFO_YEAR:
    case BVW_INFO_COMMENT:
    case BVW_INFO_ALBUM:
    case BVW_INFO_CONTAINER:
    case BVW_INFO_VIDEO_CODEC:
    case BVW_INFO_AUDIO_CODEC:
    case BVW_INFO_AUDIO_CHANNELS:
      bacon_video_widget_get_metadata_string (bvw, type, value);
      break;
    case BVW_INFO_DURATION:
    case BVW_INFO_DIMENSION_X:
    case BVW_INFO_DIMENSION_Y:
    case BVW_INFO_AUDIO_BITRATE:
    case BVW_INFO_VIDEO_BITRATE:
    case BVW_INFO_TRACK_NUMBER:
    case BVW_INFO_AUDIO_SAMPLE_RATE:
      bacon_video_widget_get_metadata_int (bvw, type, value);
      break;
    case BVW_INFO_HAS_VIDEO:
    case BVW_INFO_HAS_AUDIO:
      bacon_video_widget_get_metadata_bool (bvw, type, value);
      break;
    case BVW_INFO_FPS:
      {
	float fps = 0.0;

	if (bvw->video_fps_d > 0)
	  fps = (float) bvw->video_fps_n / (float) bvw->video_fps_d;

	g_value_init (value, G_TYPE_FLOAT);
	g_value_set_float (value, fps);
      }
      break;
    default:
      g_return_if_reached ();
    }

  return;
}

/* Screenshot functions */

/**
 * bacon_video_widget_can_get_frames:
 * @bvw: a #BaconVideoWidget
 * @error: a #GError, or %NULL
 *
 * Determines whether individual frames from the current stream can
 * be returned using bacon_video_widget_get_current_frame().
 *
 * Frames cannot be returned for audio-only streams.
 *
 * Return value: %TRUE if frames can be captured, %FALSE otherwise
 **/
gboolean
bacon_video_widget_can_get_frames (BaconVideoWidget * bvw, GError ** error)
{
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);

  /* check for video */
  if (!bvw->media_has_video) {
    g_set_error_literal (error, BVW_ERROR, BVW_ERROR_CANNOT_CAPTURE,
        _("Media contains no supported video streams."));
    return FALSE;
  }

  return TRUE;
}

/**
 * bacon_video_widget_get_current_frame:
 * @bvw: a #BaconVideoWidget
 *
 * Returns a #GdkPixbuf containing the current frame from the playing
 * stream. This will wait for any pending seeks to complete before
 * capturing the frame.
 *
 * Return value: the current frame, or %NULL; unref with g_object_unref()
 **/
GdkPixbuf *
bacon_video_widget_get_current_frame (BaconVideoWidget * bvw)
{
  GdkPixbuf *ret = NULL;
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), NULL);

  /* no video info */
  if (!bvw->video_width || !bvw->video_height) {
    GST_DEBUG ("Could not take screenshot: %s", "no video info");
    g_warning ("Could not take screenshot: %s", "no video info");
    return NULL;
  }

  ret = totem_gst_playbin_get_frame (bvw->play, &error);
  if (!ret) {
    GST_DEBUG ("Could not take screenshot: %s", error->message);
    g_warning ("Could not take screenshot: %s", error->message);
  }
  return ret;
}

/* =========================================== */
/*                                             */
/*          Widget typing & Creation           */
/*                                             */
/* =========================================== */

/**
 * bacon_video_widget_get_option_group:
 *
 * Returns the #GOptionGroup containing command-line options for
 * #BaconVideoWidget.
 *
 * Applications must call either this exactly once.
 *
 * Return value: a #GOptionGroup giving command-line options for #BaconVideoWidget
 **/
GOptionGroup*
bacon_video_widget_get_option_group (void)
{
  return gst_init_get_option_group ();
}

GQuark
bacon_video_widget_error_quark (void)
{
  static GQuark q; /* 0 */

  if (G_UNLIKELY (q == 0)) {
    q = g_quark_from_static_string ("bvw-error-quark");
  }
  return q;
}

static gboolean
bvw_set_playback_direction (BaconVideoWidget *bvw, gboolean forward)
{
  gboolean is_forward;
  gboolean retval;
  float target_rate;
  GstEvent *event;
  gint64 cur = 0;

  is_forward = (bvw->rate > 0.0);
  if (forward == is_forward)
    return TRUE;

  retval = FALSE;
  target_rate = (forward ? FORWARD_RATE : REVERSE_RATE);

  if (gst_element_query_position (bvw->play, GST_FORMAT_TIME, &cur)) {
    GST_DEBUG ("Setting playback direction to %s at %"G_GINT64_FORMAT"", DIRECTION_STR, cur);
    event = gst_event_new_seek (target_rate,
				GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
				GST_SEEK_TYPE_SET, forward ? cur : G_GINT64_CONSTANT (0),
				GST_SEEK_TYPE_SET, forward ? G_GINT64_CONSTANT (0) : cur);
    if (gst_element_send_event (bvw->play, event) == FALSE) {
      GST_WARNING ("Failed to set playback direction to %s", DIRECTION_STR);
    } else {
      gst_element_get_state (bvw->play, NULL, NULL, GST_CLOCK_TIME_NONE);
      bvw->rate = target_rate;
      retval = TRUE;
    }
  } else {
    GST_LOG ("Failed to query position to set playback to %s", DIRECTION_STR);
  }

  return retval;
}

static GstElement *
element_make_or_warn (const char *plugin,
		      const char *name)
{
  GstElement *element;
  element = gst_element_factory_make (plugin, name);
  if (!element)
    g_warning ("Element '%s' is missing, verify your installation", plugin);
  return element;
}

static gboolean
is_feature_enabled (const char *env)
{
  const char *value;

  g_return_val_if_fail (env != NULL, FALSE);
  value = g_getenv (env);
  return g_strcmp0 (value, "1") == 0;
}

static void
bacon_video_widget_init (BaconVideoWidget *bvw)
{
  GstElement *audio_sink = NULL;
  gchar *version_str;
  GstPlayFlags flags;
  GstElement *glsinkbin, *audio_bin;
  GstPad *audio_pad;
  char *template;

  gtk_widget_set_can_focus (GTK_WIDGET (bvw), TRUE);

  g_type_class_ref (BVW_TYPE_METADATA_TYPE);
  g_type_class_ref (BVW_TYPE_DVD_EVENT);
  g_type_class_ref (BVW_TYPE_ROTATION);

  bvw->volume = -1.0;
  bvw->rate = FORWARD_RATE;
  bvw->tag_update_queue = g_async_queue_new_full ((GDestroyNotify) update_tags_delayed_data_destroy);
  g_mutex_init (&bvw->seek_mutex);
  bvw->clock = gst_system_clock_obtain ();
  bvw->seek_req_time = GST_CLOCK_TIME_NONE;
  bvw->seek_time = -1;
  bvw->auth_last_result = G_MOUNT_OPERATION_HANDLED;

#ifndef GST_DISABLE_GST_DEBUG
  if (_totem_gst_debug_cat == NULL) {
    GST_DEBUG_CATEGORY_INIT (_totem_gst_debug_cat, "totem", 0,
        "Totem GStreamer Backend");
  }
#endif

  version_str = gst_version_string ();
  GST_DEBUG ("Initialized %s", version_str);
  g_free (version_str);

  gst_pb_utils_init ();

  gtk_widget_set_events (GTK_WIDGET (bvw),
			 gtk_widget_get_events (GTK_WIDGET (bvw)) |
			 GDK_SCROLL_MASK |
			 GDK_POINTER_MOTION_MASK |
			 GDK_BUTTON_MOTION_MASK |
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_KEY_PRESS_MASK);
  gtk_widget_init_template (GTK_WIDGET (bvw));

  /* Instantiate all the fallible plugins */
  bvw->play = element_make_or_warn ("playbin", "play");
  bvw->audio_pitchcontrol = element_make_or_warn ("scaletempo", "scaletempo");
  bvw->video_sink = element_make_or_warn ("gtkglsink", "video-sink");
  glsinkbin = element_make_or_warn ("glsinkbin", "glsinkbin");
  audio_sink = element_make_or_warn ("autoaudiosink", "audio-sink");

  if (!bvw->play ||
      !bvw->audio_pitchcontrol ||
      !bvw->video_sink ||
      !audio_sink ||
      !glsinkbin) {
    if (bvw->video_sink)
      g_object_ref_sink (bvw->video_sink);
    if (audio_sink)
      g_object_ref_sink (audio_sink);
    g_clear_object (&audio_sink);
    if (glsinkbin)
      g_object_ref_sink (glsinkbin);
    g_clear_object (&glsinkbin);
    bvw->init_error = g_error_new_literal (BVW_ERROR, BVW_ERROR_PLUGIN_LOAD,
					   _("Some necessary plug-ins are missing. "
					     "Make sure that the program is correctly installed."));
    return;
  }

  bvw->bus = gst_element_get_bus (bvw->play);

  /* Add the download flag, for streaming buffering,
   * and the deinterlace flag, for video only */
  g_object_get (bvw->play, "flags", &flags, NULL);
  flags |= GST_PLAY_FLAG_DOWNLOAD | GST_PLAY_FLAG_DEINTERLACE;
  g_object_set (bvw->play, "flags", flags, NULL);

  /* Keep in sync with playbin_element_setup_cb() */
  template = g_build_filename (g_get_user_cache_dir (), "totem", "stream-buffer", NULL);
  g_mkdir_with_parents (template, 0700);
  g_free (template);

  gst_bus_add_signal_watch (bvw->bus);

  bvw->sig_bus_async = 
      g_signal_connect (bvw->bus, "message", 
                        G_CALLBACK (bvw_bus_message_cb),
                        bvw);

  bvw->speakersetup = BVW_AUDIO_SOUND_STEREO;
  bvw->ratio_type = BVW_RATIO_AUTO;

  bvw->cursor_shown = TRUE;

  /* Create video output widget */

  if (is_feature_enabled ("FPS_DISPLAY")) {
    GstElement *fps;
    fps = gst_element_factory_make ("fpsdisplaysink", "fpsdisplaysink");
    g_object_set (glsinkbin, "sink", fps, NULL);
    g_object_set (fps, "video-sink", bvw->video_sink, NULL);
  } else {
    g_object_set (glsinkbin, "sink", bvw->video_sink, NULL);
  }
  g_object_get (bvw->video_sink, "widget", &bvw->video_widget, NULL);
  gtk_widget_show (bvw->video_widget);
  gtk_stack_add_named (GTK_STACK (bvw->stack), bvw->video_widget, "video");
  g_object_unref (bvw->video_widget);
  gtk_stack_set_visible_child_name (GTK_STACK (bvw->stack), "video");

  g_object_set (bvw->video_sink,
                "rotate-method", GST_VIDEO_ORIENTATION_AUTO,
                NULL);

  /* And tell playbin */
  g_object_set (bvw->play, "video-sink", glsinkbin, NULL);

  /* Link the audiopitch element */
  bvw->audio_capsfilter =
    gst_element_factory_make ("capsfilter", "audiofilter");
  audio_bin = gst_bin_new ("audiosinkbin");
  gst_bin_add_many (GST_BIN (audio_bin),
                    bvw->audio_capsfilter,
		    audio_sink, NULL);
  gst_element_link_many (bvw->audio_capsfilter,
			 audio_sink,
			 NULL);

  audio_pad = gst_element_get_static_pad (bvw->audio_capsfilter, "sink");
  gst_element_add_pad (audio_bin, gst_ghost_pad_new ("sink", audio_pad));
  gst_object_unref (audio_pad);

  /* And tell playbin */
  g_object_set (bvw->play, "audio-sink", audio_bin, NULL);
  g_object_set (bvw->play, "audio-filter", bvw->audio_pitchcontrol, NULL);

  /* Set default connection speed */
  /* Cast the value to guint64 to match the type of the 'connection-speed'
   * property to avoid problems reading variable arguments on 32-bit systems. */
  g_object_set (bvw->play, "connection-speed", (guint64) MAX_NETWORK_SPEED, NULL);

  g_signal_connect (G_OBJECT (bvw->play), "notify::volume",
      G_CALLBACK (notify_volume_cb), bvw);
  g_signal_connect (bvw->play, "source-setup",
      G_CALLBACK (playbin_source_setup_cb), bvw);
  g_signal_connect (bvw->play, "element-setup",
      G_CALLBACK (playbin_element_setup_cb), bvw);
  g_signal_connect (bvw->play, "video-changed",
      G_CALLBACK (playbin_stream_changed_cb), bvw);
  g_signal_connect (bvw->play, "audio-changed",
      G_CALLBACK (playbin_stream_changed_cb), bvw);
  g_signal_connect (bvw->play, "text-changed",
      G_CALLBACK (playbin_stream_changed_cb), bvw);
  g_signal_connect (bvw->play, "deep-notify::temp-location",
      G_CALLBACK (playbin_deep_notify_cb), bvw);

  g_signal_connect (bvw->play, "video-tags-changed",
      G_CALLBACK (video_tags_changed_cb), bvw);
  g_signal_connect (bvw->play, "audio-tags-changed",
      G_CALLBACK (audio_tags_changed_cb), bvw);
  g_signal_connect (bvw->play, "text-tags-changed",
      G_CALLBACK (text_tags_changed_cb), bvw);
}

/**
 * bacon_video_widget_new:
 *
 * Creates a new #BaconVideoWidget.
 *
 * Return value: a new #BaconVideoWidget; destroy with gtk_widget_destroy()
 **/
GtkWidget *
bacon_video_widget_new (void)
{
  return GTK_WIDGET (g_object_new (BACON_TYPE_VIDEO_WIDGET, NULL));
}

/**
 * bacon_video_widget_check_init:
 * @error: a #GError, or %NULL.
 *
 * Return value: if an error occured during initialisation, %FALSE is returned
 *   and @error is set. Otherwise, %TRUE is returned.
 **/
gboolean
bacon_video_widget_check_init (BaconVideoWidget  *bvw,
			       GError           **error)
{
  g_return_val_if_fail (bvw != NULL, FALSE);
  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);

  if (!bvw->init_error)
    return TRUE;

  g_propagate_error (error, bvw->init_error);
  bvw->init_error = NULL;
  return FALSE;
}

/**
 * bacon_video_widget_get_rate:
 * @bvw: a #BaconVideoWidget
 *
 * Get the current playback rate, with 1.0 being normal rate.
 *
 * Returns: the current playback rate
 **/
gfloat
bacon_video_widget_get_rate (BaconVideoWidget *bvw)
{
  return bvw->rate;
}

/**
 * bacon_video_widget_set_rate:
 * @bvw: a #BaconVideoWidget
 * @new_rate: the new playback rate
 *
 * Sets the current playback rate.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
bacon_video_widget_set_rate (BaconVideoWidget *bvw,
			     gfloat            new_rate)
{
  GstEvent *event;
  gboolean retval = FALSE;
  gint64 cur;

  g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
  g_return_val_if_fail (GST_IS_ELEMENT (bvw->play), FALSE);

  if (new_rate == bvw->rate)
    return TRUE;

  /* set upper and lower limit for rate */
  if (new_rate < BVW_MIN_RATE)
    return retval;
  if (new_rate > BVW_MAX_RATE)
    return retval;

  if (gst_element_query_position (bvw->play, GST_FORMAT_TIME, &cur)) {
    GST_DEBUG ("Setting new rate at %"G_GINT64_FORMAT"", cur);
    event = gst_event_new_seek (new_rate,
				GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
				GST_SEEK_TYPE_SET, cur,
				GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
    if (gst_element_send_event (bvw->play, event) == FALSE) {
      GST_DEBUG ("Failed to change rate");
    } else {
      gst_element_get_state (bvw->play, NULL, NULL, GST_CLOCK_TIME_NONE);
      bvw->rate = new_rate;
      retval = TRUE;
    }
  } else {
    GST_DEBUG ("failed to query position");
  }

  return retval;
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
