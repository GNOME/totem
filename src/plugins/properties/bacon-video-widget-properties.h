/* bacon-video-widget-properties.h: Properties dialog for BaconVideoWidget

   Copyright (C) 2002 Bastien Nocera <hadess@hadess.net>

   SPDX-License-Identifier: GPL-3-or-later

   Author: Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include <handy.h>

#define BACON_TYPE_VIDEO_WIDGET_PROPERTIES            (bacon_video_widget_properties_get_type ())
G_DECLARE_FINAL_TYPE(BaconVideoWidgetProperties, bacon_video_widget_properties, BACON, VIDEO_WIDGET_PROPERTIES, HdyWindow)

GType bacon_video_widget_properties_get_type		(void);
GtkWidget *bacon_video_widget_properties_new		(GtkWindow *transient_for);

void bacon_video_widget_properties_reset		(BaconVideoWidgetProperties *props);
void bacon_video_widget_properties_set_duration		(BaconVideoWidgetProperties *props,
							 int                         duration);
void bacon_video_widget_properties_set_has_type		(BaconVideoWidgetProperties *props,
							 gboolean                    has_video,
							 gboolean                    has_audio);
void bacon_video_widget_properties_set_framerate	(BaconVideoWidgetProperties *props,
							 float                       framerate);
