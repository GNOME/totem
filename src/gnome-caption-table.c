/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-caption-table.c - An easy way to do tables of aligned captions.

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

#include <config.h>
#include "gnome-caption-table.h"

#include <atk/atkobject.h>
#include <atk/atkobjectfactory.h>
#include <atk/atkregistry.h>
#include <atk/atkrelationset.h>

#include <gtk/gtkentry.h>
#include <gtk/gtksignal.h>
#include <gtk/gtklabel.h>

struct GnomeCaptionTableDetail
{
	GtkWidget **labels;
	GtkWidget **entries;
	guint num_rows;
	guint size;
};

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

/* GnomeCaptionTableClass methods */
static void       gnome_caption_table_class_init      (GnomeCaptionTableClass *klass);
static void       gnome_caption_table_init            (GnomeCaptionTable      *caption_table);

/* GObjectClass methods */
static void       caption_table_finalize                  (GObject              *object);

/* Private methods */
static GtkWidget* caption_table_find_next_sensitive_entry (GnomeCaptionTable      *caption_table,
							   guint                 index);
static int        caption_table_index_of_entry            (GnomeCaptionTable      *caption_table,
							   GtkWidget            *entry);

/* Entry callbacks */
static void       entry_activate                          (GtkWidget            *widget,
							   gpointer              data);

static gpointer parent_class;

GtkType
gnome_caption_table_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GnomeCaptionTableClass),
                        NULL, NULL,
			(GClassInitFunc) gnome_caption_table_class_init,
                        NULL, NULL,
			sizeof (GnomeCaptionTable), 0,
			(GInstanceInitFunc) gnome_caption_table_init,
			NULL
		};

                type = g_type_register_static (GTK_TYPE_TABLE, 
					       "GnomeCaptionTable", 
					       &info, 0);

		parent_class = g_type_class_ref (GTK_TYPE_TABLE);
	}

	return type;
}

static int caption_table_signals[LAST_SIGNAL] = { 0 };

static void
gnome_caption_table_class_init (GnomeCaptionTableClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	caption_table_signals[ACTIVATE] =
		g_signal_new ("activate",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GnomeCaptionTableClass, activate),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
	
	/* GObjectClass */
	object_class->finalize = caption_table_finalize;
}

#define CAPTION_TABLE_DEFAULT_ROWS 1

static void
gnome_caption_table_init (GnomeCaptionTable *caption_table)
{
	GtkTable *table = GTK_TABLE (caption_table);

	caption_table->detail = g_new (GnomeCaptionTableDetail, 1);

	caption_table->detail->num_rows = 0;

	caption_table->detail->size = 0;

	caption_table->detail->labels = NULL;
	caption_table->detail->entries = NULL;

	table->homogeneous = FALSE;
}

/* GtkObjectClass methods */
static void
caption_table_finalize (GObject *object)
{
	GnomeCaptionTable *caption_table;
	
	g_return_if_fail (GNOME_IS_CAPTION_TABLE (object));
	
	caption_table = GNOME_CAPTION_TABLE (object);

	g_free (caption_table->detail->labels);
	g_free (caption_table->detail->entries);
	g_free (caption_table->detail);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(*G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* This function was copied from gnome-accessibility.c */
void
gnome_accessibility_set_up_label_widget_relation (GtkWidget *label, GtkWidget *widget)
{
	AtkObject *atk_widget, *atk_label;
	AtkRelationSet *relation_set;
	AtkRelation *relation;
	AtkObject *targets[1];

	atk_label = gtk_widget_get_accessible (label);
	atk_widget = gtk_widget_get_accessible (widget);

	/* Create the label -> widget relation */
	relation_set = atk_object_ref_relation_set (atk_label);
	targets[0] = atk_widget;
	relation = atk_relation_new (targets, 1, ATK_RELATION_LABEL_FOR);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (relation);

	/* Create the widget -> label relation */
	relation_set = atk_object_ref_relation_set (atk_widget);
	targets[0] = atk_label;
	relation = atk_relation_new (targets, 1, ATK_RELATION_LABELLED_BY);
	atk_relation_set_add (relation_set, relation);
	g_object_unref (relation);
}

void
gnome_caption_table_resize (GnomeCaptionTable	*caption_table,
			       guint			num_rows)
{
	GtkTable* table = NULL;

	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (GNOME_IS_CAPTION_TABLE (caption_table));

	/* Make sure the num_rows have changed */
	if (caption_table->detail->num_rows == num_rows)
		return;

	caption_table->detail->num_rows = num_rows;

	/* Resize the GtkTable */
	table = GTK_TABLE (caption_table);
	gtk_table_resize(table, caption_table->detail->num_rows, 2);

	/* Create more label/entry pairs if needed */
	if (caption_table->detail->num_rows > caption_table->detail->size)
	{
		guint i;
		guint old_size = caption_table->detail->size;
		guint new_size = caption_table->detail->num_rows;
		guint realloc_size = sizeof(GtkWidget *) * new_size;
		
		/* FIXME bugzilla.eazel.com 680: Use a GList for this */
		caption_table->detail->labels = (GtkWidget**) g_realloc (caption_table->detail->labels,
									 realloc_size);

		caption_table->detail->entries = (GtkWidget**) g_realloc (caption_table->detail->entries,
									  realloc_size);
		
		for (i = old_size; i < new_size; i++)
		{
			caption_table->detail->labels[i] = gtk_label_new("");
			caption_table->detail->entries[i] = gtk_entry_new();
			
			gtk_label_set_mnemonic_widget (GTK_LABEL (caption_table->detail->labels[i]),
						       caption_table->detail->entries[i]);
			gnome_accessibility_set_up_label_widget_relation (caption_table->detail->labels[i],
									caption_table->detail->entries[i]);
			
			g_signal_connect (G_OBJECT (caption_table->detail->entries[i]),
					  "activate",
					  G_CALLBACK (entry_activate),
					  (gpointer) caption_table);

			gtk_misc_set_alignment (GTK_MISC (caption_table->detail->labels[i]), 1.0, 0.5);

			/* Column 1 */
			gtk_table_attach (table,
					  caption_table->detail->labels[i],	/* child */
					  0,					/* left_attatch */
					  1,					/* right_attatch */
					  i,					/* top_attatch */
					  i + 1,				/* bottom_attatch */
					  GTK_FILL,				/* xoptions */
					  (GTK_FILL|GTK_EXPAND),		/* yoptions */
					  0,					/* xpadding */
					  0);					/* ypadding */
			
			/* Column 2 */
			gtk_table_attach (table, 
					  caption_table->detail->entries[i],	/* child */
					  1,					/* left_attatch */
					  2,					/* right_attatch */
					  i,					/* top_attatch */
					  i + 1,				/* bottom_attatch */
					  (GTK_FILL|GTK_EXPAND),		/* xoptions */
					  (GTK_FILL|GTK_EXPAND),		/* yoptions */
					  0,					/* xpadding */
					  0);					/* ypadding */
		}

		caption_table->detail->size = new_size;
	}

	/* Show only the needed caption widgets */
	if (caption_table->detail->size > 0)
	{
		guint i;

		for(i = 0; i < caption_table->detail->size; i++)
		{
			if (i < caption_table->detail->num_rows)
			{
				gtk_widget_show (caption_table->detail->labels[i]);
				gtk_widget_show (caption_table->detail->entries[i]);
			}
			else
			{
				gtk_widget_hide (caption_table->detail->labels[i]);
				gtk_widget_hide (caption_table->detail->entries[i]);
			}
		}
	}

	/* Set inter row spacing */
	if (caption_table->detail->num_rows > 1)
	{
		guint i;

		for(i = 0; i < (caption_table->detail->num_rows - 1); i++)
			gtk_table_set_row_spacing (GTK_TABLE (table), i, 10);
	}
}

static int
caption_table_index_of_entry (GnomeCaptionTable *caption_table,
			      GtkWidget* entry)
{
	guint i;

	g_return_val_if_fail (caption_table != NULL, -1);
	g_return_val_if_fail (GNOME_IS_CAPTION_TABLE (caption_table), -1);

	for(i = 0; i < caption_table->detail->num_rows; i++)
		if (caption_table->detail->entries[i] == entry)
			return i;

	return -1;
}

static GtkWidget*
caption_table_find_next_sensitive_entry (GnomeCaptionTable	*caption_table,
					 guint			index)
{
	guint i;

	g_return_val_if_fail (caption_table != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CAPTION_TABLE (caption_table), NULL);

	for(i = index; i < caption_table->detail->num_rows; i++)
		if (GTK_WIDGET_SENSITIVE (caption_table->detail->entries[i]))
			return caption_table->detail->entries[i];

	return NULL;
}

static void
entry_activate (GtkWidget *widget, gpointer data)
{
	GnomeCaptionTable *caption_table = GNOME_CAPTION_TABLE (data);
	int index;
	
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (GNOME_IS_CAPTION_TABLE (caption_table));
	
	index = caption_table_index_of_entry (caption_table, widget);
	
	/* Check for an invalid index */
	if (index == -1) {
		return;
	}

	/* Check for the last index */
	if (index < (int) caption_table->detail->num_rows) {
		/* Look for the next sensitive entry */
		GtkWidget *sensitive_entry = 
			caption_table_find_next_sensitive_entry (caption_table, index + 1);
		
		/* Make the next sensitive entry take focus */
		if (sensitive_entry) {
			gtk_widget_grab_focus (sensitive_entry);
		}
	}
	
	/* Emit the activate signal */
	g_signal_emit (G_OBJECT (caption_table), 
		       caption_table_signals[ACTIVATE], 0,
		       index);
}

/* Public methods */
GtkWidget*
gnome_caption_table_new (guint num_rows)
{
	GtkWidget *widget;

	if (num_rows == 0) {
		num_rows = 1;
	}

	widget = gtk_widget_new (gnome_caption_table_get_type(), NULL);

	gnome_caption_table_resize (GNOME_CAPTION_TABLE (widget), num_rows);
	gtk_table_set_col_spacing (GTK_TABLE (widget), 0, 10);

	return widget;
}

void
gnome_caption_table_set_row_info (GnomeCaptionTable *caption_table,
				guint row,
				const char* label_text,
				const char* entry_text,
				gboolean entry_visibility,
				gboolean entry_readonly)
{
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (GNOME_IS_CAPTION_TABLE (caption_table));
	g_return_if_fail (row < caption_table->detail->num_rows);

	gtk_label_set_text_with_mnemonic (GTK_LABEL (caption_table->detail->labels[row]), label_text);

	gtk_entry_set_text (GTK_ENTRY (caption_table->detail->entries[row]), entry_text);
	gtk_entry_set_visibility (GTK_ENTRY (caption_table->detail->entries[row]), entry_visibility);
	gtk_widget_set_sensitive (caption_table->detail->entries[row], !entry_readonly);
	if (!entry_visibility) {
		AtkObject *accessible;

		accessible = gtk_widget_get_accessible (caption_table->detail->entries[row]);
		atk_object_set_role (accessible, ATK_ROLE_PASSWORD_TEXT);
	}
}

void
gnome_caption_table_set_entry_text (GnomeCaptionTable *caption_table,
				       guint row,
				       const char* entry_text)
{
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (GNOME_IS_CAPTION_TABLE (caption_table));
	g_return_if_fail (row < caption_table->detail->num_rows);

	gtk_entry_set_text (GTK_ENTRY (caption_table->detail->entries[row]), entry_text);
}

void
gnome_caption_table_set_entry_readonly (GnomeCaptionTable *caption_table,
					   guint row,
					   gboolean readonly)
{
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (GNOME_IS_CAPTION_TABLE (caption_table));
	g_return_if_fail (row < caption_table->detail->num_rows);
	
	gtk_widget_set_sensitive (caption_table->detail->entries[row], !readonly);
}

void
gnome_caption_table_entry_grab_focus (GnomeCaptionTable *caption_table, guint row)
{
	g_return_if_fail (caption_table != NULL);
	g_return_if_fail (GNOME_IS_CAPTION_TABLE (caption_table));
	g_return_if_fail (row < caption_table->detail->num_rows);

	if (GTK_WIDGET_SENSITIVE (caption_table->detail->entries[row]))
		gtk_widget_grab_focus (caption_table->detail->entries[row]);
}

char*
gnome_caption_table_get_entry_text (GnomeCaptionTable *caption_table, guint row)
{
	const char *text;

	g_return_val_if_fail (caption_table != NULL, NULL);
	g_return_val_if_fail (GNOME_IS_CAPTION_TABLE (caption_table), NULL);
	g_return_val_if_fail (row < caption_table->detail->num_rows, NULL);

	text = gtk_entry_get_text (GTK_ENTRY (caption_table->detail->entries[row]));

	return g_strdup (text);
}

guint
gnome_caption_table_get_num_rows (GnomeCaptionTable *caption_table)
{
	g_return_val_if_fail (caption_table != NULL, 0);
	g_return_val_if_fail (GNOME_IS_CAPTION_TABLE (caption_table), 0);

	return caption_table->detail->num_rows;
}
