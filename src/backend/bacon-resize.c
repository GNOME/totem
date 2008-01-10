/* bacon-resize.c
 * Copyright (C) 2003-2004, Bastien Nocera <hadess@hadess.net>
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include "bacon-resize.h"

#ifdef HAVE_XVIDMODE
#include <glib.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>

#include <X11/extensions/xf86vmode.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>

/* XRandR */
XRRScreenConfiguration *xr_screen_conf;
XRRScreenSize *xr_sizes;
Rotation xr_current_rotation;
SizeID xr_original_size;
#endif

gboolean
bacon_resize_init (void)
{
#ifdef HAVE_XVIDMODE
	int event_basep, error_basep;

	XLockDisplay (GDK_DISPLAY());

	if (!XF86VidModeQueryExtension (GDK_DISPLAY(), &event_basep, &error_basep))
		goto bail;

	if (!XRRQueryExtension (GDK_DISPLAY(), &event_basep, &error_basep))
		goto bail;

	/* We don't use the output here, checking whether XRRGetScreenInfo works */
	xr_screen_conf = XRRGetScreenInfo (GDK_DISPLAY(), GDK_ROOT_WINDOW());
	if (xr_screen_conf == NULL)
		goto bail;

	XRRFreeScreenConfigInfo (xr_screen_conf);
	xr_screen_conf = NULL;
	XUnlockDisplay (GDK_DISPLAY());

	return TRUE;

bail:
	XUnlockDisplay (GDK_DISPLAY());
	return FALSE;

#endif /* HAVE_XVIDMODE */
	return FALSE;
}

void
bacon_resize (GtkWidget *widget)
{
#ifdef HAVE_XVIDMODE
	int width, height, i, xr_nsize, res, dotclock;
	XF86VidModeModeLine modeline;
	XRRScreenSize *xr_sizes;
	gboolean found = FALSE;
	GdkWindow *root;
	GdkScreen *screen;
	Display *Display;

	Display = GDK_DRAWABLE_XDISPLAY (widget->window);
	if (Display == NULL)
		return;

	XLockDisplay (Display);

	screen = gtk_widget_get_screen (widget);
	root = gdk_screen_get_root_window (screen);

	/* XF86VidModeGetModeLine just doesn't work nicely with multiple monitors */
	if (gdk_screen_get_n_monitors (screen) > 1) {
		XUnlockDisplay (Display);
		return;
	}

	res = XF86VidModeGetModeLine (Display, GDK_SCREEN_XNUMBER (screen), &dotclock, &modeline);
	if (!res) {
		XUnlockDisplay (Display);
		return;
	}

	/* Check if there's a viewport */
	width = gdk_screen_get_width (screen);
	height = gdk_screen_get_height (screen);

	if (width <= modeline.hdisplay && height <= modeline.vdisplay) {
		XUnlockDisplay (Display);
		return;
	}

	gdk_error_trap_push ();
	/* Find the xrandr mode that corresponds to the real size */
	xr_screen_conf = XRRGetScreenInfo (Display, GDK_WINDOW_XWINDOW (root));
	xr_sizes = XRRConfigSizes (xr_screen_conf, &xr_nsize);
	xr_original_size = XRRConfigCurrentConfiguration
		(xr_screen_conf, &xr_current_rotation);
	if (gdk_error_trap_pop ()) {
		g_warning ("XRRConfigSizes or XRRConfigCurrentConfiguration failed");
		XUnlockDisplay (Display);
		return;
	}

	for (i = 0; i < xr_nsize; i++) {
		if (modeline.hdisplay == xr_sizes[i].width
		    && modeline.vdisplay == xr_sizes[i].height) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		XUnlockDisplay (Display);
		return;
	}
	gdk_error_trap_push ();
	XRRSetScreenConfig (Display,
			    xr_screen_conf,
			    GDK_WINDOW_XWINDOW (root),
			    (SizeID) i,
			    xr_current_rotation,
			    CurrentTime);
	gdk_flush ();
	if (gdk_error_trap_pop ())
		g_warning ("XRRSetScreenConfig failed");

	XUnlockDisplay (Display);
#endif /* HAVE_XVIDMODE */
}

void
bacon_restore (GtkWidget *widget)
{
#ifdef HAVE_XVIDMODE
	int width, height, res, dotclock;
	XF86VidModeModeLine modeline;
	GdkWindow *root;
	GdkScreen *screen;
	Display *Display;

	/* We haven't called bacon_resize before, or it exited
	 * as we didn't need a resize */
	if (xr_screen_conf == NULL)
		return;

	Display = GDK_DRAWABLE_XDISPLAY (widget->window);
	if (Display == NULL)
		return;

	XLockDisplay (Display);

	screen = gtk_widget_get_screen (widget);
	root = gdk_screen_get_root_window (screen);
	res = XF86VidModeGetModeLine (Display, GDK_SCREEN_XNUMBER (screen), &dotclock, &modeline);
	if (!res) {
		XUnlockDisplay (Display);
		return;
	}

	/* Check if there's a viewport */
	width = gdk_screen_get_width (screen);
	height = gdk_screen_get_height (screen);

	if (width > modeline.hdisplay && height > modeline.vdisplay) {
		XUnlockDisplay (Display);
		return;
	}
	gdk_error_trap_push ();
	XRRSetScreenConfig (Display,
			    xr_screen_conf,
			    GDK_WINDOW_XWINDOW (root),
			    xr_original_size,
			    xr_current_rotation,
			    CurrentTime);
	gdk_flush ();
	if (gdk_error_trap_pop ())
		g_warning ("XRRSetScreenConfig failed");

	XRRFreeScreenConfigInfo (xr_screen_conf);
	xr_screen_conf = NULL;

	XUnlockDisplay (Display);
#endif
}

