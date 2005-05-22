/* 
 * Copyright (C) 2001-2005 the xine project
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
 * the xine engine in a widget - implementation
 */

#include <config.h>

#ifdef HAVE_NVTV
#include <nvtv_simple.h>
#endif 


/* system */
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/* X11 */
#include <X11/X.h>
#include <X11/Xlib.h>
/* gtk+/gnome */
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glib/gi18n.h>
/* xine */
#include <xine.h>

#include "debug.h"
#include "bacon-video-widget.h"
#include "baconvideowidget-marshal.h"
#include "video-utils.h"
#include "bacon-resize.h"

#define DEFAULT_HEIGHT 180
#define DEFAULT_WIDTH 240
#define CONFIG_FILE ".gnome2"G_DIR_SEPARATOR_S"totem_config"

/* Signals */
enum {
	ERROR,
	EOS,
	REDIRECT,
	TITLE_CHANGE,
	CHANNELS_CHANGE,
	TICK,
	GOT_METADATA,
	BUFFERING,
	SPEED_WARNING,
	LAST_SIGNAL
};

/* Enum for none-signal stuff that needs to go through the AsyncQueue */
enum {
	RATIO_ASYNC,
	REDIRECT_ASYNC,
	TITLE_CHANGE_ASYNC,
	EOS_ASYNC,
	CHANNELS_CHANGE_ASYNC,
	BUFFERING_ASYNC,
	MESSAGE_ASYNC,
	SPEED_WARNING_ASYNC,
	ERROR_ASYNC
};

typedef struct {
	int signal;
	char *msg;
	int num;
	gboolean fatal;
} signal_data;

/* Arguments */
enum {
	PROP_0,
	PROP_LOGO_MODE,
	PROP_SPEED,
	PROP_POSITION,
	PROP_CURRENT_TIME,
	PROP_STREAM_LENGTH,
	PROP_PLAYING,
	PROP_SEEKABLE,
	PROP_SHOWCURSOR,
	PROP_MEDIADEV,
	PROP_SHOW_VISUALS
};

static int video_props[4] = {
	XINE_PARAM_VO_BRIGHTNESS,
	XINE_PARAM_VO_CONTRAST,
	XINE_PARAM_VO_SATURATION,
	XINE_PARAM_VO_HUE
};

static char *video_props_str[4] = {
	GCONF_PREFIX"/brightness",
	GCONF_PREFIX"/contrast",
	GCONF_PREFIX"/saturation",
	GCONF_PREFIX"/hue"
};

struct BaconVideoWidgetPrivate {
	/* Xine stuff */
	xine_t *xine;
	xine_stream_t *stream;
	xine_video_port_t *vo_driver;
	gboolean vo_driver_none;
	xine_audio_port_t *ao_driver;
	gboolean ao_driver_none;
	xine_event_queue_t *ev_queue;
	double display_ratio;

	/* Configuration */
	GConfClient *gc;
	char *mrl;
	BvwUseType type;
	char *mediadev;

	/* X stuff */
	Display *display;
	int screen;
	GdkWindow *video_window;

	/* Visual effects */
	char *vis_name;
	gboolean show_vfx;
	gboolean using_vfx;
	xine_post_t *vis;
	GList *visuals;
	char *queued_vis;

	/* Seeking stuff */
	int seeking;
	float seek_dest;
	gint64 seek_dest_time;

	/* Other stuff */
	int xpos, ypos;
	gboolean can_dvd, can_vcd, can_cdda;
	gboolean logo_mode;
	guint tick_id;
	gboolean have_xvidmode;
	gboolean auto_resize;
	int volume;
	BaconVideoWidgetAudioOutType audio_out_type;
	TvOutType tvout;
	gboolean is_live;
	char *codecs_path;
	gboolean got_redirect;

	GAsyncQueue *queue;
	int video_width, video_height;
	int init_width, init_height;

	/* fullscreen stuff */
	gboolean fullscreen_mode;
	gboolean cursor_shown;
	int screenid;
};

static const char *mms_bandwidth_strs[] = {
	"14.4 Kbps (Modem)",
	"19.2 Kbps (Modem)",
	"28.8 Kbps (Modem)",
	"33.6 Kbps (Modem)",
	"34.4 Kbps (Modem)",
	"57.6 Kbps (Modem)",
	"115.2 Kbps (ISDN)",
	"262.2 Kbps (Cable/DSL)",
	"393.2 Kbps (Cable/DSL)",
	"524.3 Kbps (Cable/DSL)",
	"1.5 Mbps (T1)",
	"10.5 Mbps (LAN)",
	NULL
};

static const char *audio_out_types_strs[] = {
	"Mono 1.0",
	"Stereo 2.0",
	"Headphones 2.0",
	"Stereo 2.1",
	"Surround 3.0",
	"Surround 4.0",
	"Surround 4.1",
	"Surround 5.0",
	"Surround 5.1",
	"Surround 6.0",
	"Surround 6.1",
	"Surround 7.1",
	"Pass Through",
	NULL
};

static const char *demux_strategies_str[] = {
	"default",
	"reverse",
	"content",
	"extension",
	NULL
};

static void bacon_video_widget_class_init (BaconVideoWidgetClass *klass);
static void bacon_video_widget_init (BaconVideoWidget *bvw);

static void setup_config (BaconVideoWidget *bvw);

static void bacon_video_widget_set_property (GObject *object,
		guint property_id, const GValue *value, GParamSpec *pspec);
static void bacon_video_widget_get_property (GObject *object,
		guint property_id, GValue *value, GParamSpec *pspec);

static void bacon_video_widget_realize (GtkWidget *widget);
static void bacon_video_widget_unrealize (GtkWidget *widget);
static void bacon_video_widget_finalize (GObject *object);

static gboolean bacon_video_widget_expose (GtkWidget *widget,
		GdkEventExpose *event);
static gboolean bacon_video_widget_motion_notify (GtkWidget *widget,
		GdkEventMotion *event);
static gboolean bacon_video_widget_button_press (GtkWidget *widget,
		GdkEventButton *event);
static void bacon_video_widget_show (GtkWidget *widget);
static void bacon_video_widget_hide (GtkWidget *widget);

static void bacon_video_widget_size_request (GtkWidget *widget,
		GtkRequisition *requisition);
static void bacon_video_widget_size_allocate (GtkWidget *widget,
		GtkAllocation *allocation);
static xine_video_port_t * load_video_out_driver (BaconVideoWidget *bvw,
		gboolean null_out);
static xine_audio_port_t * load_audio_out_driver (BaconVideoWidget *bvw,
		gboolean null_out, GError **error);
static gboolean bacon_video_widget_tick_send (BaconVideoWidget *bvw);

static GtkWidgetClass *parent_class = NULL;

static void xine_event (void *user_data, const xine_event_t *event);
static gboolean bacon_video_widget_idle_signal (BaconVideoWidget *bvw);
static void show_vfx_update (BaconVideoWidget *bvw, gboolean show_visuals);

static int bvw_table_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(BaconVideoWidget, bacon_video_widget, GTK_TYPE_BOX)

static void
bacon_video_widget_class_init (BaconVideoWidgetClass *klass)
{

	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_box_get_type ());

	/* GtkWidget */
	widget_class->realize = bacon_video_widget_realize;
	widget_class->unrealize = bacon_video_widget_unrealize;
	widget_class->size_request = bacon_video_widget_size_request;
	widget_class->size_allocate = bacon_video_widget_size_allocate;
	widget_class->expose_event = bacon_video_widget_expose;
	widget_class->motion_notify_event = bacon_video_widget_motion_notify;
	widget_class->button_press_event = bacon_video_widget_button_press;
	widget_class->show = bacon_video_widget_show;
	widget_class->hide = bacon_video_widget_hide;

	/* GObject */
	object_class->set_property = bacon_video_widget_set_property;
	object_class->get_property = bacon_video_widget_get_property;
	object_class->finalize = bacon_video_widget_finalize;

	/* Properties */
	g_object_class_install_property (object_class, PROP_LOGO_MODE,
			g_param_spec_boolean ("logo_mode", NULL, NULL,
				FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_POSITION,
			g_param_spec_int64 ("position", NULL, NULL,
				0, G_MAXINT64, 0, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_STREAM_LENGTH,
			g_param_spec_int64 ("stream_length", NULL, NULL,
				0, G_MAXINT64, 0, G_PARAM_READABLE));
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
				FALSE, G_PARAM_READWRITE));
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
				baconvideowidget_marshal_VOID__STRING_BOOLEAN_BOOLEAN,
				G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_BOOLEAN,
				G_TYPE_BOOLEAN);

	bvw_table_signals[EOS] =
		g_signal_new ("eos",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconVideoWidgetClass, eos),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	bvw_table_signals[REDIRECT] =
		g_signal_new ("got-redirect",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconVideoWidgetClass, got_redirect),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);

	bvw_table_signals[GOT_METADATA] =
		g_signal_new ("got-metadata",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconVideoWidgetClass, got_metadata),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

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
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);

	bvw_table_signals[TICK] =
		g_signal_new ("tick",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconVideoWidgetClass, tick),
				NULL, NULL,
				baconvideowidget_marshal_VOID__INT64_INT64_FLOAT_BOOLEAN,
				G_TYPE_NONE, 4, G_TYPE_INT64, G_TYPE_INT64,
				G_TYPE_FLOAT, G_TYPE_BOOLEAN);

	bvw_table_signals[BUFFERING] =
		g_signal_new ("buffering",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconVideoWidgetClass, buffering),
				NULL, NULL,
				g_cclosure_marshal_VOID__INT,
				G_TYPE_NONE, 1, G_TYPE_INT);

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
bacon_video_widget_init (BaconVideoWidget *bvw)
{
	const char *const *autoplug_list;
	int i = 0;

	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (bvw), GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (bvw), GTK_DOUBLE_BUFFERED);

	bvw->priv = g_new0 (BaconVideoWidgetPrivate, 1);
	bvw->priv->xine = xine_new ();
	bvw->priv->cursor_shown = TRUE;
	bvw->priv->vis_name = g_strdup ("goom");
	bvw->priv->volume = -1;

	bvw->priv->init_width = 0;
	bvw->priv->init_height = 0;

	bvw->priv->queue = g_async_queue_new ();

	/* init configuration  */
	bvw->priv->gc = gconf_client_get_default ();
	setup_config (bvw);

	xine_init (bvw->priv->xine);

	/* Can we play DVDs and VCDs ? */
	autoplug_list = xine_get_autoplay_input_plugin_ids (bvw->priv->xine);
	while (autoplug_list && autoplug_list[i])
	{
		if (g_ascii_strcasecmp (autoplug_list[i], "VCD") == 0)
			bvw->priv->can_vcd = TRUE;
		else if (g_ascii_strcasecmp (autoplug_list[i], "VCDO") == 0)
			bvw->priv->can_vcd = TRUE;
		else if (g_ascii_strcasecmp (autoplug_list[i], "DVD") == 0)
			bvw->priv->can_dvd = TRUE;
		else if (g_ascii_strcasecmp (autoplug_list[i], "CD") == 0)
			bvw->priv->can_cdda = TRUE;
		i++;
	}

	bvw->priv->tick_id = g_timeout_add (140,
			(GSourceFunc) bacon_video_widget_tick_send, bvw);
}

static void
bacon_video_widget_finalize (GObject *object)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) object;

	if (bvw->priv->xine != NULL) {
		xine_exit (bvw->priv->xine);
	}
	g_free (bvw->priv->vis_name);
	g_free (bvw->priv->mediadev);
	g_object_unref (G_OBJECT (bvw->priv->gc));
	g_free (bvw->priv->codecs_path);

	g_list_foreach (bvw->priv->visuals, (GFunc) g_free, NULL);
	g_list_free (bvw->priv->visuals);

	g_idle_remove_by_data (bvw);
	g_async_queue_unref (bvw->priv->queue);
	G_OBJECT_CLASS (parent_class)->finalize (object);

	bvw->priv = NULL;
	bvw = NULL;
}

static void
dest_size_cb (void *data,
	      int video_width, int video_height,
	      double video_pixel_aspect,
	      int *dest_width, int *dest_height,
	      double *dest_pixel_aspect)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *)data;

	/* correct size with video_pixel_aspect */
	if (video_pixel_aspect >= bvw->priv->display_ratio)
		video_width  = video_width * video_pixel_aspect
			/ bvw->priv->display_ratio + .5;
	else
		video_height = video_height * bvw->priv->display_ratio
			/ video_pixel_aspect + .5;

	*dest_width = GTK_WIDGET(bvw)->allocation.width;
	*dest_height = GTK_WIDGET(bvw)->allocation.height;
	*dest_pixel_aspect = bvw->priv->display_ratio;
}

static void
frame_output_cb (void *bvw_gen,
		 int video_width, int video_height,
		 double video_pixel_aspect,
		 int *dest_x, int *dest_y,
		 int *dest_width, int *dest_height,
		 double *dest_pixel_aspect,
		 int *win_x, int *win_y)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) bvw_gen;

	if (bvw == NULL || bvw->priv == NULL)
		return;

	/* correct size with video_pixel_aspect */
	if (video_pixel_aspect >= bvw->priv->display_ratio)
	{
		video_width = video_width * video_pixel_aspect
			/ bvw->priv->display_ratio + .5;
	} else {
		video_height = video_height * bvw->priv->display_ratio
			/ video_pixel_aspect + .5;
	}

	*dest_x = 0;
	*dest_y = 0;
	*win_x = bvw->priv->xpos;
	*win_y = bvw->priv->ypos;

	*dest_width = GTK_WIDGET(bvw)->allocation.width;
	*dest_height = GTK_WIDGET(bvw)->allocation.height;

	/* Size changed */
	if (bvw->priv->video_width != video_width
			|| bvw->priv->video_height != video_height)
	{
		bvw->priv->video_width = video_width;
		bvw->priv->video_height = video_height;

		if (bvw->priv->auto_resize != FALSE
				&& bvw->priv->logo_mode == FALSE
				&& bvw->priv->fullscreen_mode == FALSE)
		{
			signal_data *data;

			data = g_new0 (signal_data, 1);
			data->signal = RATIO_ASYNC;
			g_async_queue_push (bvw->priv->queue, data);
			g_idle_add ((GSourceFunc)
					bacon_video_widget_idle_signal, bvw);
		}
	}

	*dest_pixel_aspect = bvw->priv->display_ratio;
}

static xine_video_port_t *
load_video_out_driver (BaconVideoWidget *bvw, gboolean null_out)
{
	double res_h, res_v;
	x11_visual_t vis;
	const char *video_driver_id;
	xine_video_port_t *vo_driver;
	static char *drivers[] = { "xv", "xshm" };
	guint i;

	if (null_out != FALSE)
	{
		bvw->priv->vo_driver_none = TRUE;
		return xine_open_video_driver (bvw->priv->xine,
				"none", XINE_VISUAL_TYPE_NONE, NULL);
	}

	vis.display = bvw->priv->display;
	vis.screen = bvw->priv->screen;
	vis.d = GDK_WINDOW_XID (bvw->priv->video_window);
	res_h = (DisplayWidth (bvw->priv->display, bvw->priv->screen) * 1000 /
			DisplayWidthMM (bvw->priv->display,
				bvw->priv->screen));
	res_v = (DisplayHeight (bvw->priv->display, bvw->priv->screen) * 1000 /
			DisplayHeightMM (bvw->priv->display,
				bvw->priv->screen));
	bvw->priv->display_ratio = res_v / res_h;

	if (fabs (bvw->priv->display_ratio - 1.0) < 0.01) {
		bvw->priv->display_ratio = 1.0;
	}

	vis.dest_size_cb = dest_size_cb;
	vis.frame_output_cb = frame_output_cb;
	vis.user_data = bvw;

	/* Try to init video with stored information */
	video_driver_id = xine_config_register_string (bvw->priv->xine,
			"video.driver", "auto", "video driver to use",
			NULL, 10, NULL, NULL);

	/* Don't try to load anything but the xshm plugin if we're not
	 * on a local display */
	if (totem_display_is_local () == FALSE)
	{
		return xine_open_video_driver (bvw->priv->xine, "xshm",
				XINE_VISUAL_TYPE_X11, (void *) &vis); 
	}

	if (strcmp (video_driver_id, "auto") != 0)
	{
		vo_driver = xine_open_video_driver (bvw->priv->xine,
						   video_driver_id,
						   XINE_VISUAL_TYPE_X11,
						   (void *) &vis);
		if (vo_driver)
		{
			if (strcmp (video_driver_id, "dxr3") == 0)
				bvw->priv->tvout = TV_OUT_DXR3;

			return vo_driver;
		}
	}

	/* If the video driver is not dxr3, or the dxr3 failed to load
	 * we need to try loading the other ones, skipping dxr3 */

	/* The types are hardcoded for now */
	for (i = 0; i < G_N_ELEMENTS (drivers); i++) {
		vo_driver = xine_open_video_driver (bvw->priv->xine,
				drivers[i], XINE_VISUAL_TYPE_X11,
				(void *) &vis);
		if (vo_driver)
			break;
	}

	return vo_driver;
}

static xine_audio_port_t *
load_audio_out_driver (BaconVideoWidget *bvw, gboolean null_out,
		GError **error)
{
	xine_audio_port_t *ao_driver;
	const char *audio_driver_id;

	if (null_out != FALSE) {
		ao_driver = xine_open_audio_driver (bvw->priv->xine,
				"none", NULL);
		if (ao_driver != NULL) {
			bvw->priv->ao_driver_none = TRUE;
		}
		return ao_driver;
	}

	audio_driver_id = xine_config_register_string (bvw->priv->xine,
			"audio.driver", "auto", "audio driver to use",
			NULL, 10, NULL, NULL);

	/* No configuration, fallback to auto */
	if (audio_driver_id == NULL || strcmp (audio_driver_id, "") == 0)
		audio_driver_id = g_strdup ("auto");

	/* We know how to handle null driver */
	if (strcmp (audio_driver_id, "null") == 0)
		return NULL;

	/* auto probe */
	if (strcmp (audio_driver_id, "auto") == 0)
		ao_driver = xine_open_audio_driver (bvw->priv->xine,
				NULL, NULL);
	else
		ao_driver = xine_open_audio_driver (bvw->priv->xine,
				audio_driver_id, NULL);

	/* if it failed without autoprobe, probe */
	if (ao_driver == NULL && strcmp (audio_driver_id, "auto") != 0)
		ao_driver = xine_open_audio_driver (bvw->priv->xine,
				NULL, NULL);

	if (ao_driver == NULL && strcmp (audio_driver_id, "auto") != 0)
	{
		g_set_error (error, BVW_ERROR, BVW_ERROR_AUDIO_PLUGIN,
				_("Couldn't load the '%s' audio driver\n"
					"Check that the device is not busy."),
				audio_driver_id ? audio_driver_id : "auto" );
		return NULL;
	}

	return ao_driver;
}

static void
bvw_config_helper_string (xine_t *xine, const char *id, const char *val,
		xine_cfg_entry_t *entry)
{
	memset (entry, 0, sizeof (entry));

	if (!xine_config_lookup_entry (xine, id, entry))
	{
		xine_config_register_string (xine, id, val, "", NULL, 10,
				NULL, NULL);
		xine_config_lookup_entry (xine, id, entry);
	}
}

static void
bvw_config_helper_num (xine_t *xine, const char *id, int val,
		xine_cfg_entry_t *entry)
{
	memset (entry, 0, sizeof (entry));

	if (!xine_config_lookup_entry (xine, id, entry))
	{
		xine_config_register_num (xine, id, val, 0, NULL, 10,
				NULL, NULL);
		xine_config_lookup_entry (xine, id, entry);
	}
}

static void
setup_config (BaconVideoWidget *bvw)
{
	char *path;
	xine_cfg_entry_t entry;

	path = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), CONFIG_FILE, NULL);
	xine_config_load (bvw->priv->xine, path);
	g_free (path);

	/* default demux strategy */
	xine_config_register_enum (bvw->priv->xine,
			"engine.demux.strategy",
			0,
			(char **) demux_strategies_str,
			 "media format detection strategy",
			 NULL, 10, NULL, NULL);

	xine_config_lookup_entry (bvw->priv->xine,
			"engine.demux.strategy", &entry);
	entry.num_value = 0;
	xine_config_update_entry (bvw->priv->xine, &entry);

	if (bvw->priv->gc == NULL)
		return;

	/* Disable CDDB, we'll use Musicbrainz instead */
	bvw_config_helper_num (bvw->priv->xine,
			"media.audio_cd.use_cddb", 1, &entry);
	entry.num_value = 0;
	xine_config_update_entry (bvw->priv->xine, &entry);

	if (bvw->priv->gc == NULL) {
		g_warning ("GConf not available, broken installation?");
		return;
	}

	/* Debug configuration */
	if (gconf_client_get_bool (bvw->priv->gc,
				GCONF_PREFIX"/debug", NULL) == FALSE)
	{
		xine_engine_set_param (bvw->priv->xine,
				XINE_ENGINE_PARAM_VERBOSITY,
				XINE_VERBOSITY_NONE);
	} else {
		xine_engine_set_param (bvw->priv->xine,
				XINE_ENGINE_PARAM_VERBOSITY,
				0xff);
	}

	/* Proxy configuration */
	if (gconf_client_get_bool (bvw->priv->gc, "/system/http_proxy/use_http_proxy", NULL) == FALSE)
	{
		bvw_config_helper_string (bvw->priv->xine,
				"media.network.http_proxy_host", "", &entry);
		entry.str_value = "";
		xine_config_update_entry (bvw->priv->xine, &entry);

		return;
	}

	bvw_config_helper_string (bvw->priv->xine,
			"media.network.http_proxy_host", "", &entry);
	entry.str_value = gconf_client_get_string (bvw->priv->gc,
			"/system/http_proxy/host", NULL);
	xine_config_update_entry (bvw->priv->xine, &entry);
	g_free (entry.str_value);

	bvw_config_helper_num (bvw->priv->xine,
			 "media.network.http_proxy_port", 8080, &entry);
	entry.num_value = gconf_client_get_int (bvw->priv->gc,
			"/system/http_proxy/port", NULL);
	xine_config_update_entry (bvw->priv->xine, &entry);

	if (gconf_client_get_bool (bvw->priv->gc, "/system/http_proxy/use_authentication", NULL) == FALSE)
	{
		bvw_config_helper_string (bvw->priv->xine,
				"media.network.http_proxy_user", g_get_user_name(),
				&entry);
		entry.str_value = "";
		xine_config_update_entry (bvw->priv->xine, &entry);

		bvw_config_helper_string (bvw->priv->xine,
				"media.network.http_proxy_password", "",
				&entry);
		entry.str_value = "";
		xine_config_update_entry (bvw->priv->xine, &entry);
	} else {
		bvw_config_helper_string (bvw->priv->xine,
				"media.network.http_proxy_user", g_get_user_name(),
				&entry);
		entry.str_value = gconf_client_get_string (bvw->priv->gc,
				"/system/http_proxy/authentication_user",
				NULL);
		xine_config_update_entry (bvw->priv->xine, &entry);
		g_free (entry.str_value);

		bvw_config_helper_string (bvw->priv->xine,
				"media.network.http_proxy_password", "",
				&entry);
		entry.str_value = gconf_client_get_string (bvw->priv->gc,
				"/system/http_proxy/authentication_password",
				NULL);
		xine_config_update_entry (bvw->priv->xine, &entry);
		g_free (entry.str_value);
	}
}

static void
setup_config_stream (BaconVideoWidget *bvw)
{
	GConfValue *confvalue;
	int value, i, tmp;

	if (bvw->priv->gc == NULL)
		return;

	/* Setup brightness and contrast */
	if (bvw->priv->vo_driver == NULL)
		return;

	for (i = 0; i < 4; i++)
	{
		confvalue = gconf_client_get_without_default (bvw->priv->gc,
				video_props_str[i], NULL);

		if (confvalue != NULL)
		{
			value = gconf_value_get_int (confvalue);
			gconf_value_free (confvalue);
		} else {
			value = 65535 / 2;
		}

		tmp = xine_get_param (bvw->priv->stream, video_props[i]);
		if (value != tmp)
		{
			xine_set_param (bvw->priv->stream,
					video_props[i], value);
		}
	}
}

static gboolean
bacon_video_widget_plugin_exists (BaconVideoWidget *bvw, const char *filename)
{
	char *path;
	gboolean res;

	path = g_build_filename (bvw->priv->codecs_path, filename, NULL);
	res = g_file_test (path, G_FILE_TEST_IS_REGULAR);
	g_free (path);
	return res;
}

static gboolean
video_window_translate_point (BaconVideoWidget *bvw, int gui_x, int gui_y,
		int *video_x, int *video_y)
{
	int res;
	x11_rectangle_t rect;

	rect.x = gui_x;
	rect.y = gui_y;
	rect.w = 0;
	rect.h = 0;

	res = xine_port_send_gui_data (bvw->priv->vo_driver,
			XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*)&rect);

	if (res != -1)
	{
		/* the driver implements gui->video coordinate space
		 * translation so we use it */
		*video_x = rect.x;
		*video_y = rect.y;
		return TRUE;
	}

	return FALSE;
}

/* Changes the way xine skips while playing a DVD,
 * 1 == CHAPTER
 * 2 == TITLE
 */
static void 
dvd_skip_behaviour (BaconVideoWidget *bvw, int behaviour)
{
        if (behaviour < 1 || behaviour > 2)
                return;

        xine_config_register_num (bvw->priv->xine,
                        "media.dvd.skip_behaviour",
                        behaviour,
                        "DVD Skip behaviour",
                        NULL,
                        10,
                        NULL, NULL);

        return;
}

void
bacon_video_widget_dvd_event (BaconVideoWidget *bvw, BaconVideoWidgetDVDEvent type)
{
        xine_event_t event;

	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

        switch (type)
        {
        case BVW_DVD_ROOT_MENU:
                event.type = XINE_EVENT_INPUT_MENU1;
                break;
        case BVW_DVD_TITLE_MENU:
                event.type = XINE_EVENT_INPUT_MENU2;
                break;
        case BVW_DVD_SUBPICTURE_MENU:
                event.type = XINE_EVENT_INPUT_MENU4;
                break;
        case BVW_DVD_AUDIO_MENU:
                event.type = XINE_EVENT_INPUT_MENU5;
                break;
        case BVW_DVD_ANGLE_MENU:
                event.type = XINE_EVENT_INPUT_MENU6;
                break;
        case BVW_DVD_CHAPTER_MENU:
                event.type = XINE_EVENT_INPUT_MENU7;
                break;
        case BVW_DVD_NEXT_CHAPTER:
                dvd_skip_behaviour (bvw, 1);
                event.type = XINE_EVENT_INPUT_NEXT;
                break;
        case BVW_DVD_PREV_CHAPTER:
                dvd_skip_behaviour (bvw, 1);
                event.type = XINE_EVENT_INPUT_PREVIOUS;
                break;
        case BVW_DVD_NEXT_TITLE:
                dvd_skip_behaviour (bvw, 2);
                event.type = XINE_EVENT_INPUT_NEXT;
                break;
        case BVW_DVD_PREV_TITLE:
                dvd_skip_behaviour (bvw, 2);
                event.type = XINE_EVENT_INPUT_PREVIOUS;
                break;
        case BVW_DVD_NEXT_ANGLE:
                event.type = XINE_EVENT_INPUT_ANGLE_NEXT;
                break;
        case BVW_DVD_PREV_ANGLE:
                event.type = XINE_EVENT_INPUT_ANGLE_PREVIOUS;
                break;
        default:
                return;
        }

        event.stream = bvw->priv->stream;
        event.data = NULL;
        event.data_length = 0;

        xine_event_send (bvw->priv->stream,
                        (xine_event_t *) (&event));
}

static gboolean
generate_mouse_event (BaconVideoWidget *bvw, GdkEvent *event,
		gboolean is_motion)
{
	GdkEventMotion *mevent = (GdkEventMotion *) event;
	GdkEventButton *bevent = (GdkEventButton *) event;
	int x, y;
	gboolean retval;

	/* Don't even try to generate an event if we're dying */
	if (bvw->priv->stream == NULL)
		return FALSE;

	if (is_motion == FALSE && bevent->button != 1)
		return FALSE;

	if (is_motion != FALSE)
		retval = video_window_translate_point (bvw,
				mevent->x, mevent->y, &x, &y);
	else
		retval = video_window_translate_point (bvw,
				bevent->x, bevent->y, &x, &y);

	if (retval != FALSE)
	{
		xine_event_t event;
		xine_input_data_t input;

		if (is_motion != FALSE)
		{
			event.type = XINE_EVENT_INPUT_MOUSE_MOVE;
			input.button = 0; /* Just motion. */
		} else {
			event.type = XINE_EVENT_INPUT_MOUSE_BUTTON;
			input.button = 1;
		}

		input.x = x;
		input.y = y;
		event.stream = bvw->priv->stream;
		event.data = &input;
		event.data_length = sizeof(input);

		xine_event_send (bvw->priv->stream,
				(xine_event_t *) (&event));

		return TRUE;
	}

	return FALSE;
}

static gboolean
configure_cb (GtkWidget *widget, GdkEventConfigure *event,
		BaconVideoWidget *bvw)
{
	bvw->priv->xpos = event->x + GTK_WIDGET (bvw)->allocation.x;
	bvw->priv->ypos = event->y + GTK_WIDGET (bvw)->allocation.y;

	return FALSE;
}

static void
size_changed_cb (GdkScreen *screen, BaconVideoWidget *bvw)
{
	double res_h, res_v;

	XLockDisplay (bvw->priv->display);
	res_h = (DisplayWidth (bvw->priv->display, bvw->priv->screen) * 1000 /
			DisplayWidthMM (bvw->priv->display,
				bvw->priv->screen));
	res_v = (DisplayHeight (bvw->priv->display, bvw->priv->screen) * 1000 /
			DisplayHeightMM (bvw->priv->display,
				bvw->priv->screen));
	XUnlockDisplay (bvw->priv->display);

	bvw->priv->display_ratio = res_v / res_h;

	if (fabs (bvw->priv->display_ratio - 1.0) < 0.01) {
		bvw->priv->display_ratio = 1.0;
	}
}

static void
bacon_video_widget_realize (GtkWidget *widget)
{
	GdkWindowAttr attr;
	BaconVideoWidget *bvw;

	bvw = BACON_VIDEO_WIDGET (widget);
	if (bvw->priv->type != BVW_USE_TYPE_VIDEO)
	{
		g_warning ("Use type isn't video but we realized the widget");
		return;
	}

	/* set realized flag */
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	/* Create the widget's window */
	attr.x = widget->allocation.x;
	attr.y = widget->allocation.y;
	attr.width = widget->allocation.width;
	attr.height = widget->allocation.height;
	attr.window_type = GDK_WINDOW_CHILD;
	attr.wclass = GDK_INPUT_OUTPUT;
	attr.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK
		| GDK_POINTER_MOTION_MASK
		| GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK;
	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
			&attr, GDK_WA_X | GDK_WA_Y);
	gdk_window_show (widget->window);

	/* Flush, so that the window is really shown */
	gdk_flush ();
	gdk_window_set_user_data (widget->window, bvw);

	bvw->priv->video_window = widget->window;

	/* Set a black background */
	gdk_draw_rectangle (widget->window, widget->style->black_gc, TRUE,
			attr.x, attr.y,
			attr.width, attr.height);

	/* track configure events of toplevel window */
	g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)),
			"configure-event",
			G_CALLBACK (configure_cb), bvw);

	/* get screen size changes */
	g_signal_connect (G_OBJECT (gdk_screen_get_default ()),
			"size-changed", G_CALLBACK (size_changed_cb), bvw);

	/* Now onto the video out driver */
	bvw->priv->display = XOpenDisplay (gdk_display_get_name
			(gdk_display_get_default ()));
	bvw->priv->screen = DefaultScreen (bvw->priv->display);

	bvw->priv->vo_driver = load_video_out_driver
		(bvw, FALSE);

	if (bvw->priv->vo_driver == NULL)
	{
		signal_data *sigdata;

		bvw->priv->vo_driver = load_video_out_driver (bvw, TRUE);

		/* We need to use an async signal, otherwise we try to
		 * unrealize the widget before it's finished realizing */
		sigdata = g_new0 (signal_data, 1);
		sigdata->signal = ERROR_ASYNC;
		sigdata->msg = _("No video output is available. Make sure that the program is correctly installed.");
		sigdata->fatal = TRUE;
		g_async_queue_push (bvw->priv->queue, sigdata);
		g_idle_add ((GSourceFunc) bacon_video_widget_idle_signal, bvw);
	}

	bvw->priv->ao_driver = load_audio_out_driver (bvw, FALSE, NULL);

	if (bvw->priv->type == BVW_USE_TYPE_VIDEO
			&& bvw->priv->ao_driver != NULL
			&& bvw->priv->ao_driver_none == FALSE)
	{
		bvw->priv->vis = xine_post_init (bvw->priv->xine,
				bvw->priv->vis_name, 0,
				&bvw->priv->ao_driver, &bvw->priv->vo_driver);
	}

	bvw->priv->have_xvidmode = bacon_resize_init ();
	bvw->priv->stream = xine_stream_new (bvw->priv->xine,
			bvw->priv->ao_driver, bvw->priv->vo_driver);
	setup_config_stream (bvw);
	bvw->priv->ev_queue = xine_event_new_queue (bvw->priv->stream);

	/* Setup xine events */
	xine_event_create_listener_thread (bvw->priv->ev_queue,
			xine_event, (void *) bvw);

#ifdef HAVE_NVTV
	if (!(nvtv_simple_init() && nvtv_enable_autoresize(TRUE))) {
		nvtv_simple_enable(FALSE);
	} 
#endif

	return;
}

static gboolean
bacon_video_widget_idle_signal (BaconVideoWidget *bvw)
{
	int queue_length;
	signal_data *data;

	data = g_async_queue_try_pop (bvw->priv->queue);
	if (data == NULL)
		return FALSE;

	TE ();

	switch (data->signal)
	{
	case RATIO_ASYNC:
		bacon_video_widget_set_scale_ratio (bvw, 1);
		break;
	case REDIRECT_ASYNC:
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[REDIRECT],
				0, data->msg);
		break;
	case TITLE_CHANGE_ASYNC:
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[TITLE_CHANGE],
				0, data->msg);
		break;
	case EOS_ASYNC:
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[EOS], 0, NULL);
		break;
	case CHANNELS_CHANGE_ASYNC:
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[CHANNELS_CHANGE], 0, NULL);
		break;
	case BUFFERING_ASYNC:
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[BUFFERING],
				0, data->num);
		break;
	case MESSAGE_ASYNC:
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[ERROR],
				0, data->msg, TRUE, FALSE);
		break;
	case SPEED_WARNING_ASYNC:
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[SPEED_WARNING],
				0, NULL);
		break;
	case ERROR_ASYNC:
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[ERROR], 0,
				data->msg, TRUE, data->fatal);
	default:
		g_assert_not_reached ();
	}

	TL ();

	g_free (data->msg);
	g_free (data);
	queue_length = g_async_queue_length (bvw->priv->queue);

	return (queue_length > 0);
}

static void
xine_event_message (BaconVideoWidget *bvw, xine_ui_message_data_t *data)
{
	char *message;
	int num;
	signal_data *sigdata;
	const char *params;

	message = NULL;

	switch (data->type)
	{
	case XINE_MSG_NO_ERROR:
		return;
	case XINE_MSG_GENERAL_WARNING:
		return;
	case XINE_MSG_UNKNOWN_HOST:
		num = BVW_ERROR_UNKNOWN_HOST;
		message = g_strdup (_("The server you are trying to connect to is not known."));
		break;
	case XINE_MSG_UNKNOWN_DEVICE:
		num = BVW_ERROR_INVALID_DEVICE;
		message = g_strdup_printf (_("The device name you specified (%s) seems to be invalid."), (char *) data + data->parameters);
		break;
	case XINE_MSG_NETWORK_UNREACHABLE:
		num = BVW_ERROR_NETWORK_UNREACHABLE;
		message = g_strdup_printf (_("The server you are trying to connect to (%s) is unreachable."), (char *) data + data->parameters);
		break;
	case XINE_MSG_CONNECTION_REFUSED:
		num = BVW_ERROR_CONNECTION_REFUSED;
		message = g_strdup (_("The connection to this server was refused."));
		break;
	case XINE_MSG_FILE_NOT_FOUND:
		num = BVW_ERROR_FILE_NOT_FOUND;
		message = g_strdup (_("The specified movie could not be found."));
		break;
	case XINE_MSG_READ_ERROR:
		if (bvw->priv->mrl && g_str_has_prefix (bvw->priv->mrl, "dvd:") != FALSE)
                {
                        num = BVW_ERROR_DVD_ENCRYPTED;
                        message = g_strdup (_("The source seems encrypted, and can't be read. Are you trying to play an encrypted DVD without libdvdcss?"));
                } else {
			num = BVW_ERROR_READ_ERROR;
			message = g_strdup (_("The movie could not be read."));
		}
		break;
	case XINE_MSG_LIBRARY_LOAD_ERROR:
		params = (char *) data + data->parameters;
		if (bacon_video_widget_plugin_exists (bvw, params) == FALSE)
			return;
		/* Only if the file could really not be loaded */
		num = BVW_ERROR_PLUGIN_LOAD;
		message = g_strdup_printf (_("A problem occured while loading a library or a decoder (%s)."), params);
		break;
	case XINE_MSG_ENCRYPTED_SOURCE:
		if (g_str_has_prefix (bvw->priv->mrl, "dvd:") != FALSE)
		{
			num = BVW_ERROR_DVD_ENCRYPTED;
			message = g_strdup (_("The source seems encrypted, and can't be read. Are you trying to play an encrypted DVD without libdvdcss?"));
		} else {
			num = BVW_ERROR_FILE_ENCRYPTED;
			message = g_strdup (_("This file is encrypted and cannot be played back."));
		}
		break;
	case XINE_MSG_SECURITY:
		num = BVW_ERROR_GENERIC;
		message = g_strdup (_("For security reasons, this movie can not be played back."));
		break;
	case XINE_MSG_AUDIO_OUT_UNAVAILABLE:
		xine_stop (bvw->priv->stream);
		num = BVW_ERROR_AUDIO_BUSY;
		message = g_strdup (_("The audio device is busy. Is another application using it?"));
		break;
	case XINE_MSG_PERMISSION_ERROR:
		num = BVW_ERROR_FILE_PERMISSION;
		if (g_str_has_prefix (bvw->priv->mrl, "file:") != FALSE)
			message = g_strdup (_("You are not allowed to open this file."));
		else
			message = g_strdup (_("The server refused access to this file or stream."));
		break;
	default:
		D("xine_event_message: unhandled error\ntype: %d", data->type);
		return;
	}

	sigdata = g_new0 (signal_data, 1);
	sigdata->signal = ERROR_ASYNC;
	sigdata->msg = message;
	sigdata->num = num;
	g_async_queue_push (bvw->priv->queue, sigdata);
	g_idle_add ((GSourceFunc) bacon_video_widget_idle_signal, bvw);
}

static void
xine_event (void *user_data, const xine_event_t *event)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) user_data;
	xine_ui_data_t *ui_data;
	xine_progress_data_t *prg;
	xine_mrl_reference_data_t *ref;
	xine_spu_button_t *spubtn;
	signal_data *data;

	switch (event->type)
	{
	case XINE_EVENT_SPU_BUTTON:
		spubtn = (xine_spu_button_t *) event->data;
		if (spubtn->direction)
		{
			GdkCursor *cursor;
			cursor = gdk_cursor_new (GDK_HAND2);
			gdk_window_set_cursor
				(bvw->priv->video_window, cursor);
			gdk_cursor_unref (cursor);
		} else {
			gdk_window_set_cursor(bvw->priv->video_window, NULL);
		}
		break;
	case XINE_EVENT_UI_PLAYBACK_FINISHED:
		if (bvw->priv->got_redirect != FALSE)
			break;

		data = g_new0 (signal_data, 1);
		data->signal = EOS_ASYNC;
		g_async_queue_push (bvw->priv->queue, data);
		g_idle_add ((GSourceFunc) bacon_video_widget_idle_signal, bvw);
		break;
	case XINE_EVENT_UI_CHANNELS_CHANGED:
		data = g_new0 (signal_data, 1);
		data->signal = CHANNELS_CHANGE_ASYNC;
		g_async_queue_push (bvw->priv->queue, data);
		g_idle_add ((GSourceFunc) bacon_video_widget_idle_signal, bvw);
		break;
	case XINE_EVENT_UI_SET_TITLE:
		ui_data = event->data;

		data = g_new0 (signal_data, 1);
		data->signal = TITLE_CHANGE_ASYNC;
		data->msg = g_strdup (ui_data->str);
		g_async_queue_push (bvw->priv->queue, data);
		g_idle_add ((GSourceFunc) bacon_video_widget_idle_signal, bvw);
		break;
	case XINE_EVENT_PROGRESS:
		prg = event->data;

		data = g_new0 (signal_data, 1);
		data->signal = BUFFERING_ASYNC;
		data->num = prg->percent;
		g_async_queue_push (bvw->priv->queue, data);
		g_idle_add ((GSourceFunc) bacon_video_widget_idle_signal, bvw);
		break;
	case XINE_EVENT_MRL_REFERENCE:
		ref = event->data;
		data = g_new0 (signal_data, 1);
		data->signal = REDIRECT_ASYNC;
		data->msg = g_strdup (ref->mrl);
		g_async_queue_push (bvw->priv->queue, data);
		g_idle_add ((GSourceFunc) bacon_video_widget_idle_signal, bvw);
		bvw->priv->got_redirect = TRUE;
		break;
	case XINE_EVENT_UI_MESSAGE:
		xine_event_message (bvw, (xine_ui_message_data_t *)event->data);
		break;
	case XINE_EVENT_DROPPED_FRAMES:
		data = g_new0 (signal_data, 1);
		data->signal = SPEED_WARNING_ASYNC;
		g_async_queue_push (bvw->priv->queue, data);
		g_idle_add ((GSourceFunc) bacon_video_widget_idle_signal, bvw);
		break;
	case XINE_EVENT_AUDIO_LEVEL:
		/* Unhandled, we use the software mixer, not the hardware one */
		break;
	}
}

static void
xine_error (BaconVideoWidget *bvw, GError **error)
{
	signal_data *data, *save_data;
	int err;

	save_data = NULL;

	/* Steal messages from the async queue, if there's an error,
	 * to use as the error message rather than the crappy errors from
	 * xine_open() */
	while ((data = g_async_queue_try_pop (bvw->priv->queue)) != NULL)
	{
		g_assert (data->signal == MESSAGE_ASYNC
				|| data->signal == ERROR_ASYNC
				|| data->signal == BUFFERING_ASYNC
				|| data->signal == REDIRECT_ASYNC
				|| data->signal == EOS_ASYNC);

		if (data->signal == ERROR_ASYNC)
		{
			if (save_data != NULL)
			{
				g_free (save_data->msg);
				g_free (save_data);
			}
			save_data = data;
		} else {
			g_free (data->msg);
			g_free (data);
		}
	}

	if (save_data != NULL)
	{
		g_set_error (error, BVW_ERROR, save_data->num,
				"%s", save_data->msg);
		g_free (save_data->msg);
		g_free (save_data);

		return;
	}

	err = xine_get_error (bvw->priv->stream);
	if (err == XINE_ERROR_NONE)
		return;

	switch (err)
	{
	case XINE_ERROR_NO_INPUT_PLUGIN:
	case XINE_ERROR_NO_DEMUX_PLUGIN:
		g_set_error (error, BVW_ERROR, BVW_ERROR_NO_PLUGIN_FOR_FILE,
				_("There is no plugin to handle this movie."));
		break;
	case XINE_ERROR_DEMUX_FAILED:
		g_set_error (error, BVW_ERROR, BVW_ERROR_BROKEN_FILE,
				_("This movie is broken and can not be played further."));
		break;
	case XINE_ERROR_MALFORMED_MRL:
		g_set_error (error, BVW_ERROR, BVW_ERROR_UNVALID_LOCATION,
				_("This location is not a valid one."));
		break;
	case XINE_ERROR_INPUT_FAILED:
		g_set_error (error, BVW_ERROR, BVW_ERROR_FILE_GENERIC,
				_("This movie could not be opened."));
		break;
	default:
		g_set_error (error, BVW_ERROR, BVW_ERROR_GENERIC,
				_("Generic Error."));
		break;
	}
}

static void
bacon_video_widget_unrealize (GtkWidget *widget)
{
	BaconVideoWidget *bvw;
	char *configfile;
	int speed;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (widget));

	bvw = BACON_VIDEO_WIDGET (widget);

	g_source_remove (bvw->priv->tick_id);

	speed = xine_get_param (bvw->priv->stream, XINE_PARAM_SPEED);
	if (speed != XINE_SPEED_PAUSE)
		show_vfx_update (bvw, FALSE);

	/* stop the playback */
	xine_stop (bvw->priv->stream);
	xine_close (bvw->priv->stream);

	/* Save the current volume */
	if (bacon_video_widget_can_set_volume (bvw) != FALSE)
	{
		gconf_client_set_int (bvw->priv->gc, GCONF_PREFIX"/volume",
				bvw->priv->volume, NULL);
	}

	/* Kill the TV out */
#ifdef HAVE_NVTV
        nvtv_simple_exit();
#endif


/* FIXME missing stuff from libxine includes (<= 1-rc7) */
#ifndef XINE_GUI_SEND_WILL_DESTROY_DRAWABLE
#define XINE_GUI_SEND_WILL_DESTROY_DRAWABLE 9
#endif
	xine_port_send_gui_data (bvw->priv->vo_driver,
			XINE_GUI_SEND_WILL_DESTROY_DRAWABLE,
			(void*)bvw->priv->video_window);

	/* Hide all windows */
	if (GTK_WIDGET_MAPPED (widget))
		gtk_widget_unmap (widget);
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	/* Get rid of the rest of the stream */
	xine_dispose (bvw->priv->stream);
	xine_event_dispose_queue (bvw->priv->ev_queue);
	bvw->priv->stream = NULL;

	/* save config */
	configfile = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), CONFIG_FILE, NULL);
	xine_config_save (bvw->priv->xine, configfile);
	g_free (configfile);

	/* Kill the drivers */
	if (bvw->priv->vo_driver != NULL)
		xine_close_video_driver (bvw->priv->xine,
				bvw->priv->vo_driver);
	if (bvw->priv->ao_driver != NULL)
		xine_close_audio_driver (bvw->priv->xine,
				bvw->priv->ao_driver);

	/* stop event thread */
	xine_exit (bvw->priv->xine);
	bvw->priv->xine = NULL;

	/* This destroys widget->window and unsets the realized flag */
	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static struct poptOption xine_options[] = {
	POPT_TABLEEND
};

struct poptOption *
bacon_video_widget_get_popt_table (void)
{
	/* Xine backend does not need any options */
	return (struct poptOption *) xine_options;
}

void
bacon_video_widget_init_backend (int *argc, char ***argv)
{
  /* no-op */
}

GQuark
bacon_video_widget_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("bvw-error-quark");
	return q;
}

GtkWidget *
bacon_video_widget_new (int width, int height,
		BvwUseType type, GError **error)
{
	BaconVideoWidget *bvw;
	xine_cfg_entry_t entry;

	bvw = BACON_VIDEO_WIDGET (g_object_new
			(bacon_video_widget_get_type (), NULL));

	bvw->priv->init_width = width;
	bvw->priv->init_height = height;
	bvw->priv->type = type;

	/* Don't load anything yet if we're looking for proper video
	 * output */
	if (type == BVW_USE_TYPE_VIDEO)
	{
		bvw_config_helper_num (bvw->priv->xine, "engine.buffers.video_num_buffers",
				500, &entry);
		entry.num_value = 500;
		xine_config_update_entry (bvw->priv->xine, &entry);
		return GTK_WIDGET (bvw);
	}

	/* load the output drivers */
	if (type == BVW_USE_TYPE_AUDIO)
	{
		bvw->priv->ao_driver = load_audio_out_driver (bvw,
				FALSE, error);
		if (error != NULL && *error != NULL)
			return NULL;
	} else if (type == BVW_USE_TYPE_METADATA) {
		bvw->priv->ao_driver = load_audio_out_driver (bvw,
				TRUE, error);
	}

	/* We need to wait for the widget to realise if we want to
	 * load a video output with screen output, and capture is the
	 * only one actually needing a video output */
	if (type == BVW_USE_TYPE_CAPTURE || type == BVW_USE_TYPE_METADATA) {
		bvw->priv->vo_driver = load_video_out_driver (bvw, TRUE);
	}

	/* Be extra careful about exiting out nicely when a video output
	 * isn't available */
	if (type == BVW_USE_TYPE_CAPTURE && bvw->priv->vo_driver == NULL)
	{
		/* Close the xine stuff */
		if (bvw->priv->ao_driver != NULL) {
			xine_close_audio_driver (bvw->priv->xine,
					bvw->priv->ao_driver);
		}
		xine_exit (bvw->priv->xine);

		/* get rid of all our crappety crap */
		g_source_remove (bvw->priv->tick_id);
		g_idle_remove_by_data (bvw);
		g_async_queue_unref (bvw->priv->queue);
		g_free (bvw->priv->vis_name);
		g_object_unref (G_OBJECT (bvw->priv->gc));
		g_free (bvw->priv->vis_name);
		g_free (bvw->priv);
		g_free (bvw);

		g_set_error (error, BVW_ERROR, BVW_ERROR_VIDEO_PLUGIN,
				_("No video output is available. Make sure that the program is correctly installed."));
		return NULL;
	}

	bvw_config_helper_num (bvw->priv->xine, "engine.buffers.video_num_buffers",
			5, &entry);
	entry.num_value = 5;
	xine_config_update_entry (bvw->priv->xine, &entry);

	bvw->priv->stream = xine_stream_new (bvw->priv->xine,
			bvw->priv->ao_driver, bvw->priv->vo_driver);
	setup_config_stream (bvw);
	bvw->priv->ev_queue = xine_event_new_queue (bvw->priv->stream);
	xine_event_create_listener_thread (bvw->priv->ev_queue,
			xine_event, (void *) bvw);

	return GTK_WIDGET (bvw);
}

static gboolean
bacon_video_widget_expose (GtkWidget *widget, GdkEventExpose *event)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) widget;
	XExposeEvent *expose;

	if (event->count != 0)
		return FALSE;

	expose = g_new0 (XExposeEvent, 1);
	expose->count = event->count;

	xine_port_send_gui_data (bvw->priv->vo_driver,
			XINE_GUI_SEND_EXPOSE_EVENT, expose);

	g_free (expose);

	return FALSE;
}

static gboolean
bacon_video_widget_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) widget;

	generate_mouse_event (bvw, (GdkEvent *)event, TRUE);

	if (GTK_WIDGET_CLASS (parent_class)->motion_notify_event != NULL)
		(* GTK_WIDGET_CLASS (parent_class)->motion_notify_event) (widget, event);

	return FALSE;
}

static gboolean
bacon_video_widget_button_press (GtkWidget *widget, GdkEventButton *event)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) widget;

	generate_mouse_event (bvw, (GdkEvent *)event, FALSE);

	if (GTK_WIDGET_CLASS (parent_class)->button_press_event != NULL)
		                (* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event);

	return FALSE;
}

static void
bacon_video_widget_show (GtkWidget *widget)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) widget;

	gdk_window_show (bvw->priv->video_window);

	if (GTK_WIDGET_CLASS (parent_class)->show != NULL)
		(* GTK_WIDGET_CLASS (parent_class)->show) (widget);
}

static void
bacon_video_widget_hide (GtkWidget *widget)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) widget;

	gdk_window_hide (bvw->priv->video_window);

	if (GTK_WIDGET_CLASS (parent_class)->hide != NULL)
		(* GTK_WIDGET_CLASS (parent_class)->hide) (widget);
}

static void
bacon_video_widget_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(BACON_IS_VIDEO_WIDGET(widget));

	requisition->width = 240;
	requisition->height = 180;
}

static void
bacon_video_widget_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	BaconVideoWidget *bvw;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (widget));

	bvw = BACON_VIDEO_WIDGET (widget);

	widget->allocation = *allocation;
	bvw->priv->xpos = allocation->x;
	bvw->priv->ypos = allocation->y;

	if ( (bvw->priv->init_width == 0) && (bvw->priv->init_height == 0) ) {
		/* First allocation, saving values */
		bvw->priv->init_width = allocation->width;
		bvw->priv->init_height = allocation->height;
	}

	if (GTK_WIDGET_REALIZED (widget))
	{
		gdk_window_move_resize (widget->window,
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);
	}
}

static gboolean
bacon_video_widget_tick_send (BaconVideoWidget *bvw)
{
	int current_time, stream_length, current_position;
	float current_position_f;
	gboolean ret = TRUE, seekable;

	if (bvw->priv->stream == NULL || bvw->priv->logo_mode != FALSE)
		return TRUE;

	if (bvw->priv->mrl == NULL)
	{
		current_time = 0;
		stream_length = 0;
		current_position = 0;
	} else {
		ret = xine_get_pos_length (bvw->priv->stream,
				&current_position,
				&current_time,
				&stream_length);
	}

	if (ret == FALSE)
		return TRUE;

	if (bvw->priv->seeking == 1)
	{
		current_position_f = bvw->priv->seek_dest;
		current_time = bvw->priv->seek_dest * stream_length;
	} else if (bvw->priv->seeking == 2) {
		current_time = bvw->priv->seek_dest_time;
		current_position_f = (float) current_time / stream_length;
	} else {
		current_position_f = (float) current_position / 65535;
	}

	if (stream_length > 0)
		bvw->priv->is_live = FALSE;
	else
		bvw->priv->is_live = TRUE;

	if (stream_length != 0 && bvw->priv->mrl != NULL) {
		seekable = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_SEEKABLE);
	} else {
		seekable = FALSE;
	}

	g_signal_emit (G_OBJECT (bvw),
			bvw_table_signals[TICK], 0,
			(gint64) (current_time),
			(gint64) (stream_length),
			current_position_f,
			seekable);

	return TRUE;
}

static void
show_vfx_update (BaconVideoWidget *bvw, gboolean show_visuals)
{
	xine_post_out_t *audio_source;
	gboolean has_video;

	has_video = xine_get_stream_info(bvw->priv->stream,
			XINE_STREAM_INFO_HAS_VIDEO);

	if (has_video != FALSE && show_visuals != FALSE
			&& bvw->priv->using_vfx != FALSE)
	{
		audio_source = xine_get_audio_source (bvw->priv->stream);
		if (xine_post_wire_audio_port (audio_source,
					bvw->priv->ao_driver))
			bvw->priv->using_vfx = FALSE;
	} else if (has_video == FALSE && show_visuals != FALSE
			&& bvw->priv->using_vfx == FALSE
			&& bvw->priv->vis != NULL)
	{
		audio_source = xine_get_audio_source (bvw->priv->stream);
		if (xine_post_wire_audio_port (audio_source,
					bvw->priv->vis->audio_input[0]))
			bvw->priv->using_vfx = TRUE;
	} else if (has_video == FALSE && show_visuals == FALSE) {
		audio_source = xine_get_audio_source (bvw->priv->stream);
		if (xine_post_wire_audio_port (audio_source,
					bvw->priv->ao_driver))
			bvw->priv->using_vfx = FALSE;
	}
}

static char *
get_fourcc_string (uint32_t f)
{
	char fcc[5];

	memset(&fcc, 0, sizeof(fcc));

	/* Should we take care about endianess ? */
	fcc[0] = f     | 0xFFFFFF00;
	fcc[1] = f>>8  | 0xFFFFFF00;
	fcc[2] = f>>16 | 0xFFFFFF00;
	fcc[3] = f>>24 | 0xFFFFFF00;
	fcc[4] = 0;

	if(f <= 0xFFFF)
		sprintf(fcc, "0x%x", f);

	if((fcc[0] == 'm') && (fcc[1] == 's'))
	{
		if((fcc[2] = 0x0) && (fcc[3] == 0x55))
			*(uint32_t *) fcc = 0x33706d2e; /* Force to '.mp3' */
	}

	return g_strdup (fcc);
}

static char *
bacon_video_widget_get_nice_codec_name (BaconVideoWidget *bvw,
		gboolean is_audio)
{
	char *name;
	int codec, fcc;

	if (is_audio == FALSE) {
		codec = XINE_META_INFO_VIDEOCODEC;
		fcc = XINE_STREAM_INFO_VIDEO_FOURCC;
	} else {
		codec = XINE_META_INFO_AUDIOCODEC;
		fcc = XINE_STREAM_INFO_AUDIO_FOURCC;
	}

	name = g_strdup (xine_get_meta_info (bvw->priv->stream, codec));

	if (name == NULL || name[0] == '\0')
	{
		guint32 fourcc;

		g_free (name);
		fourcc = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_VIDEO_FOURCC);
		name = get_fourcc_string (fourcc);
	}

	return name;
}

char *
bacon_video_widget_get_backend_name (BaconVideoWidget *bvw)
{	
	return g_strdup_printf ("xine-lib version %d.%d.%d",
			XINE_MAJOR_VERSION,
			XINE_MINOR_VERSION,
			XINE_SUB_VERSION);
}

static char *
bacon_video_widget_get_subtitled (const char *mrl, const char *subtitle_uri)
{
	g_return_val_if_fail (g_str_has_prefix (subtitle_uri, "file://"), NULL);
	return g_strdup_printf ("%s#subtitle:%s", mrl, subtitle_uri + strlen ("file://"));
}

gboolean
bacon_video_widget_open_with_subtitle (BaconVideoWidget *bvw, const char *mrl,
		const char *subtitle_uri, GError **error)
{
	int err;

	g_return_val_if_fail (mrl != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);
	g_return_val_if_fail (bvw->priv->mrl == NULL, FALSE);

	bvw->priv->got_redirect = FALSE;

	if (subtitle_uri != NULL)
	{
		char *subtitled;
		subtitled = bacon_video_widget_get_subtitled (mrl, subtitle_uri);
		err = xine_open (bvw->priv->stream, subtitled);
		g_free (subtitled);
	} else {
		err = xine_open (bvw->priv->stream, mrl);
	}

	if (err == 0)
	{
		bacon_video_widget_close (bvw);
		xine_error (bvw, error);
		return FALSE;
	}

	if (strcmp (xine_get_meta_info (bvw->priv->stream, XINE_META_INFO_SYSTEMLAYER), "MNG") == 0 && bvw->priv->logo_mode == FALSE)
	{
		bacon_video_widget_close (bvw);
		g_set_error (error, BVW_ERROR, BVW_ERROR_STILL_IMAGE,
				_("This movie is a still image. You can open it with an image viewer."));
		return FALSE;
	}

	if (xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_VIDEO_HANDLED) == FALSE
			|| (xine_get_stream_info (bvw->priv->stream,
					XINE_STREAM_INFO_HAS_VIDEO) == FALSE
				&& xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_AUDIO_HANDLED) == FALSE))
	{
		char *name;
		gboolean is_audio;

		is_audio = (xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_HAS_VIDEO) == FALSE);

		name = bacon_video_widget_get_nice_codec_name (bvw, is_audio);

		bacon_video_widget_close (bvw);

		if (is_audio == FALSE) {
			g_set_error (error, BVW_ERROR,
					BVW_ERROR_CODEC_NOT_HANDLED,
					_("Video codec '%s' is not handled. You might need to install additional plugins to be able to play some types of movies"), name);
		} else {
			g_set_error (error, BVW_ERROR,
					BVW_ERROR_CODEC_NOT_HANDLED,
					_("Audio codec '%s' is not handled. You might need to install additional plugins to be able to play some types of movies"), name);
		}

		g_free (name);

		return FALSE;
	}

	if (xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_HAS_VIDEO) == FALSE
		&& bvw->priv->type != BVW_USE_TYPE_METADATA
		&& bvw->priv->ao_driver == NULL)
	{
		bacon_video_widget_close (bvw);

		g_set_error (error, BVW_ERROR, BVW_ERROR_AUDIO_ONLY,
				_("This is an audio-only file, and there is no audio output available."));

		return FALSE;
	}

	show_vfx_update (bvw, bvw->priv->show_vfx);

	bvw->priv->mrl = g_strdup (mrl);

	g_signal_emit (G_OBJECT (bvw),
			bvw_table_signals[GOT_METADATA], 0, NULL);

	return TRUE;
}

gboolean
bacon_video_widget_play (BaconVideoWidget *bvw, GError **gerror)
{
	int error;

	g_return_val_if_fail (bvw != NULL, -1);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
	g_return_val_if_fail (bvw->priv->xine != NULL, -1);

	error = 1;

	if (bvw->priv->seeking == 1)
	{
		error = xine_play (bvw->priv->stream,
				bvw->priv->seek_dest * 65535, 0);
		bvw->priv->seeking = 0;
	} else if (bvw->priv->seeking == 2) {
		error = xine_play (bvw->priv->stream, 0,
				bvw->priv->seek_dest_time);
		bvw->priv->seeking = 0;
	} else {
		int speed, status;

		speed = xine_get_param (bvw->priv->stream, XINE_PARAM_SPEED);
		status = xine_get_status (bvw->priv->stream);
		if (speed != XINE_SPEED_NORMAL && status == XINE_STATUS_PLAY)
		{
			xine_set_param (bvw->priv->stream,
					XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
		} else {
			error = xine_play (bvw->priv->stream, 0, 0);
		}

		bvw->priv->seeking = 0;
	}

	if (error == 0)
	{
		xine_error (bvw, gerror);
		return FALSE;
	}

	if (bvw->priv->queued_vis != NULL)
	{
		bacon_video_widget_set_visuals (bvw, bvw->priv->queued_vis);
		g_free (bvw->priv->queued_vis);
		bvw->priv->queued_vis = NULL;
	}

	/* Workaround for xine-lib: don't try to use a
	 * non-existent audio channel */
	{
		int cur, num;

		cur = xine_get_param(bvw->priv->stream,
				XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
		num = xine_get_stream_info(bvw->priv->stream,
				XINE_STREAM_INFO_AUDIO_CHANNELS);
		if (cur > num)
			xine_set_param(bvw->priv->stream,
					XINE_PARAM_AUDIO_CHANNEL_LOGICAL, -1);
	}

	return TRUE;
}

gboolean bacon_video_widget_seek (BaconVideoWidget *bvw, float position,
		GError **gerror)
{
	int error, speed;

	g_return_val_if_fail (bvw != NULL, -1);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
	g_return_val_if_fail (bvw->priv->xine != NULL, -1);

	speed = xine_get_param (bvw->priv->stream, XINE_PARAM_SPEED);
	if (speed == XINE_SPEED_PAUSE)
	{
		bvw->priv->seeking = 1;
		bvw->priv->seek_dest = position;
		return TRUE;
	}

	error = xine_play (bvw->priv->stream, position * 65535, 0);

	if (error == 0)
	{
		xine_error (bvw, gerror);
		return FALSE;
	}

	return TRUE;
}

gboolean bacon_video_widget_seek_time (BaconVideoWidget *bvw, gint64 time,
		GError **gerror)
{
	int error, speed, status;
	gint64 length;

	g_return_val_if_fail (bvw != NULL, -1);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
	g_return_val_if_fail (bvw->priv->xine != NULL, -1);

	length = bacon_video_widget_get_stream_length (bvw);

	speed = xine_get_param (bvw->priv->stream, XINE_PARAM_SPEED);
	status = xine_get_status (bvw->priv->stream);
	if (speed == XINE_SPEED_PAUSE || status == XINE_STATUS_STOP)
	{
		bvw->priv->seeking = 2;
		bvw->priv->seek_dest_time = CLAMP (time, 0, length);
		return TRUE;
	}

	length = bacon_video_widget_get_stream_length (bvw);

	error = xine_play (bvw->priv->stream, 0, CLAMP (time, 0, length));

	if (error == 0)
	{
		xine_error (bvw, gerror);
		return FALSE;
	}

	return TRUE;
}

void
bacon_video_widget_stop (BaconVideoWidget *bvw)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	xine_stop (bvw->priv->stream);
}

void
bacon_video_widget_close (BaconVideoWidget *bvw)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	xine_stop (bvw->priv->stream);
	xine_close (bvw->priv->stream);
	g_free (bvw->priv->mrl);
	bvw->priv->mrl = NULL;

	if (bvw->priv->logo_mode == FALSE)
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[CHANNELS_CHANGE], 0, NULL);
}

/* Properties */
static void
bacon_video_widget_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec)
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
bacon_video_widget_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec)
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
		g_value_set_string (value, bvw->priv->mediadev);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

void
bacon_video_widget_set_logo_mode (BaconVideoWidget *bvw, gboolean logo_mode)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	bvw->priv->logo_mode = logo_mode;
}

void
bacon_video_widget_set_logo (BaconVideoWidget *bvw, char *filename)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET(bvw));
	g_return_if_fail (bvw->priv->xine != NULL);
	g_return_if_fail (filename != NULL);

	if (bacon_video_widget_open (bvw, filename, NULL) != FALSE) {
		bacon_video_widget_play (bvw, NULL);
	}
}

gboolean
bacon_video_widget_get_logo_mode (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	return bvw->priv->logo_mode;
}

void
bacon_video_widget_pause (BaconVideoWidget *bvw)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	xine_set_param (bvw->priv->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);

	if (bvw->priv->is_live != FALSE)
		xine_stop (bvw->priv->stream);

	/* Close the audio device when on pause */
	xine_set_param (bvw->priv->stream,
			XINE_PARAM_AUDIO_CLOSE_DEVICE, 1);
}

float
bacon_video_widget_get_position (BaconVideoWidget *bvw)
{
	int pos_stream = 0, i = 0;
	int pos_time, length_time;
	gboolean ret;

	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (bvw->priv->mrl == NULL)
		return 0;

	if (bacon_video_widget_is_playing (bvw) == FALSE)
		return 0;

	ret = xine_get_pos_length (bvw->priv->stream, &pos_stream,
			&pos_time, &length_time);

	while (ret == FALSE && i < 10)
	{
		usleep (100000);
		ret = xine_get_pos_length (bvw->priv->stream, &pos_stream,
				&pos_time, &length_time);
		i++;
	}

	if (bvw->priv->seeking == 1)
	{
		return bvw->priv->seek_dest * length_time;
	} else if (bvw->priv->seeking == 2) {
		return bvw->priv->seek_dest_time;
	}

	if (ret == FALSE)
		return -1;

	return pos_stream / 65535;
}

gboolean
bacon_video_widget_can_set_volume (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (bvw->priv->ao_driver == NULL || bvw->priv->ao_driver_none != FALSE)
		return FALSE;

	if (bvw->priv->audio_out_type == BVW_AUDIO_SOUND_AC3PASSTHRU)
		return FALSE;

	if (xine_get_param (bvw->priv->stream,
				XINE_PARAM_AUDIO_CHANNEL_LOGICAL) == -2)
		return FALSE;

	return TRUE;
}

void
bacon_video_widget_set_volume (BaconVideoWidget *bvw, int volume)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	if (bacon_video_widget_can_set_volume (bvw) != FALSE)
	{
		volume = CLAMP (volume, 0, 100);
		xine_set_param (bvw->priv->stream,
				XINE_PARAM_AUDIO_AMP_LEVEL, volume);
		bvw->priv->volume = volume;
	}
}

int
bacon_video_widget_get_volume (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (bacon_video_widget_can_set_volume (bvw) == FALSE)
		return 0;

	return bvw->priv->volume;
}

gboolean
bacon_video_widget_fullscreen_mode_available (BaconVideoWidget *bvw,
		TvOutType tvout) 
{
	switch(tvout) {
	case TV_OUT_NONE:
		/* Assume that ordinary fullscreen always works */
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
	case TV_OUT_DXR3:
		{
			const char * const *list;
			int i;

			list = xine_list_video_output_plugins (bvw->priv->xine);
			for (i = 0; list[i] != NULL; i++) {
				if (strcmp ("dxr3", list[i]) == 0)
					return TRUE;
			}
			return FALSE;
		}
	default:
		g_assert_not_reached ();
	}

	return FALSE;
}

void
bacon_video_widget_set_fullscreen (BaconVideoWidget *bvw, gboolean fullscreen)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

	if (bvw->priv->have_xvidmode == FALSE &&
			bvw->priv->tvout != TV_OUT_NVTV_NTSC &&
			bvw->priv->tvout != TV_OUT_NVTV_PAL)
		return;

	bvw->priv->fullscreen_mode = fullscreen;

	if (fullscreen == FALSE)
	{
#ifdef HAVE_NVTV
		/* If NVTV is used */
		if (nvtv_simple_get_state() == NVTV_SIMPLE_TV_ON) {
			nvtv_simple_switch(NVTV_SIMPLE_TV_OFF,0,0);
        
			/* Else if just auto resize is used */
		} else if (bvw->priv->auto_resize != FALSE) {
#endif
			bacon_restore ();
#ifdef HAVE_NVTV
		}
		/* Turn fullscreen on with NVTV if that option is on */
	} else if ((bvw->priv->tvout == TV_OUT_NVTV_NTSC) ||
			(bvw->priv->tvout == TV_OUT_NVTV_PAL)) {
		nvtv_simple_switch(NVTV_SIMPLE_TV_ON,
				bvw->priv->video_width,
				bvw->priv->video_height);
#endif
		/* Turn fullscreen on when we have xvidmode */
	} else if (bvw->priv->have_xvidmode != FALSE) {
		bacon_resize ();
	}
}

void
bacon_video_widget_set_show_cursor (BaconVideoWidget *bvw,
		gboolean show_cursor)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	if (show_cursor == FALSE)
	{
		eel_gdk_window_set_invisible_cursor (bvw->priv->video_window);
	} else {
		gdk_window_set_cursor (bvw->priv->video_window, NULL);
	}

	bvw->priv->cursor_shown = show_cursor;
}

gboolean
bacon_video_widget_get_show_cursor (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	return bvw->priv->cursor_shown;
}

void
bacon_video_widget_set_media_device (BaconVideoWidget *bvw, const char *path)
{
	xine_cfg_entry_t entry;

	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (path != NULL);

	g_free (bvw->priv->mediadev);

	/* DVD device */
	bvw_config_helper_string (bvw->priv->xine, "media.dvd.device",
			path, &entry);
	entry.str_value = (char *) path;
	xine_config_update_entry (bvw->priv->xine, &entry);

	/* VCD device */
	bvw_config_helper_string (bvw->priv->xine, "media.vcd.device",
			path, &entry);
	entry.str_value = (char *) path;
	xine_config_update_entry (bvw->priv->xine, &entry);

	/* VCD device for the new input plugin */
	bvw_config_helper_string (bvw->priv->xine, "media.vcd.device",
			path, &entry);
	entry.str_value = (char *) path;
	xine_config_update_entry (bvw->priv->xine, &entry);

	/* CDDA device */
	bvw_config_helper_string (bvw->priv->xine, "media.audio_cd.device",
			path, &entry);
	entry.str_value = (char *) path;
	xine_config_update_entry (bvw->priv->xine, &entry);

	bvw->priv->mediadev = g_strdup (path);
}

void
bacon_video_widget_set_proprietary_plugins_path (BaconVideoWidget *bvw,
		const char *path)
{
	xine_cfg_entry_t entry;

	bvw_config_helper_string (bvw->priv->xine,
			"decoder.external.win32_codecs_path", path, &entry);
	entry.str_value = (char *) path;
	xine_config_update_entry (bvw->priv->xine, &entry);

	bvw_config_helper_string (bvw->priv->xine,
			"decoder.external.real_codecs_path", path, &entry);
	entry.str_value = (char *) path;
	xine_config_update_entry (bvw->priv->xine, &entry);

	/* And we try and create symlinks from /usr/lib/win32 to
	 * the local user path */
	totem_create_symlinks ("/usr/lib/win32", path);
	totem_create_symlinks ("/usr/lib/RealPlayer9/Codecs/", path);
	totem_create_symlinks ("/usr/lib/RealPlayer9/users/Real/Codecs/", path);
	totem_create_symlinks ("/usr/lib/RealPlayer8/Codecs", path);

	g_free (bvw->priv->codecs_path);
	bvw->priv->codecs_path = g_strdup (path);
}

void
bacon_video_widget_set_connection_speed (BaconVideoWidget *bvw, int speed)
{
	xine_cfg_entry_t entry;

	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);
	g_return_if_fail (speed > 0 || speed < 10);

	xine_config_register_enum (bvw->priv->xine,
			"media.network.bandwidth",
			6,
			(char **) mms_bandwidth_strs,
			"Network bandwidth",
			NULL, 0, NULL, NULL);

	xine_config_lookup_entry (bvw->priv->xine,
			"media.network.bandwidth", &entry);
	entry.num_value = speed;
	xine_config_update_entry (bvw->priv->xine, &entry);
}

int
bacon_video_widget_get_connection_speed (BaconVideoWidget *bvw)
{
	xine_cfg_entry_t entry;

	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	xine_config_register_enum (bvw->priv->xine,
			"media.network.bandwidth",
			6,
			(char **) mms_bandwidth_strs,
			"Network bandwidth",
			NULL, 0, NULL, NULL);

	xine_config_lookup_entry (bvw->priv->xine,
			"media.network.bandwidth", &entry);

	return entry.num_value;
}

void
bacon_video_widget_set_deinterlacing (BaconVideoWidget *bvw,
		gboolean deinterlace)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	xine_set_param (bvw->priv->stream, XINE_PARAM_VO_DEINTERLACE,
			deinterlace);
}

gboolean
bacon_video_widget_get_deinterlacing (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	return xine_get_param (bvw->priv->stream, XINE_PARAM_VO_DEINTERLACE);
}

gboolean
bacon_video_widget_set_tv_out (BaconVideoWidget *bvw, TvOutType tvout)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	if (tvout == TV_OUT_DXR3 || bvw->priv->tvout == TV_OUT_DXR3)
	{
		xine_cfg_entry_t entry;

		xine_config_lookup_entry (bvw->priv->xine,
				"video.driver", &entry);
		entry.str_value = (tvout == TV_OUT_DXR3 ? "dxr3" : "auto");
		xine_config_update_entry (bvw->priv->xine, &entry);

		bvw->priv->tvout = tvout;

		return TRUE;
	}

#ifdef HAVE_NVTV
	if (tvout == TV_OUT_NVTV_PAL) {
		nvtv_simple_set_tvsystem(NVTV_SIMPLE_TVSYSTEM_PAL);
	} else if (tvout == TV_OUT_NVTV_NTSC) {
		nvtv_simple_set_tvsystem(NVTV_SIMPLE_TVSYSTEM_NTSC);
	}
#endif

	bvw->priv->tvout = tvout;

	return FALSE;
}

TvOutType
bacon_video_widget_get_tv_out (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	return bvw->priv->tvout;
}

gboolean
bacon_video_widget_set_show_visuals (BaconVideoWidget *bvw,
		gboolean show_visuals)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	bvw->priv->show_vfx = show_visuals;
	show_vfx_update (bvw, show_visuals);

	return TRUE;
}

GList *
bacon_video_widget_get_visuals_list (BaconVideoWidget *bvw)
{
	const char * const* plugins;
	int i;

	g_return_val_if_fail (bvw != NULL, NULL);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
	g_return_val_if_fail (bvw->priv->xine != NULL, NULL);

	if (bvw->priv->visuals != NULL)
		return bvw->priv->visuals;

	plugins = xine_list_post_plugins_typed (bvw->priv->xine,
			XINE_POST_TYPE_AUDIO_VISUALIZATION);

	for (i = 0; plugins[i] != NULL; i++)
	{
		bvw->priv->visuals = g_list_prepend
			(bvw->priv->visuals, g_strdup (plugins[i]));
	}

	bvw->priv->visuals = g_list_reverse (bvw->priv->visuals);

	return bvw->priv->visuals;
}

gboolean
bacon_video_widget_set_visuals (BaconVideoWidget *bvw, const char *name)
{
	xine_post_t *newvis;
	int speed;

	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	if (bvw->priv->type != BVW_USE_TYPE_VIDEO)
		return FALSE;

	if (GTK_WIDGET_REALIZED (bvw) == FALSE)
	{
		g_free (bvw->priv->vis_name);
		bvw->priv->vis_name = g_strdup (name);
		return FALSE;
	}

	speed = xine_get_param (bvw->priv->stream, XINE_PARAM_SPEED);
	if (speed == XINE_SPEED_PAUSE && bvw->priv->using_vfx != FALSE)
	{
		g_free (bvw->priv->queued_vis);
		if (strcmp (name, bvw->priv->vis_name) == 0)
		{
			bvw->priv->queued_vis = NULL;
		} else {
			bvw->priv->queued_vis = g_strdup (name);
		}
		return FALSE;
	}

	newvis = xine_post_init (bvw->priv->xine,
			name, 0,
			&bvw->priv->ao_driver, &bvw->priv->vo_driver);

	if (newvis != NULL)
	{
		g_free (bvw->priv->vis_name);
		bvw->priv->vis_name = g_strdup (name);

		if (bvw->priv->vis != NULL)
		{
			xine_post_t *oldvis;

			oldvis = bvw->priv->vis;
			bvw->priv->vis = newvis;

			if (bvw->priv->using_vfx != FALSE)
			{
				show_vfx_update (bvw, FALSE);
				show_vfx_update (bvw, TRUE);
			}
			xine_post_dispose (bvw->priv->xine, oldvis);
		} else {
			bvw->priv->vis = newvis;
		}
	}

	return FALSE;
}

void
bacon_video_widget_set_visuals_quality (BaconVideoWidget *bvw,
		VisualsQuality quality)
{
	xine_cfg_entry_t entry;
	int fps, w, h;

	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

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

	bvw_config_helper_num (bvw->priv->xine, "effects.goom.fps", fps, &entry);
	entry.num_value = fps;
	xine_config_update_entry (bvw->priv->xine, &entry);

	bvw_config_helper_num (bvw->priv->xine, "effects.goom.width", w, &entry);
	entry.num_value = w;
	xine_config_update_entry (bvw->priv->xine, &entry);

	bvw_config_helper_num (bvw->priv->xine, "effects.goom.height", h, &entry);
	entry.num_value = h;
	xine_config_update_entry (bvw->priv->xine, &entry);
}

gboolean
bacon_video_widget_get_auto_resize (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	return bvw->priv->auto_resize;
}

void
bacon_video_widget_set_auto_resize (BaconVideoWidget *bvw,
		gboolean auto_resize)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	bvw->priv->auto_resize = auto_resize;
}

gint64
bacon_video_widget_get_current_time (BaconVideoWidget *bvw)
{
	int pos_time = 0, i = 0;
	int pos_stream, length_time;
	int status;
	gboolean ret;

	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	status = xine_get_status (bvw->priv->stream);
	if (status != XINE_STATUS_STOP && status != XINE_STATUS_PLAY)
		return 0;

	ret = xine_get_pos_length (bvw->priv->stream, &pos_stream,
			&pos_time, &length_time);

	while (ret == FALSE && i < 10)
	{
		usleep (100000);
		ret = xine_get_pos_length (bvw->priv->stream, &pos_stream,
				&pos_time, &length_time);
		i++;
	}

	if (bvw->priv->seeking == 1)
	{
		return bvw->priv->seek_dest * length_time;
	} else if (bvw->priv->seeking == 2) {
		return bvw->priv->seek_dest_time;
	}

	if (ret == FALSE)
		return -1;

	return pos_time;
}

gint64
bacon_video_widget_get_stream_length (BaconVideoWidget *bvw)
{
	int length_time = 0;
	int pos_stream, pos_time;

	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (bvw->priv->mrl == NULL)
		return 0;

	xine_get_pos_length (bvw->priv->stream, &pos_stream,
			&pos_time, &length_time);

	return length_time;
}

gboolean
bacon_video_widget_is_playing (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (bvw->priv->stream == NULL)
		return FALSE;

	return (xine_get_status (bvw->priv->stream) == XINE_STATUS_PLAY && xine_get_param (bvw->priv->stream, XINE_PARAM_SPEED) == XINE_SPEED_NORMAL);
}

gboolean
bacon_video_widget_is_seekable (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (bvw->priv->mrl == NULL)
		return FALSE;

	if (bacon_video_widget_get_stream_length (bvw) == 0)
		return FALSE;

	return xine_get_stream_info (bvw->priv->stream,
			XINE_STREAM_INFO_SEEKABLE);
}

gboolean
bacon_video_widget_can_play (BaconVideoWidget *bvw, MediaType type)
{
	switch (type)
	{
	case MEDIA_TYPE_DVD:
		return bvw->priv->can_dvd;
	case MEDIA_TYPE_VCD:
		return bvw->priv->can_vcd;
	case MEDIA_TYPE_CDDA:
		return bvw->priv->can_cdda;
	default:
		return FALSE;
	}
}

char
**bacon_video_widget_get_mrls (BaconVideoWidget *bvw, MediaType type)
{
	char *plugin_id;
	int num_mrls;

	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (type == MEDIA_TYPE_DVD)
		plugin_id = "DVD";
	else if (type == MEDIA_TYPE_VCD)
		plugin_id = "VCD";
	else if (type == MEDIA_TYPE_CDDA)
		plugin_id = "CD";
	else
		return NULL;

	return g_strdupv (xine_get_autoplay_mrls
		(bvw->priv->xine, plugin_id, &num_mrls));
}

void
bacon_video_widget_set_subtitle_font (BaconVideoWidget *bvw, const char *font)
{
	//FIXME
}

void
bacon_video_widget_set_video_device (BaconVideoWidget *bvw, const char *path)
{
	xine_cfg_entry_t entry;

	bvw_config_helper_string (bvw->priv->xine,
			"media.video4linux.video_device", path, &entry);
	entry.str_value = (char *) path;
	xine_config_update_entry (bvw->priv->xine, &entry);
}

void
bacon_video_widget_set_aspect_ratio (BaconVideoWidget *bvw,
		BaconVideoWidgetAspectRatio ratio)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	xine_set_param (bvw->priv->stream, XINE_PARAM_VO_ASPECT_RATIO, ratio);
}

BaconVideoWidgetAspectRatio
bacon_video_widget_get_aspect_ratio (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	return xine_get_param (bvw->priv->stream, XINE_PARAM_VO_ASPECT_RATIO);
}

void
bacon_video_widget_set_scale_ratio (BaconVideoWidget *bvw, gfloat ratio)
{
	GtkWidget *toplevel, *widget;
	int new_w, new_h, win_w, win_h;

	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);
	g_return_if_fail (ratio >= 0);

	if (bvw->priv->fullscreen_mode != FALSE)
		return;

	/* Try best fit for the screen */
	if (ratio == 0)
	{
		if (totem_ratio_fits_screen (bvw->priv->video_window, bvw->priv->video_width, bvw->priv->video_height, 2) != FALSE)
		{
			ratio = 2;
		} else if (totem_ratio_fits_screen (bvw->priv->video_window, bvw->priv->video_width, bvw->priv->video_height, 1)
				!= FALSE) {
			ratio = 1;
		} else if (totem_ratio_fits_screen (bvw->priv->video_window, bvw->priv->video_width, bvw->priv->video_height, 0.5)
				!= FALSE) {
			ratio = 0.5;
		} else {
			return;
		}
	} else {
		/* don't scale to something bigger than the screen, and leave
		 * us some room */
		if (totem_ratio_fits_screen (bvw->priv->video_window, bvw->priv->video_width, bvw->priv->video_height, ratio) == FALSE)
			return;
	}

	widget = GTK_WIDGET (bvw);

	toplevel = gtk_widget_get_toplevel (widget);

	/* Get the size of the toplevel window */
	gdk_drawable_get_size (GDK_DRAWABLE (toplevel->window),
			&win_w, &win_h);

	/* Calculate the new size of the window, depending on the size of the
	 * video widget, and the new size of the video */
	new_w = win_w - widget->allocation.width +
		bvw->priv->video_width * ratio;
	new_h = win_h - widget->allocation.height +
		bvw->priv->video_height * ratio;

	/* Change the minimum size of the widget
	 * but only if we're getting a smaller window */
	if (new_w < widget->allocation.width
			|| new_h < widget->allocation.height)
	{
		gtk_widget_set_size_request (widget,
				bvw->priv->video_width * ratio,
				bvw->priv->video_height * ratio);
	}

	gtk_window_resize (GTK_WINDOW (toplevel), 1, 1);
	totem_widget_set_preferred_size (toplevel, new_w, new_h);
}

gboolean
bacon_video_widget_can_set_zoom (BaconVideoWidget *bvw)
{
	return TRUE;
}

void
bacon_video_widget_set_zoom (BaconVideoWidget *bvw, int zoom)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);
	g_return_if_fail (zoom >= 0 && zoom <= 400);

	xine_set_param (bvw->priv->stream,
			XINE_PARAM_VO_ZOOM_X, zoom);
	xine_set_param (bvw->priv->stream,
			XINE_PARAM_VO_ZOOM_Y, zoom);
}

int
bacon_video_widget_get_zoom (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 100);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 100);
	g_return_val_if_fail (bvw->priv->xine != NULL, 100);

	return xine_get_param (bvw->priv->stream, XINE_PARAM_VO_ZOOM_X);
}

int
bacon_video_widget_get_video_property (BaconVideoWidget *bvw,
		BaconVideoWidgetVideoProperty type)
{
	g_return_val_if_fail (bvw != NULL, 65535 / 2);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 65535 / 2);
	g_return_val_if_fail (bvw->priv->xine != NULL, 65535 / 2);

	return xine_get_param (bvw->priv->stream, video_props[type]);
}

void
bacon_video_widget_set_video_property (BaconVideoWidget *bvw,
		BaconVideoWidgetVideoProperty type, int value)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	if ( !(value < 65535 && value > 0) )
		return;

	xine_set_param (bvw->priv->stream, video_props[type], value);
	gconf_client_set_int (bvw->priv->gc, video_props_str[type], value, NULL);
}

BaconVideoWidgetAudioOutType
bacon_video_widget_get_audio_out_type (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, BVW_AUDIO_SOUND_STEREO);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw),
			BVW_AUDIO_SOUND_STEREO);
	g_return_val_if_fail (bvw->priv->xine != NULL, BVW_AUDIO_SOUND_STEREO);

	return gconf_client_get_int (bvw->priv->gc,
			GCONF_PREFIX"/audio_output_type", NULL);
}

gboolean
bacon_video_widget_set_audio_out_type (BaconVideoWidget *bvw,
		BaconVideoWidgetAudioOutType type)
{
	xine_cfg_entry_t entry;
	int value;
	gboolean need_restart = FALSE;

	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	if (type == bvw->priv->audio_out_type)
		return FALSE;

	xine_config_register_enum (bvw->priv->xine,
			"audio.output.speaker_arrangement",
			1,
			(char **) audio_out_types_strs,
			"Speaker arrangement",
			NULL, 0, NULL, NULL); 

	gconf_client_set_int (bvw->priv->gc,
			GCONF_PREFIX"/audio_output_type",
			type, NULL);

	switch (type) {
	case BVW_AUDIO_SOUND_STEREO:
		value = 1;
		break;
	case BVW_AUDIO_SOUND_4CHANNEL:
		value = 5;
		break;
	case BVW_AUDIO_SOUND_41CHANNEL:
		value = 6;
		break;
	case BVW_AUDIO_SOUND_5CHANNEL:
		value = 7;
		break;
	case BVW_AUDIO_SOUND_51CHANNEL:
		value = 8;
		break;
	case BVW_AUDIO_SOUND_AC3PASSTHRU:
		value = 12;
		need_restart = TRUE;
		break;
	default:
		value = 1;
		g_warning ("Unsupported audio type %d selected", type);
	}

	xine_config_lookup_entry (bvw->priv->xine,
			"audio.output.speaker_arrangement", &entry);
	entry.num_value = value;
	xine_config_update_entry (bvw->priv->xine, &entry);

	return need_restart;
}

static void
bacon_video_widget_get_metadata_string (BaconVideoWidget *bvw, BaconVideoWidgetMetadataType type,
		GValue *value)
{
	const char *string = NULL;

	g_value_init (value, G_TYPE_STRING);

	if (bvw->priv->stream == NULL)
	{
		g_value_set_string (value, string);
		return;
	}

	switch (type)
	{
	case BVW_INFO_TITLE:
		string = xine_get_meta_info (bvw->priv->stream,
				XINE_META_INFO_TITLE);
		break;
	case BVW_INFO_ARTIST:
		string = xine_get_meta_info (bvw->priv->stream,
				XINE_META_INFO_ARTIST);
		break;
	case BVW_INFO_ALBUM:
		string = xine_get_meta_info (bvw->priv->stream,
				XINE_META_INFO_ALBUM);
		break;
	case BVW_INFO_YEAR:
		string = xine_get_meta_info (bvw->priv->stream,
				XINE_META_INFO_YEAR);
		break;
	case BVW_INFO_VIDEO_CODEC:
		string = xine_get_meta_info (bvw->priv->stream,
				XINE_META_INFO_VIDEOCODEC);
		break;
	case BVW_INFO_AUDIO_CODEC:
		string = xine_get_meta_info (bvw->priv->stream,
				XINE_META_INFO_AUDIOCODEC);
		break;
	case BVW_INFO_CDINDEX:
		string = xine_get_meta_info (bvw->priv->stream,
				XINE_META_INFO_CDINDEX_DISCID);
		break;
	default:
		g_assert_not_reached ();
	}

	if (string != NULL && string[0] == '\0')
		string = NULL;

	if (string != NULL)
	{
		if (g_utf8_validate (string, -1, NULL) == FALSE)
		{
			char *utf8;

			g_warning ("Metadata for index %d not in UTF-8 for mrl '%s'", type, bvw->priv->mrl);
			utf8 = g_locale_to_utf8 (string, -1, NULL, NULL, NULL);
			g_value_set_string (value, utf8);
			g_free (utf8);
			return;
		}
	}

	g_value_set_string (value, string);

	return;
}

static void
bacon_video_widget_get_metadata_int (BaconVideoWidget *bvw,
		BaconVideoWidgetMetadataType type, GValue *value)
{
	int integer = 0;

	g_value_init (value, G_TYPE_INT);

	if (bvw->priv->stream == NULL)
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
		integer = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_VIDEO_WIDTH);
		break;
	case BVW_INFO_DIMENSION_Y:
		integer = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_VIDEO_HEIGHT);
		break;
	case BVW_INFO_FPS:
		if (xine_get_stream_info (bvw->priv->stream,
					XINE_STREAM_INFO_FRAME_DURATION) != 0)
		{
			integer = 90000 / xine_get_stream_info
				(bvw->priv->stream,
				 XINE_STREAM_INFO_FRAME_DURATION);
		} else {
			integer = 0;
		}
		break;
	 case BVW_INFO_AUDIO_BITRATE:
		integer = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_AUDIO_BITRATE) / 1000;
		break;
	 case BVW_INFO_VIDEO_BITRATE:
		integer = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_VIDEO_BITRATE) / 1000;
		break;
	 default:
		g_assert_not_reached ();
	 }

	 g_value_set_int (value, integer);

	 return;
}

static void
bacon_video_widget_get_metadata_bool (BaconVideoWidget *bvw,
		BaconVideoWidgetMetadataType type, GValue *value)
{
	gboolean boolean = FALSE;

	g_value_init (value, G_TYPE_BOOLEAN);

	if (bvw->priv->stream == NULL)
	{
		g_value_set_boolean (value, FALSE);
		return;
	}

	switch (type)
	{
	case BVW_INFO_HAS_VIDEO:
		boolean = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_HAS_VIDEO);
		break;
	case BVW_INFO_HAS_AUDIO:
		boolean = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_HAS_AUDIO);
		break;
	default:
		g_assert_not_reached ();
	}

	g_value_set_boolean (value, boolean);

	return;
}

void
bacon_video_widget_get_metadata (BaconVideoWidget *bvw,
		BaconVideoWidgetMetadataType type, GValue *value)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);
	g_return_if_fail (value != NULL);

	switch (type)
	{
	case BVW_INFO_TITLE:
	case BVW_INFO_ARTIST:
	case BVW_INFO_ALBUM:
	case BVW_INFO_YEAR:
	case BVW_INFO_VIDEO_CODEC:
	case BVW_INFO_AUDIO_CODEC:
	case BVW_INFO_CDINDEX:
		bacon_video_widget_get_metadata_string (bvw, type, value);
		break;
	case BVW_INFO_DURATION:
	case BVW_INFO_DIMENSION_X:
	case BVW_INFO_DIMENSION_Y:
	case BVW_INFO_FPS:
	case BVW_INFO_AUDIO_BITRATE:
	case BVW_INFO_VIDEO_BITRATE:
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

GList
*bacon_video_widget_get_languages (BaconVideoWidget *bvw)
{
	GList *list = NULL;
	int i, num_channels;
	char lang[XINE_LANG_MAX];

	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
	g_return_val_if_fail (bvw->priv->stream != NULL, NULL);

	if (bvw->priv->mrl == NULL)
		return NULL;

	num_channels = xine_get_stream_info
		(bvw->priv->stream, XINE_STREAM_INFO_MAX_AUDIO_CHANNEL);

	if (num_channels < 2)
		return NULL;

	for(i = 0; i < num_channels; i++)
	{
		memset (&lang, 0, sizeof (lang));

		if (xine_get_audio_lang(bvw->priv->stream, i, lang) == 1)
		{
			char *nospace = lang;

			while (g_ascii_isspace (*nospace) != FALSE)
				nospace++;

			list = g_list_prepend (list,
					(gpointer) g_strdup (nospace));
		} else {
			/* An unnamed language, for example 'Language 2' */
			list = g_list_prepend (list,
					(gpointer) g_strdup_printf
					(_("Language %d"), i + 1));
		}
	}

	return g_list_reverse (list);
}

int
bacon_video_widget_get_language (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
	g_return_val_if_fail (bvw->priv->stream != NULL, -1);

	return xine_get_param (bvw->priv->stream,
			XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
}

void
bacon_video_widget_set_language (BaconVideoWidget *bvw, int language)
{
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->stream != NULL);

	xine_set_param (bvw->priv->stream,
			XINE_PARAM_AUDIO_CHANNEL_LOGICAL, language);
}

GList
*bacon_video_widget_get_subtitles (BaconVideoWidget *bvw)
{
	GList *list = NULL;
	int i, num_channels;
	char lang[XINE_LANG_MAX];

	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
	g_return_val_if_fail (bvw->priv->stream != NULL, NULL);

	if (bvw->priv->mrl == NULL)
		return NULL;

	num_channels = xine_get_stream_info
		(bvw->priv->stream, XINE_STREAM_INFO_MAX_SPU_CHANNEL);

	if (num_channels < 2)
		return NULL;

	for(i = 0; i < num_channels; i++)
	{
		memset (&lang, 0, sizeof (lang));

		if (xine_get_spu_lang (bvw->priv->stream, i, lang) == 1)
		{
			char *nospace = lang;

			while (g_ascii_isspace (*nospace) != FALSE)
				nospace++;

			list = g_list_prepend (list,
					(gpointer) g_strdup (nospace));
		} else {
			/* An unnamed language, for example 'Language 2' */
			list = g_list_prepend (list,
					(gpointer) g_strdup_printf
					(_("Language %d"), i + 1));
		}
	}

	return g_list_reverse (list);
}

int
bacon_video_widget_get_subtitle (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -2);
	g_return_val_if_fail (bvw->priv->stream != NULL, -2);

	return xine_get_param (bvw->priv->stream, XINE_PARAM_SPU_CHANNEL);
}

void
bacon_video_widget_set_subtitle (BaconVideoWidget *bvw, int subtitle)
{
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->stream != NULL);

	xine_set_param (bvw->priv->stream, XINE_PARAM_SPU_CHANNEL, subtitle);
}

gboolean
bacon_video_widget_can_get_frames (BaconVideoWidget *bvw, GError **error)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	if (xine_get_status (bvw->priv->stream) != XINE_STATUS_PLAY
			&& bvw->priv->logo_mode == FALSE)
	{
		g_set_error (error, BVW_ERROR, BVW_ERROR_CANNOT_CAPTURE,
				_("Movie is not playing."));
		return FALSE;
	}

	if (xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_HAS_VIDEO) == FALSE
			&& bvw->priv->using_vfx == FALSE)
	{
		g_set_error (error, BVW_ERROR, BVW_ERROR_CANNOT_CAPTURE,
				_("No video to capture."));
		return FALSE;
	}

	if (xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_VIDEO_HANDLED) == FALSE)
	{
		g_set_error (error, BVW_ERROR, BVW_ERROR_CANNOT_CAPTURE,
				_("Video codec is not handled."));
		return FALSE;
	}

	return TRUE;
}

GdkPixbuf *
bacon_video_widget_get_current_frame (BaconVideoWidget *bvw)
{
	GdkPixbuf *pixbuf = NULL;
	uint8_t *yuv, *y, *u, *v, *rgb;
	int width, height, ratio, format;

	g_return_val_if_fail (bvw != NULL, NULL);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
	g_return_val_if_fail (bvw->priv->xine != NULL, NULL);

	if (xine_get_current_frame (bvw->priv->stream, &width, &height,
			&ratio, &format, NULL) == 0)
		return NULL;

	if (width == 0 || height == 0)
		return NULL;

	yuv = g_malloc ((width + 8) * (height + 1) * 2);
	if (yuv == NULL)
		return NULL;

	if (xine_get_current_frame (bvw->priv->stream, &width, &height,
			&ratio, &format, yuv) == 0)
	{
		g_free (yuv);
		return NULL;
	}

	/* Convert to yv12 */
	switch (format) {
	case XINE_IMGFMT_YUY2:
		{
			uint8_t *yuy2 = yuv;

			yuv = g_malloc (width * height * 2);
			y = yuv;
			u = yuv + width * height;
			v = yuv + width * height * 5 / 4;

			yuy2toyv12 (y, u, v, yuy2, width, height);

			g_free (yuy2);
		}
		break;
	case XINE_IMGFMT_YV12:
		y = yuv;
		u = yuv + width * height;
		v = yuv + width * height * 5 / 4;
		break;
	default:
		g_warning ("Format '%.4s' unsupported", (char *) &format);
		g_free (yuv);
		return NULL;
	}

	/* Convert to rgb */
	rgb = yv12torgb (y, u, v, width, height);

	pixbuf = gdk_pixbuf_new_from_data (rgb,
			GDK_COLORSPACE_RGB, FALSE,
			8, width, height, 3 * width,
			(GdkPixbufDestroyNotify) g_free, NULL);

	/* MPEG streams have ratio information */
	ratio = xine_get_stream_info (bvw->priv->stream,
			XINE_STREAM_INFO_VIDEO_RATIO);

	if (ratio != 10000.0 && ratio != 0.0)
	{
		GdkPixbuf *tmp;

		if (ratio > 10000.0)
			tmp = gdk_pixbuf_scale_simple (pixbuf,
					(int) (height * ratio / 10000), height,
					GDK_INTERP_BILINEAR);
		else
			tmp = gdk_pixbuf_scale_simple (pixbuf,
					width, (int) (width * ratio / 10000),
					GDK_INTERP_BILINEAR);

		gdk_pixbuf_unref (pixbuf);

		return tmp;
	}

	return pixbuf;
}
