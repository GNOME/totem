/*
**  Sinek (Media Player)
**  Copyright (c) 2001-2002 Gurer Ozen
**
**  This code is free software; you can redistribute it and/or
**  modify it under the terms of the GNU General Public License.
**
**  screen saver control
*/

#include "config.h"
#include "scrsaver.h"

#include <glib.h>
#include <gdk/gdkx.h>
#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>

gboolean
bacon_resize_init (void)
{
	int event_basep, error_basep;

	if (XRRQueryExtension (GDK_DISPLAY(), &event_basep, &error_basep))
		return TRUE;

	return FALSE;
}

void
bacon_resize (int height, int width)
{
	XRRScreenConfiguration *xr_screen_conf;
	XRRScreenSize *xr_sizes;
	SizeID xr_current_size;
	int xr_nsize, i;
	Rotation xr_rotations;
	Rotation xr_current_rotation;
	int target = -1;
	Status status;

	if (height == 0 || width == 0)
		return -1;

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
		if (height > xr_sizes[i].height && width > xr_sizes[i].width)
			break;
		if (height < xr_sizes[i].height && width < xr_sizes[i].width)
			target = i;
	}

	if (target == -1)
		return -1;

	XLockDisplay (GDK_DISPLAY());
	status = XRRSetScreenConfig (GDK_DISPLAY(),
			xr_screen_conf,
			GDK_ROOT_WINDOW(),
			(SizeID)target,
			xr_current_rotation,
			CurrentTime);
	XUnlockDisplay (GDK_DISPLAY());

	if (status != RRSetConfigSuccess)
		return -1;

	return (int) xr_current_size;
}

int
bacon_resize_get_current (void)
{
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
}

void
bacon_restore (int id)
{
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
}

