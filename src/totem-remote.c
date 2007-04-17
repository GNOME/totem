/* 
 *  Copyright (C) 2002 James Willcox  <jwillcox@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 */


#include <config.h>
#include <glib.h>
#include <string.h>

#include "totem-remote.h"

#ifdef HAVE_REMOTE

/* Media player keys support */
#ifdef HAVE_MEDIA_PLAYER_KEYS
#include <gnome-settings-daemon/gnome-settings-client.h>
#include "totem-marshal.h"
#endif /*HAVE_MEDIA_PLAYER_KEYS */

#ifdef HAVE_LIRC
#include <stdio.h>
#include <lirc/lirc_client.h>
#include <gconf/gconf-client.h>

/* strings that we recognize as commands from lirc */
#define TOTEM_IR_COMMAND_PLAY "play"
#define TOTEM_IR_COMMAND_PAUSE "pause"
#define TOTEM_IR_COMMAND_NEXT "next"
#define TOTEM_IR_COMMAND_PREVIOUS "previous"
#define TOTEM_IR_COMMAND_SEEK_FORWARD "seek_forward"
#define TOTEM_IR_COMMAND_SEEK_BACKWARD "seek_backward"
#define TOTEM_IR_COMMAND_VOLUME_UP "volume_up"
#define TOTEM_IR_COMMAND_VOLUME_DOWN "volume_down"
#define TOTEM_IR_COMMAND_FULLSCREEN "fullscreen"
#define TOTEM_IR_COMMAND_QUIT "quit"
#define TOTEM_IR_COMMAND_UP "up"
#define TOTEM_IR_COMMAND_DOWN "down"
#define TOTEM_IR_COMMAND_LEFT "left"
#define TOTEM_IR_COMMAND_RIGHT "right"
#define TOTEM_IR_COMMAND_SELECT "select"
#define TOTEM_IR_COMMAND_MENU "menu"
#define TOTEM_IR_COMMAND_PLAYPAUSE "play_pause"
#define TOTEM_IR_COMMAND_ZOOM_UP "zoom_up"
#define TOTEM_IR_COMMAND_ZOOM_DOWN "zoom_down"
#define TOTEM_IR_COMMAND_SHOW_PLAYING "show_playing"
#define TOTEM_IR_COMMAND_SHOW_VOLUME "show_volume"
#define TOTEM_IR_COMMAND_EJECT "eject"
#define TOTEM_IR_COMMAND_PLAY_DVD "play_dvd"
#define TOTEM_IR_COMMAND_MUTE "mute"
#endif /* HAVE_LIRC */

struct _TotemRemote {
	GObject parent;

	/* Media player keys suppport */
#ifdef HAVE_MEDIA_PLAYER_KEYS
	DBusGProxy *media_player_keys_proxy;
#endif
};

enum
{
	BUTTON_PRESSED,
	LAST_SIGNAL
};

static guint totem_remote_signals[LAST_SIGNAL] = { 0 };
#ifdef HAVE_LIRC
static GIOChannel *lirc_channel = NULL;
static GList *listeners = NULL;
#endif /* HAVE_LIRC */


G_DEFINE_TYPE(TotemRemote, totem_remote, G_TYPE_OBJECT)

#ifdef HAVE_LIRC
static TotemRemoteCommand
totem_lirc_to_command (const gchar *str)
{
	if (strcmp (str, TOTEM_IR_COMMAND_PLAY) == 0)
		return TOTEM_REMOTE_COMMAND_PLAY;
	else if (strcmp (str, TOTEM_IR_COMMAND_PAUSE) == 0)
		return TOTEM_REMOTE_COMMAND_PAUSE;
	else if (strcmp (str, TOTEM_IR_COMMAND_PLAYPAUSE) == 0)
		return TOTEM_REMOTE_COMMAND_PLAYPAUSE;
	else if (strcmp (str, TOTEM_IR_COMMAND_NEXT) == 0)
		return TOTEM_REMOTE_COMMAND_NEXT;
	else if (strcmp (str, TOTEM_IR_COMMAND_PREVIOUS) == 0)
		return TOTEM_REMOTE_COMMAND_PREVIOUS;
	else if (strcmp (str, TOTEM_IR_COMMAND_SEEK_FORWARD) == 0)
		return TOTEM_REMOTE_COMMAND_SEEK_FORWARD;
	else if (strcmp (str, TOTEM_IR_COMMAND_SEEK_BACKWARD) == 0)
		return TOTEM_REMOTE_COMMAND_SEEK_BACKWARD;
	else if (strcmp (str, TOTEM_IR_COMMAND_VOLUME_UP) == 0)
		return TOTEM_REMOTE_COMMAND_VOLUME_UP;
	else if (strcmp (str, TOTEM_IR_COMMAND_VOLUME_DOWN) == 0)
		return TOTEM_REMOTE_COMMAND_VOLUME_DOWN;
	else if (strcmp (str, TOTEM_IR_COMMAND_FULLSCREEN) == 0)
		return TOTEM_REMOTE_COMMAND_FULLSCREEN;
	else if (strcmp (str, TOTEM_IR_COMMAND_QUIT) == 0)
		return TOTEM_REMOTE_COMMAND_QUIT;
	else if (strcmp (str, TOTEM_IR_COMMAND_UP) == 0)
		return TOTEM_REMOTE_COMMAND_UP;
	else if (strcmp (str, TOTEM_IR_COMMAND_DOWN) == 0)
		return TOTEM_REMOTE_COMMAND_DOWN;
	else if (strcmp (str, TOTEM_IR_COMMAND_LEFT) == 0)
		return TOTEM_REMOTE_COMMAND_LEFT;
	else if (strcmp (str, TOTEM_IR_COMMAND_RIGHT) == 0)
		return TOTEM_REMOTE_COMMAND_RIGHT;
	else if (strcmp (str, TOTEM_IR_COMMAND_SELECT) == 0)
		return TOTEM_REMOTE_COMMAND_SELECT;
	else if (strcmp (str, TOTEM_IR_COMMAND_MENU) == 0)
		return TOTEM_REMOTE_COMMAND_DVD_MENU;
	else if (strcmp (str, TOTEM_IR_COMMAND_ZOOM_UP) == 0)
		return TOTEM_REMOTE_COMMAND_ZOOM_UP;
	else if (strcmp (str, TOTEM_IR_COMMAND_ZOOM_DOWN) == 0)
		return TOTEM_REMOTE_COMMAND_ZOOM_DOWN;
	else if (strcmp (str, TOTEM_IR_COMMAND_SHOW_PLAYING) == 0)
		return TOTEM_REMOTE_COMMAND_SHOW_PLAYING;
	else if (strcmp (str, TOTEM_IR_COMMAND_SHOW_VOLUME) == 0)
		return TOTEM_REMOTE_COMMAND_SHOW_VOLUME;
	else if (strcmp (str, TOTEM_IR_COMMAND_EJECT) == 0)
		return TOTEM_REMOTE_COMMAND_EJECT;
	else if (strcmp (str, TOTEM_IR_COMMAND_PLAY_DVD) == 0)
		return TOTEM_REMOTE_COMMAND_PLAY_DVD;
	else if (strcmp (str, TOTEM_IR_COMMAND_MUTE) == 0)
		return TOTEM_REMOTE_COMMAND_MUTE;
	else
		return TOTEM_REMOTE_COMMAND_UNKNOWN;
}

static struct lirc_config *config;

static gboolean
totem_remote_read_code (GIOChannel *source, GIOCondition condition,
		   gpointer user_data)
{
	char *code;
	char *str = NULL;
	GList *tmp;
	TotemRemoteCommand cmd;

	/* this _could_ block, but it shouldn't */
	lirc_nextcode (&code);

	if (code == NULL) {
		/* the code was incomplete or something */
		return TRUE;
	}
	
	if (lirc_code2char (config, code, &str) != 0) {
		g_message ("Couldn't convert lirc code to string.");
		return TRUE;
	}

	if (str == NULL) {
		/* there was no command associated with the code */
		g_free (code);
		return TRUE;
	}

	cmd = totem_lirc_to_command (str);

	tmp = listeners;
	while (tmp) {
		TotemRemote *remote = tmp->data;

		g_signal_emit (remote, totem_remote_signals[BUTTON_PRESSED], 0,
		       cmd);

		tmp = tmp->next;
	}

	g_free (code);

	/* this causes a crash, so I guess I'm not supposed to free it?
	 * g_free (str);
	 */

	return TRUE;
}
#endif /* HAVE_LIRC */

#ifdef HAVE_MEDIA_PLAYER_KEYS
static void
on_media_player_key_pressed (DBusGProxy *proxy, const gchar *application, const gchar *key, TotemRemote *remote)
{
	if (strcmp ("Totem", application) == 0) {
		if (strcmp ("Play", key) == 0)
			g_signal_emit (remote, totem_remote_signals[BUTTON_PRESSED], 0, TOTEM_REMOTE_COMMAND_PLAYPAUSE);
		else if (strcmp ("Previous", key) == 0)
			g_signal_emit (remote, totem_remote_signals[BUTTON_PRESSED], 0, TOTEM_REMOTE_COMMAND_PREVIOUS);
		else if (strcmp ("Next", key) == 0)
			g_signal_emit (remote, totem_remote_signals[BUTTON_PRESSED], 0, TOTEM_REMOTE_COMMAND_NEXT);
		else if (strcmp ("Stop", key) == 0)
			g_signal_emit (remote, totem_remote_signals[BUTTON_PRESSED], 0, TOTEM_REMOTE_COMMAND_PAUSE);
	}
}
#endif /* HAVE_MEDIA_PLAYER_KEYS */

static void
totem_remote_finalize (GObject *object)
{
	GError *error = NULL;
	TotemRemote *remote;

	g_return_if_fail (object != NULL);
	g_return_if_fail (TOTEM_IS_REMOTE (object));

	remote = TOTEM_REMOTE (object);

#ifdef HAVE_LIRC
	lirc_freeconfig (config);

	listeners = g_list_remove (listeners, remote);

	if (listeners == NULL && lirc_channel != NULL) {
		g_io_channel_shutdown (lirc_channel, FALSE, &error);
		if (error != NULL) {
			g_warning ("Couldn't destroy lirc connection: %s",
				   error->message);
			g_error_free (error);
		}

		lirc_deinit ();
	}
#endif /* HAVE_LIRC */

#ifdef HAVE_MEDIA_PLAYER_KEYS
	if (remote->media_player_keys_proxy != NULL)
		org_gnome_SettingsDaemon_release_media_player_keys (remote->media_player_keys_proxy, "Totem", NULL);
#endif /* HAVE_MEDIA_PLAYER_KEYS */
}

static void
totem_remote_class_init (TotemRemoteClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = totem_remote_finalize;

	totem_remote_signals[BUTTON_PRESSED] =
		g_signal_new ("button_pressed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemRemoteClass, button_pressed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);
}


static void
totem_remote_init (TotemRemote *remote)
{
#ifdef HAVE_MEDIA_PLAYER_KEYS
	DBusGConnection *bus;
	GError *error = NULL;
#endif /* HAVE_MEDIA_PLAYER_KEYS */
#ifdef HAVE_LIRC
	int fd;
#endif /* HAVE_LIRC */

#ifdef HAVE_MEDIA_PLAYER_KEYS
	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (bus == NULL) {
		g_warning ("Error connecting to DBus: %s", error->message);
	} else {
		remote->media_player_keys_proxy = dbus_g_proxy_new_for_name (bus,
				"org.gnome.SettingsDaemon", "/org/gnome/SettingsDaemon",
				"org.gnome.SettingsDaemon");

		org_gnome_SettingsDaemon_grab_media_player_keys (remote->media_player_keys_proxy,
				"Totem", 0, NULL);

		dbus_g_object_register_marshaller (totem_marshal_VOID__STRING_STRING,
				G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

		dbus_g_proxy_add_signal (remote->media_player_keys_proxy, "MediaPlayerKeyPressed",
				G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

		dbus_g_proxy_connect_signal (remote->media_player_keys_proxy, "MediaPlayerKeyPressed",
				G_CALLBACK (on_media_player_key_pressed), remote, NULL);
	}
#endif

#ifdef HAVE_LIRC
	if (lirc_channel == NULL) {
		fd = lirc_init ("Totem", 0);

		if (fd < 0) {
			GConfClient *gc;

			gc = gconf_client_get_default ();
			if (gc == NULL)
				return;

			if (gconf_client_get_bool (gc, GCONF_PREFIX"/debug", NULL) != FALSE)
				g_message ("Couldn't initialize lirc.\n");
			g_object_unref (gc);
			return;
		}

		if (lirc_readconfig (NULL, &config, NULL) != 0) {
			g_message ("Couldn't read lirc config.");
			config = NULL;
			return;
		}

		lirc_channel = g_io_channel_unix_new (fd);

		g_io_add_watch (lirc_channel, G_IO_IN,
				(GIOFunc) totem_remote_read_code, NULL);
	}

	listeners = g_list_prepend (listeners, remote);
#endif /* HAVE_LIRC */

}

TotemRemote *
totem_remote_new (void)
{
	return g_object_new (TOTEM_TYPE_REMOTE, NULL);
}


#ifdef HAVE_MEDIA_PLAYER_KEYS
void
totem_remote_window_activated (TotemRemote *remote)
{

	g_return_if_fail (TOTEM_IS_REMOTE (remote));
	g_return_if_fail (DBUS_IS_G_PROXY (remote->media_player_keys_proxy));

	org_gnome_SettingsDaemon_grab_media_player_keys (remote->media_player_keys_proxy,
			"Totem", 0, NULL);
}
#endif /* HAVE_MEDIA_PLAYER_KEYS */

#endif /* HAVE_REMOTE */
