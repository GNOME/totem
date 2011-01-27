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

#ifdef GDK_WINDOWING_X11
#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#endif /* GDK_WINDOWING_X11 */

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

static gboolean
totem_ratio_fits_screen_generic (GtkWidget *video_widget,
				 int new_w, int new_h,
				 gfloat ratio)
{
	GdkRectangle fullscreen_rect;
	GdkScreen *screen;
	GdkWindow *window;

	window = gtk_widget_get_window (video_widget);
	g_return_val_if_fail (window != NULL, FALSE);

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

#ifdef GDK_WINDOWING_X11
static gboolean
get_work_area (GdkScreen      *screen,
	       GdkRectangle   *rect)
{
	Atom            workarea;
	Atom            type;
	Window          win;
	int             format;
	gulong          num;
	gulong          leftovers;
	gulong          max_len = 4 * 32;
	guchar         *ret_workarea;
	long           *workareas;
	int             result;
	int             disp_screen;
	Display        *display;

	display = GDK_DISPLAY_XDISPLAY (gdk_screen_get_display (screen));
	workarea = XInternAtom (display, "_NET_WORKAREA", True);

	disp_screen = GDK_SCREEN_XNUMBER (screen);

	/* Defaults in case of error */
	rect->x = 0;
	rect->y = 0;
	rect->width = gdk_screen_get_width (screen);
	rect->height = gdk_screen_get_height (screen);

	if (workarea == None)
		return FALSE;

	win = XRootWindow (display, disp_screen);
	result = XGetWindowProperty (display,
				     win,
				     workarea,
				     0,
				     max_len,
				     False,
				     AnyPropertyType,
				     &type,
				     &format,
				     &num,
				     &leftovers,
				     &ret_workarea);

	if (result != Success
	    || type == None
	    || format == 0
	    || leftovers
	    || num % 4) {
		return FALSE;
	}

	workareas = (long *) ret_workarea;
	rect->x = workareas[disp_screen * 4];
	rect->y = workareas[disp_screen * 4 + 1];
	rect->width = workareas[disp_screen * 4 + 2];
	rect->height = workareas[disp_screen * 4 + 3];

	XFree (ret_workarea);

	return TRUE;
}

static gboolean
totem_ratio_fits_screen_x11 (GtkWidget *video_widget,
			     int new_w, int new_h,
			     gfloat ratio)
{
	GdkScreen *screen;
	GdkRectangle work_rect, mon_rect;
	GdkWindow *window;

	window = gtk_widget_get_window (video_widget);
	g_return_val_if_fail (window != NULL, FALSE);

	screen = gtk_widget_get_screen (video_widget);

	/* Get the work area */
	if (get_work_area (screen, &work_rect) == FALSE) {
		return totem_ratio_fits_screen_generic (video_widget,
							new_w,
							new_h,
							ratio);
	}

	gdk_screen_get_monitor_geometry (screen,
					 gdk_screen_get_monitor_at_window (screen, window),
					 &mon_rect);
	gdk_rectangle_intersect (&mon_rect, &work_rect, &work_rect);

	if (new_w > work_rect.width || new_h > work_rect.height)
		return FALSE;

	return TRUE;
}
#endif /* GDK_WINDOWING_X11 */

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
	GdkDisplay *display;
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
	} else {
		return totem_ratio_fits_screen_generic (video_widget, new_w, new_h, ratio);
	}

#ifdef GDK_WINDOWING_X11
	display = gtk_widget_get_display (video_widget);

	if (GDK_IS_X11_DISPLAY (display)) {
		return totem_ratio_fits_screen_x11 (video_widget, new_w, new_h, ratio);
	}
#endif
	return totem_ratio_fits_screen_generic (video_widget, new_w, new_h, ratio);
}

