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
#include <X11/Xlib.h>

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
	int event_basep, error_basep, res;

	/* FIXME multihead */
	XLockDisplay (GDK_DISPLAY());

	res = XF86VidModeQueryExtension (GDK_DISPLAY(), &event_basep, &error_basep) || !XRRQueryExtension (GDK_DISPLAY(), &event_basep, &error_basep);

	if (!res) {
		XUnlockDisplay (GDK_DISPLAY());
		return FALSE;
	}

	xr_screen_conf = XRRGetScreenInfo (GDK_DISPLAY(), GDK_ROOT_WINDOW());

	XUnlockDisplay (GDK_DISPLAY());

	return TRUE;

#endif /* HAVE_XVIDMODE */
	return FALSE;
}

void
bacon_resize (void)
{
#ifdef HAVE_XVIDMODE
	int width, height, i, xr_nsize, res, dotclock;
	XF86VidModeModeLine modeline;
	XRRScreenSize *xr_sizes;
	gboolean found = FALSE;

	/* FIXME multihead */
	XLockDisplay (GDK_DISPLAY());

	res = XF86VidModeGetModeLine (GDK_DISPLAY(), XDefaultScreen (GDK_DISPLAY()), &dotclock, &modeline);
	if (!res) {
		XUnlockDisplay (GDK_DISPLAY());
		return;
	}

	/* Check if there's a viewport */
	width = gdk_screen_width ();
	height = gdk_screen_height ();
	if (width > modeline.hdisplay
			&& height > modeline.vdisplay) {
		XUnlockDisplay (GDK_DISPLAY());
		return;
	}

	gdk_error_trap_push ();
	/* Find the xrandr mode that corresponds to the real size */
	xr_sizes = XRRConfigSizes (xr_screen_conf, &xr_nsize);
	xr_original_size = XRRConfigCurrentConfiguration
		(xr_screen_conf, &xr_current_rotation);
	if (gdk_error_trap_pop ())
		g_warning ("XRRConfigSizes or XRRConfigCurrentConfiguration failed");

	for (i = 0; i < xr_nsize; i++) {
		if (modeline.hdisplay == xr_sizes[i].width
		&& modeline.vdisplay == xr_sizes[i].height) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		XUnlockDisplay (GDK_DISPLAY());
		return;
	}
	gdk_error_trap_push ();
	XRRSetScreenConfig (GDK_DISPLAY(),
			xr_screen_conf,
			GDK_ROOT_WINDOW(),
			(SizeID) i,
			xr_current_rotation,
			CurrentTime);
	gdk_flush ();
	if (gdk_error_trap_pop ())
		g_warning ("XRRSetScreenConfig failed");

	XUnlockDisplay (GDK_DISPLAY());
#endif /* HAVE_XVIDMODE */
}

void
bacon_restore (void)
{
#ifdef HAVE_XVIDMODE
	int width, height, res, dotclock;
	XF86VidModeModeLine modeline;

	/* FIXME multihead */
	XLockDisplay (GDK_DISPLAY());

	res = XF86VidModeGetModeLine (GDK_DISPLAY(), XDefaultScreen (GDK_DISPLAY()), &dotclock, &modeline);
	if (!res) {
		XUnlockDisplay (GDK_DISPLAY());
		return;
	}

	/* Check if there's a viewport */
	width = gdk_screen_width ();
	height = gdk_screen_height ();
	if (width > modeline.hdisplay
			&& height > modeline.vdisplay) {
		XUnlockDisplay (GDK_DISPLAY());
		return;
	}
	gdk_error_trap_push ();
	XRRSetScreenConfig (GDK_DISPLAY(),
			xr_screen_conf,
			GDK_ROOT_WINDOW(),
			xr_original_size,
			xr_current_rotation,
			CurrentTime);
	gdk_flush ();
	if (gdk_error_trap_pop ())
		g_warning ("XRRSetScreenConfig failed");
	XUnlockDisplay (GDK_DISPLAY());
#endif
}

