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

#ifndef HAVE_BACON_CD_SELECTION_H
#define HAVE_BACON_CD_SELECTION_H

#include <gtk/gtkwidget.h>
#include <cd-drive.h>

G_BEGIN_DECLS

#define BACON_CD_SELECTION(obj)              (GTK_CHECK_CAST ((obj), bacon_cd_selection_get_type (), BaconCdSelection))
#define BACON_CD_SELECTION_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), bacon_cd_selection_get_type (), BaconCdSelectionClass))
#define BACON_IS_CD_SELECTION(obj)           (GTK_CHECK_TYPE (obj, bacon_cd_selection_get_type ()))
#define BACON_IS_CD_SELECTION_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), bacon_cd_selection_get_type ()))

typedef struct BaconCdSelectionPrivate BaconCdSelectionPrivate;

typedef struct {
	GtkVBox widget;
	BaconCdSelectionPrivate *priv;
} BaconCdSelection;

typedef struct {
	GtkVBoxClass parent_class;
	void (*device_changed) (GtkWidget *gtx, const char *title);
} BaconCdSelectionClass;

GtkType bacon_cd_selection_get_type              (void);
GtkWidget *bacon_cd_selection_new                (void);

void bacon_cd_selection_set_device		 (BaconCdSelection *bcs,
						  const char *device);
const char *bacon_cd_selection_get_device	 (BaconCdSelection *bcs);
const char *bacon_cd_selection_get_default_device (BaconCdSelection *bcs);
const CDDrive *bacon_cd_selection_get_cdrom       (BaconCdSelection *bcs);

G_END_DECLS

#endif				/* HAVE_BACON_CD_SELECTION_H */
