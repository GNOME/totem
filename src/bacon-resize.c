/* bacon-resize.c
 * Copyright (C) 2003, Bastien Nocera <hadess@hadess.net>
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

#include <glib.h>
#include <gdk/gdkx.h>
#include <X11/Xproto.h>
#include <X11/Xlib.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#endif

gboolean
bacon_resize_init (void)
{
#ifdef HAVE_RANDR
	int event_basep, error_basep, res;

	XLockDisplay (GDK_DISPLAY());
	res = XRRQueryExtension (GDK_DISPLAY(), &event_basep, &error_basep);
	XUnlockDisplay (GDK_DISPLAY());

	//FIXME http://bugzilla.gnome.org/show_bug.cgi?id=118203
#if 0
	if (res)
		return TRUE;
#endif
#endif /* HAVE_RANDR */
	return FALSE;
}

void
bacon_resize (int height, int width)
{
#ifdef HAVE_RANDR
	XRRScreenConfiguration *xr_screen_conf;
	XRRScreenSize *xr_sizes;
	SizeID xr_current_size;
	int xr_nsize, i;
	Rotation xr_rotations;
	Rotation xr_current_rotation;
	int target = -1;
	Status status;

	if (height == 0 || width == 0)
		return;

	/* Getting the current info */
	XLockDisplay (GDK_DISPLAY());
	xr_screen_conf = XRRGetScreenInfo
		(GDK_DISPLAY(), GDK_ROOT_WINDOW());
	xr_current_size = XRRConfigCurrentConfiguration
		(xr_screen_conf, &xr_current_rotation);
	xr_sizes = XRRConfigSizes (xr_screen_conf, &xr_nsize);
	xr_rotations = XRRConfigRotations (xr_screen_conf,
			&xr_current_rotation);
	XUnlockDisplay (GDK_DISPLAY());

	for (i = 0; i < xr_nsize; i++)
	{
		/* Avoid non-multiples of 16 for the resolutions as it
		 * would break ffmpeg's direct rendering and probably
		 * make everything slower */
		if (xr_sizes[i].height % 16 != 0 || xr_sizes[i].width % 16 != 0)
			continue;
		if (height > xr_sizes[i].height && width > xr_sizes[i].width)
			break;
		if (height < xr_sizes[i].height && width < xr_sizes[i].width)
		{
			if (target == -1)
			{
				target = i;
				continue;
			}
			if (xr_sizes[i].height < xr_sizes[target].height
					 && xr_sizes[i].width < xr_sizes[target].width)
			{
				target = i;
			}
		}
	}

	if (target == -1)
		return;

	XLockDisplay (GDK_DISPLAY());
	status = XRRSetScreenConfig (GDK_DISPLAY(),
			xr_screen_conf,
			GDK_ROOT_WINDOW(),
			(SizeID)target,
			xr_current_rotation,
			CurrentTime);
	XUnlockDisplay (GDK_DISPLAY());
#endif /* HAVE_RANDR */
}

int
bacon_resize_get_current (void)
{
#ifdef HAVE_RANDR
	XRRScreenConfiguration *xr_screen_conf;
	SizeID xr_current_size;
	Rotation xr_current_rotation;

	XLockDisplay (GDK_DISPLAY());
	xr_screen_conf = XRRGetScreenInfo
		(GDK_DISPLAY(), GDK_ROOT_WINDOW());
	xr_current_size = XRRConfigCurrentConfiguration
		(xr_screen_conf, &xr_current_rotation);
	XUnlockDisplay (GDK_DISPLAY());

	return (int) xr_current_size;
#else
	return -1;
#endif /* HAVE_RANDR */
}

void
bacon_restore (int id)
{
#ifdef HAVE_RANDR
	XRRScreenConfiguration *xr_screen_conf;
	Rotation xr_current_rotation;
	SizeID xr_current_size;

	if (id == -1)
		return;

	XLockDisplay (GDK_DISPLAY());
	xr_screen_conf = XRRGetScreenInfo
		(GDK_DISPLAY(), GDK_ROOT_WINDOW());
	xr_current_size = XRRConfigCurrentConfiguration
		(xr_screen_conf, &xr_current_rotation);

	XRRSetScreenConfig (GDK_DISPLAY(),
			xr_screen_conf,
			GDK_ROOT_WINDOW(),
			id,
			xr_current_rotation,
			CurrentTime);
	XUnlockDisplay (GDK_DISPLAY());
#endif
}

