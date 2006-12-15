/* Totem Plugin Viewer
 *
 * Copyright © 2004-2006 Bastien Nocera <hadess@hadess.net>
 * Copyright © 2002 David A. Schleef <ds@schleef.org>
 * Copyright © 2006 Christian Persch
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>

#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <totem-pl-parser.h>

#include <dbus/dbus-glib.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "bacon-video-widget.h"
#include "totem-interface.h"
#include "totem-statusbar.h"
#include "bacon-volume.h"
#include "video-utils.h"

#include "totem-plugin-viewer-commands.h"
#include "totem-plugin-viewer-options.h"
#include "totempluginviewer-marshal.h"

GtkWidget *totem_statusbar_create (void);
GtkWidget *totem_volume_create (void);

#define VOLUME_DOWN_OFFSET -8
#define VOLUME_UP_OFFSET 8

/* For newer D-Bus version */
#ifndef DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT
#define DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT 0
#endif

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED,
	LAST_STATE
} TotemStates;

typedef enum {
	TOTEM_PLUGIN_TYPE_BASIC,
	TOTEM_PLUGIN_TYPE_GMP,
	TOTEM_PLUGIN_TYPE_COMPLEX,
	TOTEM_PLUGIN_TYPE_NARROWSPACE,
	TOTEM_PLUGIN_TYPE_MULLY,
	TOTEM_PLUGIN_TYPE_LAST
} TotemPluginType;

#define TOTEM_TYPE_EMBEDDED (totem_embedded_get_type ())
#define TOTEM_EMBEDDED(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_EMBEDDED, TotemEmbedded))
#define TOTEM_EMBEDDED_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_EMBEDDED, TotemEmbeddedClass))
#define TOTEM_IS_EMBEDDED(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_EMBEDDED))
#define TOTEM_IS_EMBEDDED_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_EMBEDDED))
#define TOTEM_EMBEDDED_GET_CLASS(o)(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_EMBEDDED, TotemEmbeddedClass))

typedef GObjectClass TotemEmbeddedClass;

typedef struct _TotemEmbedded {
	GObject parent;

	GtkWidget *window;
	GladeXML *menuxml, *xml;
	GtkWidget *about;
	TotemStatusbar *statusbar;
	int width, height;
	const char *mimetype;
	char *base_uri;
	char *current_uri;
	char *href_uri;
	char *target;
	char *stream_uri;
	BaconVideoWidget *bvw;
	TotemStates state;
	GdkCursor *cursor;

	/* Playlist */
	GList *playlist, *current;
	guint parser_id;
	int num_items;

	/* Open menu item */
	GnomeVFSMimeApplication *app;
	GtkWidget *menu_item;

	/* Seek bits */
	GtkAdjustment *seekadj;
	GtkWidget *seek;

	/* Error */

	guint type : 3; /* TotemPluginType */

	guint is_browser_stream : 1;
	guint is_playlist : 1;
	guint controller_hidden : 1;
	guint show_statusbar : 1;
	guint hidden : 1;
	guint repeat : 1;
	guint seeking : 1;
	guint noautostart : 1;
	guint autostart : 1;
} TotemEmbedded;

GType totem_embedded_get_type (void);

#define TOTEM_EMBEDDED_ERROR_QUARK (g_quark_from_static_string ("TotemEmbeddedErrorQuark"))

enum
{
	TOTEM_EMBEDDED_UNKNOWN_PLUGIN_TYPE,
	TOTEM_EMBEDDED_SETWINDOW_UNSUPPORTED_CONTROLS,
	TOTEM_EMBEDDED_SETWINDOW_HAVE_WINDOW,
	TOTEM_EMBEDDED_SETWINDOW_INVALID_XID,
	TOTEM_EMBEDDED_NO_URI,
	TOTEM_EMBEDDED_OPEN_FAILED,
	TOTEM_EMBEDDED_UNKNOWN_COMMAND
};

G_DEFINE_TYPE (TotemEmbedded, totem_embedded, G_TYPE_OBJECT);
static void totem_embedded_init (TotemEmbedded *emb) { }

static gboolean totem_embedded_do_command (TotemEmbedded *emb, const char *command, GError **err);
static gboolean totem_embedded_push_parser (gpointer data);
static gboolean totem_embedded_play (TotemEmbedded *embedded, GError **error);

static void totem_embedded_clear_playlist (TotemEmbedded *embedded);

static void totem_embedded_update_menu (TotemEmbedded *emb);
static void on_open1_activate (GtkButton *button, TotemEmbedded *emb);

enum {
	BUTTON_PRESS,
	START_STREAM,
	STOP_STREAM,
	LAST_SIGNAL
};
static int signals[LAST_SIGNAL] = { 0 };

static void
totem_embedded_finalize (GObject *object)
{
	TotemEmbedded *embedded = TOTEM_EMBEDDED (object);

	if (embedded->window)
		gtk_widget_destroy (embedded->window);

	if (embedded->xml)
		g_object_unref (embedded->xml);
	if (embedded->menuxml)
		g_object_unref (embedded->menuxml);

	/* FIXME etc */

	G_OBJECT_CLASS (totem_embedded_parent_class)->finalize (object);
}

static void totem_embedded_class_init (TotemEmbeddedClass *klass)
{
	GType param_types[2];
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = totem_embedded_finalize;

	param_types[0] = param_types[1] = G_TYPE_UINT;
	signals[BUTTON_PRESS] =
		g_signal_newv ("button-press",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				NULL /* class closure */,
				NULL /* accu */, NULL /* accu data */,
				totempluginviewer_marshal_VOID__UINT_UINT,
				G_TYPE_NONE, 2, param_types);
	signals[START_STREAM] =
		g_signal_newv ("start-stream",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				NULL /* class closure */,
				NULL /* accu */, NULL /* accu data */,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE,
				0, NULL);
	signals[STOP_STREAM] =
		g_signal_newv ("stop-stream",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				NULL /* class closure */,
				NULL /* accu */, NULL /* accu data */,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE,
				0, NULL);
}

static void
totem_embedded_exit (TotemEmbedded *emb)
{
	//FIXME what happens when embedded, and we can't go on?
	exit (1);
}

static void
totem_embedded_error_and_exit (char *title, char *reason, TotemEmbedded *emb)
{
	/* FIXME send a signal to the plugin with the error message instead! */
	totem_interface_error_blocking (title, reason,
			GTK_WINDOW (emb->window));
	totem_embedded_exit (emb);
}

static void
totem_embedded_set_error (TotemEmbedded *emb,
			  char *primary,
			  char *secondary)
{
	/* FIXME */
	g_message ("totem_embedded_set_error: '%s', '%s'", primary, secondary);
}

static void
totem_embedded_set_state (TotemEmbedded *emb, TotemStates state)
{
	GtkWidget *image;
	GdkCursor *cursor;
	char id[32];
	static const char *states[] = {
		"PLAYING",
		"PAUSED",
		"STOPPED",
		"INVALID"
	};

	if (state == emb->state)
		return;

	g_message ("Viewer state: %s", states[state]);

	cursor = NULL;
	image = glade_xml_get_widget (emb->xml, "emb_pp_button_image");

	switch (state) {
	case STATE_STOPPED:
		g_snprintf (id, sizeof (id), "gtk-media-play-%s",
			    gtk_widget_get_direction (image) ? "ltr" : "rtl");
		totem_statusbar_set_text (emb->statusbar, _("Stopped"));
		totem_statusbar_set_time_and_length (emb->statusbar, 0, 0);
		break;
	case STATE_PAUSED:
		g_snprintf (id, sizeof (id), "gtk-media-play-%s",
				gtk_widget_get_direction (image) ? "ltr" : "rtl");
		totem_statusbar_set_text (emb->statusbar, _("Paused"));
		break;
	case STATE_PLAYING:
		g_snprintf (id, sizeof (id), "gtk-media-pause");
		totem_statusbar_set_text (emb->statusbar, _("Playing"));
		break;
	default:
		break;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (image), id, GTK_ICON_SIZE_MENU);

	emb->state = state;
}

static void
totem_embedded_set_pp_state (TotemEmbedded *emb, gboolean state)
{
	GtkWidget *item;

	item = glade_xml_get_widget (emb->xml, "pp_button");
	gtk_widget_set_sensitive (item, state);
}

static gboolean
totem_embedded_open_internal (TotemEmbedded *emb,
			      GError **error)
{
	GError *err = NULL;
	gboolean retval;
	const char *uri;

	/* FIXME: stop previous content, or is that automatic ? */

	if (emb->is_browser_stream)
		uri = "fd://0";
	else
		uri = emb->current_uri;

	if (!uri) {
		g_set_error (error,
			     TOTEM_EMBEDDED_ERROR_QUARK,
			     TOTEM_EMBEDDED_NO_URI,
			     _("No URI to play"));
		//FIXME totem_embedded_set_error (emb, error); |error| may be null?

		return FALSE;
	}

	g_message ("totem_embedded_open_internal '%s' is-browser-stream %d", uri, emb->is_browser_stream);

	bacon_video_widget_set_logo_mode (emb->bvw, FALSE);

	/* FIXME: remove |err| and rely on async on_error? */
	g_message ("BEFORE _open");
	retval = bacon_video_widget_open (emb->bvw, uri, &err);
	g_message ("AFTER _open");
	if (retval == FALSE)
	{
		GError *errint;
		char *primary;

		/* FIXME we haven't even started sending yet! */
		//g_signal_emit (emb, signals[STOP_STREAM], 0);

		totem_embedded_set_state (emb, STATE_STOPPED);
		bacon_video_widget_set_logo_mode (emb->bvw, TRUE);

		errint = g_error_new (TOTEM_EMBEDDED_ERROR_QUARK,
				      TOTEM_EMBEDDED_OPEN_FAILED,
				      _("Totem could not play '%s'"),
				     emb->current_uri);
		
		//FIXME disp = gnome_vfs_unescape_string_for_display (totem->mrl); ?
		primary = g_strdup_printf(_("Totem could not play '%s'"), emb->current_uri);
		totem_embedded_set_error (emb, primary, err->message);;
		g_free (primary);

		g_propagate_error (error, err);

		totem_embedded_set_pp_state (emb, FALSE);
	} else {
		/* FIXME this doesn't deal correctly with playlists */
		//if (emb->autostart)
			totem_embedded_play (emb, NULL);

		//totem_embedded_set_state (emb, STATE_PAUSED);
		totem_embedded_set_pp_state (emb, TRUE);
	}

	totem_embedded_update_menu (emb);

	return retval;
}

static gboolean
totem_embedded_play (TotemEmbedded *emb,
		     GError **error)
{
	if (bacon_video_widget_play (emb->bvw, NULL))
		totem_embedded_set_state (emb, STATE_PLAYING);

	return TRUE;
}

static gboolean
totem_embedded_pause (TotemEmbedded *emb,
		      GError **error)
{
	bacon_video_widget_pause (emb->bvw);
	totem_embedded_set_state (emb, STATE_PAUSED);

	return TRUE;
}

static gboolean
totem_embedded_stop (TotemEmbedded *emb,
		     GError **error)
{
	bacon_video_widget_stop (emb->bvw);
	totem_embedded_set_state (emb, STATE_STOPPED);

	return TRUE;
}

static gboolean
totem_embedded_do_command (TotemEmbedded *embedded,
			   const char *command,
			   GError **error)
{
	g_return_val_if_fail (command != NULL, FALSE);

	if (strcmp (command, TOTEM_COMMAND_PLAY) == 0) {
		return totem_embedded_play (embedded, error);
	}
	if (strcmp (command, TOTEM_COMMAND_PAUSE) == 0) {
		return totem_embedded_pause (embedded, error);
	}
	if (strcmp (command, TOTEM_COMMAND_STOP) == 0) {
		return totem_embedded_stop (embedded, error);
	}
		
	g_set_error (error,
		     TOTEM_EMBEDDED_ERROR_QUARK,
		     TOTEM_EMBEDDED_UNKNOWN_COMMAND,
		     "Unknown command '%s'", command);
	return FALSE;
}

static gboolean
totem_embedded_set_href (TotemEmbedded *embedded,
			 const char *href_uri,
			 const char *target,
			 GError *error)
{
	g_free (embedded->href_uri);
	g_free (embedded->target);

	if (href_uri != NULL) {
		embedded->href_uri = g_strdup (href_uri);
		gdk_window_set_cursor
			(GTK_WIDGET (embedded->bvw)->window,
			 embedded->cursor);
	} else {
		embedded->href_uri = NULL;
		gdk_window_set_cursor
			(GTK_WIDGET (embedded->bvw)->window, NULL);
	}

	if (target != NULL) {
		embedded->target = g_strdup (target);
	} else {
		embedded->target = NULL;
	}

	return TRUE;
}

/* Copied from nautilus-program-choosing.c */

extern char **environ;

/* Cut and paste from gdkspawn-x11.c */
static gchar **
my_gdk_spawn_make_environment_for_screen (GdkScreen  *screen,
					  gchar     **envp)
{
  gchar **retval = NULL;
  gchar  *display_name;
  gint    display_index = -1;
  gint    i, env_len;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  if (envp == NULL)
    envp = environ;

  for (env_len = 0; envp[env_len]; env_len++)
    if (strncmp (envp[env_len], "DISPLAY", strlen ("DISPLAY")) == 0)
      display_index = env_len;

  retval = g_new (char *, env_len + 1);
  retval[env_len] = NULL;

  display_name = gdk_screen_make_display_name (screen);

  for (i = 0; i < env_len; i++)
    if (i == display_index)
      retval[i] = g_strconcat ("DISPLAY=", display_name, NULL);
    else
      retval[i] = g_strdup (envp[i]);

  g_assert (i == env_len);

  g_free (display_name);

  return retval;
}


static void
sn_error_trap_push (SnDisplay *display,
		    Display   *xdisplay)
{
	gdk_error_trap_push ();
}

static void
sn_error_trap_pop (SnDisplay *display,
		   Display   *xdisplay)
{
	gdk_error_trap_pop ();
}

static char **
make_spawn_environment_for_sn_context (SnLauncherContext *sn_context,
				       char             **envp)
{
	char **retval;
	int    i, j;

	retval = NULL;
	
	if (envp == NULL) {
		envp = environ;
	}
	
	for (i = 0; envp[i]; i++) {
		/* Count length */
	}

	retval = g_new (char *, i + 2);

	for (i = 0, j = 0; envp[i]; i++) {
		if (!g_str_has_prefix (envp[i], "DESKTOP_STARTUP_ID=")) {
			retval[j] = g_strdup (envp[i]);
			++j;
	        }
	}

	retval[j] = g_strdup_printf ("DESKTOP_STARTUP_ID=%s",
				     sn_launcher_context_get_startup_id (sn_context));
	++j;
	retval[j] = NULL;

	return retval;
}

/* This should be fairly long, as it's confusing to users if a startup
 * ends when it shouldn't (it appears that the startup failed, and
 * they have to relaunch the app). Also the timeout only matters when
 * there are bugs and apps don't end their own startup sequence.
 *
 * This timeout is a "last resort" timeout that ignores whether the
 * startup sequence has shown activity or not.  Metacity and the
 * tasklist have smarter, and correspondingly able-to-be-shorter
 * timeouts. The reason our timeout is dumb is that we don't monitor
 * the sequence (don't use an SnMonitorContext)
 */
#define STARTUP_TIMEOUT_LENGTH (30 /* seconds */ * 1000)

typedef struct
{
	GdkScreen *screen;
	GSList *contexts;
	guint timeout_id;
} StartupTimeoutData;

static void
free_startup_timeout (void *data)
{
	StartupTimeoutData *std;

	std = data;

	g_slist_foreach (std->contexts,
			 (GFunc) sn_launcher_context_unref,
			 NULL);
	g_slist_free (std->contexts);

	if (std->timeout_id != 0) {
		g_source_remove (std->timeout_id);
		std->timeout_id = 0;
	}

	g_free (std);
}

static gboolean
startup_timeout (void *data)
{
	StartupTimeoutData *std;
	GSList *tmp;
	GTimeVal now;
	int min_timeout;

	std = data;

	min_timeout = STARTUP_TIMEOUT_LENGTH;
	
	g_get_current_time (&now);
	
	tmp = std->contexts;
	while (tmp != NULL) {
		SnLauncherContext *sn_context;
		GSList *next;
		long tv_sec, tv_usec;
		double elapsed;
		
		sn_context = tmp->data;
		next = tmp->next;
		
		sn_launcher_context_get_last_active_time (sn_context,
							  &tv_sec, &tv_usec);

		elapsed =
			((((double)now.tv_sec - tv_sec) * G_USEC_PER_SEC +
			  (now.tv_usec - tv_usec))) / 1000.0;

		if (elapsed >= STARTUP_TIMEOUT_LENGTH) {
			std->contexts = g_slist_remove (std->contexts,
							sn_context);
			sn_launcher_context_complete (sn_context);
			sn_launcher_context_unref (sn_context);
		} else {
			min_timeout = MIN (min_timeout, (STARTUP_TIMEOUT_LENGTH - elapsed));
		}
		
		tmp = next;
	}

	if (std->contexts == NULL) {
		std->timeout_id = 0;
	} else {
		std->timeout_id = g_timeout_add (min_timeout,
						 startup_timeout,
						 std);
	}

	/* always remove this one, but we may have reinstalled another one. */
	return FALSE;
}

static void
add_startup_timeout (GdkScreen         *screen,
		     SnLauncherContext *sn_context)
{
	StartupTimeoutData *data;

	data = g_object_get_data (G_OBJECT (screen), "nautilus-startup-data");
	if (data == NULL) {
		data = g_new (StartupTimeoutData, 1);
		data->screen = screen;
		data->contexts = NULL;
		data->timeout_id = 0;
		
		g_object_set_data_full (G_OBJECT (screen),
					"nautilus-startup-data",
					data, free_startup_timeout);		
	}

	sn_launcher_context_ref (sn_context);
	data->contexts = g_slist_prepend (data->contexts, sn_context);
	
	if (data->timeout_id == 0) {
		data->timeout_id = g_timeout_add (STARTUP_TIMEOUT_LENGTH,
						  startup_timeout,
						  data);		
	}
}

static gboolean
totem_embedded_launch_player (TotemEmbedded *embedded,
			      const char *uri,
 			      guint32 user_time,
			      GError *error)
{
	GList *uris = NULL;
	GdkScreen *screen;
	GnomeVFSResult result;
	SnLauncherContext *sn_context;
	SnDisplay *sn_display;
	char **envp;

	g_return_val_if_fail (embedded->app != NULL, FALSE);

	if (uri != NULL) {
		uris = g_list_prepend (uris, (gpointer) uri);
	} else if (embedded->type == TOTEM_PLUGIN_TYPE_NARROWSPACE
		   && embedded->href_uri != NULL) {
		uris = g_list_prepend (uris, embedded->href_uri);
	} else {
		uris = g_list_prepend (uris, embedded->current_uri);
	}

	screen = gtk_widget_get_screen (embedded->window);
	g_return_val_if_fail (screen != NULL, FALSE);
	envp = my_gdk_spawn_make_environment_for_screen (screen, NULL);

	sn_display = sn_display_new (gdk_display,
				     sn_error_trap_push,
				     sn_error_trap_pop);

	if (gnome_vfs_mime_application_supports_startup_notification (embedded->app)) {
		char *name;

		sn_context = sn_launcher_context_new (sn_display,
						      gdk_screen_get_number (screen));

		name = g_filename_display_basename (uri);
		if (name != NULL) {
			char *description;

			sn_launcher_context_set_name (sn_context, name);
			description = g_strdup_printf (_("Opening %s"), name);
			sn_launcher_context_set_description (sn_context,
							     description);
			g_free (name);
			g_free (description);
		}

		if (!sn_launcher_context_get_initiated (sn_context)) {
			const char *binary_name;
			char **old_envp;

			binary_name = gnome_vfs_mime_application_get_binary_name
				(embedded->app);

			sn_launcher_context_set_binary_name (sn_context,
							     binary_name);

			sn_launcher_context_initiate (sn_context,
						      g_get_prgname (),
						      binary_name,
						      (Time) user_time);

			old_envp = envp;
			envp = make_spawn_environment_for_sn_context
				(sn_context, envp);
			g_strfreev (old_envp);
		}
	} else {
		sn_context = NULL;
  	}
  
	result = gnome_vfs_mime_application_launch_with_env (embedded->app,
							     uris, envp);

	if (sn_context != NULL) {
		if (result != GNOME_VFS_OK) {
			/* end sequence */
			sn_launcher_context_complete (sn_context);
		} else {
			add_startup_timeout (screen, sn_context);
		}
		sn_launcher_context_unref (sn_context);
	}

	sn_display_unref (sn_display);

	g_list_free (uris);
	g_strfreev (envp);

	return (result == GNOME_VFS_OK);
}

static void
totem_embedded_set_uri (TotemEmbedded *emb,
		        const char *uri,
		        const char *base_uri,
		        gboolean is_browser_stream)
{
	char *old_uri, *old_base;

	old_uri = emb->current_uri;
	old_base = emb->base_uri;

	emb->current_uri = g_strdup (uri);
	emb->base_uri = g_strdup (base_uri);
	emb->is_browser_stream = (is_browser_stream != FALSE);

	g_free (old_uri);
	g_free (old_base);
}

static gboolean
totem_embedded_open_uri (TotemEmbedded *emb,
			 const char *uri,
			 const char *base_uri,
			 GError **error)
{
	totem_embedded_clear_playlist (emb);

	bacon_video_widget_close (emb->bvw);

	totem_embedded_set_uri (emb, uri, base_uri, FALSE);

	return totem_embedded_open_internal (emb, error);
}

static gboolean
totem_embedded_open_stream (TotemEmbedded *emb,
			    const char *uri,
			    const char *base_uri,
			    GError **error)
{
	totem_embedded_clear_playlist (emb);

	bacon_video_widget_close (emb->bvw);

	totem_embedded_set_uri (emb, uri, base_uri, TRUE);

	/* FIXME: consume any remaining input from stdin */

	return totem_embedded_open_internal (emb, error);
}

static gboolean
totem_embedded_close_stream (TotemEmbedded *emb,
			     GError *error)
{
	if (!emb->is_browser_stream)
		return TRUE;

	/* FIXME this enough? */
	bacon_video_widget_close (emb->bvw);

	return TRUE;
}

static gboolean
totem_embedded_open_playlist_item (TotemEmbedded *emb,
				   GList *item)
{
	gboolean eop;

	if (!emb->playlist)
		return FALSE;

	eop = item == NULL;

	/* Start at the head */
	if (item == NULL)
		item = emb->playlist;

	/* FIXME: if (emb->current == item) { just start again, depending on repeat/autostart settings } */
	emb->current = item;
	g_assert (item != NULL);

	totem_embedded_set_uri (emb,
				(const char *) item->data,
			        emb->base_uri /* FIXME? */,
			        FALSE);

	bacon_video_widget_close (emb->bvw);
	if (totem_embedded_open_internal (emb, NULL /* FIXME */) &&
	    ((eop != FALSE && emb->repeat != FALSE)
		|| (eop == FALSE))) {
		totem_embedded_play (emb, NULL);
	}

	return TRUE;
}

static gboolean
totem_embedded_set_local_file (TotemEmbedded *emb,
			       const char *path,
			       const char *uri,
			       const char *base_uri,
			       GError **error)
{
	char *file_uri;

	g_message ("Setting the current path to %s", path);

	totem_embedded_clear_playlist (emb);

	file_uri = g_filename_to_uri (path, NULL, error);
	if (!file_uri)
		return FALSE;

	/* FIXME what about |uri| param?!! */
	totem_embedded_set_uri (emb, file_uri, base_uri, FALSE);
	g_free (file_uri);

	return totem_embedded_open_internal (emb, error);
}

static gboolean
totem_embedded_set_playlist (TotemEmbedded *emb,
			     const char *path,
			     const char *uri,
			     const char *base_uri,
			     GError **error)
{
	char *file_uri;

	g_message ("Setting the current playlist to %s (base: %s)",
		   path, base_uri);

	totem_embedded_clear_playlist (emb);

	file_uri = g_filename_to_uri (path, NULL, error);
	if (!file_uri)
		return FALSE;

	totem_embedded_set_uri (emb, file_uri, base_uri, FALSE);
	g_free (file_uri);

	/* Schedule parsing on idle */
	if (emb->parser_id == 0)
		emb->parser_id = g_idle_add (totem_embedded_push_parser,
					     emb);

	return TRUE;
}

static void
totem_embedded_update_menu (TotemEmbedded *emb)
{
	GtkWidget *menu, *item, *image;
	char *label;

	if (emb->menu_item != NULL) {
		gtk_widget_destroy (emb->menu_item);
		emb->menu_item = NULL;
	}
	if (emb->app != NULL) {
		gnome_vfs_mime_application_free (emb->app);
		emb->app = NULL;
	}

	if (emb->mimetype) {
		emb->app = gnome_vfs_mime_get_default_application_for_uri
				(emb->current_uri, emb->mimetype);
	} else {
		emb->app = gnome_vfs_mime_get_default_application_for_uri
				(emb->current_uri,
				 gnome_vfs_get_mime_type_for_name (emb->current_uri));
	}

	if (emb->app == NULL)
		return;

	/* translators: this is:
	 * Open With ApplicationName
	 * as in nautilus' right-click menu */
	label = g_strdup_printf ("_Open with \"%s\"", emb->app->name);
	item = gtk_image_menu_item_new_with_mnemonic (label);
	g_free (label);
	image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (on_open1_activate), emb);
	gtk_widget_show (item);
	emb->menu_item = item;

	menu = glade_xml_get_widget (emb->menuxml, "menu");
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
}

static void
on_open1_activate (GtkButton *button, TotemEmbedded *emb)
{
	GTimeVal val;
	g_get_current_time (&val);
	totem_embedded_launch_player (emb, NULL, val.tv_sec, NULL);
	totem_embedded_stop (emb, NULL);
}

static void
on_about1_activate (GtkButton *button, TotemEmbedded *emb)
{
	char *backend_version, *description;

	const char *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
		"Ronald Bultje <rbultje@ronald.bitfreak.net>",
		"Christian Persch" " <" "chpe" "@" "gnome" "." "org" ">",
		NULL
	};
	
	if (emb->about != NULL)
	{
		gtk_window_present (GTK_WINDOW (emb->about));
		return;
	}

	backend_version = bacon_video_widget_get_backend_name (emb->bvw);
	description = g_strdup_printf (_("Browser Plugin using %s"),
				       backend_version);

	emb->about = g_object_new (GTK_TYPE_ABOUT_DIALOG,
				   "name", _("Totem Browser Plugin"),
				   "version", VERSION,
				   "copyright", _("Copyright \xc2\xa9 2002-2006 Bastien Nocera"),
				   "comments", description,
				   "authors", authors,
				   "translator-credits", _("translator-credits"),
				   "logo-icon-name", "totem",
				   NULL);

	g_free (backend_version);
	g_free (description);

	totem_interface_set_transient_for (GTK_WINDOW (emb->about),
					   GTK_WINDOW (emb->window));

	g_object_add_weak_pointer (G_OBJECT (emb->about),
				   (gpointer *)&emb->about);
	g_signal_connect (G_OBJECT (emb->about), "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (emb->about);
}

static void
on_copy_location1_activate (GtkButton *button, TotemEmbedded *emb)
{
	GdkDisplay *display;
	GtkClipboard *clip;
	const char *uri;

	if (emb->is_browser_stream) {
		uri = emb->base_uri; /* FIXME! */
	} else if (emb->href_uri != NULL) {
		uri = emb->href_uri;
	} else {
		uri = emb->current_uri;
	}

	display = gtk_widget_get_display (GTK_WIDGET (emb->window));

	/* Set both the middle-click and the super-paste buffers */
	clip = gtk_clipboard_get_for_display (display,
					      GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clip, uri, -1);

	clip = gtk_clipboard_get_for_display (display,
					      GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clip, uri, -1);
}

static void
on_play_pause (GtkWidget *widget, TotemEmbedded *emb)
{
	if (emb->state == STATE_PLAYING) {
		totem_embedded_pause (emb, NULL);
	} else {
		totem_embedded_play (emb, NULL);
	}
}

static void
on_got_redirect (GtkWidget *bvw, const char *mrl, TotemEmbedded *emb)
{
	char *new_uri = NULL;

	g_message ("url: %s", emb->base_uri);
	g_message ("redirect: %s", mrl);

	bacon_video_widget_close (emb->bvw);

	if (emb->current_uri)
		new_uri = totem_resolve_relative_link (emb->current_uri, mrl);
	/* FIXME: not sure that this is actually correct... */
	else if (emb->base_uri)
		new_uri = totem_resolve_relative_link (emb->base_uri, mrl);

	if (!new_uri)
		return;

	g_message ("Redirecting to '%s'", new_uri);

	/* FIXME: clear playlist? or replace current entry? or add a new entry? */
	/* FIXME: use totem_embedded_open_uri? */

	totem_embedded_set_uri (emb, new_uri, emb->base_uri /* FIXME? */, FALSE);

	totem_embedded_set_state (emb, STATE_STOPPED);

	if (totem_embedded_open_internal (emb, NULL /* FIXME? */) != FALSE)
		totem_embedded_play (emb, NULL);
}

static gboolean
on_video_button_press_event (BaconVideoWidget *bvw,
			     GdkEventButton *event,
			     TotemEmbedded *emb)
{
	guint state = event->state & gtk_accelerator_get_default_mod_mask ();
	gboolean handled = FALSE;

	//g_print ("button-press type %d button %d state %d play-state %d\n",
	//	 event->type, event->button, state, emb->state);

	if (event->type == GDK_BUTTON_PRESS &&
	    event->button == 1 &&
	    state == 0 &&
	    emb->state == STATE_STOPPED) {
		g_message ("emitting signal");
		g_signal_emit (emb, signals[BUTTON_PRESS], 0,
			       event->time,
			       event->button);
	} else if (event->type == GDK_BUTTON_PRESS &&
		   event->button == 3 &&
		   state == 0) {
		GtkMenu *menu;

		menu = GTK_MENU (glade_xml_get_widget (emb->menuxml, "menu"));
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
				event->button, event->time);

		handled = TRUE;
	}

	return handled;
}

static void
on_eos_event (GtkWidget *bvw, TotemEmbedded *emb)
{
	totem_embedded_set_state (emb, STATE_STOPPED);
	gtk_adjustment_set_value (emb->seekadj, 0);

	/* FIXME: the plugin needs to handle EOS itself, e.g. for QTNext */

	/* No playlist if we have fd://0, right? */
	if (emb->is_browser_stream) {
		/* FIXME: totem_embedded_set_state missing? */
		/* FIXME: should find a way to enable playback of the stream again without re-requesting it */
		totem_embedded_set_pp_state (emb, FALSE);
	/* FIXME? else if (emb->playing_nth_item == emb->playlist_num_items) ? */
	} else if (emb->num_items == 1) {
		if (g_str_has_prefix (emb->current_uri, "file://") != FALSE) {
			if (bacon_video_widget_is_seekable (emb->bvw) != FALSE) {
				bacon_video_widget_pause (emb->bvw);
				bacon_video_widget_seek (emb->bvw, 0.0, NULL);
			} else {
				bacon_video_widget_close (emb->bvw);
				totem_embedded_open_internal (emb, NULL /* FIXME? */);
			}
		} else {
			bacon_video_widget_close (emb->bvw);
			totem_embedded_open_internal (emb, NULL /* FIXME? */);
		}
		if (emb->repeat != FALSE && emb->autostart)
			totem_embedded_play (emb, NULL);
	} else if (emb->current) {
		totem_embedded_open_playlist_item (emb, emb->current->next);
	}
}

static void
on_error_event (BaconVideoWidget *bvw,
		char *message,
                gboolean playback_stopped,
		gboolean fatal,
		TotemEmbedded *emb)
{
	if (playback_stopped) {
		/* FIXME: necessary? */
		if (emb->is_browser_stream)
			g_signal_emit (emb, signals[STOP_STREAM], 0);
	
		totem_embedded_set_state (emb, STATE_STOPPED);
	}

	if (fatal) {
		/* FIXME: report error back to plugin */
		exit (1);
	}

	totem_embedded_set_error (emb, _("An error occurred"), message);
}

static void
cb_vol (GtkWidget *val, TotemEmbedded *emb)
{
	bacon_video_widget_set_volume (emb->bvw,
		bacon_volume_button_get_value (BACON_VOLUME_BUTTON (val)));
}

static gboolean
on_volume_scroll_event (GtkWidget *win, GdkEventScroll *event, TotemEmbedded *emb)
{
	GtkWidget *vbut;
	int vol, offset;

	switch (event->direction) {
	case GDK_SCROLL_UP:
		offset = VOLUME_UP_OFFSET;
		break;
	case GDK_SCROLL_DOWN:
		offset = VOLUME_DOWN_OFFSET;
		break;
	default:
		return FALSE;
	}

	vbut = glade_xml_get_widget (emb->xml, "volume_button");
	vol = bacon_volume_button_get_value (BACON_VOLUME_BUTTON (vbut));
	bacon_volume_button_set_value (BACON_VOLUME_BUTTON (vbut),
			vol + offset);

	return FALSE;
}

static void
on_tick (GtkWidget *bvw,
		gint64 current_time,
		gint64 stream_length,
		float current_position,
		gboolean seekable,
		TotemEmbedded *emb)
{
	if (emb->state != STATE_STOPPED) {
		gtk_widget_set_sensitive (emb->seek, seekable);
		if (emb->seeking == FALSE)
			gtk_adjustment_set_value (emb->seekadj,
					current_position * 65535);
		if (stream_length == 0) {
			totem_statusbar_set_time_and_length (emb->statusbar,
					(int) (current_time / 1000), -1);
		} else {
			totem_statusbar_set_time_and_length (emb->statusbar,
					(int) (current_time / 1000),
					(int) (stream_length / 1000));
		}
	}
}

static gboolean
on_seek_start (GtkWidget *widget, GdkEventButton *event, TotemEmbedded *emb)
{
	emb->seeking = TRUE;

	return FALSE;
}

static gboolean
cb_on_seek (GtkWidget *widget, GdkEventButton *event, TotemEmbedded *emb)
{
	bacon_video_widget_seek (emb->bvw,
		gtk_range_get_value (GTK_RANGE (widget)) / 65535, NULL);
	emb->seeking = FALSE;

	return FALSE;
}

#ifdef GNOME_ENABLE_DEBUG
static void
controls_size_allocate_cb (GtkWidget *controls,
			   GtkAllocation *allocation,
			   gpointer data)
{
	g_message ("Viewer: Controls height is %dpx", allocation->height);
	g_signal_handlers_disconnect_by_func (controls, G_CALLBACK (controls_size_allocate_cb), NULL);
}
#endif

static gboolean
totem_embedded_construct (TotemEmbedded *emb,
			  GdkNativeWindow xid,
			  int width,
			  int height)
{
	GtkWidget *child, *container, *pp_button, *vbut;
	BvwUseType type;
	GError *err = NULL;
	GConfClient *gc;
	char *logo_path;
	int volume;

	if (xid != 0) {
		g_assert (!emb->hidden);

		emb->window = gtk_plug_new (xid);

		/* Can't do anything before it's realized */
		gtk_widget_realize (emb->window);

		emb->xml = totem_interface_load_with_root ("mozilla-viewer.glade",
				"content_box", _("Plugin") /* FIXME! */, TRUE,
				GTK_WINDOW (emb->window));
		g_assert (emb->xml);

		child = glade_xml_get_widget (emb->xml, "content_box");
		gtk_container_add (GTK_CONTAINER (emb->window), child);
	} else {
		g_assert (emb->hidden);

		emb->xml = totem_interface_load_with_root ("mozilla-viewer.glade",
				"window", _("Plugin") /* FIXME! */, TRUE,
				GTK_WINDOW (emb->window));
		g_assert (emb->xml);

		emb->window = glade_xml_get_widget (emb->xml, "window");
	}

	if (emb->hidden)
		type = BVW_USE_TYPE_AUDIO;
	else
		type = BVW_USE_TYPE_VIDEO;

	emb->bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new
			(-1, -1, type, &err));

	/* FIXME! */
	if (emb->bvw == NULL)
	{
		/* FIXME! */
		/* FIXME construct and show error message */
		totem_embedded_error_and_exit (_("The Totem plugin could not startup."), err != NULL ? err->message : _("No reason."), emb);

		if (err != NULL)
			g_error_free (err);
	}

	logo_path = g_build_filename (SHAREDIR, LOGO_NAME, NULL);
	bacon_video_widget_set_logo (emb->bvw, logo_path);
	g_free (logo_path);

	bacon_video_widget_set_logo_mode (emb->bvw, TRUE);

	g_signal_connect (G_OBJECT(emb->bvw), "got-redirect",
			G_CALLBACK (on_got_redirect), emb);
	g_signal_connect (G_OBJECT (emb->bvw), "eos",
			G_CALLBACK (on_eos_event), emb);
	g_signal_connect (G_OBJECT (emb->bvw), "error",
			G_CALLBACK (on_error_event), emb);
	g_signal_connect (G_OBJECT(emb->bvw), "button-press-event",
			G_CALLBACK (on_video_button_press_event), emb);
	g_signal_connect (G_OBJECT(emb->bvw), "tick",
			G_CALLBACK (on_tick), emb);

	container = glade_xml_get_widget (emb->xml, "video_box");
	gtk_container_add (GTK_CONTAINER (container), GTK_WIDGET (emb->bvw));
	if (type == BVW_USE_TYPE_VIDEO) {
		/* FIXME: why can't this wait until the whole window is realised? */
		gtk_widget_realize (GTK_WIDGET (emb->bvw));
		gtk_widget_show (GTK_WIDGET (emb->bvw));
	}

	emb->seek = glade_xml_get_widget (emb->xml, "time_hscale");
	emb->seekadj = gtk_range_get_adjustment (GTK_RANGE (emb->seek));
	g_signal_connect (emb->seek, "button-press-event",
			  G_CALLBACK (on_seek_start), emb);
	g_signal_connect (emb->seek, "button-release-event",
			  G_CALLBACK (cb_on_seek), emb);

	pp_button = glade_xml_get_widget (emb->xml, "pp_button");
	g_signal_connect (G_OBJECT (pp_button), "clicked",
			  G_CALLBACK (on_play_pause), emb);

	gc = gconf_client_get_default ();
	volume = gconf_client_get_int (gc, GCONF_PREFIX"/volume", NULL);
	g_object_unref (G_OBJECT (gc));

	bacon_video_widget_set_volume (emb->bvw, volume);
	vbut = glade_xml_get_widget (emb->xml, "volume_button");
	bacon_volume_button_set_value (BACON_VOLUME_BUTTON (vbut), volume);
	g_signal_connect (G_OBJECT (vbut), "value-changed",
			  G_CALLBACK (cb_vol), emb);
	gtk_widget_add_events (vbut, GDK_SCROLL_MASK);
	g_signal_connect (G_OBJECT (vbut), "scroll_event",
			G_CALLBACK (on_volume_scroll_event), emb);

	emb->statusbar = TOTEM_STATUSBAR (glade_xml_get_widget (emb->xml, "statusbar"));

	gtk_widget_set_size_request (emb->window, width, height);

#if GNOME_ENABLE_DEBUG
	child = glade_xml_get_widget (emb->xml, "controls");
	g_signal_connect_after (child, "size-allocate", G_CALLBACK (controls_size_allocate_cb), NULL);
#endif

	if (emb->controller_hidden != FALSE) {
		child = glade_xml_get_widget (emb->xml, "controls");
		gtk_widget_hide (child);
	}

	if (!emb->show_statusbar) {
		child = glade_xml_get_widget (emb->xml, "statusbar");
		gtk_widget_hide (child);
	}

	/* Try to make controls smaller */
	{
        	GtkRcStyle *rcstyle;

		rcstyle = gtk_rc_style_new ();
		rcstyle->xthickness = rcstyle->ythickness = 0;

		child = glade_xml_get_widget (emb->xml, "pp_button");
		gtk_widget_modify_style (child, rcstyle);
		
		child = glade_xml_get_widget (emb->xml, "time_hscale");
		gtk_widget_modify_style (child, rcstyle);

		child = glade_xml_get_widget (emb->xml, "volume_button");
		gtk_widget_modify_style (child, rcstyle);

		gtk_rc_style_unref (rcstyle);
	}

	totem_embedded_set_state (emb, STATE_STOPPED);

	if (!emb->hidden) {
		gtk_widget_show (emb->window);
	}

	/* popup */
	emb->menuxml = totem_interface_load_with_root ("mozilla-viewer.glade",
			"menu", _("Menu"), TRUE,
			GTK_WINDOW (emb->window));
	g_assert (emb->menuxml);

	child = glade_xml_get_widget (emb->menuxml, "about1");
	g_signal_connect (G_OBJECT (child), "activate",
			  G_CALLBACK (on_about1_activate), emb);
	child = glade_xml_get_widget (emb->menuxml, "copy_location1");
	g_signal_connect (G_OBJECT (child), "activate",
			G_CALLBACK (on_copy_location1_activate), emb);
	child = glade_xml_get_widget (emb->menuxml, "preferences1");
	gtk_widget_hide (child);

	/* Create cursor */
	if (!emb->hidden) {
		emb->cursor = gdk_cursor_new_for_display
			(gtk_widget_get_display (emb->window),
			 GDK_HAND2);
	}

	return TRUE;
}

static gboolean
totem_embedded_construct_placeholder (TotemEmbedded *emb,
			  GdkNativeWindow xid,
			  int width,
			  int height)
{
	GtkWidget *window, *label;

	if (xid == 0)
		return TRUE;

	window = gtk_plug_new (xid);
	/* FIXME why? */
	gtk_widget_realize (window);

	label = gtk_label_new ("Not yet supported");
	gtk_container_add (GTK_CONTAINER (window), label);

	gtk_widget_show_all (window);

	return TRUE;
}

static gboolean
totem_embedded_set_window (TotemEmbedded *embedded,
			   const char *controls,
			   guint window,
			   int width,
			   int height,
			   GError **error)
{
	g_print ("Viewer: SetWindow XID %u size %d:%d\n", window, width, height);

	if (embedded->type == TOTEM_PLUGIN_TYPE_COMPLEX) {
		/* FIXME!!! */
		if (strcmp (controls, "All") != 0 &&
		    strcmp (controls, "ImageWindow") != 0) {
			totem_embedded_construct_placeholder (embedded, (GdkNativeWindow) window, width, height);
			return TRUE;
		}
	} else {
		if (strcmp (controls, "All") != 0 &&
		    strcmp (controls, "ImageWindow") != 0) {
			g_set_error (error,
				TOTEM_EMBEDDED_ERROR_QUARK,
				TOTEM_EMBEDDED_SETWINDOW_UNSUPPORTED_CONTROLS,
				"Unsupported controls '%s'", controls);
			return FALSE;
		}
	}

	if (embedded->window != NULL) {
		g_warning ("Viewer: Already have a window!");

		g_set_error (error,
			     TOTEM_EMBEDDED_ERROR_QUARK,
			     TOTEM_EMBEDDED_SETWINDOW_HAVE_WINDOW,
			     "Already have a window");
		return FALSE;
	}

	if (window == 0) {
		g_set_error (error,
			     TOTEM_EMBEDDED_ERROR_QUARK,
			     TOTEM_EMBEDDED_SETWINDOW_INVALID_XID,
			     "Invalid XID");
		return FALSE;
	}

	embedded->width = width;
	embedded->height = height;

	totem_embedded_construct (embedded, (GdkNativeWindow) window,
				  width, height);

	return TRUE;
}

static gboolean
totem_embedded_unset_window (TotemEmbedded *embedded,
			    guint window,
			    GError **error)
{
	g_warning ("UnsetWindow unimplemented");
	return TRUE;
}

static void
totem_embedded_clear_playlist (TotemEmbedded *embedded)
{
	g_list_foreach (embedded->playlist, (GFunc) g_free, NULL);
	g_list_free (embedded->playlist);

	embedded->playlist = NULL;
	embedded->current = NULL;
	embedded->num_items = 0;
}

static void
entry_added (TotemPlParser *parser,
	     const char *uri,
	     const char *title,
	     const char *genre,
	     gpointer data)
{
	GList **list = (GList **) data;

	g_print ("added URI '%s' with title '%s' genre '%s'\n", uri,
			title ? title : "empty", genre);

	//FIXMEchpe
	//FIXME need new struct to hold that
	*list = g_list_prepend (*list, g_strdup (uri));
}

static gboolean
totem_embedded_push_parser (gpointer data)
{
	TotemEmbedded *emb = (TotemEmbedded *) data;
	TotemPlParser *parser;
	TotemPlParserResult res;
	GList *list = NULL;

	emb->parser_id = 0;

	totem_embedded_clear_playlist (emb);

	parser = totem_pl_parser_new ();
	g_object_set (parser, "force", TRUE,
			      "disable-unsafe", TRUE,
			       NULL);
	g_signal_connect (parser, "entry", G_CALLBACK (entry_added), &list);
	res = totem_pl_parser_parse_with_base (parser, emb->current_uri,
					       emb->base_uri, FALSE);
	g_object_unref (parser);

	if (res != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		//FIXME show a proper error message
		switch (res) {
		case TOTEM_PL_PARSER_RESULT_UNHANDLED:
			g_print ("url '%s' unhandled\n", emb->current_uri);
			break;
		case TOTEM_PL_PARSER_RESULT_ERROR:
			g_print ("error handling url '%s'\n", emb->current_uri);
			break;
		case TOTEM_PL_PARSER_RESULT_IGNORED:
			g_print ("ignored url '%s'\n", emb->current_uri);
			break;
		default:
			g_assert_not_reached ();
			;;
		}
	}

	/* Check if we have anything in the playlist now */
	if (list == NULL) {
		g_message ("No playlist or playlist empty");
		totem_embedded_set_error (emb, _("No playlist or playlist empty") /* FIXME */,
					  NULL);
		return FALSE;
	}

	emb->playlist = g_list_reverse (list);
	emb->num_items = g_list_length (emb->playlist);

	totem_embedded_open_playlist_item (emb, NULL /* first */);

	/* don't run again */
	return FALSE;
}

G_MODULE_EXPORT GtkWidget *
totem_volume_create (void)
{
	GtkWidget *widget;

	widget = bacon_volume_button_new (GTK_ICON_SIZE_MENU,
					  0, 100, -1);
	gtk_button_set_focus_on_click (GTK_BUTTON (widget), FALSE);
	gtk_widget_show (widget);

	return widget;
}

G_MODULE_EXPORT GtkWidget *
totem_statusbar_create (void)
{
	GtkWidget *widget;

	widget = totem_statusbar_new ();
	totem_statusbar_set_has_resize_grip (TOTEM_STATUSBAR (widget), FALSE);
	gtk_widget_show (widget);

	return widget;
}

static char *arg_user_agent = NULL;
static char *arg_mime_type = NULL;
static char **arg_remaining = NULL;
static gboolean arg_no_controls = FALSE;
static gboolean arg_statusbar = FALSE;
static gboolean arg_hidden = FALSE;
static gboolean arg_is_playlist = FALSE;
static gboolean arg_repeat = FALSE;
static gboolean arg_no_autostart = FALSE;
static TotemPluginType arg_plugin_type = TOTEM_PLUGIN_TYPE_LAST;

static gboolean
parse_plugin_type (const gchar *option_name,
	           const gchar *value,
	           gpointer data,
	           GError **error)
{
	const char types[TOTEM_PLUGIN_TYPE_LAST][12] = {
		"basic",
		"gmp",
		"complex",
		"narrowspace",
		"mully"
	};
	TotemPluginType type;

	for (type = 0; type < TOTEM_PLUGIN_TYPE_LAST; ++type) {
		if (strcmp (value, types[type]) == 0) {
			arg_plugin_type = type;
			return TRUE;
		}
	}

	g_print ("Unknown plugin type '%s'\n", value);
	exit (1);
}

static GOptionEntry option_entries [] =
{
	{ TOTEM_OPTION_PLUGIN_TYPE, 0, 0, G_OPTION_ARG_CALLBACK, parse_plugin_type, NULL, NULL },
	{ TOTEM_OPTION_USER_AGENT, 0, 0, G_OPTION_ARG_STRING, &arg_user_agent, NULL, NULL },
	{ TOTEM_OPTION_MIMETYPE, 0, 0, G_OPTION_ARG_STRING, &arg_mime_type, NULL, NULL },
	{ TOTEM_OPTION_CONTROLS_HIDDEN, 0, 0, G_OPTION_ARG_NONE, &arg_no_controls, NULL, NULL },
	{ TOTEM_OPTION_STATUSBAR, 0, 0, G_OPTION_ARG_NONE, &arg_statusbar, NULL, NULL },
	{ TOTEM_OPTION_HIDDEN, 0, 0, G_OPTION_ARG_NONE, &arg_hidden, NULL, NULL },
	{ TOTEM_OPTION_PLAYLIST, 0, 0, G_OPTION_ARG_NONE, &arg_is_playlist, NULL, NULL },
	{ TOTEM_OPTION_REPEAT, 0, 0, G_OPTION_ARG_NONE, &arg_repeat, NULL, NULL },
	{ TOTEM_OPTION_NOAUTOSTART, 0, 0, G_OPTION_ARG_NONE, &arg_no_autostart, NULL, NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY /* STRING? */, &arg_remaining, NULL },
	{ NULL }
};

#include "totem-plugin-viewer-interface.h"

int main (int argc, char **argv)
{
	TotemEmbedded *emb;
	DBusGProxy *proxy;
	DBusGConnection *conn;
	guint res;
	GError *e = NULL;
	char svcname[256];

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

#ifdef GNOME_ENABLE_DEBUG
	{
		int i;
		g_print ("Viewer: PID %d, args: ", getpid ());
		for (i = 0; i < argc; i++)
			g_print ("%s ", argv[i]);
		g_print ("\n");
	}
#endif

	if (XInitThreads () == 0)
	{
		gtk_init (&argc, &argv);
		totem_embedded_error_and_exit (_("Could not initialise the thread-safe libraries."), _("Verify your system installation. The Totem plugin will now exit."), NULL);
	}

	g_thread_init (NULL);
	dbus_g_thread_init ();

#ifdef GNOME_ENABLE_DEBUG
{
	const char *env;

	env = g_getenv ("TOTEM_EMBEDDED_GDB");
	if (env && g_ascii_strtoull (env, NULL, 10) == 1) {
		char *gdbargv[6];
		char pid[32];
		GError *gdberr = NULL;
		int gdbargc = 0;

		g_snprintf (pid, sizeof (pid), "%d", getpid ());

		gdbargv[gdbargc++] = "/usr/bin/xterm";
		gdbargv[gdbargc++] = "-e";
		gdbargv[gdbargc++] = "gdb";
		gdbargv[gdbargc++] = argv[0];
		gdbargv[gdbargc++] = pid;
		gdbargv[gdbargc++] = NULL;

		if (!g_spawn_async (NULL,
				    gdbargv,
				    NULL /* env */,
				    0,
				    NULL, NULL,
				    NULL,
				    &gdberr)) {
			g_warning ("Failed to spawn debugger: %s", gdberr->message);
			g_error_free (gdberr);
		} else {
			g_print ("Sleeping....\n");
			g_usleep (10* 1000 * 1000); /* 10s */
		}
	}
}
#endif

	if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &e))
	{
		g_print ("%s\n", e->message);
		g_error_free (e);
		exit (1);
	}

        // FIXME check that ALL necessary params were given!
	if (arg_plugin_type == TOTEM_PLUGIN_TYPE_LAST) {
		g_warning ("Plugin type is required\n");
		exit (1);
	}

	/* FIXME: check the UA strings of the legacy plugins themselves */
	/* FIXME: at least hxplayer seems to send different UAs depending on the protocol!? */
	if (arg_user_agent != NULL) {
		g_setenv ("GNOME_VFS_HTTP_USER_AGENT", arg_user_agent, TRUE);
		g_free (arg_user_agent);
		arg_user_agent = NULL;
	}

	bacon_video_widget_init_backend (NULL, NULL);
	gnome_vfs_init ();

	dbus_g_object_type_install_info (TOTEM_TYPE_EMBEDDED,
					 &dbus_glib_totem_embedded_object_info);
	g_snprintf (svcname, sizeof (svcname),
		    TOTEM_PLUGIN_VIEWER_NAME_TEMPLATE, getpid());

	if (!(conn = dbus_g_bus_get (DBUS_BUS_SESSION, &e))) {
		g_warning ("Failed to get DBUS connection: %s", e->message);
		g_error_free (e);
		exit (1);
	}

	proxy = dbus_g_proxy_new_for_name (conn,
					   "org.freedesktop.DBus",
					   "/org/freedesktop/DBus",
					   "org.freedesktop.DBus");
	g_assert (proxy != NULL);

	if (!dbus_g_proxy_call (proxy, "RequestName", &e,
	     			G_TYPE_STRING, svcname,
				G_TYPE_UINT, DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT,
				G_TYPE_INVALID,
				G_TYPE_UINT, &res,
				G_TYPE_INVALID)) {
		g_warning ("RequestName for '%s'' failed: %s\n",
			   svcname, e->message);
		g_error_free (e);

		exit (1);
	}

	emb = g_object_new (TOTEM_TYPE_EMBEDDED, NULL);

	emb->state = LAST_STATE;
	emb->width = -1;
	emb->height = -1;
	emb->controller_hidden = arg_no_controls;
	emb->show_statusbar = arg_statusbar;
	emb->current_uri = arg_remaining ? arg_remaining[0] : NULL;
	emb->mimetype = arg_mime_type;
	emb->hidden = arg_hidden;
	emb->is_playlist = arg_is_playlist;
	emb->repeat = arg_repeat;
	emb->autostart = !arg_no_autostart;
	emb->type = arg_plugin_type;

	/* FIXME: register this BEFORE requesting the service name? */
	dbus_g_connection_register_g_object
	(conn, TOTEM_PLUGIN_VIEWER_DBUS_PATH, G_OBJECT (emb));

	/* If we're hidden, construct a hidden window;
	 * else wait to be plugged in.
	 */
	if (emb->hidden) {
		totem_embedded_construct (emb, 0, -1, -1);
	}

	gtk_main ();

	g_object_unref (emb);

	g_message ("Viewer [PID %d]: exiting\n", getpid ());

	return 0;
}
