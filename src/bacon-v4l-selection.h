/* 
 * Copyright (C) 2002 Bastien Nocera <hadess@hadess.net>
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
 * Authors: Bastien Nocera <hadess@hadess.net>
 */

#ifndef HAVE_BACON_V4L_SELECTION_H
#define HAVE_BACON_V4L_SELECTION_H

#include <gtk/gtkwidget.h>
#include <video-dev.h>

G_BEGIN_DECLS

#define BACON_V4L_SELECTION(obj)              (GTK_CHECK_CAST ((obj), bacon_v4l_selection_get_type (), BaconV4lSelection))
#define BACON_V4L_SELECTION_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), bacon_v4l_selection_get_type (), BaconV4lSelectionClass))
#define BACON_IS_CD_SELECTION(obj)           (GTK_CHECK_TYPE (obj, bacon_v4l_selection_get_type ()))
#define BACON_IS_CD_SELECTION_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), bacon_v4l_selection_get_type ()))

typedef struct BaconV4lSelectionPrivate BaconV4lSelectionPrivate;

typedef struct {
	GtkVBox widget;
	BaconV4lSelectionPrivate *priv;
} BaconV4lSelection;

typedef struct {
	GtkVBoxClass parent_class;
	void (*device_changed) (GtkWidget *gtx, const char *device_path);
} BaconV4lSelectionClass;

GtkType bacon_v4l_selection_get_type               (void);
GtkWidget *bacon_v4l_selection_new                 (void);

void bacon_v4l_selection_set_device		   (BaconV4lSelection *bvs,
						    const char *device);
const char *bacon_v4l_selection_get_device	   (BaconV4lSelection *bvs);
const char *bacon_v4l_selection_get_default_device (BaconV4lSelection *bvs);
const VideoDev *bacon_v4l_selection_get_v4l_device (BaconV4lSelection *bvs);
const VideoDev *bacon_v4l_selection_get_video_device (BaconV4lSelection *bvs);
G_END_DECLS

#endif				/* HAVE_BACON_V4L_SELECTION_H */
