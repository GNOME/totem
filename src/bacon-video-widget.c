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
 * the xine engine in a widget - implementation
 */

#include <config.h>

/* system */
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
/* X11 */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
/* gtk+/gnome */
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
/* xine */
#include <xine.h>

#include "debug.h"
#include "bacon-video-widget.h"
#include "baconvideowidget-marshal.h"
#include "scrsaver.h"
#include "video-utils.h"

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
	RATIO,
	PROGRESS,
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

static int speeds[2] = {
	XINE_SPEED_PAUSE,
	XINE_SPEED_NORMAL,
};

struct BaconVideoWidgetPrivate {
	/* Xine stuff */
	xine_t *xine;
	xine_stream_t *stream;
	xine_vo_driver_t *vo_driver;
	xine_ao_driver_t *ao_driver;
	xine_event_queue_t *ev_queue;
	double display_ratio;

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
	xine_post_t *vis;

	/* Other stuff */
	int xpos, ypos;
	gboolean can_dvd, can_vcd, can_cdda;
	gboolean logo_mode;
	guint tick_id;
	gboolean auto_resize;
	GdkPixbuf *icon;

	GAsyncQueue *queue;
	int video_width, video_height;

	/* fullscreen stuff */
	gboolean fullscreen_mode;
	GdkWindow *fullscreen_window;
	GdkRectangle fullscreen_rect;
	gboolean cursor_shown;
	gboolean pml;
};

static void bacon_video_widget_class_init (BaconVideoWidgetClass *klass);
static void bacon_video_widget_instance_init (BaconVideoWidget *bvw);

static void setup_config (BaconVideoWidget *bvw);

static void bacon_video_widget_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec);
static void bacon_video_widget_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec);

static void bacon_video_widget_realize (GtkWidget *widget);
static void bacon_video_widget_unrealize (GtkWidget *widget);
static void bacon_video_widget_finalize (GObject *object);

static gboolean bacon_video_widget_expose (GtkWidget *widget,
		GdkEventExpose *event);
static gboolean bacon_video_widget_motion_notify (GtkWidget *widget,
		GdkEventMotion *event);
static gboolean bacon_video_widget_button_press (GtkWidget *widget,
		GdkEventButton *event);
static gboolean bacon_video_widget_key_press (GtkWidget *widget,
		GdkEventKey *event);

static void bacon_video_widget_size_allocate (GtkWidget *widget,
		GtkAllocation *allocation);
static xine_vo_driver_t * load_video_out_driver (BaconVideoWidget *bvw,
		gboolean null_out);
static xine_ao_driver_t * load_audio_out_driver (BaconVideoWidget *bvw,
		GError **error);
static gboolean bacon_video_widget_tick_send (BaconVideoWidget *bvw);

static GtkWidgetClass *parent_class = NULL;

static void xine_event (void *user_data, const xine_event_t *event);
static gboolean bacon_video_widget_idle_signal (BaconVideoWidget *bvw);
static void size_changed_cb (GdkScreen *screen, gpointer user_data);

static int bvw_table_signals[LAST_SIGNAL] = { 0 };

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

		bacon_video_widget_type = g_type_register_static (GTK_TYPE_WIDGET,
				"BaconVideoWidget", &bacon_video_widget_info, (GTypeFlags)0);
	}

	return bacon_video_widget_type;
}

static void
bacon_video_widget_class_init (BaconVideoWidgetClass *klass)
{

	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_widget_get_type ());

	/* GtkWidget */
	widget_class->realize = bacon_video_widget_realize;
	widget_class->unrealize = bacon_video_widget_unrealize;
	widget_class->size_allocate = bacon_video_widget_size_allocate;
	widget_class->expose_event = bacon_video_widget_expose;
	widget_class->motion_notify_event = bacon_video_widget_motion_notify;
	widget_class->button_press_event = bacon_video_widget_button_press;
	widget_class->key_press_event = bacon_video_widget_key_press;

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
				G_STRUCT_OFFSET (BaconVideoWidgetClass, title_change),
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

	if (!g_thread_supported ())
		g_thread_init (NULL);
	gdk_threads_init ();
}

static void
bacon_video_widget_instance_init (BaconVideoWidget *bvw)
{
	GtkWidget *widget = (GtkWidget *) bvw;
	const char *const *autoplug_list;
	int i = 0;

	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (bvw), GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (bvw), GTK_DOUBLE_BUFFERED);

	/* Set the default size to be a 4:3 ratio */
	widget->requisition.width = DEFAULT_HEIGHT;
	widget->requisition.height = DEFAULT_WIDTH;

	bvw->priv = g_new0 (BaconVideoWidgetPrivate, 1);
	bvw->priv->xine = xine_new ();
	bvw->priv->cursor_shown = TRUE;

	bvw->priv->queue = g_async_queue_new ();

	/* init configuration  */
	setup_config (bvw);

	xine_init (bvw->priv->xine);

	/* Can we play DVDs and VCDs ? */
	autoplug_list = xine_get_autoplay_input_plugin_ids (bvw->priv->xine);
	while (autoplug_list && autoplug_list[i])
	{
		if (g_ascii_strcasecmp (autoplug_list[i], "VCD") == 0)
			bvw->priv->can_vcd = TRUE;
		else if (g_ascii_strcasecmp (autoplug_list[i], "DVD") == 0)
			bvw->priv->can_dvd = TRUE;
		else if (g_ascii_strcasecmp (autoplug_list[i], "CDDA") == 0)
			bvw->priv->can_cdda = TRUE;
		i++;
	}

	bvw->priv->tick_id = g_timeout_add (200,
			(GSourceFunc) bacon_video_widget_tick_send, bvw);

	bvw->priv->icon = gdk_pixbuf_new_from_file (ICON_PATH, NULL);
}

static void
bacon_video_widget_finalize (GObject *object)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) object;

	if (bvw->priv->icon != NULL)
		gdk_pixbuf_unref (bvw->priv->icon);

	/* Should put here what needs to be destroyed */
	g_idle_remove_by_data (bvw);
	g_async_queue_unref (bvw->priv->queue);
	G_OBJECT_CLASS (parent_class)->finalize (object);

	bvw->priv = NULL;
	bvw = NULL;
}

static void
dest_size_cb (void *bvw_gen,
	      int video_width, int video_height,
	      double video_pixel_aspect,
	      int *dest_width, int *dest_height,
	      double *dest_pixel_aspect)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) bvw_gen;

	/* correct size with video_pixel_aspect */
	if (video_pixel_aspect >= bvw->priv->display_ratio)
		video_width  = video_width * video_pixel_aspect
			/ bvw->priv->display_ratio + .5;
	else
		video_height = video_height * bvw->priv->display_ratio
			/ video_pixel_aspect + .5;

	if (bvw->priv->fullscreen_mode)
	{
		*dest_width = bvw->priv->fullscreen_rect.width;
		*dest_height = bvw->priv->fullscreen_rect.height;
	} else {
		*dest_width = bvw->widget.allocation.width;
		*dest_height = bvw->widget.allocation.height;
	}

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
		video_width = video_width * video_pixel_aspect
			/ bvw->priv->display_ratio + .5;
	else
		video_height = video_height * bvw->priv->display_ratio
			/ video_pixel_aspect + .5;

	*dest_x = 0;
	*dest_y = 0;
	*win_x = bvw->priv->xpos;
	*win_y = bvw->priv->ypos;

	if (bvw->priv->fullscreen_mode)
	{
		*dest_width = bvw->priv->fullscreen_rect.width;
		*dest_height = bvw->priv->fullscreen_rect.height;
	} else {
		*dest_width = bvw->widget.allocation.width;
		*dest_height = bvw->widget.allocation.height;

		/* Size changed */
		if (bvw->priv->video_width != video_width
				|| bvw->priv->video_height != video_height)
		{
			bvw->priv->video_width = video_width;
			bvw->priv->video_height = video_height;

			if (bvw->priv->auto_resize == TRUE
					&& bvw->priv->logo_mode == FALSE)
			{
				g_async_queue_push (bvw->priv->queue,
						GINT_TO_POINTER (RATIO));
				g_idle_add ((GSourceFunc)
						bacon_video_widget_idle_signal,
						bvw);
			}
		}
	}

	*dest_pixel_aspect = bvw->priv->display_ratio;
}

static xine_vo_driver_t *
load_video_out_driver (BaconVideoWidget *bvw, gboolean null_out)
{
	double res_h, res_v;
	x11_visual_t vis;
	const char *video_driver_id;
	xine_vo_driver_t *vo_driver;

	if (null_out == TRUE)
	{
		return xine_open_video_driver (bvw->priv->xine,
				"none", XINE_VISUAL_TYPE_NONE, NULL);
	}

	vis.display = bvw->priv->display;
	vis.screen = bvw->priv->screen;
	vis.d = GDK_WINDOW_XID (bvw->priv->video_window);
	res_h =
	    (DisplayWidth (bvw->priv->display, bvw->priv->screen) * 1000 /
	     DisplayWidthMM (bvw->priv->display, bvw->priv->screen));
	res_v =
	    (DisplayHeight (bvw->priv->display, bvw->priv->screen) * 1000 /
	     DisplayHeightMM (bvw->priv->display, bvw->priv->screen));
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

	if (strcmp (video_driver_id, "auto") != 0)
	{
		vo_driver = xine_open_video_driver (bvw->priv->xine,
						   video_driver_id,
						   XINE_VISUAL_TYPE_X11,
						   (void *) &vis);
		if (vo_driver)
			return vo_driver;
	}

	vo_driver = xine_open_video_driver (bvw->priv->xine, NULL,
			XINE_VISUAL_TYPE_X11, (void *) &vis);

	return vo_driver;
}

static xine_ao_driver_t *
load_audio_out_driver (BaconVideoWidget *bvw, GError **err)
{
	xine_ao_driver_t *ao_driver;
	const char *audio_driver_id;

	if (bvw->priv->null_out == TRUE)
		return NULL;

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

	if (ao_driver == NULL)
	{
		g_set_error (err, 0, 0,
				_("Couldn't load the '%s' audio driver\n"
					"Check that the device is not busy."),
				audio_driver_id ? audio_driver_id : "auto" );
		return NULL;
	}

	return ao_driver;
}

static void
update_fullscreen_size (BaconVideoWidget *bvw)
{
	gdk_screen_get_monitor_geometry (gdk_screen_get_default (),
			gdk_screen_get_monitor_at_window
			(gdk_screen_get_default (),
			 bvw->priv->video_window),
			&bvw->priv->fullscreen_rect);
}

static void
size_changed_cb (GdkScreen *screen, gpointer user_data)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) bvw;
	update_fullscreen_size (bvw);
	if (bvw->priv->fullscreen_mode)
	{
		gdk_window_resize (bvw->priv->fullscreen_window,
				bvw->priv->fullscreen_rect.width,
				bvw->priv->fullscreen_rect.height);
	}
}

static void
setup_config (BaconVideoWidget *bvw)
{
	char *configfile;
	xine_cfg_entry_t entry;
	char *demux_strategies[] = {"default", "reverse", "content",
		"extension", NULL};

	configfile = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), CONFIG_FILE, NULL);
	xine_config_load (bvw->priv->xine, configfile);
	g_free (configfile);

	/* default demux strategy */
	xine_config_register_enum (bvw->priv->xine,
			"misc.demux_strategy",
			0,
			demux_strategies,
			 "media format detection strategy",
			 NULL, 10, NULL, NULL);

	xine_config_lookup_entry (bvw->priv->xine,
			"misc.demux_strategy", &entry);
	entry.num_value = 0;
	xine_config_update_entry (bvw->priv->xine, &entry);
}

static gboolean
video_window_translate_point (BaconVideoWidget *bvw, int gui_x, int gui_y,
		int *video_x, int *video_y)
{
	x11_rectangle_t rect;

	rect.x = gui_x;
	rect.y = gui_y;
	rect.w = 0;
	rect.h = 0;

	if (xine_gui_send_vo_data (bvw->priv->stream,
				XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO,
				(void*)&rect) != -1)
	{
		/* the driver implements gui->video coordinate space translation
		 * so we use it */
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
                        "input.dvd_skip_behaviour",
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
generate_mouse_event (BaconVideoWidget *bvw, GdkEvent *event, gboolean is_motion)
{
	GdkEventMotion *mevent = (GdkEventMotion *) event;
	GdkEventButton *bevent = (GdkEventButton *) event;
	int x, y;
	gboolean retval;

	if (is_motion == FALSE && bevent->button != 1)
		return FALSE;

	if (is_motion == TRUE)
		retval = video_window_translate_point (bvw,
				mevent->x, mevent->y, &x, &y);
	else
		retval = video_window_translate_point (bvw,
				bevent->x, bevent->y, &x, &y);

	if (retval == TRUE)
	{
		xine_event_t event;
		xine_input_data_t input;

		if (is_motion == TRUE)
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
configure_cb (GtkWidget *widget, GdkEventConfigure *event, gpointer user_data)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) user_data;

	bvw->priv->xpos = event->x;
	bvw->priv->ypos = event->y;

	return FALSE;
}

static void
bacon_video_widget_realize (GtkWidget *widget)
{
	GdkWindowAttr attr;
	BaconVideoWidget *bvw;

	bvw = BACON_VIDEO_WIDGET (widget);

	/* set realized flag */
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	/* Close the old stream */
	xine_close (bvw->priv->stream);
	xine_event_dispose_queue (bvw->priv->ev_queue);
	xine_dispose (bvw->priv->stream);
	xine_close_video_driver(bvw->priv->xine, bvw->priv->vo_driver);

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

	/* track configure events of toplevel window */
	g_signal_connect (G_OBJECT (gtk_widget_get_toplevel (widget)),
			"configure-event",
			G_CALLBACK (configure_cb), bvw);

	scrsaver_init (GDK_DISPLAY ());

	/* Now onto the video out driver */
	bvw->priv->display = XOpenDisplay (gdk_display_get_name
			(gdk_display_get_default ()));
	bvw->priv->screen = DefaultScreen (bvw->priv->display);

	bvw->priv->vo_driver = load_video_out_driver (bvw, bvw->priv->null_out);

	g_assert (bvw->priv->vo_driver != NULL);

	if (bvw->priv->null_out == FALSE)
		bvw->priv->vis = xine_post_init (bvw->priv->xine, "goom", 0,
				&bvw->priv->ao_driver, &bvw->priv->vo_driver);

	bvw->priv->stream = xine_stream_new (bvw->priv->xine,
			bvw->priv->ao_driver, bvw->priv->vo_driver);
	bvw->priv->ev_queue = xine_event_new_queue (bvw->priv->stream);

	/* Setup xine events */
	xine_event_create_listener_thread (bvw->priv->ev_queue,
			xine_event, (void *) bvw);

	/* Setup the default screen stuff */
	update_fullscreen_size (bvw);
	g_signal_connect (G_OBJECT (gdk_screen_get_default ()),
			"size-changed", G_CALLBACK (size_changed_cb), bvw);

	return;
}

static gboolean
bacon_video_widget_idle_signal (BaconVideoWidget *bvw)
{
	int queue_length;
	char *i;

	i = g_async_queue_try_pop (bvw->priv->queue);
	if (i == NULL)
		return FALSE;

	TE ();

	switch (GPOINTER_TO_INT (i))
	{
	case RATIO:
		bacon_video_widget_set_scale_ratio (bvw, 0);
		queue_length = g_async_queue_length (bvw->priv->queue);
		break;
	case PROGRESS:
		while (gtk_events_pending ())
			gtk_main_iteration ();
		//FIXME
		break;
	}

	TL ();

	return (queue_length > 0);
}

static void
xine_event (void *user_data, const xine_event_t *event)
{
	BaconVideoWidget *bvw = (BaconVideoWidget *) user_data;
	xine_ui_data_t *ui_data;
	xine_progress_data_t *prg;
	xine_mrl_reference_data_t *ref;

	switch (event->type)
	{
	case XINE_EVENT_UI_PLAYBACK_FINISHED:
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[EOS], 0, NULL);
		break;
	case XINE_EVENT_UI_SET_TITLE:
		ui_data = event->data;
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[TITLE_CHANGE],
				0, ui_data->str);
		break;
	case XINE_EVENT_PROGRESS:
		prg = event->data;

		g_async_queue_push (bvw->priv->queue,
				GINT_TO_POINTER (PROGRESS));
		g_idle_add ((GSourceFunc) bacon_video_widget_idle_signal, bvw);

		g_message ("pct: %d msg: %s", prg->percent, prg->description);
		break;
	case XINE_EVENT_MRL_REFERENCE:
		ref = event->data;

		g_message ("ref mrl detected: %s", ref->mrl);
		break;
	}
}

static void
xine_error (BaconVideoWidget *bvw, GError **error)
{
	int err;

	err = xine_get_error (bvw->priv->stream);
	if (err == XINE_ERROR_NONE)
		return;

	switch (err)
	{
	case XINE_ERROR_NO_INPUT_PLUGIN:
	case XINE_ERROR_NO_DEMUX_PLUGIN:
		g_set_error (error, 0, 0, _("There is no plugin to handle this"
					" movie"));
		break;
	case XINE_ERROR_DEMUX_FAILED:
		g_set_error (error, 0, 0, _("This movie is broken and can not "
					"be played further"));
		break;
	case XINE_ERROR_MALFORMED_MRL:
		g_set_error (error, 0, 0, _("This location is not "
					"a valid one"));
		break;
	default:
		g_set_error (error, 0, 0, _("Generic Error"));
		break;
	}
}

static void
bacon_video_widget_unrealize (GtkWidget *widget)
{
	BaconVideoWidget *bvw;
	char *configfile;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (widget));

	/* Hide all windows */
	if (GTK_WIDGET_MAPPED (widget))
		gtk_widget_unmap (widget);
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	bvw = BACON_VIDEO_WIDGET (widget);

	/* stop the playback */
	xine_close (bvw->priv->stream);

	/* Get rid of the rest of the stream */
	xine_event_dispose_queue (bvw->priv->ev_queue);
	xine_dispose (bvw->priv->stream);
	bvw->priv->stream = NULL;

	/* Kill the drivers */
	if (bvw->priv->vo_driver != NULL)
		xine_close_video_driver (bvw->priv->xine, bvw->priv->vo_driver);
	if (bvw->priv->ao_driver != NULL)
		xine_close_audio_driver (bvw->priv->xine, bvw->priv->ao_driver);

	/* save config */
	configfile = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), CONFIG_FILE, NULL);
	xine_config_save (bvw->priv->xine, configfile);
	g_free (configfile);

	/* stop event thread */
	xine_exit (bvw->priv->xine);
	bvw->priv->xine = NULL;

	/* Finally, kill the left-over windows */
	if (bvw->priv->fullscreen_window != NULL)
		gdk_window_destroy (bvw->priv->fullscreen_window);

	/* This destroys widget->window and unsets the realized flag */
	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

GtkWidget *
bacon_video_widget_new (int width, int height, gboolean null_out, GError **err)
{
	BaconVideoWidget *bvw;

	bvw = BACON_VIDEO_WIDGET (g_object_new
			(bacon_video_widget_get_type (), NULL));

	bvw->priv->null_out = null_out;

	/* defaults are fine if both are negative */
	if (width > 0 && height > 0)
	{
		/* figure out the missing measure from the other one
		 * with a 4:3 ratio */
		if (width <= 0)
			width = (int) (height * 4 / 3);
		if (height <= 0)
			height = (int) (width * 3 / 4);
	} else {
		width = 0;
		height = 0;
	}

	bvw->widget.requisition.width = width;
	bvw->widget.requisition.height = height;

	/* load the video drivers */
	bvw->priv->ao_driver = load_audio_out_driver (bvw, err);
	if (*err != NULL)
	{
		//FIXME
		return NULL;
	}

	bvw->priv->vo_driver = load_video_out_driver (bvw, TRUE);

	bvw->priv->stream = xine_stream_new (bvw->priv->xine,
			bvw->priv->ao_driver, bvw->priv->vo_driver);
	bvw->priv->ev_queue = xine_event_new_queue (bvw->priv->stream);

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

	xine_gui_send_vo_data (bvw->priv->stream,
			XINE_GUI_SEND_EXPOSE_EVENT,
			expose);

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

	if (generate_mouse_event (bvw, (GdkEvent *)event, FALSE) == TRUE)
		return FALSE;

	if (GTK_WIDGET_CLASS (parent_class)->button_press_event != NULL)
		                (* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event);

	return FALSE;
}

static gboolean
bacon_video_widget_key_press (GtkWidget *widget, GdkEventKey *event)
{
	if (GTK_WIDGET_CLASS (parent_class)->key_press_event != NULL)
		(* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event);

	return FALSE;
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
	gboolean ret = TRUE;

	if (bvw->priv->stream == NULL)
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

	if (ret == TRUE)
		g_signal_emit (G_OBJECT (bvw),
				bvw_table_signals[TICK], 0,
				current_time, stream_length, current_position);

	return TRUE;
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

gboolean
bacon_video_widget_open (BaconVideoWidget *bvw, const gchar *mrl,
		GError **gerror)
{
	int error;
	gboolean has_video;
	xine_post_out_t *audio_source;

	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (mrl != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);
	g_return_val_if_fail (bvw->priv->mrl == NULL, FALSE);

	bvw->priv->mrl = g_strdup (mrl);

	error = xine_open (bvw->priv->stream, mrl);
	if (error == 0)
	{
		bacon_video_widget_close (bvw);
		xine_error (bvw, gerror);
		return FALSE;
	}

	if (xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_VIDEO_HANDLED) == FALSE
			|| (xine_get_stream_info (bvw->priv->stream,
					XINE_STREAM_INFO_HAS_VIDEO) == FALSE
				&& xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_AUDIO_HANDLED) == FALSE))
	{
		uint32_t fourcc;
		char *fourcc_str, *name;

		fourcc = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_VIDEO_FOURCC);
		fourcc_str = get_fourcc_string (fourcc);
		name = g_strdup (xine_get_meta_info (bvw->priv->stream,
				XINE_META_INFO_VIDEOCODEC));

		bacon_video_widget_close (bvw);

		g_set_error (gerror, 0, 0,
				_("Video type '%s' is not handled"),
				name ? name : fourcc_str);

		g_free (fourcc_str);
		g_free (name);

		return FALSE;
	}

	has_video = xine_get_stream_info(bvw->priv->stream,
			XINE_STREAM_INFO_HAS_VIDEO);

	if (has_video == TRUE && bvw->priv->using_vfx == TRUE)
	{
		audio_source = xine_get_audio_source (bvw->priv->stream);
		if (xine_post_wire_audio_port (audio_source,
					bvw->priv->ao_driver))
			bvw->priv->using_vfx = FALSE;
	} else if (has_video == FALSE && bvw->priv->show_vfx == TRUE
			&& bvw->priv->using_vfx == FALSE
			&& bvw->priv->vis != NULL)
	{
		audio_source = xine_get_audio_source (bvw->priv->stream);
		if (xine_post_wire_audio_port (audio_source,
					bvw->priv->vis->audio_input[0]))
			bvw->priv->using_vfx = TRUE;
	}

	return TRUE;
}

gboolean
bacon_video_widget_play (BaconVideoWidget *bvw, guint pos,
		guint start_time, GError **gerror)
{
	int error, length;

	g_return_val_if_fail (bvw != NULL, -1);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), -1);
	g_return_val_if_fail (bvw->priv->xine != NULL, -1);

	length = bacon_video_widget_get_stream_length (bvw);

	error = xine_play (bvw->priv->stream, pos,
			CLAMP (start_time, 0, length));

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

	if (bacon_video_widget_is_playing (bvw) == FALSE)
		return;

	xine_stop (bvw->priv->stream);
}

void
bacon_video_widget_close (BaconVideoWidget *bvw)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	xine_close (bvw->priv->stream);
	g_free (bvw->priv->mrl);
	bvw->priv->mrl = NULL;
}

/* Properties */
static void
bacon_video_widget_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec)
{
	BaconVideoWidget *bvw;

	g_return_if_fail (BACON_IS_VIDEO_WIDGET (object));

	bvw = BACON_VIDEO_WIDGET (object);

	switch (property_id)
	{
	case PROP_LOGO_MODE:
		bacon_video_widget_set_logo_mode (bvw, g_value_get_boolean (value));
		break;
	case PROP_FULLSCREEN:
		bacon_video_widget_set_fullscreen (bvw, g_value_get_boolean (value));
		break;
	case PROP_SPEED:
		bacon_video_widget_set_speed (bvw, g_value_get_int (value));
		break;
	case PROP_SHOWCURSOR:
		bacon_video_widget_set_show_cursor (bvw, g_value_get_boolean (value));
		break;
	case PROP_MEDIADEV:
		bacon_video_widget_set_media_device (bvw, g_value_get_string (value));
		break;
	case PROP_SHOW_VISUALS:
		bacon_video_widget_set_show_visuals (bvw, g_value_get_boolean (value));
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

	g_return_if_fail (BACON_IS_VIDEO_WIDGET (object));

	bvw = BACON_VIDEO_WIDGET (object);

	switch (property_id)
	{
	case PROP_LOGO_MODE:
		g_value_set_boolean (value, bacon_video_widget_get_logo_mode (bvw));
		break;
	case PROP_FULLSCREEN:
		g_value_set_boolean (value, bacon_video_widget_is_fullscreen (bvw));
		break;
	case PROP_SPEED:
		g_value_set_int (value, bacon_video_widget_get_speed (bvw));
		break;
	case PROP_POSITION:
		g_value_set_int (value, bacon_video_widget_get_position (bvw));
		break;
	case PROP_STREAM_LENGTH:
		g_value_set_int (value, bacon_video_widget_get_stream_length (bvw));
		break;
	case PROP_PLAYING:
		g_value_set_boolean (value, bacon_video_widget_is_playing (bvw));
		break;
	case PROP_SEEKABLE:
		g_value_set_boolean (value, bacon_video_widget_is_seekable (bvw));
		break;
	case PROP_SHOWCURSOR:
		g_value_set_boolean (value, bacon_video_widget_get_show_cursor (bvw));
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

gboolean
bacon_video_widget_get_logo_mode (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	return bvw->priv->logo_mode;
}

void
bacon_video_widget_set_speed (BaconVideoWidget *bvw, Speeds speed)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	xine_set_param (bvw->priv->stream, XINE_PARAM_SPEED, speeds[speed]);
}

int
bacon_video_widget_get_speed (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, SPEED_NORMAL);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), SPEED_NORMAL);
	g_return_val_if_fail (bvw->priv->xine != NULL, SPEED_NORMAL);

	return xine_get_param (bvw->priv->stream, XINE_PARAM_SPEED);
}

int
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

	if (ret == FALSE)
		return -1;

	return pos_stream;
}

void
bacon_video_widget_set_fullscreen (BaconVideoWidget *bvw, gboolean fullscreen)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	if (bvw->priv->pml == FALSE)
		bvw->priv->pml = TRUE;
	else
		return;

	if (fullscreen == bvw->priv->fullscreen_mode)
		return;

	bvw->priv->fullscreen_mode = fullscreen;

	if (fullscreen)
	{
		GdkWindow *parent;
		GdkWindowAttr attr;

		parent = gdk_window_get_toplevel (bvw->widget.window);

		update_fullscreen_size (bvw);

		attr.x = bvw->priv->fullscreen_rect.x;
		attr.y = bvw->priv->fullscreen_rect.y;
		attr.width = bvw->priv->fullscreen_rect.width;
		attr.height = bvw->priv->fullscreen_rect.height;
		attr.window_type = GDK_WINDOW_TOPLEVEL;
		attr.wclass = GDK_INPUT_OUTPUT;
		attr.event_mask = gtk_widget_get_events (GTK_WIDGET (bvw))
			| GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK
			| GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK;
		bvw->priv->fullscreen_window = gdk_window_new
			(NULL, &attr, GDK_WA_X | GDK_WA_Y);
		gdk_window_show (bvw->priv->fullscreen_window);
		gdk_window_fullscreen (bvw->priv->fullscreen_window);
		/* Flush, so that the window is really shown */
		gdk_flush ();

		gdk_window_set_user_data (bvw->priv->fullscreen_window, bvw);

		xine_gui_send_vo_data (bvw->priv->stream,
			 XINE_GUI_SEND_DRAWABLE_CHANGED,
			 (void*) GDK_WINDOW_XID (bvw->priv->fullscreen_window));

		/* switch off mouse cursor */
		bacon_video_widget_set_show_cursor (bvw, FALSE);

		scrsaver_disable (bvw->priv->display);

		/* Set the icon and the name */
		if (bvw->priv->icon != NULL)
		{
			GList *list = NULL;

			list = g_list_append (list, bvw->priv->icon);
			gdk_window_set_icon_list (bvw->priv->fullscreen_window,
					list);
			g_list_free (list);
		}
		gdk_window_set_title (bvw->priv->fullscreen_window,
				DEFAULT_TITLE);
	} else {
		gdk_window_set_user_data (bvw->widget.window, bvw);

		xine_gui_send_vo_data (bvw->priv->stream,
			 XINE_GUI_SEND_DRAWABLE_CHANGED,
			 (void *) GDK_WINDOW_XID (bvw->priv->video_window));

		/* Hide the window */
		gdk_window_destroy (bvw->priv->fullscreen_window);
		bvw->priv->fullscreen_window = NULL;

		scrsaver_enable (bvw->priv->display);

		gdk_window_focus (gdk_window_get_toplevel
				(gtk_widget_get_parent_window
				 (GTK_WIDGET (bvw))), GDK_CURRENT_TIME);
	}

	bvw->priv->pml = FALSE;
}

gboolean
bacon_video_widget_is_fullscreen (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);

	return bvw->priv->fullscreen_mode;
}

gboolean
bacon_video_widget_can_set_volume (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (xine_get_param (bvw->priv->stream, XINE_PARAM_AUDIO_VOLUME) == -1)
		return FALSE;

	if (xine_get_param (bvw->priv->stream,
				XINE_PARAM_AUDIO_CHANNEL_LOGICAL) == -2)
		return FALSE;

	return xine_get_stream_info (bvw->priv->stream,
			XINE_STREAM_INFO_HAS_AUDIO);
}

void
bacon_video_widget_set_volume (BaconVideoWidget *bvw, int volume)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	if (bacon_video_widget_can_set_volume (bvw) == TRUE)
	{
		volume = CLAMP (volume, 0, 100);
		xine_set_param (bvw->priv->stream, XINE_PARAM_AUDIO_VOLUME,
				volume);
	}
}

int
bacon_video_widget_get_volume (BaconVideoWidget *bvw)
{
	int volume = 0;

	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (bacon_video_widget_can_set_volume (bvw) == FALSE)
		return 0;

	volume = xine_get_param (bvw->priv->stream,
			XINE_PARAM_AUDIO_VOLUME);

	return volume;
}

void
bacon_video_widget_set_show_cursor (BaconVideoWidget *bvw, gboolean show_cursor)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	if (GDK_IS_WINDOW (bvw->priv->fullscreen_window) == FALSE)
		return;

	if (show_cursor == FALSE)
	{
		eel_gdk_window_set_invisible_cursor
			(bvw->priv->fullscreen_window);
	} else {
		gdk_window_set_cursor (bvw->priv->fullscreen_window, NULL);
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

	/* DVD device */
	xine_config_register_string (bvw->priv->xine,
			"input.dvd_device", path,
			"device used for dvd drive",
			NULL, 10, NULL, NULL);
	xine_config_lookup_entry (bvw->priv->xine,
			"input.dvd_device", &entry);
	entry.str_value = g_strdup (path);
	xine_config_update_entry (bvw->priv->xine, &entry);

	/* VCD device */
	xine_config_register_string (bvw->priv->xine,
			"input.vcd_device", path,
			"device used for cdrom drive",
			NULL, 10, NULL, NULL);
	xine_config_lookup_entry (bvw->priv->xine,
			"input.vcd_device", &entry);
	entry.str_value = g_strdup (path);
	xine_config_update_entry (bvw->priv->xine, &entry);
}

void
bacon_video_widget_set_show_visuals (BaconVideoWidget *bvw, gboolean show_visuals)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	bvw->priv->show_vfx = show_visuals;
}

void
bacon_video_widget_set_auto_resize (BaconVideoWidget *bvw, gboolean auto_resize)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	bvw->priv->auto_resize = auto_resize;
}

int
bacon_video_widget_get_current_time (BaconVideoWidget *bvw)
{
	int pos_time = 0, i = 0;
	int pos_stream, length_time;
	gboolean ret;

	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

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

	if (ret == FALSE)
		return -1;

	return pos_time;
}

int
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

	return xine_get_status (bvw->priv->stream) == XINE_STATUS_PLAY;
}

gboolean
bacon_video_widget_is_seekable (BaconVideoWidget *bvw)
{
	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

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
	case MEDIA_DVD:
		return bvw->priv->can_dvd;
	case MEDIA_VCD:
		return bvw->priv->can_vcd;
	case MEDIA_CDDA:
		return bvw->priv->can_cdda;
	default:
		return FALSE;
	}
}

G_CONST_RETURN gchar
**bacon_video_widget_get_mrls (BaconVideoWidget *bvw, MediaType type)
{
	char *plugin_id;
	int num_mrls;

	g_return_val_if_fail (bvw != NULL, 0);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), 0);
	g_return_val_if_fail (bvw->priv->xine != NULL, 0);

	if (type == MEDIA_DVD)
		plugin_id = "DVD";
	else if (type == MEDIA_VCD)
		plugin_id = "VCD";
	else if (type == MEDIA_CDDA)
		plugin_id = "CDDA";
	else
		return NULL;

	return (G_CONST_RETURN gchar **) xine_get_autoplay_mrls
		(bvw->priv->xine, plugin_id, &num_mrls);
}

void
bacon_video_widget_toggle_aspect_ratio (BaconVideoWidget *bvw)
{
	int tmp;

	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

	tmp = xine_get_param (bvw->priv->stream, XINE_PARAM_VO_ASPECT_RATIO);
	xine_set_param (bvw->priv->stream, XINE_PARAM_VO_ASPECT_RATIO, tmp + 1);
}

static gboolean
bacon_video_widget_ratio_fits_screen (BaconVideoWidget *bvw, gfloat ratio)
{
	int new_w, new_h;

	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	new_w = bvw->priv->video_width * ratio;
	new_h = bvw->priv->video_height * ratio;

	update_fullscreen_size (bvw);

	if (new_w > (bvw->priv->fullscreen_rect.width - 128) ||
			new_h > (bvw->priv->fullscreen_rect.height - 128))
	{
		return FALSE;
	}

	return TRUE;
}

void
bacon_video_widget_set_scale_ratio (BaconVideoWidget *bvw, gfloat ratio)
{
	GtkWindow *toplevel;
	int new_w, new_h;

	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);
	g_return_if_fail (ratio >= 0);

	if (bvw->priv->fullscreen_mode == TRUE)
		return;

	/* Try best fit for the screen */
	if (ratio == 0)
	{
		if (bacon_video_widget_ratio_fits_screen (bvw, 2) == TRUE)
			ratio = 2;
		else if (bacon_video_widget_ratio_fits_screen (bvw, 1) == TRUE)
			ratio = 1;
		else if (bacon_video_widget_ratio_fits_screen (bvw, 0.5) == TRUE)
			ratio = 0.5;
		else
			return;
	} else {
		/* don't scale to something bigger than the screen, and leave
		 * us some room */
		if (bacon_video_widget_ratio_fits_screen (bvw, ratio) == FALSE)
			return;
	}

	new_w = bvw->priv->video_width * ratio;
	new_h = bvw->priv->video_height * ratio;

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (bvw)));

	gtk_window_set_resizable (toplevel, FALSE);
	bvw->widget.allocation.width = new_w;
	bvw->widget.allocation.height = new_h;
	gtk_widget_set_size_request (gtk_widget_get_parent (GTK_WIDGET (bvw)),
			new_w, new_h);
	gtk_widget_queue_resize (gtk_widget_get_parent (GTK_WIDGET (bvw)));
	while (gtk_events_pending ())
		gtk_main_iteration ();
	gtk_window_set_resizable (toplevel, TRUE);
}

static void
bacon_video_widget_get_metadata_string (BaconVideoWidget *bvw, BaconVideoWidgetMetadataType type,
		GValue *value)
{
	const char *string = NULL;

	g_value_init (value, G_TYPE_STRING);

	if (bvw->priv->stream == NULL)
	{
		g_value_set_string (value, "");
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
	default:
		g_assert_not_reached ();
	}

	g_value_set_string (value, string);

	return;
}

static void
bacon_video_widget_get_metadata_int (BaconVideoWidget *bvw, BaconVideoWidgetMetadataType type,
		GValue *value)
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
	 case BVW_INFO_BITRATE:
		integer = xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_AUDIO_BITRATE) / 1000;
		break;
	 default:
		g_assert_not_reached ();
	 }

	 g_value_set_int (value, integer);

	 return;
}

static void
bacon_video_widget_get_metadata_bool (BaconVideoWidget *bvw, BaconVideoWidgetMetadataType type,
		GValue *value)
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
bacon_video_widget_get_metadata (BaconVideoWidget *bvw, BaconVideoWidgetMetadataType type, GValue *value)
{
	g_return_if_fail (bvw != NULL);
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (bvw));
	g_return_if_fail (bvw->priv->xine != NULL);

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

char
*bacon_video_widget_properties_get_title (BaconVideoWidget *bvw)
{
	const char *short_title, *artist;

	artist = xine_get_meta_info (bvw->priv->stream, XINE_META_INFO_ARTIST);
	short_title = xine_get_meta_info (bvw->priv->stream,
			XINE_META_INFO_TITLE);

	if (artist == NULL && short_title == NULL)
		return NULL;

	if (artist == NULL && short_title != NULL)
		return g_strdup (short_title);

	if (artist != NULL && short_title != NULL)
		return g_strdup_printf ("%s - %s", artist, short_title);

	return NULL;
}

/*
 *  For screen shot. Nicked from pornview which is in turn nicked from xine-ui.
 */

#define PIXSZ 3

static guchar *bacon_video_widget_get_current_frame_rgb (BaconVideoWidget *bvw,
		int *width_ret, int * height_ret);

gboolean
bacon_video_widget_can_get_frames (BaconVideoWidget *bvw, GError **error)
{
	g_return_val_if_fail (bvw != NULL, FALSE);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), FALSE);
	g_return_val_if_fail (bvw->priv->xine != NULL, FALSE);

	if (bacon_video_widget_is_playing (bvw) == FALSE)
	{
		g_set_error (error, 0, 0, _("Movie is not playing"));
		return FALSE;
	}

	if (xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_HAS_VIDEO) == FALSE)
	{
		g_set_error (error, 0, 0, bvw->priv->using_vfx ?
				_("Can't capture visual effects")
				: _("No video to capture"));
		return FALSE;
	}

	if (xine_get_stream_info (bvw->priv->stream,
				XINE_STREAM_INFO_VIDEO_HANDLED) == FALSE)
	{
		g_set_error (error, 0, 0, _("Video codec is not handled"));
		return FALSE;
	}

	return TRUE;
}

GdkPixbuf *
bacon_video_widget_get_current_frame (BaconVideoWidget *bvw)
{
	guchar *pixels;
	int width, height;
	float ratio;
	GdkPixbuf *pixbuf = NULL;

	g_return_val_if_fail (bvw != NULL, NULL);
	g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
	g_return_val_if_fail (bvw->priv->xine != NULL, NULL);

	pixels = bacon_video_widget_get_current_frame_rgb
		(bvw, &width, &height);

	if (pixels == NULL)
		return NULL;

	pixbuf = gdk_pixbuf_new_from_data (pixels,
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

/* internal function use to scale yuv data */
typedef void (*scale_line_func_t) (uint8_t * source, uint8_t * dest,
		int width, int step);

/* Holdall structure */
struct prvt_image_s
{
    int     width;
    int     height;
    int     ratio_code;
    int     format;
    uint8_t *y, *u, *v, *yuy2;
    uint8_t *img;

    int     u_width, v_width;
    int     u_height, v_height;

    scale_line_func_t scale_line;
    unsigned long scale_factor;
};

static guchar *xine_frame_to_rgb (struct prvt_image_s *image);

static guchar *
bacon_video_widget_get_current_frame_rgb (BaconVideoWidget * bvw, int * width_ret,
				int * height_ret)
{
    int    err = 0;
    struct prvt_image_s *image;
    guchar *rgb = NULL;
    int    width, height;

    g_return_val_if_fail (bvw, NULL);
    g_return_val_if_fail (BACON_IS_VIDEO_WIDGET (bvw), NULL);
    g_return_val_if_fail (bvw->priv->xine, NULL);
    g_return_val_if_fail (bvw->priv->stream != NULL, 0);
    g_return_val_if_fail (width_ret && height_ret, NULL);

    image = g_new0 (struct prvt_image_s, 1);
    if (!image)
    {
	*width_ret = 0;
	*height_ret = 0;

	return NULL;
    }

    image->y = image->u = image->v = image->yuy2 = image->img = NULL;

    width = xine_get_stream_info (bvw->priv->stream,
		    XINE_STREAM_INFO_VIDEO_WIDTH);
    height = xine_get_stream_info (bvw->priv->stream,
		    XINE_STREAM_INFO_VIDEO_HEIGHT);

    image->img = g_malloc (width * height * 2);

    if (!image->img)
    {
	*width_ret = 0;
	*height_ret = 0;

	g_free (image);
	return NULL;
    }

    err = xine_get_current_frame (bvw->priv->stream,
				  &image->width, &image->height,
				  &image->ratio_code,
				  &image->format, image->img);

    if (err == 0)
    {
	*width_ret = 0;
	*height_ret = 0;

	g_free (image->img);
	g_free (image);

	return NULL;
    }

    /*
     * the dxr3 driver does not allocate yuv buffers 
     */
    /*
     * image->u and image->v are always 0 for YUY2 
     */
    if (!image->img)
    {
	*width_ret = 0;
	*height_ret = 0;

	g_free (image->img);
	g_free (image);

	return NULL;
    }

    rgb = xine_frame_to_rgb (image);
    *width_ret = image->width;
    *height_ret = image->height;

    g_free (image->img);
    g_free (image);

    return rgb;
}

/******************************************************************************
 *
 *   Private functions for snap shot.
 *
 *   These codes are mostly taken from xine-ui.
 *
******************************************************************************/

/*
 * Scale line with no horizontal scaling. For NTSC mpeg2 dvd input in
 * 4:3 output format (720x480 -> 720x540)
 */
static void
scale_line_1_1 (guchar * source, guchar * dest, int width, int step)
{
    memcpy (dest, source, width);
}

/*
 * Interpolates 64 output pixels from 45 source pixels using shifts.
 * Useful for scaling a PAL mpeg2 dvd input source to 1024x768
 * fullscreen resolution, or to 16:9 format on a monitor using square
 * pixels.
 * (720 x 576 ==> 1024 x 576)
 */
/* uuuuum */
static void
scale_line_45_64 (guchar * source, guchar * dest, int width, int step)
{
    int    p1, p2;

    while ((width -= 64) >= 0)
    {
	p1 = source[0];
	p2 = source[1];
	dest[0] = p1;
	dest[1] = (1 * p1 + 3 * p2) >> 2;
	p1 = source[2];
	dest[2] = (5 * p2 + 3 * p1) >> 3;
	p2 = source[3];
	dest[3] = (7 * p1 + 1 * p2) >> 3;
	dest[4] = (1 * p1 + 3 * p2) >> 2;
	p1 = source[4];
	dest[5] = (1 * p2 + 1 * p1) >> 1;
	p2 = source[5];
	dest[6] = (3 * p1 + 1 * p2) >> 2;
	dest[7] = (1 * p1 + 7 * p2) >> 3;
	p1 = source[6];
	dest[8] = (3 * p2 + 5 * p1) >> 3;
	p2 = source[7];
	dest[9] = (5 * p1 + 3 * p2) >> 3;
	p1 = source[8];
	dest[10] = p2;
	dest[11] = (1 * p2 + 3 * p1) >> 2;
	p2 = source[9];
	dest[12] = (5 * p1 + 3 * p2) >> 3;
	p1 = source[10];
	dest[13] = (7 * p2 + 1 * p1) >> 3;
	dest[14] = (1 * p2 + 7 * p1) >> 3;
	p2 = source[11];
	dest[15] = (1 * p1 + 1 * p2) >> 1;
	p1 = source[12];
	dest[16] = (3 * p2 + 1 * p1) >> 2;
	dest[17] = p1;
	p2 = source[13];
	dest[18] = (3 * p1 + 5 * p2) >> 3;
	p1 = source[14];
	dest[19] = (5 * p2 + 3 * p1) >> 3;
	p2 = source[15];
	dest[20] = p1;
	dest[21] = (1 * p1 + 3 * p2) >> 2;
	p1 = source[16];
	dest[22] = (1 * p2 + 1 * p1) >> 1;
	p2 = source[17];
	dest[23] = (7 * p1 + 1 * p2) >> 3;
	dest[24] = (1 * p1 + 7 * p2) >> 3;
	p1 = source[18];
	dest[25] = (3 * p2 + 5 * p1) >> 3;
	p2 = source[19];
	dest[26] = (3 * p1 + 1 * p2) >> 2;
	dest[27] = p2;
	p1 = source[20];
	dest[28] = (3 * p2 + 5 * p1) >> 3;
	p2 = source[21];
	dest[29] = (5 * p1 + 3 * p2) >> 3;
	p1 = source[22];
	dest[30] = (7 * p2 + 1 * p1) >> 3;
	dest[31] = (1 * p2 + 3 * p1) >> 2;
	p2 = source[23];
	dest[32] = (1 * p1 + 1 * p2) >> 1;
	p1 = source[24];
	dest[33] = (3 * p2 + 1 * p1) >> 2;
	dest[34] = (1 * p2 + 7 * p1) >> 3;
	p2 = source[25];
	dest[35] = (3 * p1 + 5 * p2) >> 3;
	p1 = source[26];
	dest[36] = (3 * p2 + 1 * p1) >> 2;
	p2 = source[27];
	dest[37] = p1;
	dest[38] = (1 * p1 + 3 * p2) >> 2;
	p1 = source[28];
	dest[39] = (5 * p2 + 3 * p1) >> 3;
	p2 = source[29];
	dest[40] = (7 * p1 + 1 * p2) >> 3;
	dest[41] = (1 * p1 + 7 * p2) >> 3;
	p1 = source[30];
	dest[42] = (1 * p2 + 1 * p1) >> 1;
	p2 = source[31];
	dest[43] = (3 * p1 + 1 * p2) >> 2;
	dest[44] = (1 * p1 + 7 * p2) >> 3;
	p1 = source[32];
	dest[45] = (3 * p2 + 5 * p1) >> 3;
	p2 = source[33];
	dest[46] = (5 * p1 + 3 * p2) >> 3;
	p1 = source[34];
	dest[47] = p2;
	dest[48] = (1 * p2 + 3 * p1) >> 2;
	p2 = source[35];
	dest[49] = (1 * p1 + 1 * p2) >> 1;
	p1 = source[36];
	dest[50] = (7 * p2 + 1 * p1) >> 3;
	dest[51] = (1 * p2 + 7 * p1) >> 3;
	p2 = source[37];
	dest[52] = (1 * p1 + 1 * p2) >> 1;
	p1 = source[38];
	dest[53] = (3 * p2 + 1 * p1) >> 2;
	dest[54] = p1;
	p2 = source[39];
	dest[55] = (3 * p1 + 5 * p2) >> 3;
	p1 = source[40];
	dest[56] = (5 * p2 + 3 * p1) >> 3;
	p2 = source[41];
	dest[57] = (7 * p1 + 1 * p2) >> 3;
	dest[58] = (1 * p1 + 3 * p2) >> 2;
	p1 = source[42];
	dest[59] = (1 * p2 + 1 * p1) >> 1;
	p2 = source[43];
	dest[60] = (7 * p1 + 1 * p2) >> 3;
	dest[61] = (1 * p1 + 7 * p2) >> 3;
	p1 = source[44];
	dest[62] = (3 * p2 + 5 * p1) >> 3;
	p2 = source[45];
	dest[63] = (3 * p1 + 1 * p2) >> 2;
	source += 45;
	dest += 64;
    }

    if ((width += 64) <= 0)
	goto done;
    *dest++ = source[0];
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[0] + 3 * source[1]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[1] + 3 * source[2]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[2] + 1 * source[3]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[2] + 3 * source[3]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[3] + 1 * source[4]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[4] + 1 * source[5]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[4] + 7 * source[5]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[5] + 5 * source[6]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[6] + 3 * source[7]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = source[7];
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[7] + 3 * source[8]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[8] + 3 * source[9]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[9] + 1 * source[10]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[9] + 7 * source[10]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[10] + 1 * source[11]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[11] + 1 * source[12]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = source[12];
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[12] + 5 * source[13]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[13] + 3 * source[14]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = source[14];
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[14] + 3 * source[15]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[15] + 1 * source[16]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[16] + 1 * source[17]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[16] + 7 * source[17]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[17] + 5 * source[18]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[18] + 1 * source[19]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = source[19];
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[19] + 5 * source[20]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[20] + 3 * source[21]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[21] + 1 * source[22]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[21] + 3 * source[22]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[22] + 1 * source[23]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[23] + 1 * source[24]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[23] + 7 * source[24]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[24] + 5 * source[25]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[25] + 1 * source[26]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = source[26];
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[26] + 3 * source[27]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[27] + 3 * source[28]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[28] + 1 * source[29]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[28] + 7 * source[29]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[29] + 1 * source[30]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[30] + 1 * source[31]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[30] + 7 * source[31]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[31] + 5 * source[32]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[32] + 3 * source[33]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = source[33];
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[33] + 3 * source[34]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[34] + 1 * source[35]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[35] + 1 * source[36]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[35] + 7 * source[36]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[36] + 1 * source[37]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[37] + 1 * source[38]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = source[38];
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[38] + 5 * source[39]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[39] + 3 * source[40]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[40] + 1 * source[41]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[40] + 3 * source[41]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[41] + 1 * source[42]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[42] + 1 * source[43]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[42] + 7 * source[43]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[43] + 5 * source[44]) >> 3;
  done:;
}

/*
 * Interpolates 16 output pixels from 15 source pixels using shifts.
 * Useful for scaling a PAL mpeg2 dvd input source to 4:3 format on
 * a monitor using square pixels.
 * (720 x 576 ==> 768 x 576)
 */
/* uum */
static void
scale_line_15_16 (guchar * source, guchar * dest, int width, int step)
{
    int    p1, p2;

    while ((width -= 16) >= 0)
    {
	p1 = source[0];
	dest[0] = p1;
	p2 = source[1];
	dest[1] = (1 * p1 + 7 * p2) >> 3;
	p1 = source[2];
	dest[2] = (1 * p2 + 7 * p1) >> 3;
	p2 = source[3];
	dest[3] = (1 * p1 + 3 * p2) >> 2;
	p1 = source[4];
	dest[4] = (1 * p2 + 3 * p1) >> 2;
	p2 = source[5];
	dest[5] = (3 * p1 + 5 * p2) >> 3;
	p1 = source[6];
	dest[6] = (3 * p2 + 5 * p1) >> 3;
	p2 = source[7];
	dest[7] = (1 * p1 + 1 * p1) >> 1;
	p1 = source[8];
	dest[8] = (1 * p2 + 1 * p1) >> 1;
	p2 = source[9];
	dest[9] = (5 * p1 + 3 * p2) >> 3;
	p1 = source[10];
	dest[10] = (5 * p2 + 3 * p1) >> 3;
	p2 = source[11];
	dest[11] = (3 * p1 + 1 * p2) >> 2;
	p1 = source[12];
	dest[12] = (3 * p2 + 1 * p1) >> 2;
	p2 = source[13];
	dest[13] = (7 * p1 + 1 * p2) >> 3;
	p1 = source[14];
	dest[14] = (7 * p2 + 1 * p1) >> 3;
	dest[15] = p1;
	source += 15;
	dest += 16;
    }

    if ((width += 16) <= 0)
	goto done;
    *dest++ = source[0];
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[0] + 7 * source[1]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[1] + 7 * source[2]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[2] + 3 * source[3]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[3] + 3 * source[4]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[4] + 5 * source[5]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[5] + 5 * source[6]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[6] + 1 * source[7]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (1 * source[7] + 1 * source[8]) >> 1;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[8] + 3 * source[9]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (5 * source[9] + 3 * source[10]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[10] + 1 * source[11]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (3 * source[11] + 1 * source[12]) >> 2;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[12] + 1 * source[13]) >> 3;
    if (--width <= 0)
	goto done;
    *dest++ = (7 * source[13] + 1 * source[14]) >> 3;
  done:;
}

int
scale_image (struct prvt_image_s *image)
{
    int    i;
    int    step = 1;		/* unused variable for the scale functions */

    /*
     * pointers for post-scaled line buffer 
     */
    guchar *n_y;
    guchar *n_u;
    guchar *n_v;

    /*
     * pointers into pre-scaled line buffers 
     */
    guchar *oy = image->y;
    guchar *ou = image->u;
    guchar *ov = image->v;
    guchar *oy_p = image->y;
    guchar *ou_p = image->u;
    guchar *ov_p = image->v;

    /*
     * pointers into post-scaled line buffers 
     */
    guchar *ny_p;
    guchar *nu_p;
    guchar *nv_p;

    /*
     * old line widths 
     */
    int    oy_width = image->width;
    int    ou_width = image->u_width;
    int    ov_width = image->v_width;

    /*
     * new line widths NB scale factor is factored by 32768 for rounding 
     */
    int    ny_width = (oy_width * image->scale_factor) / 32768;
    int    nu_width = (ou_width * image->scale_factor) / 32768;
    int    nv_width = (ov_width * image->scale_factor) / 32768;

    /*
     * allocate new buffer space space for post-scaled line buffers 
     */
    n_y = g_malloc (ny_width * image->height);
    if (!n_y)
	return 0;
    n_u = g_malloc (nu_width * image->u_height);
    if (!n_u)
	return 0;
    n_v = g_malloc (nv_width * image->v_height);
    if (!n_v)
	return 0;

    /*
     * set post-scaled line buffer progress pointers 
     */
    ny_p = n_y;
    nu_p = n_u;
    nv_p = n_v;

    /*
     * Do the scaling 
     */

    for (i = 0; i < image->height; ++i)
    {
	image->scale_line (oy_p, ny_p, ny_width, step);
	oy_p += oy_width;
	ny_p += ny_width;
    }

    for (i = 0; i < image->u_height; ++i)
    {
	image->scale_line (ou_p, nu_p, nu_width, step);
	ou_p += ou_width;
	nu_p += nu_width;
    }

    for (i = 0; i < image->v_height; ++i)
    {
	image->scale_line (ov_p, nv_p, nv_width, step);
	ov_p += ov_width;
	nv_p += nv_width;
    }

    /*
     * switch to post-scaled data and widths 
     */
    image->y = n_y;
    image->u = n_u;
    image->v = n_v;
    image->width = ny_width;
    image->u_width = nu_width;
    image->v_width = nv_width;

    if (image->yuy2)
    {
	g_free (oy);
	g_free (ou);
	g_free (ov);
    }

    return 1;
}

/*
 *  This function was pinched from filter_yuy2tov12.c, part of
 *  transcode, a linux video stream processing tool
 *
 *  Copyright (C) Thomas Östreich - June 2001
 *
 *  Thanks Thomas
 *      
 */
static void
yuy2toyv12 (struct prvt_image_s *image)
{
    int    i, j, w2;

    /*
     * I420 
     */
    guchar *y = image->y;
    guchar *u = image->u;
    guchar *v = image->v;

    guchar *input = image->yuy2;

    int    width = image->width;
    int    height = image->height;

    w2 = width / 2;

    for (i = 0; i < height; i += 2)
    {
	for (j = 0; j < w2; j++)
	{
	    /*
	     * packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] 
	     */
	    *(y++) = *(input++);
	    *(u++) = *(input++);
	    *(y++) = *(input++);
	    *(v++) = *(input++);
	}

	/*
	 * down sampling 
	 */

	for (j = 0; j < w2; j++)
	{
	    /*
	     * skip every second line for U and V 
	     */
	    *(y++) = *(input++);
	    input++;
	    *(y++) = *(input++);
	    input++;
	}
    }
}

/*
 *  This function is a fudge .. a hack.
 *
 *  It is in place purely to get snapshots going for YUY2 data
 *  longer term there needs to be a bit of a reshuffle to account
 *  for the two fundamentally different YUV formats. Things would
 *  have been different had I known how YUY2 was done before designing
 *  the flow. Teach me to make assumptions I guess.
 *
 *  So .. this function converts the YUY2 image to YV12. The downside
 *  being that as YV12 has half as many chroma rows as YUY2, there is
 *  a loss of image quality.
 */
/* uuuuuuuuuuuuuuum. */
static  int
yuy2_fudge (struct prvt_image_s *image)
{
/* FIXME !!!!!!!!! */
    image->y = g_malloc0 (image->height * image->width * 2);
    if (!image->y)
	goto ERROR0;

    image->u = g_malloc0 (image->u_height * image->u_width * 2);
    if (!image->u)
	goto ERROR1;

    image->v = g_malloc0 (image->v_height * image->v_width * 2);
    if (!image->v)
	goto ERROR2;

    yuy2toyv12 (image);

    /*
     * image->yuy2 = NULL; 
 *//*
 * * * * * I will use this value as flag
 * * * * * to free yuv data in scale_image () 
 */

    return 1;

  ERROR2:
    g_free (image->u);
    image->u = NULL;
  ERROR1:
    g_free (image->y);
    image->y = NULL;
  ERROR0:
    return 0;
}

#define clip_8_bit(val)                                                        \
{                                                                              \
   if (val < 0)                                                                \
      val = 0;                                                                 \
   else                                                                        \
      if (val > 255) val = 255;                                                \
}

/*
 *   Create rgb data from yv12
 */
static guchar *
yv12_2_rgb (struct prvt_image_s *image)
{
    int    i, j;

    int    y, u, v;
    int    r, g, b;

    int    sub_i_u;
    int    sub_i_v;

    int    sub_j_u;
    int    sub_j_v;

    guchar *rgb;

    rgb = g_malloc0 (image->width * image->height * PIXSZ * sizeof (guchar));
    if (!rgb)
	return NULL;

    for (i = 0; i < image->height; ++i)
    {
	/*
	 * calculate u & v rows 
	 */
	sub_i_u = ((i * image->u_height) / image->height);
	sub_i_v = ((i * image->v_height) / image->height);

	for (j = 0; j < image->width; ++j)
	{
	    /*
	     * calculate u & v columns 
	     */
	    sub_j_u = ((j * image->u_width) / image->width);
	    sub_j_v = ((j * image->v_width) / image->width);

	 /***************************************************
          *
          *  Colour conversion from 
          *    http://www.inforamp.net/~poynton/notes/colour_and_gamma/ColorFAQ.html#RTFToC30
          *
          *  Thanks to Billy Biggs <vektor@dumbterm.net>
          *  for the pointer and the following conversion.
          *
          *   R' = [ 1.1644         0    1.5960 ]   ([ Y' ]   [  16 ])
          *   G' = [ 1.1644   -0.3918   -0.8130 ] * ([ Cb ] - [ 128 ])
          *   B' = [ 1.1644    2.0172         0 ]   ([ Cr ]   [ 128 ])
          *
          *  Where in Xine the above values are represented as
          *
          *   Y' == image->y
          *   Cb == image->u
          *   Cr == image->v
          *
          ***************************************************/

	    y = image->y[(i * image->width) + j] - 16;
	    u = image->u[(sub_i_u * image->u_width) + sub_j_u] - 128;
	    v = image->v[(sub_i_v * image->v_width) + sub_j_v] - 128;

	    r = (1.1644 * y) + (1.5960 * v);
	    g = (1.1644 * y) - (0.3918 * u) - (0.8130 * v);
	    b = (1.1644 * y) + (2.0172 * u);

	    clip_8_bit (r);
	    clip_8_bit (g);
	    clip_8_bit (b);

	    rgb[(i * image->width + j) * PIXSZ + 0] = r;
	    rgb[(i * image->width + j) * PIXSZ + 1] = g;
	    rgb[(i * image->width + j) * PIXSZ + 2] = b;
	}
    }

    return rgb;
}

static guchar *
xine_frame_to_rgb (struct prvt_image_s *image)
{
    guchar *rgb;

    g_return_val_if_fail (image, NULL);

    switch (image->ratio_code)
    {
      case XINE_VO_ASPECT_SQUARE:
	  image->scale_line = scale_line_1_1;
	  image->scale_factor = (32768 * 1) / 1;
	  break;

      case XINE_VO_ASPECT_4_3:
	  image->scale_line = scale_line_15_16;
	  image->scale_factor = (32768 * 16) / 15;
	  break;

      case XINE_VO_ASPECT_ANAMORPHIC:
	  image->scale_line = scale_line_45_64;
	  image->scale_factor = (32768 * 64) / 45;
	  break;

      case XINE_VO_ASPECT_DVB:
	  image->scale_line = scale_line_45_64;
	  image->scale_factor = (32768 * 64) / 45;
	  break;

      case XINE_VO_ASPECT_DONT_TOUCH:
	  image->scale_line = scale_line_1_1;
	  image->scale_factor = (32768 * 1) / 1;
	  break;

      default:
	  /*
	   * the mpeg standard has a few that we don't know about 
	   */
	  image->scale_line = scale_line_1_1;
	  image->scale_factor = (32768 * 1) / 1;
	  break;
    }

    switch (image->format)
    {
      case XINE_IMGFMT_YV12:
	  image->y = image->img;
	  image->u = image->img + (image->width * image->height);
	  image->v =
	      image->img + (image->width * image->height) +
	      (image->width * image->height) / 4;
	  image->u_width = ((image->width + 1) / 2);
	  image->v_width = ((image->width + 1) / 2);
	  image->u_height = ((image->height + 1) / 2);
	  image->v_height = ((image->height + 1) / 2);
	  break;

      case XINE_IMGFMT_YUY2:
	  image->yuy2 = image->img;
	  image->u_width = ((image->width + 1) / 2);
	  image->v_width = ((image->width + 1) / 2);
	  image->u_height = ((image->height + 1) / 2);
	  image->v_height = ((image->height + 1) / 2);
	  break;

      default:
	  g_warning ("Image format %d not supported", image->format);
	  return NULL;
    }

    /*
     *  If YUY2 convert to YV12
     */
    if (image->format == XINE_IMGFMT_YUY2)
    {
	if (!yuy2_fudge (image))
	    return NULL;
    }

    scale_image (image);

    rgb = yv12_2_rgb (image);

    /*
     * FIXME 
     */
    g_free (image->y);
    g_free (image->u);
    g_free (image->v);
    image->y = NULL;
    image->u = NULL;
    image->v = NULL;

    return rgb;
}

