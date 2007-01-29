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

#ifndef HAVE_BACON_VIDEO_WIDGET_COMMON_H
#define HAVE_BACON_VIDEO_WIDGET_COMMON_H

#include "bacon-video-widget.h"
#include <glib.h>

G_BEGIN_DECLS

#define SMALL_STREAM_WIDTH 200
#define SMALL_STREAM_HEIGHT 120

struct BaconVideoWidgetCommon {
	char *mrl;
};

gboolean bacon_video_widget_common_can_direct_seek (BaconVideoWidgetCommon *com);
gboolean bacon_video_widget_common_get_vis_quality (VisualsQuality q,
						    int *height,
						    int *fps);

G_END_DECLS

#endif				/* HAVE_BACON_VIDEO_WIDGET_COMMON_H */
