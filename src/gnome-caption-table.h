/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-caption-table.h - An easy way to do tables of aligned captions.

   Copyright (C) 1999, 2000 Eazel, Inc.

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef GNOME_CAPTION_TABLE_H
#define GNOME_CAPTION_TABLE_H

#include <glib.h>
#include <gtk/gtktable.h>

/*
 * GnomeCaptionTable is a GtkTable sublass that allows you to painlessly
 * create tables of nicely aligned captions.
 */

G_BEGIN_DECLS

#define GNOME_TYPE_CAPTION_TABLE			(gnome_caption_table_get_type ())
#define GNOME_CAPTION_TABLE(obj)		(GTK_CHECK_CAST ((obj), GNOME_TYPE_CAPTION_TABLE, GnomeCaptionTable))
#define GNOME_CAPTION_TABLE_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CAPTION_TABLE, GnomeCaptionTableClass))
#define GNOME_IS_CAPTION_TABLE(obj)		(GTK_CHECK_TYPE ((obj), GNOME_TYPE_CAPTION_TABLE))
#define GNOME_IS_CAPTION_TABLE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CAPTION_TABLE))

typedef struct GnomeCaptionTable		GnomeCaptionTable;
typedef struct GnomeCaptionTableClass	GnomeCaptionTableClass;
typedef struct GnomeCaptionTableDetail	GnomeCaptionTableDetail;

struct GnomeCaptionTable
{
	GtkTable table;

	GnomeCaptionTableDetail *detail;
};

struct GnomeCaptionTableClass
{
	GtkTableClass parent_class;

	void (*activate) (GtkWidget *caption_table, guint active_entry);
};

GtkType    gnome_caption_table_get_type           (void);
GtkWidget* gnome_caption_table_new                (guint            num_rows);
void       gnome_caption_table_set_row_info       (GnomeCaptionTable *caption_table,
						 guint            row,
						 const char      *label_text,
						 const char      *entry_text,
						 gboolean         entry_visibility,
						 gboolean         entry_readonly);
void       gnome_caption_table_set_entry_text     (GnomeCaptionTable *caption_table,
						 guint            row,
						 const char      *entry_text);
void       gnome_caption_table_set_entry_readonly (GnomeCaptionTable *caption_table,
						 guint            row,
						 gboolean         readonly);
void       gnome_caption_table_entry_grab_focus   (GnomeCaptionTable *caption_table,
						 guint            row);
char*      gnome_caption_table_get_entry_text     (GnomeCaptionTable *caption_table,
						 guint            row);
guint      gnome_caption_table_get_num_rows       (GnomeCaptionTable *caption_table);
void       gnome_caption_table_resize             (GnomeCaptionTable *caption_table,
						 guint            num_rows);

G_END_DECLS

#endif /* GNOME_CAPTION_TABLE_H */


