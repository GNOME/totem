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
#include <gconf/gconf-client.h>
#include <glade/glade.h>
/* xine */
#include <xine.h>

#include "debug.h"
#include "gtk-xine.h"
#include "gtkxine-marshal.h"
#include "scrsaver.h"
#include "video-utils.h"

#ifdef ENABLE_NLS
#    include <libintl.h>
#    ifdef BONOBO_EXPLICIT_TRANSLATION_DOMAIN
#        undef _
#        define _(String) dgettext (BONOBO_EXPLICIT_TRANSLATION_DOMAIN, String)
#    else
#        define _(String) gettext (String)
#    endif
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

#define DEFAULT_HEIGHT 420
#define DEFAULT_WIDTH 315
#define CONFIG_FILE ".gnome2"G_DIR_SEPARATOR_S"totem_config"
#define DEFAULT_TITLE _("Totem Video Window")

/* this struct is used to decouple signals coming out of the Xine threads */
typedef struct
{
	gint type;		/* one of the signals in the following enum */
	GtkXineError error_type;
	char *message;		/* or NULL */
} GtkXineSignal;

/* Signals */
enum {
	ERROR,
	EOS,
	TITLE_CHANGE,
	LAST_SIGNAL
};

/* Enum for none-signal stuff that needs to go through the AsyncQueue */
enum {
	RATIO = LAST_SIGNAL
};

/* Arguments */
enum {
	PROP_0,
	PROP_FULLSCREEN,
	PROP_SPEED,
	PROP_POSITION,
	PROP_AUDIOCHANNEL,
	PROP_CURRENT_TIME,
	PROP_STREAM_LENGTH,
	PROP_PLAYING,
	PROP_SEEKABLE,
	PROP_SHOWCURSOR,
};

static int speeds[2] = {
	XINE_SPEED_PAUSE,
	XINE_SPEED_NORMAL,
};

struct GtkXinePrivate {
	/* Xine stuff */
	xine_t *xine;
	xine_stream_t *stream;
	xine_vo_driver_t *vo_driver;
	xine_ao_driver_t *ao_driver;
	pthread_t thread;
	xine_event_queue_t *ev_queue;
	double display_ratio;

	/* Configuration */
	gboolean null_out;

	/* X stuff */
	Display *display;
	int screen;
	GdkWindow *video_window;
	int completion_event;

	/* Visual effects */
	gboolean show_vfx;
	gboolean using_vfx;
	xine_post_t *vis;

	/* Other stuff */
	int xpos, ypos;
	gboolean init_finished;
	gboolean can_dvd, can_vcd;

	GAsyncQueue *queue;
	int video_width, video_height;

	/* fullscreen stuff */
	gboolean fullscreen_mode;
	GdkWindow *fullscreen_window;
	gboolean cursor_shown;
	gboolean pml;

	/* properties dialog */
	char *mrl;
	GtkWidget *dialog;
	GladeXML *xml;
	gboolean properties_reset_state;
};

static void gtk_xine_class_init (GtkXineClass *klass);
static void gtk_xine_instance_init (GtkXine *gtx);

static void load_config_from_gconf (GtkXine *gtx);

static void gtk_xine_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec);
static void gtk_xine_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec);

static void gtk_xine_realize (GtkWidget *widget);
static void gtk_xine_unrealize (GtkWidget *widget);
static void gtk_xine_finalize (GObject *object);

static gboolean gtk_xine_expose (GtkWidget *widget, GdkEventExpose *event);
static gboolean gtk_xine_motion_notify (GtkWidget *widget,
				        GdkEventMotion *event);
static gboolean gtk_xine_button_press (GtkWidget *widget,
				       GdkEventButton *event);
static gboolean gtk_xine_key_press (GtkWidget *widget, GdkEventKey *event);

static void gtk_xine_size_allocate (GtkWidget *widget,
				    GtkAllocation *allocation);

static GtkWidgetClass *parent_class = NULL;

static void xine_event (void *user_data, const xine_event_t *event);
static gboolean gtk_xine_idle_signal (GtkXine *gtx);

static int gtx_table_signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_xine_get_type (void)
{
	static GtkType gtk_xine_type = 0;

	if (!gtk_xine_type) {
		static const GTypeInfo gtk_xine_info = {
			sizeof (GtkXineClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_xine_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GtkXine),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gtk_xine_instance_init,
		};

		gtk_xine_type = g_type_register_static (GTK_TYPE_WIDGET,
				"GtkXine", &gtk_xine_info, (GTypeFlags)0);
	}

	return gtk_xine_type;
}

static void
gtk_xine_class_init (GtkXineClass *klass)
{

	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_widget_get_type ());

	/* GtkWidget */
	widget_class->realize = gtk_xine_realize;
	widget_class->unrealize = gtk_xine_unrealize;
	widget_class->size_allocate = gtk_xine_size_allocate;
	widget_class->expose_event = gtk_xine_expose;
	widget_class->motion_notify_event = gtk_xine_motion_notify;
	widget_class->button_press_event = gtk_xine_button_press;
	widget_class->key_press_event = gtk_xine_key_press;

	/* GObject */
	object_class->set_property = gtk_xine_set_property;
	object_class->get_property = gtk_xine_get_property;
	object_class->finalize = gtk_xine_finalize;

	/* Properties */
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
	g_object_class_install_property (object_class, PROP_AUDIOCHANNEL,
			g_param_spec_int ("audiochannel", NULL, NULL,
				0, 65535, 0, G_PARAM_READWRITE));
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

	/* Signals */
	gtx_table_signals[ERROR] =
		g_signal_new ("error",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GtkXineClass, error),
				NULL, NULL,
				gtkxine_marshal_VOID__INT_STRING,
				G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);

	gtx_table_signals[EOS] =
		g_signal_new ("eos",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GtkXineClass, eos),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	gtx_table_signals[TITLE_CHANGE] =
		g_signal_new ("title-change",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GtkXineClass, title_change),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);

	if (!g_thread_supported ())
		g_thread_init (NULL);
	gdk_threads_init ();
}

static void
gtk_xine_instance_init (GtkXine *gtx)
{
	char *configfile;

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (gtx), GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (gtx), GTK_DOUBLE_BUFFERED);

	/* Set the default size to be a 4:3 ratio */
	gtx->widget.requisition.width = DEFAULT_HEIGHT;
	gtx->widget.requisition.height = DEFAULT_WIDTH;

	gtx->priv = g_new0 (GtkXinePrivate, 1);
	gtx->priv->xine = xine_new ();
	gtx->priv->stream = NULL;
	gtx->priv->vo_driver = NULL;
	gtx->priv->ao_driver = NULL;
	gtx->priv->ev_queue = NULL;
	gtx->priv->display = NULL;
	gtx->priv->null_out = FALSE;
	gtx->priv->show_vfx = FALSE;
	gtx->priv->using_vfx = FALSE;
	gtx->priv->vis = NULL;
	gtx->priv->fullscreen_mode = FALSE;
	gtx->priv->init_finished = FALSE;
	gtx->priv->cursor_shown = TRUE;
	gtx->priv->can_dvd = FALSE;
	gtx->priv->can_vcd = FALSE;
	gtx->priv->pml = FALSE;
	gtx->priv->mrl = NULL;
	gtx->priv->dialog = NULL;
	gtx->priv->xml = NULL;
	gtx->priv->properties_reset_state = FALSE;

	gtx->priv->queue = g_async_queue_new ();

	/* generate and init configuration  */
	configfile = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), CONFIG_FILE, NULL);
	xine_config_load (gtx->priv->xine, configfile);
	g_free (configfile);

	load_config_from_gconf (gtx);

	xine_init (gtx->priv->xine);
}

static void
gtk_xine_finalize (GObject *object)
{
	GtkXine *gtx = (GtkXine *) object;

	/* Should put here what needs to be destroyed */
	g_idle_remove_by_data (gtx);
	g_async_queue_unref (gtx->priv->queue);
	if (gtx->priv->dialog != NULL)
		gtk_widget_destroy (gtx->priv->dialog);
	if (gtx->priv->xml != NULL)
		g_object_unref (gtx->priv->xml);
	G_OBJECT_CLASS (parent_class)->finalize (object);

	gtx->priv = NULL;
	gtx = NULL;
}

static void
dest_size_cb (void *gtx_gen,
	      int video_width, int video_height,
	      double video_pixel_aspect,
	      int *dest_width, int *dest_height,
	      double *dest_pixel_aspect)
{
	GtkXine *gtx = (GtkXine *) gtx_gen;

	/* correct size with video_pixel_aspect */
	if (video_pixel_aspect >= gtx->priv->display_ratio)
		video_width  = video_width * video_pixel_aspect
			/ gtx->priv->display_ratio + .5;
	else
		video_height = video_height * gtx->priv->display_ratio
			/ video_pixel_aspect + .5;

	if (gtx->priv->fullscreen_mode)
	{
		*dest_width = gdk_screen_width ();
		*dest_height = gdk_screen_height ();
	} else {
		*dest_width = gtx->widget.allocation.width;
		*dest_height = gtx->widget.allocation.height;
	}

	*dest_pixel_aspect = gtx->priv->display_ratio;
}

static void
frame_output_cb (void *gtx_gen,
		 int video_width, int video_height,
		 double video_pixel_aspect,
		 int *dest_x, int *dest_y,
		 int *dest_width, int *dest_height,
		 double *dest_pixel_aspect,
		 int *win_x, int *win_y)
{
	GtkXine *gtx = (GtkXine *) gtx_gen;

	if (gtx == NULL || gtx->priv == NULL)
		return;

	/* correct size with video_pixel_aspect */
	if (video_pixel_aspect >= gtx->priv->display_ratio)
		video_width = video_width * video_pixel_aspect
			/ gtx->priv->display_ratio + .5;
	else
		video_height = video_height * gtx->priv->display_ratio
			/ video_pixel_aspect + .5;

	*dest_x = 0;
	*dest_y = 0;
	*win_x = gtx->priv->xpos;
	*win_y = gtx->priv->ypos;

	if (gtx->priv->fullscreen_mode)
	{
		*dest_width = gdk_screen_width ();
		*dest_height = gdk_screen_height ();
	} else {
		*dest_width = gtx->widget.allocation.width;
		*dest_height = gtx->widget.allocation.height;

		/* Size changed */
		if (gtx->priv->video_width != video_width
				|| gtx->priv->video_height != video_height)
		{
			GConfClient *gc;

			gtx->priv->video_width = video_width;
			gtx->priv->video_height = video_height;

			gc = gconf_client_get_default ();

			if (gconf_client_get_bool (gc, GCONF_PREFIX"auto_resize", NULL) == TRUE
					&& strcmp (gtx->priv->mrl, LOGO_PATH) != 0)
			{
				GtkXineSignal *signal;

				signal = g_new0 (GtkXineSignal, 1);
				signal->type = RATIO;
				g_async_queue_push (gtx->priv->queue, signal);
				g_idle_add ((GSourceFunc) gtk_xine_idle_signal,
						gtx);
			}
		}
	}

	*dest_pixel_aspect = gtx->priv->display_ratio;
}

static xine_vo_driver_t *
load_video_out_driver (GtkXine *gtx)
{
	double res_h, res_v;
	x11_visual_t vis;
	const char *video_driver_id;
	xine_vo_driver_t *vo_driver;

	if (gtx->priv->null_out == TRUE)
	{
		return xine_open_video_driver (gtx->priv->xine,
				"none", XINE_VISUAL_TYPE_NONE, NULL);
	}

	vis.display = gtx->priv->display;
	vis.screen = gtx->priv->screen;
	vis.d = GDK_WINDOW_XID (gtx->priv->video_window);
	res_h =
	    (DisplayWidth (gtx->priv->display, gtx->priv->screen) * 1000 /
	     DisplayWidthMM (gtx->priv->display, gtx->priv->screen));
	res_v =
	    (DisplayHeight (gtx->priv->display, gtx->priv->screen) * 1000 /
	     DisplayHeightMM (gtx->priv->display, gtx->priv->screen));
	gtx->priv->display_ratio = res_v / res_h;

	if (fabs (gtx->priv->display_ratio - 1.0) < 0.01) {
		gtx->priv->display_ratio = 1.0;
	}

	vis.dest_size_cb = dest_size_cb;
	vis.frame_output_cb = frame_output_cb;
	vis.user_data = gtx;

	/* Try to init video with stored information */
	video_driver_id = xine_config_register_string (gtx->priv->xine,
			"video.driver", "auto", "video driver to use",
			NULL, 10, NULL, NULL);

	if (strcmp (video_driver_id, "auto") != 0)
	{
		vo_driver = xine_open_video_driver (gtx->priv->xine,
						   video_driver_id,
						   XINE_VISUAL_TYPE_X11,
						   (void *) &vis);
		if (vo_driver)
			return vo_driver;
	}

	vo_driver = xine_open_video_driver (gtx->priv->xine, NULL,
			XINE_VISUAL_TYPE_X11, (void *) &vis);

	return vo_driver;
}

static xine_ao_driver_t *
load_audio_out_driver (GtkXine *gtx)
{
	GConfClient *conf;
	xine_ao_driver_t *ao_driver;
	char *audio_driver_id;

	if (gtx->priv->null_out == TRUE)
		return NULL;

	conf = gconf_client_get_default ();

	audio_driver_id = gconf_client_get_string (conf,
			GCONF_PREFIX"audio_driver",
			NULL);

	/* No configuration, fallback to auto */
	if (audio_driver_id == NULL || strcmp (audio_driver_id, "") == 0)
		audio_driver_id = g_strdup ("auto");

	/* We know how to handle null driver */
	if (strcmp (audio_driver_id, "null") == 0)
		return NULL;

	/* auto probe */
	if (strcmp (audio_driver_id, "auto") == 0)
		ao_driver = xine_open_audio_driver (gtx->priv->xine,
				NULL, NULL);
	else
		ao_driver = xine_open_audio_driver (gtx->priv->xine,
				audio_driver_id, NULL);

	/* if it failed without autoprobe, probe */
	if (ao_driver == NULL && strcmp (audio_driver_id, "auto") != 0)
		ao_driver = xine_open_audio_driver (gtx->priv->xine,
				NULL, NULL);

	if (ao_driver == NULL)
	{
		char *msg;

		msg = g_strdup_printf (_("Couldn't load the '%s' audio driver\n"
					"Check that the device is not busy."),
				audio_driver_id ? audio_driver_id : "auto" );
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				0, msg);
		g_free (msg);
	}

	g_free (audio_driver_id);

	return ao_driver;
}

static void
update_mediadev_conf (GtkXine *gtx, GConfClient *conf)
{
	char *tmp;

	conf = gconf_client_get_default ();

	/* DVD and VCD Device */
	tmp = gconf_client_get_string (conf, GCONF_PREFIX"mediadev", NULL);
	if (tmp == NULL || strcmp (tmp, "") == 0)
		tmp = g_strdup ("/dev/cdrom");

	xine_config_register_string (gtx->priv->xine,
			"input.dvd_device", tmp,
			"device used for dvd drive",
			NULL, 10, NULL, NULL);

	xine_config_register_string (gtx->priv->xine,
			"input.vcd_device", tmp,
			"device used for cdrom drive",
			NULL, 10, NULL, NULL);
}

static void             
show_vfx_changed_cb (GConfClient *client, guint cnxn_id,
		                GConfEntry *entry, gpointer user_data)
{
	GtkXine *gtx = (GtkXine *) user_data;

	gtx->priv->show_vfx = gconf_client_get_bool (client,
			GCONF_PREFIX"show_vfx", NULL);
}

static void
load_config_from_gconf (GtkXine *gtx)
{
	GConfClient *conf;

	/* default demux strategy */
	xine_config_register_string (gtx->priv->xine,
			"misc.demux_strategy", "reverse",
			"demuxer selection strategy",
			"{ default  reverse  content  extension }, default: 0",
			10, NULL, NULL);

	conf = gconf_client_get_default ();

	gconf_client_add_dir (conf, "/apps/totem",
			GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (conf, GCONF_PREFIX"show_vfx",
			show_vfx_changed_cb, gtx, NULL, NULL);
	gtx->priv->show_vfx = gconf_client_get_bool (conf,
			GCONF_PREFIX"show_vfx", NULL);
	update_mediadev_conf (gtx, conf);
}

static gboolean
video_window_translate_point (GtkXine *gtx, int gui_x, int gui_y,
		int *video_x, int *video_y)
{
	x11_rectangle_t rect;

	rect.x = gui_x;
	rect.y = gui_y;
	rect.w = 0;
	rect.h = 0;

	if (xine_gui_send_vo_data (gtx->priv->stream,
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
dvd_skip_behaviour (GtkXine *gtx, int behaviour)
{
        if (behaviour < 1 || behaviour > 2)
                return;

        xine_config_register_num (gtx->priv->xine,
                        "input.dvd_skip_behaviour",
                        behaviour,
                        "DVD Skip behaviour",
                        NULL,
                        10,
                        NULL, NULL);

        return;
}

void
gtk_xine_dvd_event (GtkXine *gtx, GtkXineDVDEvent type)
{
        xine_event_t event;

	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

        switch (type)
        {
        case GTX_DVD_ROOT_MENU:
                event.type = XINE_EVENT_INPUT_MENU1;
                break;
        case GTX_DVD_TITLE_MENU:
                event.type = XINE_EVENT_INPUT_MENU2;
                break;
        case GTX_DVD_SUBPICTURE_MENU:
                event.type = XINE_EVENT_INPUT_MENU4;
                break;
        case GTX_DVD_AUDIO_MENU:
                event.type = XINE_EVENT_INPUT_MENU5;
                break;
        case GTX_DVD_ANGLE_MENU:
                event.type = XINE_EVENT_INPUT_MENU6;
                break;
        case GTX_DVD_CHAPTER_MENU:
                event.type = XINE_EVENT_INPUT_MENU7;
                break;
        case GTX_DVD_NEXT_CHAPTER:
                dvd_skip_behaviour (gtx, 1);
                event.type = XINE_EVENT_INPUT_NEXT;
                break;
        case GTX_DVD_PREV_CHAPTER:
                dvd_skip_behaviour (gtx, 1);
                event.type = XINE_EVENT_INPUT_PREVIOUS;
                break;
        case GTX_DVD_NEXT_TITLE:
                dvd_skip_behaviour (gtx, 2);
                event.type = XINE_EVENT_INPUT_NEXT;
                break;
        case GTX_DVD_PREV_TITLE:
                dvd_skip_behaviour (gtx, 2);
                event.type = XINE_EVENT_INPUT_PREVIOUS;
                break;
        case GTX_DVD_NEXT_ANGLE:
                event.type = XINE_EVENT_INPUT_ANGLE_NEXT;
                break;
        case GTX_DVD_PREV_ANGLE:
                event.type = XINE_EVENT_INPUT_ANGLE_PREVIOUS;
                break;
        default:
                return;
        }

        event.stream = gtx->priv->stream;
        event.data = NULL;
        event.data_length = 0;

        xine_event_send (gtx->priv->stream,
                        (xine_event_t *) (&event));
}

static gboolean
generate_mouse_event (GtkXine *gtx, GdkEvent *event, gboolean is_motion)
{
	GdkEventMotion *mevent = (GdkEventMotion *) event;
	GdkEventButton *bevent = (GdkEventButton *) event;
	int x, y;
	gboolean retval;

	if (is_motion == FALSE && bevent->button != GDK_BUTTON_PRESS)
		return FALSE;

	if (is_motion == TRUE)
		retval = video_window_translate_point (gtx,
				mevent->x, mevent->y, &x, &y);
	else
		retval = video_window_translate_point (gtx,
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
		event.stream = gtx->priv->stream;
		event.data = &input;
		event.data_length = sizeof(input);

		xine_event_send (gtx->priv->stream,
				(xine_event_t *) (&event));

		return TRUE;
	}

	return FALSE;
}

static void *
xine_thread (void *gtx_gen)
{
	GtkXine *gtx = (GtkXine *) gtx_gen;
	XEvent event;

	gtx->priv->init_finished = TRUE;

	while (1)
	{
		if (gtx->priv->stream == NULL)
			break;

		if (XPending (gtx->priv->display))
		{
			XNextEvent (gtx->priv->display, &event);
		} else {
			usleep (100);
			continue;
		}

		if (event.type == gtx->priv->completion_event)
		{
			xine_gui_send_vo_data (gtx->priv->stream,
					XINE_GUI_SEND_COMPLETION_EVENT,
					&event);
		}
	}

	pthread_exit (NULL);
	return NULL;
}

static gboolean
configure_cb (GtkWidget *widget, GdkEventConfigure *event, gpointer user_data)
{
	GtkXine *gtx = (GtkXine *) user_data;

	gtx->priv->xpos = event->x;
	gtx->priv->ypos = event->y;

	return FALSE;
}

static void
gtk_xine_realize (GtkWidget *widget)
{
	GtkXine *gtx;
	const char *const *autoplug_list;
	int i = 0;
	GdkWindowAttr attr;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_XINE (widget));

	gtx = GTK_XINE (widget);

	/* set realized flag */
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

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
	gdk_window_set_user_data (widget->window, gtx);

	gtx->priv->video_window = widget->window;

	/* track configure events of toplevel window */
	g_signal_connect (GTK_OBJECT (gtk_widget_get_toplevel (widget)),
			  "configure-event",
			  GTK_SIGNAL_FUNC (configure_cb), gtx);

	/* Init threads in X and setup the needed X stuff */
	if (!XInitThreads ())
	{
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				0,
				_("Could not initialise the threads support.\n"
				"You should install a thread-safe Xlib."));
		return;
	}

	gtx->priv->display = XOpenDisplay (gdk_display_get_name
			(gdk_display_get_default ()));
	XLockDisplay (gtx->priv->display);
	gtx->priv->screen = DefaultScreen (gtx->priv->display);

	if (XShmQueryExtension (gtx->priv->display) == True)
	{
		gtx->priv->completion_event =
			XShmGetEventBase (gtx->priv->display) + ShmCompletion;
	} else {
		gtx->priv->completion_event = -1;
	}

	/* load audio, video drivers */
	gtx->priv->ao_driver = load_audio_out_driver (gtx);
	gtx->priv->vo_driver = load_video_out_driver (gtx);

	if (gtx->priv->vo_driver == NULL)
	{
		XUnlockDisplay (gtx->priv->display);
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				GTX_STARTUP,
				_("Could not find a suitable video output."));
		return;
	}

	if (gtx->priv->null_out == FALSE)
		gtx->priv->vis = xine_post_init (gtx->priv->xine, "goom", 0,
				&gtx->priv->ao_driver, &gtx->priv->vo_driver);

	gtx->priv->stream = xine_stream_new (gtx->priv->xine,
			gtx->priv->ao_driver, gtx->priv->vo_driver);
	gtx->priv->ev_queue = xine_event_new_queue (gtx->priv->stream);

	/* Setup xine events, the screensaver and the event filter */
	xine_event_create_listener_thread (gtx->priv->ev_queue,
			xine_event, (void *) gtx);

	scrsaver_init (gtx->priv->display);

	XUnlockDisplay (gtx->priv->display);

	/* Can we play DVDs and VCDs ? */
	autoplug_list = xine_get_autoplay_input_plugin_ids (gtx->priv->xine);
	while (autoplug_list && autoplug_list[i])
	{
		if (g_ascii_strcasecmp (autoplug_list[i], "VCD") == 0)
			gtx->priv->can_vcd = TRUE;
		else if (g_ascii_strcasecmp (autoplug_list[i], "DVD") == 0)
			gtx->priv->can_dvd = TRUE;
		i++;
	}

	/* now, create a xine thread */
	pthread_create (&gtx->priv->thread, NULL, xine_thread, gtx);

	return;
}

static gboolean
gtk_xine_idle_signal (GtkXine *gtx)
{
	GtkXineSignal *signal;
	int queue_length;

	signal = g_async_queue_try_pop (gtx->priv->queue);
	if (signal == NULL)
		return FALSE;

	TE ();
	switch (signal->type)
	{
	case ERROR:
		/* We don't emit the ERROR signal when in fullscreen mode */
		if (gtx->priv->fullscreen_mode == TRUE)
			break;

		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				signal->error_type, signal->message);
		break;
	/* A bit of cheating right here */
	case RATIO:
		gtk_xine_set_scale_ratio (gtx, 0);
		break;
	default:
	}

	g_free (signal->message);
	g_free (signal);

	queue_length = g_async_queue_length (gtx->priv->queue);
	TL ();

	return (queue_length > 0);
}

static void
xine_event (void *user_data, const xine_event_t *event)
{
	GtkXine *gtx = (GtkXine *) user_data;
	GtkXineSignal *signal;
	xine_ui_data_t *ui_data;

	switch (event->type)
	{
	case XINE_EVENT_UI_PLAYBACK_FINISHED:
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[EOS], 0, NULL);
		break;
	case XINE_EVENT_UI_SET_TITLE:
		ui_data = (xine_ui_data_t *) event->data;
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[TITLE_CHANGE],
				0, ui_data->str);
		break;
	}
}

static void
xine_error (GtkXine *gtx)
{
	GtkXineSignal *signal;
	int error;

	error = xine_get_error (gtx->priv->stream);
	if (error == XINE_ERROR_NONE)
		return;

	signal = g_new0 (GtkXineSignal, 1);
	signal->type = ERROR;

	switch (error)
	{
	case XINE_ERROR_NO_INPUT_PLUGIN:
		signal->error_type = GTX_NO_INPUT_PLUGIN;
		break;
	case XINE_ERROR_NO_DEMUX_PLUGIN:
		signal->error_type = GTX_NO_DEMUXER_PLUGIN;
		break;
	case XINE_ERROR_DEMUX_FAILED:
		signal->error_type = GTX_DEMUXER_FAILED;
		break;
	default:
		break;
	}

	g_async_queue_push (gtx->priv->queue, signal);
	g_idle_add ((GSourceFunc) gtk_xine_idle_signal, gtx);
}

static void
gtk_xine_unrealize (GtkWidget *widget)
{
	GtkXine *gtx;
	char *configfile;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_XINE (widget));

	/* Hide all windows */
	if (GTK_WIDGET_MAPPED (widget))
		gtk_widget_unmap (widget);
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	gtx = GTK_XINE (widget);

	/* stop the playback */
	xine_close (gtx->priv->stream);

	/* Get rid of the rest of the stream */
	xine_event_dispose_queue (gtx->priv->ev_queue);
	xine_dispose (gtx->priv->stream);
	gtx->priv->stream = NULL;

	/* Kill the drivers */
	if (gtx->priv->vo_driver != NULL)
		xine_close_video_driver (gtx->priv->xine, gtx->priv->vo_driver);
	if (gtx->priv->ao_driver != NULL)
		xine_close_audio_driver (gtx->priv->xine, gtx->priv->ao_driver);

	/* stop the completion event thread */
	pthread_cancel (gtx->priv->thread);
	pthread_join (gtx->priv->thread, NULL);

	/* save config */
	configfile = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), CONFIG_FILE, NULL);
	xine_config_save (gtx->priv->xine, configfile);
	g_free (configfile);

	/* stop event thread */
	xine_exit (gtx->priv->xine);
	gtx->priv->xine = NULL;

	/* Finally, kill the left-over windows */
	if (gtx->priv->fullscreen_window != NULL)
		gdk_window_destroy (gtx->priv->fullscreen_window);

	/* This destroys widget->window and unsets the realized flag */
	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

GtkWidget *
gtk_xine_new (int width, int height, gboolean null_out)
{
	GtkWidget *gtx;

	gtx = GTK_WIDGET (g_object_new (gtk_xine_get_type (), NULL));

	GTK_XINE (gtx)->priv->null_out = null_out;

	/* defaults are fine if both are negative */
	if (width <= 0 && height <= 0)
		return gtx;
	/* figure out the missing measure from the other one with a 4:3 ratio */
	if (width <= 0)
		width = (int) (height * 4 / 3);
	if (height <= 0)
		height = (int) (width * 3 / 4);

	GTK_XINE (gtx)->widget.requisition.width = width;
	GTK_XINE (gtx)->widget.requisition.height = height;

	return gtx;
}

gboolean
gtk_xine_check (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_XINE (gtx), FALSE);

	if (gtx->priv->stream == NULL)
		return FALSE;

	return gtx->priv->init_finished;
}

static gboolean
gtk_xine_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GtkXine *gtx = (GtkXine *) widget;
	XExposeEvent *expose;

	if (event->count != 0)
		return FALSE;

	expose = g_new0 (XExposeEvent, 1);
	expose->count = event->count;

	xine_gui_send_vo_data (gtx->priv->stream,
			XINE_GUI_SEND_EXPOSE_EVENT,
			expose);

	return FALSE;
}

static gboolean
gtk_xine_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
	GtkXine *gtx = (GtkXine *) widget;

	generate_mouse_event (GTK_XINE (widget), (GdkEvent *)event, TRUE);

	if (GTK_WIDGET_CLASS (parent_class)->motion_notify_event != NULL)
		(* GTK_WIDGET_CLASS (parent_class)->motion_notify_event) (widget, event);

	return FALSE;
}

static gboolean
gtk_xine_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GtkXine *gtx = (GtkXine *) widget;

	if (generate_mouse_event (GTK_XINE (widget), (GdkEvent *)event,
				FALSE) == TRUE)
		return FALSE;

	if (GTK_WIDGET_CLASS (parent_class)->button_press_event != NULL)
		                (* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event);

	return FALSE;
}

static gboolean
gtk_xine_key_press (GtkWidget *widget, GdkEventKey *event)
{
	if (GTK_WIDGET_CLASS (parent_class)->key_press_event != NULL)
		(* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event);

	return FALSE;
}

static void
gtk_xine_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkXine *gtx;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_XINE (widget));

	gtx = GTK_XINE (widget);

	widget->allocation = *allocation;
	gtx->priv->xpos = allocation->x;
	gtx->priv->ypos = allocation->y;

	if (GTK_WIDGET_REALIZED (widget))
	{
		/* HACK it seems to be 1 pixel off, weird */
		gdk_window_move_resize (widget->window,
					allocation->x,
					allocation->y,
					allocation->width,
					allocation->height);
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

	return g_strdup (&fcc[0]);
}


gboolean
gtk_xine_open (GtkXine *gtx, const gchar *mrl)
{
	int error;
	gboolean has_video;
	xine_post_out_t *audio_source;

	g_return_val_if_fail (gtx != NULL, -1);
	g_return_val_if_fail (mrl != NULL, -1);
	g_return_val_if_fail (GTK_IS_XINE (gtx), -1);
	g_return_val_if_fail (gtx->priv->xine != NULL, -1);

	error = xine_open (gtx->priv->stream, mrl);
	if (error == 0)
	{
		xine_error (gtx);
		return FALSE;
	}

	if (xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_VIDEO_HANDLED) == FALSE
		&& xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_AUDIO_HANDLED) == FALSE)
	{
		GtkXineSignal *signal;
		uint32_t fourcc;
		char *fourcc_str;
		const char *name;

		fourcc = xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_VIDEO_FOURCC);
		fourcc_str = get_fourcc_string (fourcc);
		name = xine_get_meta_info (gtx->priv->stream,
				XINE_META_INFO_VIDEOCODEC);

		gtk_xine_close (gtx);

		signal = g_new0 (GtkXineSignal, 1);
		signal->type = ERROR;
		signal->error_type = GTX_NO_CODEC;
		signal->message = g_strdup_printf (_("Reason: Video type '%s' is not handled."), name ? name : fourcc_str );
		g_async_queue_push (gtx->priv->queue, signal);
		g_idle_add ((GSourceFunc) gtk_xine_idle_signal, gtx);

		D("Reason: Video type '%s' is not handled.", name ? name : fourcc_str );

		g_free (fourcc_str);

		return FALSE;
	}

	gtx->priv->mrl = g_strdup (mrl);

	has_video = xine_get_stream_info(gtx->priv->stream,
			XINE_STREAM_INFO_HAS_VIDEO);

	if (has_video == TRUE && gtx->priv->using_vfx == TRUE)
	{
		audio_source = xine_get_audio_source (gtx->priv->stream);
		if (xine_post_wire_audio_port (audio_source,
					gtx->priv->ao_driver))
			gtx->priv->using_vfx = FALSE;
	} else if (has_video == FALSE && gtx->priv->show_vfx == TRUE
			&& gtx->priv->using_vfx == FALSE
			&& gtx->priv->vis != NULL)
	{
		audio_source = xine_get_audio_source (gtx->priv->stream);
		if (xine_post_wire_audio_port (audio_source,
					gtx->priv->vis->audio_input[0]))
			gtx->priv->using_vfx = TRUE;
	}

	return TRUE;
}

gboolean
gtk_xine_play (GtkXine *gtx, guint pos, guint start_time)
{
	int error, length;

	g_return_val_if_fail (gtx != NULL, -1);
	g_return_val_if_fail (GTK_IS_XINE (gtx), -1);
	g_return_val_if_fail (gtx->priv->xine != NULL, -1);

	length = gtk_xine_get_stream_length (gtx);
	error = xine_play (gtx->priv->stream, pos,
			CLAMP (start_time, 0, length));

	if (error == 0)
	{
		xine_error (gtx);
		return FALSE;
	}
	return TRUE;
}

void
gtk_xine_stop (GtkXine *gtx)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	if (gtx->priv->stream == NULL)
		return;

	xine_stop (gtx->priv->stream);
}

void
gtk_xine_close (GtkXine *gtx)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	if (gtx->priv->stream == NULL)
		return;

	xine_close (gtx->priv->stream);
	g_free (gtx->priv->mrl);
}

/* Properties */
static void
gtk_xine_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec)
{
	GtkXine *gtx;

	g_return_if_fail (GTK_IS_XINE (object));

	gtx = GTK_XINE (object);

	switch (property_id)
	{
	case PROP_FULLSCREEN:
		gtk_xine_set_fullscreen (gtx, g_value_get_boolean (value));
		break;
	case PROP_SPEED:
		gtk_xine_set_speed (gtx, g_value_get_int (value));
		break;
	case PROP_AUDIOCHANNEL:
		gtk_xine_set_audio_channel (gtx, g_value_get_int (value));
		break;
	case PROP_SHOWCURSOR:
		gtk_xine_set_show_cursor (gtx, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
gtk_xine_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec)
{
	GtkXine *gtx;

	g_return_if_fail (GTK_IS_XINE (object));

	gtx = GTK_XINE (object);

	switch (property_id)
	{
	case PROP_FULLSCREEN:
		g_value_set_boolean (value, gtk_xine_is_fullscreen (gtx));
		break;
	case PROP_SPEED:
		g_value_set_int (value, gtk_xine_get_speed (gtx));
		break;
	case PROP_POSITION:
		g_value_set_int (value, gtk_xine_get_position (gtx));
		break;
	case PROP_AUDIOCHANNEL:
		g_value_set_int (value, gtk_xine_get_audio_channel (gtx));
		break;
	case PROP_STREAM_LENGTH:
		g_value_set_int (value, gtk_xine_get_stream_length (gtx));
		break;
	case PROP_PLAYING:
		g_value_set_boolean (value, gtk_xine_is_playing (gtx));
		break;
	case PROP_SEEKABLE:
		g_value_set_boolean (value, gtk_xine_is_seekable (gtx));
		break;
	case PROP_SHOWCURSOR:
		g_value_set_boolean (value, gtk_xine_get_show_cursor (gtx));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

void
gtk_xine_set_speed (GtkXine *gtx, Speeds speed)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	xine_set_param (gtx->priv->stream, XINE_PARAM_SPEED, speeds[speed]);
}

gint
gtk_xine_get_speed (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, SPEED_NORMAL);
	g_return_val_if_fail (GTK_IS_XINE (gtx), SPEED_NORMAL);
	g_return_val_if_fail (gtx->priv->xine != NULL, SPEED_NORMAL);

	return xine_get_param (gtx->priv->stream, XINE_PARAM_SPEED);
}

gint
gtk_xine_get_position (GtkXine *gtx)
{
	int pos_stream = 0, i = 0;
	int pos_time, length_time;
	gboolean ret;

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	if (gtk_xine_is_playing (gtx) == FALSE)
		return 0;

	ret = xine_get_pos_length (gtx->priv->stream, &pos_stream,
			&pos_time, &length_time);

	while (ret == FALSE && i < 10)
	{
		usleep (100000);
		ret = xine_get_pos_length (gtx->priv->stream, &pos_stream,
				&pos_time, &length_time);
		i++;
	}

	if (ret == FALSE)
		return -1;

	return pos_stream;
}

void
gtk_xine_set_audio_channel (GtkXine *gtx, gint audio_channel)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	xine_set_param (gtx->priv->stream,
			XINE_PARAM_AUDIO_CHANNEL_LOGICAL, audio_channel);
}

gint
gtk_xine_get_audio_channel (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return xine_get_param (gtx->priv->stream,
			XINE_PARAM_AUDIO_CHANNEL_LOGICAL);
}

void
gtk_xine_set_fullscreen (GtkXine *gtx, gboolean fullscreen)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	XLockDisplay (gtx->priv->display);

	if (gtx->priv->pml == FALSE)
		gtx->priv->pml = TRUE;
	else
		return;

	if (fullscreen == gtx->priv->fullscreen_mode)
	{
		XUnlockDisplay (gtx->priv->display);
		return;
	}

	gtx->priv->fullscreen_mode = fullscreen;

	if (fullscreen)
	{
		GdkWindow *parent;
		GdkWindowAttr attr;

		parent = gdk_window_get_toplevel (gtx->widget.window);

		attr.x = 0;
		attr.y = 0;
		attr.width = gdk_screen_width ();
		attr.height = gdk_screen_height ();
		attr.window_type = GDK_WINDOW_TOPLEVEL;
		attr.wclass = GDK_INPUT_OUTPUT;
		attr.event_mask = gtk_widget_get_events (GTK_WIDGET (gtx))
			| GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK
			| GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK;
		gtx->priv->fullscreen_window = gdk_window_new
			(NULL, &attr, GDK_WA_X | GDK_WA_Y);
		gdk_window_show (gtx->priv->fullscreen_window);
		gdk_window_fullscreen (gtx->priv->fullscreen_window);
		/* Flush, so that the window is really shown */
		gdk_flush ();

		gdk_window_set_user_data (gtx->priv->fullscreen_window, gtx);

		xine_gui_send_vo_data (gtx->priv->stream,
			 XINE_GUI_SEND_DRAWABLE_CHANGED,
			 (void*) GDK_WINDOW_XID (gtx->priv->fullscreen_window));

		/* switch off mouse cursor */
		gtk_xine_set_show_cursor (gtx, FALSE);

		scrsaver_disable (gtx->priv->display);
	} else {
		gdk_window_set_user_data (gtx->widget.window, gtx);

		xine_gui_send_vo_data (gtx->priv->stream,
			 XINE_GUI_SEND_DRAWABLE_CHANGED,
			 (void *) GDK_WINDOW_XID (gtx->priv->video_window));

		/* Hide the window */
		gdk_window_destroy (gtx->priv->fullscreen_window);
		gtx->priv->fullscreen_window = NULL;

		scrsaver_enable (gtx->priv->display);

		gdk_window_focus (gdk_window_get_toplevel
				(gtk_widget_get_parent_window
				 (GTK_WIDGET (gtx))), GDK_CURRENT_TIME);
	}

	gtx->priv->pml = FALSE;
	XUnlockDisplay (gtx->priv->display);
}

gint
gtk_xine_is_fullscreen (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return gtx->priv->fullscreen_mode;
}

gboolean
gtk_xine_can_set_volume (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	if (gtx->priv->stream == NULL)
		return FALSE;

	if (xine_get_param (gtx->priv->stream, XINE_PARAM_AUDIO_VOLUME) == -1)
		return FALSE;

	if (xine_get_param (gtx->priv->stream,
				XINE_PARAM_AUDIO_CHANNEL_LOGICAL) == -2)
		return FALSE;

	return xine_get_stream_info (gtx->priv->stream,
			XINE_STREAM_INFO_HAS_AUDIO);
}

void
gtk_xine_set_volume (GtkXine *gtx, gint volume)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	if (gtk_xine_can_set_volume (gtx) == TRUE)
	{
		volume = CLAMP (volume, 0, 100);
		xine_set_param (gtx->priv->stream, XINE_PARAM_AUDIO_VOLUME,
				volume);
	}
}

gint
gtk_xine_get_volume (GtkXine *gtx)
{
	int volume = 0;

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	volume = xine_get_param (gtx->priv->stream,
			XINE_PARAM_AUDIO_VOLUME);

	return volume;
}

void
gtk_xine_set_show_cursor (GtkXine *gtx, gboolean show_cursor)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	if (GDK_IS_WINDOW (gtx->priv->fullscreen_window) == FALSE)
		return;

	if (show_cursor == FALSE)
	{
		eel_gdk_window_set_invisible_cursor
			(gtx->priv->fullscreen_window);
	} else {
		gdk_window_set_cursor (gtx->priv->fullscreen_window, NULL);
	}

	gtx->priv->cursor_shown = show_cursor;
}

gboolean
gtk_xine_get_show_cursor (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_XINE (gtx), FALSE);
	g_return_val_if_fail (gtx->priv->xine != NULL, FALSE);

	return gtx->priv->cursor_shown;
}

gint
gtk_xine_get_current_time (GtkXine *gtx)
{
	int pos_time = 0, i = 0;
	int pos_stream, length_time;
	gboolean ret;

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	if (gtk_xine_is_playing (gtx) == FALSE)
		return 0;

	ret = xine_get_pos_length (gtx->priv->stream, &pos_stream,
			&pos_time, &length_time);

	while (ret == FALSE && i < 10)
	{
		usleep (100000);
		ret = xine_get_pos_length (gtx->priv->stream, &pos_stream,
				&pos_time, &length_time);
		i++;
	}

	if (ret == FALSE)
		return -1;

	return pos_time;
}

gint
gtk_xine_get_stream_length (GtkXine *gtx)
{
	int length_time = 0;
	int pos_stream, pos_time;

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	xine_get_pos_length (gtx->priv->stream, &pos_stream,
			&pos_time, &length_time);

	return length_time;
}

gboolean
gtk_xine_is_playing (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	if (gtx->priv->stream == NULL)
		return FALSE;

	return xine_get_status (gtx->priv->stream) == XINE_STATUS_PLAY;
}

gboolean
gtk_xine_is_seekable (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	if (gtk_xine_get_stream_length (gtx) == 0)
		return FALSE;

	return xine_get_stream_info (gtx->priv->stream,
			XINE_STREAM_INFO_SEEKABLE);
}

gboolean
gtk_xine_can_play (GtkXine *gtx, MediaType type)
{
	switch (type)
	{
	case MEDIA_DVD:
		return gtx->priv->can_dvd;
	case MEDIA_VCD:
		return gtx->priv->can_vcd;
	default:
		return FALSE;
	}
}

G_CONST_RETURN gchar
**gtk_xine_get_mrls (GtkXine *gtx, MediaType type)
{
	char *plugin_id;
	int num_mrls;

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	if (type == MEDIA_DVD)
		plugin_id = "DVD";
	else if (type == MEDIA_VCD)
		plugin_id = "VCD";
	else
		return NULL;

	update_mediadev_conf (gtx, gconf_client_get_default ());

	return (G_CONST_RETURN gchar **) xine_get_autoplay_mrls
		(gtx->priv->xine, plugin_id, &num_mrls);
}

void
gtk_xine_toggle_aspect_ratio (GtkXine *gtx)
{
	int tmp;

	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	tmp = xine_get_param (gtx->priv->stream, XINE_PARAM_VO_ASPECT_RATIO);
	xine_set_param (gtx->priv->stream, XINE_PARAM_VO_ASPECT_RATIO, tmp + 1);
}

static gboolean
gtk_xine_ratio_fits_screen (GtkXine *gtx, gfloat ratio)
{
	int new_w, new_h;

	g_return_val_if_fail (gtx != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_XINE (gtx), FALSE);
	g_return_val_if_fail (gtx->priv->xine != NULL, FALSE);

	new_w = gtx->priv->video_width * ratio;
	new_h = gtx->priv->video_height * ratio;

	if (new_w > (gdk_screen_width () - 128) ||
			new_h > (gdk_screen_height () - 128))
	{
		return FALSE;
	}

	return TRUE;
}

void
gtk_xine_set_scale_ratio (GtkXine *gtx, gfloat ratio)
{
	GtkWindow *toplevel;
	int new_w, new_h;

	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);
	g_return_if_fail (ratio >= 0);

	if (gtx->priv->fullscreen_mode == TRUE)
		return;

	/* Try best fit for the screen */
	if (ratio == 0)
	{
		if (gtk_xine_ratio_fits_screen (gtx, 2) == TRUE)
			ratio = 2;
		else if (gtk_xine_ratio_fits_screen (gtx, 1) == TRUE)
			ratio = 1;
		else if (gtk_xine_ratio_fits_screen (gtx, 0.5) == TRUE)
			ratio = 0.5;
		else
			return;
	} else {
		/* don't scale to something bigger than the screen, and leave
		 * us some room */
		if (gtk_xine_ratio_fits_screen (gtx, ratio) == FALSE)
			return;
	}

	new_w = gtx->priv->video_width * ratio;
	new_h = gtx->priv->video_height * ratio;

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gtx)));

	gtk_window_set_resizable (toplevel, FALSE);
	gtx->widget.allocation.width = new_w;
	gtx->widget.allocation.height = new_h;
	gtk_widget_set_size_request (gtk_widget_get_parent (GTK_WIDGET (gtx)),
			new_w, new_h);
	gtk_widget_queue_resize (gtk_widget_get_parent (GTK_WIDGET (gtx)));
	while (gtk_events_pending ())
		gtk_main_iteration ();
	gtk_window_set_resizable (toplevel, TRUE);
}

static void
gtk_xine_get_metadata_string (GtkXine *gtx, GtkXineMetadataType type,
		GValue *value)
{
	const char *string;

	g_value_init (value, G_TYPE_STRING);

	switch (type)
	{
	case GTX_INFO_TITLE:
		string = xine_get_meta_info (gtx->priv->stream,
				XINE_META_INFO_TITLE);
		break;
	case GTX_INFO_ARTIST:
		string = xine_get_meta_info (gtx->priv->stream,
				XINE_META_INFO_ARTIST);
		break;
	case GTX_INFO_YEAR:
		string = xine_get_meta_info (gtx->priv->stream,
				XINE_META_INFO_YEAR);
		break;
	case GTX_INFO_VIDEO_CODEC:
		string = xine_get_meta_info (gtx->priv->stream,
				XINE_META_INFO_VIDEOCODEC);
		break;
	case GTX_INFO_AUDIO_CODEC:
		string = xine_get_meta_info (gtx->priv->stream,
				XINE_META_INFO_AUDIOCODEC);
		break;
	default:
		g_assert_not_reached ();
	}

	g_value_set_string (value, string);

	return;
}

static void
gtk_xine_get_metadata_int (GtkXine *gtx, GtkXineMetadataType type,
		GValue *value)
{
	int integer;

	g_value_init (value, G_TYPE_INT);

	switch (type)
	{
	case GTX_INFO_DURATION:
		integer = gtk_xine_get_stream_length (gtx) / 1000;
		break;
	case GTX_INFO_DIMENSION_X:
		xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_VIDEO_WIDTH);
		break;
	case GTX_INFO_DIMENSION_Y:
		xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_VIDEO_HEIGHT);
		break;
	case GTX_INFO_FPS:
		if (xine_get_stream_info (gtx->priv->stream,
					XINE_STREAM_INFO_FRAME_DURATION) != 0)
		{
			integer = 90000 / xine_get_stream_info
				(gtx->priv->stream,
				 XINE_STREAM_INFO_FRAME_DURATION);
		} else {
			integer = 0;
		}
		break;
	 case GTX_INFO_BITRATE:
		integer = xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_AUDIO_BITRATE) / 1000;
		break;
	 default:
		g_assert_not_reached ();
	 }

	 g_value_set_int (value, integer);

	 return;
}

static void
gtk_xine_get_metadata_bool (GtkXine *gtx, GtkXineMetadataType type,
		GValue *value)
{
	gboolean boolean;

	g_value_init (value, G_TYPE_BOOLEAN);

	switch (type)
	{
	case GTX_INFO_HAS_VIDEO:
		boolean = xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_HAS_VIDEO);
		break;
	case GTX_INFO_HAS_AUDIO:
		boolean = xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_HAS_AUDIO);
		break;
	default:
		g_assert_not_reached ();
	}

	g_value_set_boolean (value, boolean);

	return;
}

void
gtk_xine_get_metadata (GtkXine *gtx, GtkXineMetadataType type, GValue *value)
{
	switch (type)
	{
	case GTX_INFO_TITLE:
	case GTX_INFO_ARTIST:
	case GTX_INFO_YEAR:
	case GTX_INFO_VIDEO_CODEC:
	case GTX_INFO_AUDIO_CODEC:
		gtk_xine_get_metadata_string (gtx, type, value);
		break;
	case GTX_INFO_DURATION:
	case GTX_INFO_DIMENSION_X:
	case GTX_INFO_DIMENSION_Y:
	case GTX_INFO_FPS:
	case GTX_INFO_BITRATE:
		gtk_xine_get_metadata_int (gtx, type, value);
		break;
	case GTX_INFO_HAS_VIDEO:
	case GTX_INFO_HAS_AUDIO:
		gtk_xine_get_metadata_bool (gtx, type, value);
		break;
	default:
		g_assert_not_reached ();
	}

	return;
}

static void
hide_dialog (GtkWidget *widget, int trash, gpointer user_data)
{
	GtkXine *gtx = (GtkXine *) user_data;

	gtk_widget_hide (gtx->priv->dialog);
}

GtkWidget
*gtk_xine_properties_dialog_get (GtkXine *gtx)
{
	char *filename;

	if (gtx->priv->dialog != NULL)
		return gtx->priv->dialog;

	filename = g_build_filename (G_DIR_SEPARATOR_S, DATADIR,
			"totem", "properties.glade", NULL);

	gtx->priv->xml = glade_xml_new (filename, NULL, NULL);
	g_free (filename);

	if (gtx->priv->xml == NULL)
		return NULL;

	gtx->priv->dialog = glade_xml_get_widget (gtx->priv->xml, "dialog1");

	g_signal_connect (G_OBJECT (gtx->priv->dialog),
			"response", G_CALLBACK (hide_dialog), (gpointer) gtx);
	g_signal_connect (G_OBJECT (gtx->priv->dialog), "delete-event",
			G_CALLBACK (hide_dialog), (gpointer) gtx);

	gtk_xine_properties_update (gtx, gtx->priv->properties_reset_state);

	return gtx->priv->dialog;
}

static char
*time_to_string (int time)
{
	char *secs, *mins, *hours, *string;
	int sec, min, hour;

	sec = time % 60;
	time = time - sec;
	min = (time % (60*60)) / 60;
	time = time - (min * 60);
	hour = time / (60*60);

	if (hour == 1)
		/* One hour */
		hours = g_strdup_printf (_("%d hour"), hour);
	else
		/* Multiple hours */
		hours = g_strdup_printf (_("%d hours"), hour);

	if (min == 1)
		/* One minute */
		mins = g_strdup_printf (_("%d minute"), min);
	else
		/* Multiple minutes */
		mins = g_strdup_printf (_("%d minutes"), min);

	if (sec == 1)
		/* One second */
		secs = g_strdup_printf (_("%d second"), sec);
	else
		/* Multiple seconds */
		secs = g_strdup_printf (_("%d seconds"), sec);

	if (hour > 0)
	{
		/* hour:minutes:seconds */
		string = g_strdup_printf (_("%s %s %s"), hours, mins, secs);
	} else if (min > 0) {
		/* minutes:seconds */
		string = g_strdup_printf (_("%s %s"), mins, secs);
	} else if (sec > 0) {
		/* seconds */
		string = g_strdup_printf (_("%s"), secs);
	} else {
		/* 0 seconds */
		string = g_strdup (_("0 seconds"));
	}

	g_free (hours);
	g_free (mins);
	g_free (secs);

	return string;
}

char
*gtk_xine_properties_get_title (GtkXine *gtx)
{
	const char *short_title, *artist;

	artist = xine_get_meta_info (gtx->priv->stream, XINE_META_INFO_ARTIST);
	short_title = xine_get_meta_info (gtx->priv->stream,
			XINE_META_INFO_TITLE);

	if (artist == NULL && short_title == NULL)
		return NULL;

	if (artist == NULL && short_title != NULL)
		return g_strdup (short_title);

	if (artist != NULL && short_title != NULL)
		return g_strdup_printf ("%s - %s", artist, short_title);

	return NULL;
}

static void
gtk_xine_properties_set_label (GtkXine *gtx, const char *name, const char *text)
{
	GtkWidget *item;

	item = glade_xml_get_widget (gtx->priv->xml, name);
	gtk_label_set_text (GTK_LABEL (item), text);
}

static void
gtk_xine_properties_reset (GtkXine *gtx)
{
	GtkWidget *item;

	item = glade_xml_get_widget (gtx->priv->xml, "video");
	gtk_widget_set_sensitive (item, FALSE);
	item = glade_xml_get_widget (gtx->priv->xml, "audio");
	gtk_widget_set_sensitive (item, FALSE);

	/* Title */
	gtk_xine_properties_set_label (gtx, "title", _("Unknown"));
	/* Artist */
	gtk_xine_properties_set_label (gtx, "artist", _("Unknown"));
	/* Year */
	gtk_xine_properties_set_label (gtx, "year", _("N/A"));
	/* Duration */
	gtk_xine_properties_set_label (gtx, "duration", _("0 second"));
	/* Dimensions */
	gtk_xine_properties_set_label (gtx, "dimensions", _("0 x 0"));
	/* Video Codec */
	gtk_xine_properties_set_label (gtx, "vcodec", _("N/A"));
	/* Framerate */
	gtk_xine_properties_set_label (gtx, "framerate",
			_("0 frames per second"));
	/* Bitrate */
	gtk_xine_properties_set_label (gtx, "bitrate", _("0 kbps"));
	/* Audio Codec */
	gtk_xine_properties_set_label (gtx, "acodec", _("N/A"));
}

static void
gtk_xine_properties_set_from_current (GtkXine *gtx)
{
	GtkWidget *item;
	const char *text;
	char *string;
	int fps;

	/* General */
	text = xine_get_meta_info (gtx->priv->stream, XINE_META_INFO_TITLE);
	gtk_xine_properties_set_label (gtx, "title",
			text ? text : _("Unknown"));

	text = xine_get_meta_info (gtx->priv->stream, XINE_META_INFO_ARTIST);
	gtk_xine_properties_set_label (gtx, "artist",
			text ? text : _("Unknown"));

	text = xine_get_meta_info (gtx->priv->stream, XINE_META_INFO_YEAR);
	gtk_xine_properties_set_label (gtx, "year",
			text ? text : _("N/A"));

	string = time_to_string (gtk_xine_get_stream_length (gtx) / 1000);
	gtk_xine_properties_set_label (gtx, "duration", string);
	g_free (string);

	/* Video */
	item = glade_xml_get_widget (gtx->priv->xml, "video");
	if (xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_HAS_VIDEO) == FALSE)
		gtk_widget_set_sensitive (item, FALSE);
	else
		gtk_widget_set_sensitive (item, TRUE);

	string = g_strdup_printf ("%d x %d",
			xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_VIDEO_WIDTH),
			xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_VIDEO_HEIGHT));
	gtk_xine_properties_set_label (gtx, "dimensions", string);
	g_free (string);

	text = xine_get_meta_info (gtx->priv->stream,
			XINE_META_INFO_VIDEOCODEC);
	gtk_xine_properties_set_label (gtx, "vcodec",
			text ? text : _("N/A"));

	if (xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_FRAME_DURATION) != 0)
	{
		fps = 90000 / xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_FRAME_DURATION);
	} else {
		fps = 0;
	}
	string = g_strdup_printf (_("%d frames per second"), fps);
	gtk_xine_properties_set_label (gtx, "framerate", string);
	g_free (string);

	/* Audio */
	item = glade_xml_get_widget (gtx->priv->xml, "audio");
	if (xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_HAS_AUDIO) == FALSE)
		gtk_widget_set_sensitive (item, FALSE);
	else
		gtk_widget_set_sensitive (item, TRUE);

	string = g_strdup_printf (_("%d kbps"),
			xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_AUDIO_BITRATE) / 1000);
	gtk_xine_properties_set_label (gtx, "bitrate", string);
	g_free (string);

	text = xine_get_meta_info (gtx->priv->stream,
			XINE_META_INFO_AUDIOCODEC);
	gtk_xine_properties_set_label (gtx, "acodec",
			text ? text : _("N/A"));
}

void
gtk_xine_properties_update (GtkXine *gtx, gboolean reset)
{
	gtx->priv->properties_reset_state = reset;

	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	if (gtx->priv->dialog == NULL)
		return;

	if (reset == TRUE)
		gtk_xine_properties_reset (gtx);
	else
		gtk_xine_properties_set_from_current (gtx);
}


/*
 *  For screen shot. Nicked from pornview which is in turn nicked from xine-ui.
 */

#define PIXSZ 3

static guchar *gtk_xine_get_current_frame_rgb (GtkXine *gtx, gint *width_ret,
					       gint * height_ret);

gboolean
gtk_xine_can_get_frames (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_XINE (gtx), FALSE);
	g_return_val_if_fail (gtx->priv->xine != NULL, FALSE);
	g_return_val_if_fail (gtk_xine_is_playing (gtx) == TRUE, FALSE);

	if (xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_HAS_VIDEO) == FALSE)
		return FALSE;

	if (xine_get_stream_info (gtx->priv->stream,
				XINE_STREAM_INFO_VIDEO_HANDLED) == FALSE)
		return FALSE;

	return TRUE;
}

GdkPixbuf *
gtk_xine_get_current_frame (GtkXine *gtx)
{
	guchar *pixels;
	gint width, height;
	GdkPixbuf *pixbuf = NULL;

	g_return_val_if_fail (gtx != NULL, NULL);
	g_return_val_if_fail (GTK_IS_XINE (gtx), NULL);
	g_return_val_if_fail (gtx->priv->xine != NULL, NULL);

	pixels = gtk_xine_get_current_frame_rgb (gtx, &width, &height);
	if (pixels != NULL)
	{
		pixbuf = gdk_pixbuf_new_from_data (pixels,
				GDK_COLORSPACE_RGB, FALSE,
				8, width, height, 3 * width,
				(GdkPixbufDestroyNotify) g_free, NULL);
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
gtk_xine_get_current_frame_rgb (GtkXine * gtx, gint * width_ret,
				gint * height_ret)
{
    gint    err = 0;
    struct prvt_image_s *image;
    guchar *rgb = NULL;
    gint    width, height;

    g_return_val_if_fail (gtx, NULL);
    g_return_val_if_fail (GTK_IS_XINE (gtx), NULL);
    g_return_val_if_fail (gtx->priv->xine, NULL);
    g_return_val_if_fail (gtx->priv->stream != NULL, 0);
    g_return_val_if_fail (width_ret && height_ret, NULL);

    image = g_new0 (struct prvt_image_s, 1);
    if (!image)
    {
	*width_ret = 0;
	*height_ret = 0;

	return NULL;
    }

    image->y = image->u = image->v = image->yuy2 = image->img = NULL;

    width = xine_get_stream_info (gtx->priv->stream,
		    XINE_STREAM_INFO_VIDEO_WIDTH);
    height = xine_get_stream_info (gtx->priv->stream,
		    XINE_STREAM_INFO_VIDEO_HEIGHT);

    image->img = g_malloc (width * height * 2);

    if (!image->img)
    {
	*width_ret = 0;
	*height_ret = 0;

	g_free (image);
	return NULL;
    }

    err = xine_get_current_frame (gtx->priv->stream,
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
scale_line_1_1 (guchar * source, guchar * dest, gint width, gint step)
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
scale_line_45_64 (guchar * source, guchar * dest, gint width, gint step)
{
    gint    p1, p2;

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
scale_line_15_16 (guchar * source, guchar * dest, gint width, gint step)
{
    gint    p1, p2;

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
    gint    i;
    gint    step = 1;		/* unused variable for the scale functions */

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
    gint    oy_width = image->width;
    gint    ou_width = image->u_width;
    gint    ov_width = image->v_width;

    /*
     * new line widths NB scale factor is factored by 32768 for rounding 
     */
    gint    ny_width = (oy_width * image->scale_factor) / 32768;
    gint    nu_width = (ou_width * image->scale_factor) / 32768;
    gint    nv_width = (ov_width * image->scale_factor) / 32768;

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
 *  Copyright (C) Thomas ŽÖstreich - June 2001
 *
 *  Thanks Thomas
 *      
 */
static void
yuy2toyv12 (struct prvt_image_s *image)
{
    gint    i, j, w2;

    /*
     * I420 
     */
    guchar *y = image->y;
    guchar *u = image->u;
    guchar *v = image->v;

    guchar *input = image->yuy2;

    gint    width = image->width;
    gint    height = image->height;

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
static  gint
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
    gint    i, j;

    gint    y, u, v;
    gint    r, g, b;

    gint    sub_i_u;
    gint    sub_i_v;

    gint    sub_j_u;
    gint    sub_j_v;

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

