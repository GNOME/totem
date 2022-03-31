/* bacon-video-widget-properties.h: Properties dialog for BaconVideoWidget

   Copyright (C) 2002 Bastien Nocera <hadess@hadess.net>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include <gtk/gtk.h>

#define BACON_TYPE_VIDEO_WIDGET_PROPERTIES            (bacon_video_widget_properties_get_type ())
G_DECLARE_FINAL_TYPE(BaconVideoWidgetProperties, bacon_video_widget_properties, BACON, VIDEO_WIDGET_PROPERTIES, GtkBox)

GType bacon_video_widget_properties_get_type		(void);
GtkWidget *bacon_video_widget_properties_new		(void);

void bacon_video_widget_properties_reset		(BaconVideoWidgetProperties *props);
void bacon_video_widget_properties_set_label		(BaconVideoWidgetProperties *props,
							 const char                 *name,
							 const char                 *text);
void bacon_video_widget_properties_set_duration		(BaconVideoWidgetProperties *props,
							 int                         duration);
void bacon_video_widget_properties_set_has_type		(BaconVideoWidgetProperties *props,
							 gboolean                    has_video,
							 gboolean                    has_audio);
void bacon_video_widget_properties_set_framerate	(BaconVideoWidgetProperties *props,
							 float                       framerate);
