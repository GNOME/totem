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
#include <X11/cursorfont.h>
/* gtk+/gnome */
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkinvisible.h>
#include <libgnome/gnome-i18n.h>
/* xine */
#include <xine/video_out_x11.h>

#include "debug.h"
#include "gtk-xine.h"
#include "gtkxine-marshal.h"

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
	xine_t *xine;
	char *configfile;
	config_values_t *config;
	vo_driver_t *vo_driver;
	ao_driver_t *ao_driver;
	Display *display;
	int screen;
	Window video_window;
	GC gc;
	pthread_t thread;
	int completion_event;
	int mixer;

	int xpos, ypos;

	GAsyncQueue *queue;

	/* fullscreen stuff */
	gboolean fullscreen_mode;
	int fullscreen_width, fullscreen_height;
	Window fullscreen_window, toplevel;
	Cursor no_cursor;
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
	gtx->widget.requisition.width = 420;
	gtx->widget.requisition.height = 315;

	gtx->priv = g_new0 (GtkXinePrivate, 1);
	gtx->priv->config = NULL;
	gtx->priv->xine = NULL;
	gtx->priv->vo_driver = NULL;
	gtx->priv->ao_driver = NULL;
	gtx->priv->display = NULL;
	gtx->priv->fullscreen_mode = FALSE;
	gtx->priv->mixer = -1;
	gtx->priv->cursor_shown = TRUE;

	gtx->priv->queue = g_async_queue_new ();
}

static void
dest_size_cb (void *gtx_gen,
	      int video_width, int video_height,
	      int *dest_width, int *dest_height)
{
	GtkXine *gtx = (GtkXine *) gtx_gen;

	if (gtx->priv->fullscreen_mode) {
		*dest_width = gtx->priv->fullscreen_width;
		*dest_height = gtx->priv->fullscreen_height;
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
	if (gtx->priv->fullscreen_mode) {
		*dest_width = gtx->priv->fullscreen_width;
		*dest_height = gtx->priv->fullscreen_height;
	} else {
		*dest_width = gtx->widget.allocation.width;
		*dest_height = gtx->widget.allocation.height;
	}
}

static vo_driver_t *
load_video_out_driver (GtkXine * gtx)
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
	res_h =
	    (DisplayWidth (gtx->priv->display, gtx->priv->screen) * 1000 /
	     DisplayWidthMM (gtx->priv->display, gtx->priv->screen));
	res_v =
	    (DisplayHeight (gtx->priv->display, gtx->priv->screen) * 1000 /
	     DisplayHeightMM (gtx->priv->display, gtx->priv->screen));
	vis.display_ratio = res_h / res_v;

	if (fabs (vis.display_ratio - 1.0) < 0.01) {
		vis.display_ratio = 1.0;
	}

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
	if (!strcmp (video_driver_id, "auto")) {

		vo_driver =
		    xine_load_video_output_plugin (gtx->priv->config,
						   video_driver_id,
						   VISUAL_TYPE_X11,
						   (void *) &vis);
		if (vo_driver) {
			g_free (driver_ids);
			return vo_driver;
		}
	}

	i = 0;
	while (driver_ids[i])
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
			return vo_driver;
		}
		i++;
	}

	return NULL;
}

static ao_driver_t *
load_audio_out_driver (GtkXine * gtx)
{
	ao_driver_t *ao_driver = NULL;
	char *audio_driver_id;
	char **driver_ids = xine_list_audio_output_plugins ();
	int i = 0;

	/* try to init audio with stored information */
	audio_driver_id =
	    gtx->priv->config->register_string (gtx->priv->config,
						"audio.driver", "auto",
						"audio driver to use",
						NULL, NULL, NULL);

	if (strcmp (audio_driver_id, "auto"))
	{
		return xine_load_audio_output_plugin (gtx->priv->config,
						      audio_driver_id);
	}

	while (driver_ids[i])
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

			return ao_driver;
		}
		i++;
	}

	return ao_driver;
}

static void *
xine_thread (void *gtx_gen)
{
	GtkXine *gtx = (GtkXine *) gtx_gen;

	while (1) {
		XEvent event;

		XNextEvent (gtx->priv->display, &event);

		switch (event.type) {
		case Expose:
			if (event.xexpose.count != 0)
				break;

			gtx->priv->vo_driver->gui_data_exchange
				(gtx->priv->vo_driver,
				 GUI_DATA_EX_EXPOSE_EVENT, &event);
			break;
		case FocusIn:
			/* happens only in fullscreen mode */
			XLockDisplay (gtx->priv->display);
			XSetInputFocus (gtx->priv->display,
					gtx->priv->toplevel, RevertToNone,
					CurrentTime);
			XUnlockDisplay (gtx->priv->display);
			break;
		}

		if (event.type == gtx->priv->completion_event)
			gtx->priv->vo_driver->gui_data_exchange
				(gtx->priv->vo_driver,
				 GUI_DATA_EX_COMPLETION_EVENT, &event);
	}

	pthread_exit (NULL);
	return NULL;
}

static gboolean
configure_cb (GtkWidget * widget,
	      GdkEventConfigure * event, gpointer user_data)
{
	GtkXine *gtx;

	D ("CONFIGURE");
	gtx = GTK_XINE (user_data);

	gtx->priv->xpos = event->x;
	gtx->priv->ypos = event->y;

	return FALSE;
}

static unsigned char bm_no_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

static void
gtk_xine_realize (GtkWidget * widget)
{
	GtkXine *gtx;
	XGCValues values;
	Pixmap bm_no;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_XINE (widget));

	gtx = GTK_XINE (widget);

	/* set realized flag */
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	/* create our own video window */
	gtx->priv->video_window = XCreateSimpleWindow (gdk_display,
			GDK_WINDOW_XWINDOW (gtk_widget_get_parent_window (widget)),
			0, 0, widget->allocation.width,
			widget->allocation.height, 1,
			BLACK_PIXEL, BLACK_PIXEL);

	widget->window = gdk_window_foreign_new (gtx->priv->video_window);

	/* prepare for fullscreen playback */
	gtx->priv->fullscreen_width =
	    DisplayWidth (gdk_display, gdk_x11_get_default_screen ());
	gtx->priv->fullscreen_height =
	    DisplayHeight (gdk_display, gdk_x11_get_default_screen ());

	gtx->priv->toplevel =
	    GDK_WINDOW_XWINDOW (gdk_window_get_toplevel
				(gtk_widget_get_parent_window (widget)));

	/* track configure events of toplevel window */
	g_signal_connect (GTK_OBJECT (gtk_widget_get_toplevel (widget)),
			  "configure-event",
			  GTK_SIGNAL_FUNC (configure_cb), gtx);

	D("xine_thread: init threads");

	if (!XInitThreads ()) {
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				0,
				_("Could not initialise the threads support\n"
				"You should install a thread-safe Xlib."));
		return;
	}

	D ("xine_thread: open display\n");

	gtx->priv->display = XOpenDisplay (NULL);

	if (!gtx->priv->display) {
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				GTX_STARTUP,
				_("Failed to open the display\n"));
//FIXME					XOpenDisplay failed"));
		return;
	}

	XLockDisplay (gtx->priv->display);

	gtx->priv->screen = DefaultScreen (gtx->priv->display);

	if (XShmQueryExtension (gtx->priv->display) == True) {
		gtx->priv->completion_event =
		    XShmGetEventBase (gtx->priv->display) + ShmCompletion;
	} else {
		gtx->priv->completion_event = -1;
	}

	XSelectInput (gtx->priv->display, gtx->priv->video_window,
		      StructureNotifyMask | ExposureMask |
		      ButtonPressMask | PointerMotionMask);

	/* generate and init a config "object" */
	gtx->priv->configfile =
	    g_strdup_printf ("%s/.xine/config", g_get_home_dir ());
	gtx->priv->config = xine_config_file_init (gtx->priv->configfile);

	/* load audio, video drivers */
	gtx->priv->vo_driver = load_video_out_driver (gtx);

	if (!gtx->priv->vo_driver) {
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				GTX_STARTUP,
				_("couldn't open video driver"));
		return;
	}

	gtx->priv->ao_driver = load_audio_out_driver (gtx);

	/* init xine engine */
	gtx->priv->xine =
	    xine_init (gtx->priv->vo_driver, gtx->priv->ao_driver,
		       gtx->priv->config);

	values.foreground = BLACK_PIXEL;
	values.background = BLACK_PIXEL;

	gtx->priv->gc =
	    XCreateGC (gtx->priv->display, gtx->priv->video_window,
		       (GCForeground | GCBackground), &values);

	XUnlockDisplay (gtx->priv->display);

	/* create mouse cursors */
	bm_no = XCreateBitmapFromData (gtx->priv->display,
				       gtx->priv->video_window,
				       bm_no_data, 8, 8);
	gtx->priv->no_cursor =
	    XCreatePixmapCursor (gtx->priv->display, bm_no, bm_no,
				 (XColor *) & BLACK_PIXEL,
				 (XColor *) & BLACK_PIXEL, 0, 0);

	/* Setup xine events */
	xine_register_event_listener (gtx->priv->xine, xine_event,
			(void *) gtx);
	xine_register_report_codec_cb(gtx->priv->xine, codec_reporting,
			(void *) gtx);

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

	gdk_threads_enter ();
	switch (signal->type)
	{
	case ERROR:
		TE ();
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[ERROR], 0,
				signal->error_type, signal->message);
		TL ();
		break;
	case MOUSE_MOTION:
		/* mouse motion doesn't need to go through here */
		break;
	case EOS:
		D("EOS");
		g_signal_emit (G_OBJECT (gtx),
				gtx_table_signals[EOS], 0, NULL);
		D("EOS called");
		break;
	default:
		D("fooo");
	}

	g_free (signal->message);
	g_free (signal);

	queue_length = g_async_queue_length (gtx->priv->queue);
	gdk_threads_leave ();

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

	if( !handled )
	{
		if( codec_type == XINE_CODEC_VIDEO)
		{
			GtkXineSignal *signal;

			signal = g_new0 (GtkXineSignal, 1);
			signal->type = ERROR;
			
			/* display fourcc if no description available */
			if( !description[0])
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

	g_message ("xine_error: %d", error);
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

	/* stop event thread */
	pthread_cancel (gtx->priv->thread);
	xine_exit (gtx->priv->xine);

	/* save configuration */
	gtx->priv->config->save (gtx->priv->config);

	/* Hide all windows */
	if (GTK_WIDGET_MAPPED (widget))
		gtk_widget_unmap (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

	/* This destroys widget->window and unsets the realized flag */
	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

GtkWidget *
gtk_xine_new (void)
{
	GtkWidget *gtx = GTK_WIDGET (gtk_type_new (gtk_xine_get_type ()));
	return gtx;
}


static gint
gtk_xine_expose (GtkWidget * widget, GdkEventExpose * event)
{
	return FALSE;
}

static void
gtk_xine_size_allocate (GtkWidget * widget, GtkAllocation * allocation)
{
	GtkXine *gtx;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_XINE (widget));

	D ("ALLOCATE x: %d y: %d w: %d h: %d", allocation->x,
		      allocation->y, allocation->width,
		      allocation->height);

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

/* Commands */

gint
gtk_xine_play (GtkXine * gtx, gchar * mrl, gint pos, gint start_time)
{
	int error;

	g_return_val_if_fail (gtx != NULL, -1);
	g_return_val_if_fail (GTK_IS_XINE (gtx), -1);
	g_return_val_if_fail (gtx->priv->xine != NULL, -1);

	D ("gtkxine: calling xine_play start_pos = %d, start_time = %d\n",
	    pos, start_time);

	error = xine_play (gtx->priv->xine, mrl, pos, start_time);
	if (!error)
	{
		g_message ("BROKEN");
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
static void gtk_xine_set_property (GObject *object, guint property_id,
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
		//FIXME
		g_message ("change cursor to %d", g_value_get_boolean (value));
		break;
	default:
		g_assert_not_reached ();
	}
}

static void gtk_xine_get_property (GObject *object, guint property_id,
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
		//FIXME
		//g_value_set_boolean (value, 
		g_message ("set cursor");
		break;
	default:
		g_assert_not_reached ();
	}
}

void
gtk_xine_set_speed (GtkXine * gtx, gint speed)
{

	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	xine_set_speed (gtx->priv->xine, speed);
}

gint
gtk_xine_get_speed (GtkXine * gtx)
{

	g_return_val_if_fail (gtx != NULL, SPEED_NORMAL);
	g_return_val_if_fail (GTK_IS_XINE (gtx), SPEED_NORMAL);
	g_return_val_if_fail (gtx->priv->xine != NULL, SPEED_NORMAL);

	return xine_get_speed (gtx->priv->xine);
}

gint
gtk_xine_get_position (GtkXine * gtx)
{

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	if (gtk_xine_is_playing (gtx))
		return xine_get_current_position (gtx->priv->xine);
	else
		return 0;
}

void
gtk_xine_set_audio_channel (GtkXine * gtx, gint audio_channel)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	xine_select_audio_channel (gtx->priv->xine, audio_channel);
}

gint
gtk_xine_get_audio_channel (GtkXine * gtx)
{

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return xine_get_audio_selection (gtx->priv->xine);
}

#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct {
	uint32_t flags;
	uint32_t functions;
	uint32_t decorations;
	int32_t input_mode;
	uint32_t status;
} MWMHints;

static void
wmspec_change_xwindow_state (gboolean add,
			     Window window, GdkAtom state1, GdkAtom state2)
{
	XEvent xev;

#define _NET_WM_STATE_REMOVE        0	/* remove/unset property */
#define _NET_WM_STATE_ADD           1	/* add/set property */
#define _NET_WM_STATE_TOGGLE        2	/* toggle property  */

	xev.xclient.type = ClientMessage;
	xev.xclient.serial = 0;
	xev.xclient.send_event = True;
	xev.xclient.display = gdk_display;
	xev.xclient.window = window;
	xev.xclient.message_type =
	    gdk_x11_get_xatom_by_name ("_NET_WM_STATE");
	xev.xclient.format = 32;
	xev.xclient.data.l[0] =
	    add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = gdk_x11_atom_to_xatom (state1);
	xev.xclient.data.l[2] = gdk_x11_atom_to_xatom (state2);

	XSendEvent (gdk_display,
		    GDK_WINDOW_XID (gdk_get_default_root_window ()),
		    False,
		    SubstructureRedirectMask | SubstructureNotifyMask,
		    &xev);
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
gtk_xine_set_fullscreen (GtkXine * gtx, gboolean fullscreen)
{
	static char *window_title = "Totem";
	XSizeHints hint;
	Atom prop;
	MWMHints mwmhints;
	XEvent xev;
	GdkWindow *win;

	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	if (fullscreen == gtx->priv->fullscreen_mode)
		return;

	gtx->priv->fullscreen_mode = fullscreen;

	XLockDisplay (gtx->priv->display);

	if (gtx->priv->fullscreen_mode)
	{
		hint.x = 0;
		hint.y = 0;
		hint.width = gtx->priv->fullscreen_width;
		hint.height = gtx->priv->fullscreen_height;

		gtx->priv->fullscreen_window =
		    XCreateSimpleWindow (gtx->priv->display,
				    GDK_ROOT_WINDOW (),
				    0, 0,
				    gtx->priv->fullscreen_width,
				    gtx->priv->fullscreen_height, 1,
				    BLACK_PIXEL, BLACK_PIXEL);

		hint.win_gravity = StaticGravity;
		hint.flags = PPosition | PSize | PWinGravity;

		XSetStandardProperties (gtx->priv->display,
					gtx->priv->fullscreen_window,
					window_title, window_title, None,
					NULL, 0, 0);

		XSetWMNormalHints (gtx->priv->display,
				   gtx->priv->fullscreen_window, &hint);

		/* wm, no borders please */
		prop = XInternAtom (gtx->priv->display, "_MOTIF_WM_HINTS",
				 False);
		mwmhints.flags = MWM_HINTS_DECORATIONS;
		mwmhints.decorations = 0;
		XChangeProperty (gtx->priv->display,
				 gtx->priv->fullscreen_window, prop, prop,
				 32, PropModeReplace,
				 (unsigned char *) &mwmhints,
				 PROP_MWM_HINTS_ELEMENTS);

		XSetTransientForHint (gtx->priv->display,
				      gtx->priv->fullscreen_window, None);
		XRaiseWindow (gtx->priv->display,
			      gtx->priv->fullscreen_window);

		XSelectInput (gtx->priv->display,
			      gtx->priv->fullscreen_window,
			      StructureNotifyMask | ExposureMask |
			      FocusChangeMask);

		wmspec_change_xwindow_state (TRUE,
					     gtx->priv->fullscreen_window,
					     gdk_atom_intern
					     ("_NET_WM_STATE_FULLSCREEN",
					      FALSE), GDK_NONE);


		/* Map window. */
		XMapRaised (gtx->priv->display,
			    gtx->priv->fullscreen_window);

		XFlush (gtx->priv->display);

		/* Wait for map. */
		do {
			XMaskEvent (gtx->priv->display,
				    StructureNotifyMask, &xev);
		} while (xev.type != MapNotify
			 || xev.xmap.event !=
			 gtx->priv->fullscreen_window);

		XSetInputFocus (gtx->priv->display,
				gtx->priv->toplevel, RevertToNone,
				CurrentTime);
		XMoveWindow (gtx->priv->display,
			     gtx->priv->fullscreen_window, 0, 0);

		gtx->priv->vo_driver->gui_data_exchange
			(gtx->priv->vo_driver,
			 GUI_DATA_EX_DRAWABLE_CHANGED,
			 (void *) gtx->priv->fullscreen_window);

		/* switch off mouse cursor */
		gtk_xine_set_show_cursor (gtx, FALSE);

		/* setup the mouse motion stuff */
		win = gdk_window_foreign_new (gtx->priv->fullscreen_window);
		gtx->priv->invisible = gtk_invisible_new ();
		gtk_widget_show (gtx->priv->invisible);
		gdk_window_set_user_data (win,
				gtx->priv->invisible);
		gdk_window_set_events (win,
				GDK_POINTER_MOTION_MASK);

		g_signal_connect (GTK_OBJECT (gtx->priv->invisible),
				"motion-notify-event",
				GTK_SIGNAL_FUNC (motion_notify_event_cb), gtx);
	} else {
		gtx->priv->vo_driver->gui_data_exchange
			(gtx->priv->vo_driver,
			 GUI_DATA_EX_DRAWABLE_CHANGED,
			 (void *) gtx->priv->video_window);

		XDestroyWindow (gtx->priv->display,
				gtx->priv->fullscreen_window);
		gtx->priv->invisible = NULL;
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
	if(caps & AO_CAP_PCM_VOL)
		mixer = AO_PROP_PCM_VOL;
	else if(caps & AO_CAP_MIXER_VOL)
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

	if (volume < 0)
		volume = 0;
	else if (volume > 100)
		volume = 100;

	if(gtk_xine_can_set_volume (gtx) == TRUE)
	{
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

	if(gtk_xine_can_set_volume (gtx) == TRUE)
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

	gdk_error_trap_push ();
	if (show_cursor == FALSE)
	{
		XDefineCursor (gtx->priv->display,
				gtx->priv->fullscreen_window,
				gtx->priv->no_cursor);
	} else {
		XUndefineCursor (gtx->priv->display,
				gtx->priv->fullscreen_window);
	}
	XFlush (gtx->priv->display);
	gdk_flush ();
	gdk_error_trap_pop ();

	gtx->priv->cursor_shown = show_cursor;
}

gboolean
gtk_xine_get_cursor (GtkXine *gtx)
{
	g_return_val_if_fail (gtx != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_XINE (gtx), FALSE);
	g_return_val_if_fail (gtx->priv->xine != NULL, FALSE);

	return gtx->priv->cursor_shown;
}

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
gtk_xine_save_config (GtkXine * gtx)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	gtx->priv->config->save (gtx->priv->config);
}

void
gtk_xine_set_video_property (GtkXine * gtx, gint property, gint value)
{
	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	gtx->priv->vo_driver->set_property (gtx->priv->vo_driver, property,
					    value);
}

gint
gtk_xine_get_video_property (GtkXine * gtx, gint property)
{

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return gtx->priv->vo_driver->get_property (gtx->priv->vo_driver,
						   property);
}

void
gtk_xine_toggle_aspect_ratio (GtkXine * gtx)
{
	int tmp;

	g_return_if_fail (gtx != NULL);
	g_return_if_fail (GTK_IS_XINE (gtx));
	g_return_if_fail (gtx->priv->xine != NULL);

	tmp = gtk_xine_get_video_property (gtx, VO_PROP_ASPECT_RATIO);
	gtk_xine_set_video_property (gtx, VO_PROP_ASPECT_RATIO, tmp + 1);
}

gint
gtk_xine_get_log_section_count (GtkXine * gtx)
{

	g_return_val_if_fail (gtx != NULL, 0);
	g_return_val_if_fail (GTK_IS_XINE (gtx), 0);
	g_return_val_if_fail (gtx->priv->xine != NULL, 0);

	return xine_get_log_section_count (gtx->priv->xine);
}

gchar **
gtk_xine_get_log_names (GtkXine * gtx)
{
	g_return_val_if_fail (gtx != NULL, NULL);
	g_return_val_if_fail (GTK_IS_XINE (gtx), NULL);
	g_return_val_if_fail (gtx->priv->xine != NULL, NULL);

	return xine_get_log_names (gtx->priv->xine);
}

gchar **
gtk_xine_get_log (GtkXine * gtx, gint buf)
{

	g_return_val_if_fail (gtx != NULL, NULL);
	g_return_val_if_fail (GTK_IS_XINE (gtx), NULL);
	g_return_val_if_fail (gtx->priv->xine != NULL, NULL);

	return xine_get_log (gtx->priv->xine, buf);
}
