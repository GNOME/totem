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
/* X11 */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <X11/keysym.h>
/* gtk+/gnome */
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkinvisible.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
/* xine */
#include <xine/video_out_x11.h>

#include "debug.h"
#include "gtk-xine.h"
#include "gtkxine-marshal.h"
#include "scrsaver.h"
#include "video-utils.h"

#define DEFAULT_HEIGHT 420
#define DEFAULT_WIDTH 315
#define CONFIG_FILE ".xine"G_DIR_SEPARATOR_S"config"
#define DEFAULT_TITLE "Totem Video Window"
#define GCONF_PREFIX "/apps/totem/"
#define LOGO_PATH DATADIR""G_DIR_SEPARATOR_S"totem"G_DIR_SEPARATOR_S"totem_logo.mpv"

#define BLACK_PIXEL \
	BlackPixel ((gtx->priv->display ? gtx->priv->display : gdk_display), \
			gtx->priv->screen)

/* missing stuff from X includes */
#ifndef XShmGetEventBase
extern int XShmGetEventBase (Display *);
#endif

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
	MOUSE_MOTION,
	EOS,
	LAST_SIGNAL
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

struct GtkXinePrivate {
	/* Xine stuff */
	xine_t *xine;
	config_values_t *config;
	vo_driver_t *vo_driver;
	ao_driver_t *ao_driver;

	/* X stuff */
	Display *display;
	int screen;
	Window video_window;

	/* Other stuff */
	pthread_t thread;
	int completion_event;
	int mixer;
	int xpos, ypos;
	gboolean init_finished;

	GAsyncQueue *queue;

	/* fullscreen stuff */
	gboolean fullscreen_mode;
	GdkWindow *fullscreen_window;
	GtkWidget *invisible;
	gboolean cursor_shown;
};


static void gtk_xine_class_init (GtkXineClass * klass);
static void gtk_xine_instance_init (GtkXine * gtx);

static void gtk_xine_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec);
static void gtk_xine_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec);

static void gtk_xine_realize (GtkWidget * widget);
static void gtk_xine_unrealize (GtkWidget * widget);
static void gtk_xine_finalize (GObject *object);

static gint gtk_xine_expose (GtkWidget * widget, GdkEventExpose * event);

static void gtk_xine_size_allocate (GtkWidget * widget,
				    GtkAllocation * allocation);

static GtkWidgetClass *parent_class = NULL;

static void xine_event (void *user_data, xine_event_t *event);
static void codec_reporting(void *user_data, int codec_type,
		uint32_t fourcc, char *description, int handled);

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
gtk_xine_class_init (GtkXineClass * klass)
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
				SPEED_PAUSE, SPEED_FAST_4,
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

	gtx_table_signals[MOUSE_MOTION] =
	    g_signal_new ("mouse-motion",
			  G_TYPE_FROM_CLASS (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkXineClass, mouse_motion),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	gtx_table_signals[EOS] =
		g_signal_new ("eos",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (GtkXineClass, eos),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	if (!g_thread_supported ())
		g_thread_init (NULL);
	gdk_threads_init ();
}

static void
gtk_xine_instance_init (GtkXine * gtx)
{
	/* Set the default size to be a 4:3 ratio */
	gtx->widget.requisition.width = DEFAULT_HEIGHT;
	gtx->widget.requisition.height = DEFAULT_WIDTH;

	gtx->priv = g_new0 (GtkXinePrivate, 1);
	gtx->priv->config = NULL;
	gtx->priv->xine = NULL;
	gtx->priv->vo_driver = NULL;
	gtx->priv->ao_driver = NULL;
	gtx->priv->display = NULL;
	gtx->priv->fullscreen_mode = FALSE;
	gtx->priv->mixer = -1;
	gtx->priv->init_finished = FALSE;
	gtx->priv->cursor_shown = TRUE;

	gtx->priv->queue = g_async_queue_new ();
}

static void
gtk_xine_finalize (GObject *object)
{
	GtkXine *gtx = (GtkXine *) object;

	/* Should put here what needs to be destroyed */

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
dest_size_cb (void *gtx_gen,
	      int video_width, int video_height,
	      int *dest_width, int *dest_height)
{
	GtkXine *gtx = (GtkXine *) gtx_gen;

	if (gtx->priv->fullscreen_mode)
	{
		*dest_width = gdk_screen_width ();
		*dest_height = gdk_screen_height ();
	} else {
		*dest_width = gtx->widget.allocation.width;
		*dest_height = gtx->widget.allocation.height;
	}
}

static void
frame_output_cb (void *gtx_gen,
		 int video_width, int video_height,
		 int *dest_x, int *dest_y,
		 int *dest_width, int *dest_height, int *win_x, int *win_y)
{
	GtkXine *gtx = (GtkXine *) gtx_gen;

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
	}
}

static vo_driver_t *
load_video_out_driver (GtkXine *gtx)
{
	double res_h, res_v;
	x11_visual_t vis;
	char **driver_ids;
	int i;
	char *video_driver_id;
	vo_driver_t *vo_driver;

	vis.display = gtx->priv->display;
	vis.screen = gtx->priv->screen;
	vis.d = gtx->priv->video_window;
	res_h = gdk_screen_height () * 1000 / gdk_screen_height_mm ();
	res_v = gdk_screen_width () * 1000 / gdk_screen_width_mm ();
	vis.display_ratio = res_h / res_v;

	if (fabs (vis.display_ratio - 1.0) < 0.01)
		vis.display_ratio = 1.0;

	vis.dest_size_cb = dest_size_cb;
	vis.frame_output_cb = frame_output_cb;
	vis.user_data = gtx;

	/* Video output driver auto-probing */
	driver_ids = xine_list_video_output_plugins (VISUAL_TYPE_X11);

	/* Try to init video with stored information */
	video_driver_id =
	    gtx->priv->config->register_string (gtx->priv->config,
						"video.driver", "auto",
						"video driver to use",
						NULL, NULL, NULL);

	if (strcmp (video_driver_id, "auto") == 0)
	{
		vo_driver = xine_load_video_output_plugin (gtx->priv->config,
				video_driver_id,
				VISUAL_TYPE_X11,
				(void *) &vis);

		if (vo_driver != NULL)
		{
			g_strfreev (driver_ids);
			return vo_driver;
		}
	}

	i = 0;
	while (driver_ids[i] != NULL)
	{
		video_driver_id = driver_ids[i];

		vo_driver =
		    xine_load_video_output_plugin (gtx->priv->config,
						   video_driver_id,
						   VISUAL_TYPE_X11,
						   (void *) &vis);
		if (vo_driver) {
			gtx->priv->config->update_string
				(gtx->priv->config, "video.driver",
				 video_driver_id);
			g_strfreev (driver_ids);
			return vo_driver;
		}
		i++;
	}

	g_strfreev (driver_ids);
	return NULL;
}

static ao_driver_t *
load_audio_out_driver (GtkXine * gtx, char *audio_driver_id)
{
	ao_driver_t *ao_driver = NULL;
	char **driver_ids;
	int i = 0;

	if (strcmp (audio_driver_id, "auto") == 0)
		return xine_load_audio_output_plugin (gtx->priv->config,
						      audio_driver_id);

	driver_ids = xine_list_audio_output_plugins ();

	while (driver_ids[i] != NULL)
	{
		audio_driver_id = driver_ids[i];

		ao_driver =
		    xine_load_audio_output_plugin (gtx->priv->config,
						   driver_ids[i]);

		if (ao_driver) {
			D ("main: ...worked, using '%s' audio driver.\n",
			    driver_ids[i]);

			gtx->priv->config->update_string (gtx->priv-> config,
							  "audio.driver",
							  audio_driver_id);

			g_strfreev (driver_ids);
			return ao_driver;
		}
		i++;
	}

	g_strfreev (driver_ids);
	return ao_driver;
}

static void
load_config_from_gconf (GtkXine *gtx)
{
	GConfClient *conf;
	char *tmp;

	if (!gconf_is_initialized ())
	{
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				GTX_STARTUP,
				_("The configuration system is not initialised.\nThe defaults will be used."));
		return;
	}
	conf = gconf_client_get_default ();

	/* The logo path */
	tmp = LOGO_PATH;
	gtx->priv->config->register_string (gtx->priv->config,
			"misc.logo_mrl", tmp,
			"audio driver to use",
			NULL, NULL, NULL);
	gtx->priv->config->update_string (gtx->priv->config,
			"misc.logo_mrl", tmp);

	/* The audio output, equivalent to audio.driver*/
	tmp = gconf_client_get_string (conf,
			GCONF_PREFIX"audio_driver",
			NULL);
	if (tmp == NULL || strcmp (tmp, "") == 0)
		tmp = g_strdup ("auto");

	gtx->priv->ao_driver = load_audio_out_driver (gtx, tmp);
	g_free (tmp);

	/* Fallback on null, just in case */
	if (!gtx->priv->ao_driver)
	{
		tmp = "null";
		gtx->priv->ao_driver = load_audio_out_driver (gtx, tmp);
	}

	/* default demux strategy */
	gtx->priv->config->register_string (gtx->priv->config,
			"misc.demux_strategy", "reverse",
			"demuxer selection strategy",
			"{ default  reverse  content  extension }, default: 0",
			NULL, NULL);
	gtx->priv->config->update_string (gtx->priv->config,
			"misc.demux_strategy", "reverse");

	/* TODO skip by chapter for DVD */
	/* TODO input.cda_device:/dev/cdaudio
	 * input.cda_cddb_server:freedb.freedb.org
	 * input.cda_cddb_port:8880 (or is it 888 like gnome-media says ?
	 * input.dvd_device:/dev/dvd
	 * input.dvd_raw_device:/dev/rdvd
	 */
}

static gboolean
x_window_is_visible(Display *display, Window window)
{
	XWindowAttributes  wattr;
	Status             status;

	if((display == NULL) || (window == None))
		return FALSE;

	XLockDisplay (display);
	status = XGetWindowAttributes(display, window, &wattr);
	XUnlockDisplay(display);

	if((status != BadDrawable)
			&& (status != BadWindow)
			&& (wattr.map_state == IsViewable))
		return TRUE;

	return FALSE;
}

static void *
xine_thread (void *gtx_gen)
{
	GtkXine *gtx = (GtkXine *) gtx_gen;
	XEvent event;

	gtx->priv->init_finished = TRUE;

	while (gtx->priv->display)
	{
		XNextEvent (gtx->priv->display, &event);

		switch (event.type)
		{
		case Expose:
			if (event.xexpose.count != 0)
				break;

			gtx->priv->vo_driver->gui_data_exchange
				(gtx->priv->vo_driver,
				 GUI_DATA_EX_EXPOSE_EVENT, &event);
			break;
		case FocusIn:
			/* happens only in fullscreen mode
			 * wait for the window to get visible first
			 * to avoid BadMatch problems */
			while (x_window_is_visible (gtx->priv->display,
						GDK_WINDOW_XWINDOW (gdk_window_get_toplevel
							                                (gtk_widget_get_parent_window (GTK_WIDGET(gtx))))) == FALSE)
				usleep(5000);

			XSetInputFocus (gtx->priv->display,
					GDK_WINDOW_XWINDOW (gdk_window_get_toplevel
						                                (gtk_widget_get_parent_window (GTK_WIDGET (gtx)))), RevertToNone,
					CurrentTime);
			break;
		}

		if (event.type == gtx->priv->completion_event)
		{
			gtx->priv->vo_driver->gui_data_exchange
				(gtx->priv->vo_driver,
				 GUI_DATA_EX_COMPLETION_EVENT, &event);
		}
	}

	pthread_exit (NULL);
	return NULL;
}

static gboolean
configure_cb (GtkWidget * widget,
	      GdkEventConfigure * event, gpointer user_data)
{
	GtkXine *gtx = (GtkXine *) user_data;

	gtx->priv->xpos = event->x;
	gtx->priv->ypos = event->y;

	return FALSE;
}

static void
gtk_xine_realize (GtkWidget * widget)
{
	GtkXine *gtx;
	char *configfile;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_XINE (widget));

	gtx = GTK_XINE (widget);

	/* set realized flag */
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	/* create our own video window */
	gtx->priv->video_window = XCreateSimpleWindow
		(gdk_display,
		 GDK_WINDOW_XWINDOW (gtk_widget_get_parent_window (widget)),
		 0, 0,
		 widget->allocation.width, widget->allocation.height,
		 1, BLACK_PIXEL, BLACK_PIXEL);

	widget->window = gdk_window_foreign_new (gtx->priv->video_window);

	/* track configure events of toplevel window */
	g_signal_connect (GTK_OBJECT (gtk_widget_get_toplevel (widget)),
			  "configure-event",
			  GTK_SIGNAL_FUNC (configure_cb), gtx);

	if (!XInitThreads ()) {
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				0,
				_("Could not initialise the threads support.\n"
				"You should install a thread-safe Xlib."));
		return;
	}

	gtx->priv->display = XOpenDisplay (NULL);

	if (!gtx->priv->display) {
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				GTX_STARTUP,
				_("Failed to open the display."));
		return;
	}

	XLockDisplay (gtx->priv->display);

	gtx->priv->screen = DefaultScreen (gtx->priv->display);

	if (XShmQueryExtension (gtx->priv->display) == True)
	{
		gtx->priv->completion_event =
		    XShmGetEventBase (gtx->priv->display) + ShmCompletion;
	} else {
		gtx->priv->completion_event = -1;
	}

	XSelectInput (gtx->priv->display, gtx->priv->video_window,
		      StructureNotifyMask | ExposureMask |
		      ButtonPressMask | PointerMotionMask);

	/* generate and init a config "object" */
	configfile = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), CONFIG_FILE, NULL);
	gtx->priv->config = xine_config_file_init (configfile);
	g_free (configfile);

	/* load audio, video drivers */
	gtx->priv->vo_driver = load_video_out_driver (gtx);

	if (!gtx->priv->vo_driver)
	{
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				GTX_STARTUP,
				_("Could not find a suitable video output."));
		return;
	}

	/* configure some more things, including audio output */
	load_config_from_gconf (gtx);

	/* init xine engine */
	gtx->priv->xine = xine_init (gtx->priv->vo_driver,
			gtx->priv->ao_driver,
			gtx->priv->config);

	XUnlockDisplay (gtx->priv->display);

	/* Setup xine events and codec reporting */
	xine_register_event_listener (gtx->priv->xine, xine_event,
			(void *) gtx);
	xine_register_report_codec_cb(gtx->priv->xine, codec_reporting,
			(void *) gtx);

	scrsaver_init (gtx->priv->display);

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
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				signal->error_type, signal->message);
		break;
	case MOUSE_MOTION:
		/* mouse motion doesn't need to go through here */
		break;
	case EOS:
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[EOS], 0, NULL);
		break;
	default:
		break;
	}

	g_free (signal->message);
	g_free (signal);

	queue_length = g_async_queue_length (gtx->priv->queue);
	TL ();

	return (queue_length > 0);
}

static void
xine_event (void *user_data, xine_event_t *event)
{
	GtkXine *gtx = (GtkXine *) user_data;

	/* XINE_EVENT_MOUSE_MOVE doesn't actually work */
	if (event->type == XINE_EVENT_PLAYBACK_FINISHED)
	{
		GtkXineSignal *signal;

		signal = g_new0 (GtkXineSignal, 1);
		signal->type = EOS;
		g_async_queue_push (gtx->priv->queue, signal);
		g_idle_add ((GSourceFunc) gtk_xine_idle_signal, gtx);
	}
}

static void codec_reporting(void *user_data, int codec_type,
		uint32_t fourcc, char *description, int handled)
{
	GtkXine *gtx = (GtkXine *) user_data;
	char fourcc_txt[10];

	/* store fourcc as text */
	*(uint32_t *)fourcc_txt = fourcc;
	fourcc_txt[4] = '\0';

	if (!handled)
	{
		if (codec_type == XINE_CODEC_VIDEO)
		{
			GtkXineSignal *signal;

			signal = g_new0 (GtkXineSignal, 1);
			signal->type = ERROR;

			/* display fourcc if no description available */
			if (!description[0])
				description = fourcc_txt;
			signal->error_type = GTX_NO_CODEC;
			signal->message = g_strdup_printf
				(_("No video plugin available to decode '%s'."),
				 description);
			g_async_queue_push (gtx->priv->queue, signal);
			g_idle_add ((GSourceFunc) gtk_xine_idle_signal, gtx);
		}
	}
}

static void
xine_error (GtkXine *gtx)
{
	GtkXineSignal *signal;
	int error;

	error = xine_get_error (gtx->priv->xine);
	if (error == XINE_ERROR_NONE)
		return;

	signal = g_new0 (GtkXineSignal, 1);
	signal->type = ERROR;

	switch (error)
	{
	case XINE_ERROR_NO_INPUT_PLUGIN:
		signal->error_type = GTX_NO_INPUT_PLUGIN;
		break;
	case XINE_ERROR_NO_DEMUXER_PLUGIN:
		signal->error_type = GTX_NO_DEMUXER_PLUGIN;
		break;
	case XINE_ERROR_DEMUXER_FAILED:
		signal->error_type = GTX_DEMUXER_FAILED;
		break;
	default:
		break;
	}

	g_async_queue_push (gtx->priv->queue, signal);
	g_idle_add ((GSourceFunc) gtk_xine_idle_signal, gtx);
}

static void
gtk_xine_unrealize (GtkWidget * widget)
{
	GtkXine *gtx;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_XINE (widget));

	gtx = GTK_XINE (widget);

	/* We don't need to save the configuration,
	 * It's overriden in the load by either GConf values or
	 * hard-coded values
	 gtx->priv->config->save (gtx->priv->config); */

	/* stop event thread */
	xine_exit (gtx->priv->xine);

	/* Hide all windows */
	if (GTK_WIDGET_MAPPED (widget))
		gtk_widget_unmap (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	/* This destroys widget->window and unsets the realized flag */
	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

GtkWidget *
gtk_xine_new (int width, int height)
{
	GtkWidget *gtx;

	gtx = GTK_WIDGET (gtk_type_new (gtk_xine_get_type ()));

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

	if (gtx->priv->xine == NULL)
		return FALSE;

	return gtx->priv->init_finished;
}

static gint
gtk_xine_expose (GtkWidget * widget, GdkEventExpose * event)
{
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

	if (GTK_WIDGET_REALIZED (widget)) {
		/* HACK it seems to be 1 pixel off, weird */
		gdk_window_move_resize (widget->window,
					allocation->x - 1,
					allocation->y - 1,
					allocation->width,
					allocation->height);
	}
}

gint
gtk_xine_play (GtkXine *gtx, gchar *mrl, gint pos, gint start_time)
{
	int error;

	g_return_val_if_fail (gtx != NULL, -1);
	g_return_val_if_fail (GTK_IS_XINE (gtx), -1);
	g_return_val_if_fail (gtx->priv->xine != NULL, -1);

	error = xine_play (gtx->priv->xine, mrl, pos, start_time);
	if (!error)
	{
		xine_error (gtx);
		return FALSE;
	}
	return TRUE;
}

void
gtk_xine_stop (GtkXine * gtx)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	xine_stop (gtx->priv->xine);
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
gtk_xine_set_speed (GtkXine *gtx, gint speed)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	xine_set_speed (gtx->priv->xine, speed);
}

gint
gtk_xine_get_speed (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, SPEED_NORMAL);
	g_return_val_if_fail (GTK_IS_XINE (gtx), SPEED_NORMAL);
	g_return_val_if_fail (gtx->priv->xine != NULL, SPEED_NORMAL);

	return xine_get_speed (gtx->priv->xine);
}

gint
gtk_xine_get_position (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return (gtk_xine_is_playing (gtx)
			? xine_get_current_position (gtx->priv->xine)
			: 0);
}

void
gtk_xine_set_audio_channel (GtkXine *gtx, gint audio_channel)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	xine_select_audio_channel (gtx->priv->xine, audio_channel);
}

gint
gtk_xine_get_audio_channel (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return xine_get_audio_selection (gtx->priv->xine);
}

static gboolean
motion_notify_event_cb (GtkWidget * widget, GdkEventMotion * event,
			gpointer user_data)
{
	GtkXine *gtx = GTK_XINE (user_data);

	g_signal_emit (G_OBJECT (gtx),
		       gtx_table_signals[MOUSE_MOTION], 0, NULL);

	return TRUE;
}

void
gtk_xine_set_fullscreen (GtkXine *gtx, gboolean fullscreen)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	if (fullscreen == gtx->priv->fullscreen_mode)
		return;

	XLockDisplay (gtx->priv->display);

	gtx->priv->fullscreen_mode = fullscreen;

	if (gtx->priv->fullscreen_mode)
	{
		GdkWindowAttr attr;

		attr.title = DEFAULT_TITLE;
		attr.event_mask = GDK_STRUCTURE_MASK | GDK_EXPOSURE_MASK
			| GDK_FOCUS_CHANGE_MASK;
		attr.x = 0;
		attr.y = 0;
		attr.width = gdk_screen_width ();
		attr.height = gdk_screen_height ();
		attr.wclass = GDK_INPUT_OUTPUT;
		attr.window_type = GDK_WINDOW_TOPLEVEL;
		attr.override_redirect = FALSE;

		gtx->priv->fullscreen_window = gdk_window_new
			(gdk_get_default_root_window (), &attr,
			 GDK_WA_TITLE | GDK_WA_X | GDK_WA_Y);

		/* TODO add check for full-screen from fullscreen_callback
		 * in terminal-window.c (profterm) */
		gdk_window_set_fullscreen (gtx->priv->fullscreen_window, TRUE);

		/* Map window. */
		gdk_window_show (gtx->priv->fullscreen_window);
		gdk_window_raise (gtx->priv->fullscreen_window);

		/* Wait for map */
		gdk_flush ();

/*FIXME */ 	XSetInputFocus (gtx->priv->display, GDK_WINDOW_XWINDOW (gdk_window_get_toplevel (gtk_widget_get_parent_window (GTK_WIDGET (gtx)))), RevertToNone, CurrentTime);

		gtx->priv->vo_driver->gui_data_exchange
			(gtx->priv->vo_driver,
			 GUI_DATA_EX_DRAWABLE_CHANGED,
			 (void*) GDK_WINDOW_XID (gtx->priv->fullscreen_window));

		/* switch off mouse cursor */
		gtk_xine_set_show_cursor (gtx, FALSE);

		/* setup the mouse motion stuff */
		gtx->priv->invisible = gtk_invisible_new ();
		gtk_widget_show (gtx->priv->invisible);
		gdk_window_set_user_data (gtx->priv->fullscreen_window,
				gtx->priv->invisible);
		gdk_window_set_events (gtx->priv->fullscreen_window,
				GDK_POINTER_MOTION_MASK);

		g_signal_connect (GTK_OBJECT (gtx->priv->invisible),
				"motion-notify-event",
				GTK_SIGNAL_FUNC (motion_notify_event_cb), gtx);

		/* Disable the screen saver */
		scrsaver_disable (gtx->priv->display);

		/* FIXME Bogger, sometimes it seems that some Expose event
		 * is going down the drain and Xine doesn't redraw us */
#if 0
		{
			GdkRectangle rect;

			rect.x = 0;
			rect.y = 0;
			rect.width = gdk_screen_height ();
			rect.height = gdk_screen_width ();
			gdk_window_invalidate_rect
				(gtx->priv->fullscreen_window,
				&rect, FALSE);
		}
		gdk_window_move (gtx->priv->fullscreen_window, 0, 0);
#endif
	} else {
		gtx->priv->vo_driver->gui_data_exchange
			(gtx->priv->vo_driver,
			 GUI_DATA_EX_DRAWABLE_CHANGED,
			 (void *) gtx->priv->video_window);

		gdk_window_set_fullscreen (gtx->priv->fullscreen_window, FALSE);

		gdk_window_destroy (gtx->priv->fullscreen_window);
		gtk_widget_destroy (gtx->priv->invisible);
		gtx->priv->invisible = NULL;

		scrsaver_enable (gtx->priv->display);
	}

	XUnlockDisplay (gtx->priv->display);
}

gint
gtk_xine_is_fullscreen (GtkXine * gtx)
{

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return gtx->priv->fullscreen_mode;
}

gboolean
gtk_xine_can_set_volume (GtkXine *gtx)
{
	static int can_set_volume = FALSE;
	int caps, mixer;

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	if (can_set_volume == TRUE)
		return can_set_volume;

	caps = xine_get_audio_capabilities (gtx->priv->xine);
	if (caps & AO_CAP_PCM_VOL)
		mixer = AO_PROP_PCM_VOL;
	else if (caps & AO_CAP_MIXER_VOL)
		mixer = AO_PROP_MIXER_VOL;

	if (caps & (AO_CAP_PCM_VOL | AO_CAP_MIXER_VOL))
		can_set_volume = TRUE;

	gtx->priv->mixer = mixer;

	return can_set_volume;
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
		xine_set_audio_property (gtx->priv->xine,
				gtx->priv->mixer, volume);
	}
}

gint
gtk_xine_get_volume (GtkXine *gtx)
{
	int volume = 0;

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	if (gtk_xine_can_set_volume (gtx) == TRUE)
	{
		volume = xine_get_audio_property (gtx->priv->xine,
				gtx->priv->mixer);
	}

	return volume;
}

void
gtk_xine_set_show_cursor (GtkXine *gtx, gboolean show_cursor)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	if (gtk_xine_is_fullscreen (gtx) == FALSE)
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

//FIXME
gchar **
gtk_xine_get_autoplay_plugins (GtkXine * gtx)
{
	g_return_val_if_fail (gtx != NULL, NULL);
	g_return_val_if_fail (GTK_IS_XINE (gtx), NULL);
	g_return_val_if_fail (gtx->priv->xine != NULL, NULL);

	return xine_get_autoplay_input_plugin_ids (gtx->priv->xine);
}

gint
gtk_xine_get_current_time (GtkXine * gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return xine_get_current_time (gtx->priv->xine);
}

gint
gtk_xine_get_stream_length (GtkXine * gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return xine_get_stream_length (gtx->priv->xine);
}

gboolean
gtk_xine_is_playing (GtkXine * gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return xine_get_status (gtx->priv->xine) == XINE_PLAY;
}

gboolean
gtk_xine_is_seekable (GtkXine * gtx)
{
	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return xine_is_stream_seekable (gtx->priv->xine);
}

void
gtk_xine_toggle_aspect_ratio (GtkXine *gtx)
{
	int tmp;

	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	tmp = gtx->priv->vo_driver->get_property (gtx->priv->vo_driver,
			VO_PROP_ASPECT_RATIO);
	gtx->priv->vo_driver->set_property (gtx->priv->vo_driver,
			VO_PROP_ASPECT_RATIO, tmp + 1);
}

void
gtk_xine_set_scale_ratio (GtkXine *gtx, gfloat ratio)
{
	GtkWindow *toplevel;
	uint8_t *y, *u, *v;
	int width, height, ratio_code, format;
	int new_w, new_h;

	y = u = v = NULL;

	if (gtx->priv->fullscreen_mode == TRUE)
		return;

	if (xine_get_current_frame(gtx->priv->xine,
				&width, &height, &ratio_code, &format,
				&y, &u, &v) == 0)
		return;

	D ("Orig width: %d height: %d", width, height);

	new_w = width * ratio;
	new_h = height * ratio;

	D ("New width: %d height: %d", new_w, new_h);

	/* y, u and v don't need to be freed apparently */
/*FIXME	g_free (y);
	g_free (u);
	g_free (v);*/

	/* don't scale to something bigger than the screen, and leave us
	 * some room */
	if (new_w > (gdk_screen_width () - 128) ||
			new_h > (gdk_screen_height () - 128))
		return;

	/* Massive resize hack... actually it's just tricky and not
	 * too hacky */
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

