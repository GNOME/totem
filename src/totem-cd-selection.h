/* 
 * Copyright (C) 2001-2002 the xine project
 * 	Heavily modified by Bastien Nocera <hadess@hadess.net>
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
 * $Id$
 *
 * the xine engine in a widget - header
 */

#ifndef HAVE_TOTEM_CD_SELECTION_H
#define HAVE_TOTEM_CD_SELECTION_H

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define TOTEM_CD_SELECTION(obj)              (GTK_CHECK_CAST ((obj), totem_cd_selection_get_type (), TotemCdSelection))
#define TOTEM_CD_SELECTION_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), totem_cd_selection_get_type (), TotemCdSelectionClass))
#define TOTEM_IS_CD_SELECTION(obj)           (GTK_CHECK_TYPE (obj, totem_cd_selection_get_type ()))
#define TOTEM_IS_CD_SELECTION_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), totem_cd_selection_get_type ()))

typedef struct TotemCdSelectionPrivate TotemCdSelectionPrivate;

typedef struct {
	GtkWidget widget;
	TotemCdSelectionPrivate *priv;
} TotemCdSelection;

typedef struct {
	GtkWidgetClass parent_class;
	void (*device_changed) (GtkWidget *gtx, const char *title);
} TotemCdSelectionClass;

GtkType totem_cd_selection_get_type              (void);
GtkWidget *totem_cd_selection_new                ();

G_END_DECLS

#endif				/* HAVE_TOTEM_CD_SELECTION_H */
