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
 */


#include <config.h>
#include <glib.h>
#include <string.h>

#include "totem-remote.h"

#ifdef HAVE_REMOTE

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

struct _TotemRemote {
	GObject parent;
};

enum
{
	BUTTON_PRESSED,
	LAST_SIGNAL
};

static guint totem_remote_signals[LAST_SIGNAL] = { 0 };
static GIOChannel *lirc_channel = NULL;
static GList *listeners = NULL;

static TotemRemoteCommand
totem_lirc_to_command (const gchar *str)
{
	if (strcmp (str, TOTEM_IR_COMMAND_PLAY) == 0)
		return TOTEM_REMOTE_COMMAND_PLAY;
	else if (strcmp (str, TOTEM_IR_COMMAND_PAUSE) == 0)
		return TOTEM_REMOTE_COMMAND_PAUSE;
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
	else
		return TOTEM_REMOTE_COMMAND_UNKNOWN;
}

static gboolean
totem_remote_read_code (GIOChannel *source, GIOCondition condition,
		   gpointer user_data)
{
	struct lirc_config *config;
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
	
	/* FIXME:  we really should only do this once, but there appears to be
	 * a bug in lirc where it will drop every other key press if we keep
	 * a config struct around for more than one use.
	 */
	if (lirc_readconfig (NULL, &config, NULL) != 0) {
		g_message ("Couldn't read lirc config.");
		return FALSE;
	}

	if (lirc_code2char (config, code, &str) != 0) {
		g_message ("Couldn't convert lirc code to string.");
		lirc_freeconfig (config);
		return TRUE;
	}

	if (str == NULL) {
		/* there was no command associated with the code */
		lirc_freeconfig (config);
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

	lirc_freeconfig (config);
	g_free (code);

	/* this causes a crash, so I guess I'm not supposed to free it?
	 * g_free (str);
	 */

	return TRUE;
}

static void
totem_remote_finalize (GObject *object)
{
	GError *error = NULL;
	TotemRemote *remote;

	g_return_if_fail (object != NULL);
	g_return_if_fail (TOTEM_IS_REMOTE (object));

	remote = TOTEM_REMOTE (object);

	listeners = g_list_remove (listeners, remote);

	if (listeners == NULL) {
		g_io_channel_shutdown (lirc_channel, FALSE, &error);
		if (error != NULL) {
			g_warning ("Couldn't destroy lirc connection: %s",
				   error->message);
			g_error_free (error);
		}

		lirc_deinit ();
	}
}

static void
totem_remote_class_init (TotemRemote *klass)
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
	int fd;

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

		lirc_channel = g_io_channel_unix_new (fd);

		g_io_add_watch (lirc_channel, G_IO_IN,
				(GIOFunc) totem_remote_read_code, NULL);
	}

	listeners = g_list_prepend (listeners, remote);
}

GType
totem_remote_get_type (void)
{
	static GType type = 0;
                                                                              
	if (type == 0)
	{ 
		static GTypeInfo info =
		{
			sizeof (TotemRemoteClass),
			NULL, 
			NULL,
			(GClassInitFunc) totem_remote_class_init, 
			NULL,
			NULL, 
			sizeof (TotemRemote),
			0,
			(GInstanceInitFunc) totem_remote_init
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "TotemRemote",
					       &info, 0);
	}

	return type;
}

TotemRemote *
totem_remote_new (void)
{
	return g_object_new (TOTEM_TYPE_REMOTE, NULL);
}

#endif /* HAVE_REMOTE */
