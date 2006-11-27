/*
 * Copyright (C) 2006 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 */

#include <config.h>

#include "bacon-video-widget-common.h"

struct {
	int height;
	int fps;
} vis_qualities[] = {
	{ 240, 15 }, /* VISUAL_SMALL */
	{ 320, 25 }, /* VISUAL_NORMAL */
	{ 480, 25 }, /* VISUAL_LARGE */
	{ 600, 30 }  /* VISUAL_EXTRA_LARGE */
};

gboolean
bacon_video_widget_common_can_direct_seek (BaconVideoWidgetCommon *com)
{
	g_return_val_if_fail (com != NULL, FALSE);

	if (com->mrl == NULL)
		return FALSE;

	/* (instant seeking only make sense with video,
	 * hence no cdda:// here) */
	if (g_str_has_prefix (com->mrl, "file://") ||
			g_str_has_prefix (com->mrl, "dvd://") ||
			g_str_has_prefix (com->mrl, "vcd://"))
		return TRUE;

	return FALSE;
}

gboolean
bacon_video_widget_common_get_vis_quality (VisualsQuality q,
					   int *height, int *fps)
{
	g_return_val_if_fail (height == NULL, FALSE);
	g_return_val_if_fail (fps == NULL, FALSE);
	g_return_val_if_fail (q < G_N_ELEMENTS (vis_qualities), FALSE);

	*height = vis_qualities[q].height;
	*fps = vis_qualities[q].fps;
	return TRUE;
}

