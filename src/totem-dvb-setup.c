/* totem-dvb-setup.c

   Copyright (C) 2008 Bastien Nocera

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
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <gdk/gdk.h>

#include "totem-dvb-setup.h"
#include "debug.h"

#ifdef GDK_WINDOWING_X11

static gboolean in_progress = FALSE;

#include <gdk/gdkx.h>
#include <sys/wait.h>

typedef struct {
	TotemDvbSetupResultFunc func;
	char *device;
	gpointer user_data;
} TotemDvbSetupHelper;

static const char *
totem_dvb_setup_helper (void)
{
	char *path = NULL;

	if (path == NULL)
		path = g_build_filename (LIBEXECDIR, "totem-dvb-scanner", NULL);
	return path;
}

static void
child_watch_func (GPid pid,
		  gint status,
		  gpointer data)
{
	TotemDvbSetupHelper *helper = (TotemDvbSetupHelper *) data;
	int ret;

	if (!WIFEXITED (status))
		ret = TOTEM_DVB_SETUP_CRASHED;
	else
		ret = WEXITSTATUS (status);

	helper->func (ret, helper->device, helper->user_data);

	in_progress = FALSE;

	g_free (helper->device);
	g_free (helper);
}

int
totem_dvb_setup_device (const char *device,
			GtkWindow *parent,
			TotemDvbSetupResultFunc func,
			gpointer user_data)
{
	GPtrArray *arr;
	char *tmp, **argv;
	GPid pid;
	TotemDvbSetupHelper *helper;

	if (in_progress != FALSE)
		return TOTEM_DVB_SETUP_FAILURE;

	if (g_file_test (totem_dvb_setup_helper(), G_FILE_TEST_IS_EXECUTABLE) == FALSE)
		return TOTEM_DVB_SETUP_MISSING;

	arr = g_ptr_array_new ();
	g_ptr_array_add (arr, g_strdup (totem_dvb_setup_helper ()));
	tmp = g_strdup_printf ("--transient-for=%u", (unsigned int) GDK_WINDOW_XID (GTK_WIDGET (parent)->window));
	g_ptr_array_add (arr, tmp);
	g_ptr_array_add (arr, g_strdup (device));
	g_ptr_array_add (arr, NULL);

	argv = (gchar **) arr->pdata;

	if (g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL,
			   NULL, &pid, NULL) == FALSE) {
		g_ptr_array_free (arr, TRUE);
		return TOTEM_DVB_SETUP_FAILURE;
	}

	g_ptr_array_free (arr, TRUE);

	helper = g_new (TotemDvbSetupHelper, 1);
	helper->user_data = user_data;
	helper->func = func;
	helper->device = g_strdup (device);

	in_progress = TRUE;

	g_child_watch_add (pid, child_watch_func, helper);

	return TOTEM_DVB_SETUP_STARTED_OK;
}

#else /* GDK_WINDOWING_X11 */

int
totem_dvb_setup_device (const char *device,
			GtkWindow *parent,
			TotemDvbSetupResultFunc func,
			gpointer user_data)
{
	return TOTEM_DVB_SETUP_FAILURE;
}

#endif /* GDK_WINDOWING_X11 */
