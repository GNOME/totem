/* totem-session.c

   Copyright (C) 2004 Bastien Nocera

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include "totem.h"
#include "totem-private.h"

#ifndef HAVE_GTK_ONLY

#include <libgnome/gnome-config.h>
#include <libgnomeui/gnome-client.h>

static gboolean
totem_save_yourself_cb (GnomeClient *client, int phase, GnomeSaveStyle style,
		gboolean shutting_down, GnomeInteractStyle interact_style,
		gboolean fast, Totem *totem)
{
	char *argv[] = { "rm", "-r", NULL };
	const char *prefix;
	gboolean succeeded = TRUE;

	/* How to discard the save */
	prefix = gnome_client_get_config_prefix (client);
	g_message ("prefix %s real_path %s", prefix, gnome_config_get_real_path (prefix));
	argv[2] = gnome_config_get_real_path (prefix);
	gnome_client_set_discard_command (client, 3, argv);

	/* How to clone or restart */
	argv[0] = (char *) totem->argv0;
	argv[1] = NULL;

	gnome_client_set_clone_command (client, 1, argv);
	gnome_client_set_restart_command (client, 1, argv);
	return succeeded;
}

static void
totem_client_die_cb (GnomeClient *client, Totem *totem)
{
	totem_action_exit (totem);
}

void
totem_session_setup (Totem *totem, char **argv)
{
	GnomeClient *client;

	totem->argv0 = argv[0];

	client = gnome_master_client ();
	g_signal_connect (G_OBJECT (client), "save-yourself",
			G_CALLBACK (totem_save_yourself_cb), totem);
	g_signal_connect (G_OBJECT (client), "die",
			G_CALLBACK (totem_client_die_cb), totem);
}

#else

void
totem_session_setup (Totem *totem, char **argv)
{
}

#endif /* !HAVE_GTK_ONLY */
