/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   video-dev.c: detection of video devices

   Copyright (C) 2003 Bastien Nocera

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Bastien Nocera <hadess@hadess.net>
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include <glib.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(GETTEXT_PACKAGE,String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif /* gettext_noop */
#else
#define _(String) (String)
#define N_(String) (String)
#endif /* ENABLE_NLS */

#include "video-dev.h"

#ifdef __linux__

#ifdef USE_STABLE_LIBGLIB
static gboolean
g_str_has_prefix (gchar *haystack, gchar *needle)
{
	if (haystack == NULL && needle == NULL) {
		return TRUE;
	}

	if (haystack == NULL || needle == NULL) {
		return FALSE;
	}

	if (strncmp (haystack, needle, strlen (needle)) == 0) {
		return TRUE;
	}

	return FALSE;
}
#endif /* USE_STABLE_LIBGLIB */

static char **
read_lines (char *filename)
{
	char *contents;
	gsize len;
	char *p, *n;
	GPtrArray *array;
	
	if (g_file_get_contents (filename,
				 &contents,
				 &len, NULL)) {
		
		array = g_ptr_array_new ();
		
		p = contents;
		while ((n = memchr (p, '\n', len - (p - contents))) != NULL) {
			*n = 0;
			g_ptr_array_add (array, g_strdup (p));
			p = n + 1;
		}
		if ((gsize)(p - contents) < len) {
			g_ptr_array_add (array, g_strndup (p, len - (p - contents)));
		}

		g_ptr_array_add (array, NULL);
		
		g_free (contents);
		return (char **)g_ptr_array_free (array, FALSE);
	}
	return NULL;
}

static char*
linux_get_device_path (const char *name)
{
	char *filename;

	filename = g_build_filename ("/dev", name, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS) != FALSE) {
		return filename;
	}

	g_free (filename);
	filename = g_build_filename ("/dev/v4l", name, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS) != FALSE) {
		return filename;
	}

	g_free (filename);
	return filename;
}

static VideoDev*
linux_add_video_dev (const char *name)
{
	VideoDev *dev;
	char *proc;
	char **lines, *tmp, *filename;

	filename = linux_get_device_path (name);
	if (filename == NULL) {
		return NULL;
	}

	if (g_file_test ("/proc/video/dev", G_FILE_TEST_IS_DIR) != FALSE) {
		proc = g_build_filename ("/proc/video/dev", name, NULL);
		lines = read_lines (proc);
		g_free (proc);

		if (lines == NULL) {
			return NULL;
		}

		if (g_str_has_prefix (lines[0], "name") == FALSE) {
			g_strfreev (lines);
			return NULL;
		}

		tmp = strstr (lines[0], ":");
		if (tmp == NULL || tmp + 1 == NULL || tmp + 2 == NULL) {
			g_strfreev (lines);
			return NULL;
		}
		tmp = tmp + 2;
	} else {
		proc = g_build_filename ("/sys/class/video4linux/",
				name, "model", NULL);
		lines = read_lines (proc);
		g_free (proc);

		if (lines == NULL) {
			return NULL;
		}

		tmp = lines[0];
	}

	dev = g_new0 (VideoDev, 1);
	dev->display_name = g_strdup (tmp);
	dev->device = filename;

	g_strfreev (lines);

	return dev;
}

static GList *
linux_scan (void)
{
	GList *devs = NULL;
	GDir *dir;
	const char *name;
	VideoDev *dev;

	if (g_file_test ("/proc/video/dev", G_FILE_TEST_IS_DIR) != FALSE) {
		dir = g_dir_open ("/proc/video/dev", 0, NULL);
	} else if (g_file_test ("/sys/class/video4linux/", G_FILE_TEST_IS_DIR) != FALSE) {
		dir = g_dir_open ("/sys/class/video4linux/", 0, NULL);
	} else {
		return NULL;
	}

	name = g_dir_read_name (dir);
	while (name != NULL) {
		dev = linux_add_video_dev (name);
		if (dev != NULL) {
			devs = g_list_prepend (devs, dev);
		}
		name = g_dir_read_name (dir);
	}
	g_dir_close (dir);

	if (devs != NULL) {
		devs = g_list_reverse (devs);
	}

	return devs;
}
#endif /* __linux__ */

GList *
scan_for_video_devices (void)
{
	GList *devs = NULL;

#ifdef __linux__
	devs = linux_scan ();
#endif

	return devs;
}

void
video_dev_free (VideoDev *dev)
{
	g_return_if_fail (dev != NULL);

	g_free (dev->display_name);
	g_free (dev->device);
	g_free (dev);
}

