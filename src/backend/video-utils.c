/*
 * Copyright © 2002-2010 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission is above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include "config.h"

#include "video-utils.h"

#include <glib/gi18n.h>
#include <libintl.h>

#include <gdk/gdk.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

void
totem_gdk_window_set_invisible_cursor (GdkWindow *window)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (GDK_BLANK_CURSOR);
	gdk_window_set_cursor (window, cursor);
	g_object_unref (cursor);
}

void
totem_gdk_window_set_waiting_cursor (GdkWindow *window)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor (window, cursor);
	g_object_unref (cursor);

	gdk_flush ();
}

gboolean
totem_display_is_local (void)
{
	const char *name, *work;
	int display, screen;
	gboolean has_hostname;

	name = gdk_display_get_name (gdk_display_get_default ());
	if (name == NULL)
		return TRUE;

	work = strstr (name, ":");
	if (work == NULL)
		return TRUE;

	has_hostname = (work - name) > 0;

	/* Get to the character after the colon */
	work++;
	if (*work == '\0')
		return TRUE;

	if (sscanf (work, "%d.%d", &display, &screen) != 2)
		return TRUE;

	if (has_hostname == FALSE)
		return TRUE;

	if (display < 10)
		return TRUE;

	return FALSE;
}

char *
totem_time_to_string (gint64 msecs)
{
	int sec, min, hour, _time;

	_time = (int) (msecs / 1000);
	sec = _time % 60;
	_time = _time - sec;
	min = (_time % (60*60)) / 60;
	_time = _time - (min * 60);
	hour = _time / (60*60);

	if (hour > 0)
	{
		/* hour:minutes:seconds */
		/* Translators: This is a time format, like "9:05:02" for 9
		 * hours, 5 minutes, and 2 seconds. You may change ":" to
		 * the separator that your locale uses or use "%Id" instead
		 * of "%d" if your locale uses localized digits.
		 */
		return g_strdup_printf (C_("long time format", "%d:%02d:%02d"), hour, min, sec);
	}

	/* minutes:seconds */
	/* Translators: This is a time format, like "5:02" for 5
	 * minutes and 2 seconds. You may change ":" to the
	 * separator that your locale uses or use "%Id" instead of
	 * "%d" if your locale uses localized digits.
	 */
	return g_strdup_printf (C_("short time format", "%d:%02d"), min, sec);
}

gint64
totem_string_to_time (const char *time_string)
{
	int sec, min, hour, args;

	args = sscanf (time_string, C_("long time format", "%d:%02d:%02d"), &hour, &min, &sec);

	if (args == 3) {
		/* Parsed all three arguments successfully */
		return (hour * (60 * 60) + min * 60 + sec) * 1000;
	} else if (args == 2) {
		/* Only parsed the first two arguments; treat hour and min as min and sec, respectively */
		return (hour * 60 + min) * 1000;
	} else if (args == 1) {
		/* Only parsed the first argument; treat hour as sec */
		return hour * 1000;
	} else {
		/* Error! */
		return -1;
	}
}

char *
totem_time_to_string_text (gint64 msecs)
{
	char *secs, *mins, *hours, *string;
	int sec, min, hour, _time;

	_time = (int) (msecs / 1000);
	sec = _time % 60;
	_time = _time - sec;
	min = (_time % (60*60)) / 60;
	_time = _time - (min * 60);
	hour = _time / (60*60);

	hours = g_strdup_printf (ngettext ("%d hour", "%d hours", hour), hour);

	mins = g_strdup_printf (ngettext ("%d minute",
					  "%d minutes", min), min);

	secs = g_strdup_printf (ngettext ("%d second",
					  "%d seconds", sec), sec);

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

gboolean
totem_ratio_fits_screen (GtkWidget *video_widget, int video_width,
			 int video_height, gfloat ratio)
{
	GdkRectangle fullscreen_rect;
	int new_w, new_h;
	GdkScreen *screen;
	GdkWindow *window;

	if (video_width <= 0 || video_height <= 0)
		return TRUE;

	window = gtk_widget_get_window (video_widget);
	g_return_val_if_fail (window != NULL, FALSE);

	new_w = video_width * ratio;
	new_h = video_height * ratio;

	screen = gtk_widget_get_screen (video_widget);
	gdk_screen_get_monitor_geometry (screen,
					 gdk_screen_get_monitor_at_window (screen, window),
					 &fullscreen_rect);

	if (new_w > (fullscreen_rect.width - 128) ||
	    new_h > (fullscreen_rect.height - 128)) {
		return FALSE;
	}

	return TRUE;
}

