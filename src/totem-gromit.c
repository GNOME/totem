/*
 *  Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>
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

#ifndef HAVE_GTK_ONLY

#include <glib.h>

#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include "totem-gromit.h"

#define INTERVAL 10000

static const char *start_cmd[] =	{ NULL, "-a", "-k", "none", NULL };
static const char *toggle_cmd[] =	{ NULL, "-t", NULL };
static const char *clear_cmd[] =	{ NULL, "-c", NULL };
static const char *visibility_cmd[] =	{ NULL, "-v", NULL };
/* no quit command, we just kill the process */

static char *path = NULL;
static int id = -1;
static GPid pid = -1;

#define DEFAULT_CONFIG							\
"#Default gromit configuration for Totem's telestrator mode		\n\
\"red Pen\" = PEN (size=5 color=\"red\");				\n\
\"blue Pen\" = \"red Pen\" (color=\"blue\");				\n\
\"yellow Pen\" = \"red Pen\" (color=\"yellow\");			\n\
\"green Marker\" = PEN (size=6 color=\"green\" arrowsize=1);		\n\
									\n\
\"Eraser\" = ERASER (size = 100);					\n\
									\n\
\"Core Pointer\" = \"red Pen\";						\n\
\"Core Pointer\"[SHIFT] = \"blue Pen\";					\n\
\"Core Pointer\"[CONTROL] = \"yellow Pen\";				\n\
\"Core Pointer\"[2] = \"green Marker\";					\n\
\"Core Pointer\"[Button3] = \"Eraser\";					\n\
\n"

static void
totem_gromit_ensure_config_file (void)
{
	char *path;
	int fd;

	path = g_build_filename (g_get_home_dir (), ".gromitrc", NULL);
	if (g_file_test (path, G_FILE_TEST_EXISTS) != FALSE) {
		g_free (path);
		return;
	}

	g_message ("%s doesn't exist", path);

	fd = creat (path, 0755);
	g_free (path);
	if (fd < 0) {
		return;
	}

	write (fd, DEFAULT_CONFIG, sizeof (DEFAULT_CONFIG));
	close (fd);
}

gboolean
totem_gromit_available (void)
{
	static int gromit_available = -1;

	if (gromit_available != -1)
		return (gboolean) gromit_available;

	path = g_find_program_in_path ("gromit");
	gromit_available = (path != NULL);
	if (path != NULL) {
		start_cmd[0] = toggle_cmd[0] = clear_cmd[0] =
			visibility_cmd[0] = path;
		totem_gromit_ensure_config_file ();
	}

	return gromit_available;
}

static void
launch (const char **cmd)
{
	g_spawn_sync (NULL, (char **)cmd, NULL, 0, NULL, NULL,
			NULL, NULL, NULL, NULL);
}

static void
gromit_exit (void)
{
	/* Nothing to do */
	if (pid == -1) {
		if (id != -1) {
			g_source_remove (id);
			id = -1;
		}
		return;
	}

	kill ((pid_t) pid, SIGKILL);
	pid = -1;
}

static gboolean
gromit_timeout_cb (gpointer data)
{
	id = -1;
	gromit_exit ();
	return FALSE;
}

#include <gtk/gtk.h>

void
totem_gromit_toggle (void)
{
	if (totem_gromit_available () == FALSE)
		return;

	/* Not started */
	if (pid == -1) {
		if (g_spawn_async (NULL,
				(char **)start_cmd, NULL, 0, NULL, NULL,
				&pid, NULL) == FALSE) {
			g_printerr ("Couldn't start gromit");
			return;
		}
	} else if (id == -1) { /* Started but disabled */
		g_source_remove (id);
		id = -1;
		launch (toggle_cmd);
	} else {
		/* Started and visible */
		g_source_remove (id);
		id = -1;
		launch (toggle_cmd);
	}
}

void
totem_gromit_clear (gboolean now)
{
	if (totem_gromit_available () == FALSE)
		return;

	if (now != FALSE) {
		gromit_exit ();
		if (path != NULL) {
			g_free (path);
			path = NULL;
		}
		return;
	}

	launch (visibility_cmd);
	launch (clear_cmd);
	id = g_timeout_add (INTERVAL, gromit_timeout_cb, NULL);
}

#endif /* !HAVE_GTK_ONLY */
