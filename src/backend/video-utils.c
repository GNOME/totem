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

#include <glib/gi18n-lib.h>
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

static gboolean
totem_ratio_fits_screen_helper (GtkWidget *video_widget,
				int new_w, int new_h,
				gfloat ratio)
{
	GdkScreen *screen;
	GdkRectangle work_rect, mon_rect;
	GdkWindow *window;
	int monitor;

	window = gtk_widget_get_window (video_widget);
	g_return_val_if_fail (window != NULL, FALSE);

	screen = gtk_widget_get_screen (video_widget);
	window = gtk_widget_get_window (video_widget);
	monitor = gdk_screen_get_monitor_at_window (screen, window);

	gdk_screen_get_monitor_workarea (screen, monitor, &work_rect);
	gdk_screen_get_monitor_geometry (screen,
					 gdk_screen_get_monitor_at_window (screen, window),
					 &mon_rect);
	gdk_rectangle_intersect (&mon_rect, &work_rect, &work_rect);

	if (new_w > work_rect.width || new_h > work_rect.height)
		return FALSE;

	return TRUE;
}

static void
get_window_size (GtkWidget *widget,
		 int *width,
		 int *height)
{
	GdkWindow *window;
	GdkRectangle rect;

	window = gtk_widget_get_window (widget);
	gdk_window_get_frame_extents (window, &rect);
	*width = rect.width;
	*height = rect.height;
}

gboolean
totem_ratio_fits_screen (GtkWidget *video_widget,
			 int video_width, int video_height,
			 gfloat ratio)
{
	int new_w, new_h;
	GtkWidget *window;

	if (video_width <= 0 || video_height <= 0)
		return TRUE;

	new_w = video_width * ratio;
	new_h = video_height * ratio;

	/* Now add the width of the rest of the movie player UI */
	window = gtk_widget_get_toplevel (video_widget);
	if (gtk_widget_is_toplevel (window)) {
		GdkWindow *video_win;
		int win_w, win_h;

		get_window_size (window, &win_w, &win_h);
		video_win = gtk_widget_get_window (video_widget);

		new_w += win_w - gdk_window_get_width (video_win);
		new_h += win_h - gdk_window_get_height (video_win);
	}

	return totem_ratio_fits_screen_helper (video_widget, new_w, new_h, ratio);
}

