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
 * the xine engine in a widget - implementation
 */

#include <config.h>

/* gtk+/gnome */
#include <gnome.h>
#include <gconf/gconf-client.h>

#include "totem-cd-selection.h"
#include "cd-drive.h"

/* Signals */
enum {
	DEVICE_CHANGED,
	LAST_SIGNAL
};

/* Arguments */
enum {
	PROP_0,
	PROP_DEVICE,
};

struct TotemCdSelectionPrivate {
	gboolean is_entry;
	GtkWidget *entry;
};


static void totem_cd_selection_class_init (TotemCdSelectionClass *klass);
static void totem_cd_selection_instance_init (TotemCdSelection *tcs);

static void totem_cd_selection_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec);
static void totem_cd_selection_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec);

static void totem_cd_selection_realize (GtkWidget *widget);
static void totem_cd_selection_unrealize (GtkWidget *widget);
static void totem_cd_selection_finalize (GObject *object);

static GtkWidgetClass *parent_class = NULL;

static int tcs_table_signals[LAST_SIGNAL] = { 0 };

GtkType
totem_cd_selection_get_type (void)
{
	static GtkType totem_cd_selection_type = 0;

	if (!totem_cd_selection_type) {
		static const GTypeInfo totem_cd_selection_info = {
			sizeof (TotemCdSelectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) totem_cd_selection_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (TotemCdSelection),
			0 /* n_preallocs */,
			(GInstanceInitFunc) totem_cd_selection_instance_init,
		};

		totem_cd_selection_type = g_type_register_static
			(GTK_TYPE_VBOX,
			 "TotemCdSelection", &totem_cd_selection_info,
			 (GTypeFlags)0);
	}

	return totem_cd_selection_type;
}

static void
totem_cd_selection_class_init (TotemCdSelectionClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_vbox_get_type ());

	/* GtkWidget */
	widget_class->realize = totem_cd_selection_realize;
	widget_class->unrealize = totem_cd_selection_unrealize;

	/* GObject */
	object_class->set_property = totem_cd_selection_set_property;
	object_class->get_property = totem_cd_selection_get_property;
	object_class->finalize = totem_cd_selection_finalize;

	/* Properties */
	g_object_class_install_property (object_class, PROP_DEVICE,
			g_param_spec_string ("device", NULL, NULL,
				FALSE, G_PARAM_READWRITE));

	/* Signals */
	tcs_table_signals[DEVICE_CHANGED] =
		g_signal_new ("device-changed",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemCdSelectionClass,
					device_changed),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
totem_cd_selection_instance_init (TotemCdSelection *tcs)
{
	tcs->priv = g_new0 (TotemCdSelectionPrivate, 1);

	tcs->priv->is_entry = TRUE;
}

static void
totem_cd_selection_realize (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->realize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
	}
}

static void
totem_cd_selection_unrealize (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->unrealize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
	}
}

static void
totem_cd_selection_finalize (GObject *object)
{
	TotemCdSelection *tcs = (TotemCdSelection *) object;
	G_OBJECT_CLASS (parent_class)->finalize (object);

	tcs->priv = NULL;
	tcs = NULL;
}

static void
on_combo_entry_changed (GnomeFileEntry *entry, gpointer user_data)
{
	TotemCdSelection *tcs = (TotemCdSelection *) user_data;
	const char *str;
	GtkWidget *widget;

	widget = gnome_file_entry_gtk_entry (entry);
	str = gtk_entry_get_text (GTK_ENTRY (widget));

	g_signal_emit (G_OBJECT (tcs),
			tcs_table_signals[DEVICE_CHANGED],
			0, str);
}

GtkWidget *
totem_cd_selection_new (void)
{
	GtkWidget *widget;
	TotemCdSelection *tcs;

	widget = GTK_WIDGET
		(g_object_new (totem_cd_selection_get_type (), NULL));
	tcs = TOTEM_CD_SELECTION (widget);

	if (tcs->priv->is_entry)
	{
		tcs->priv->entry = gnome_file_entry_new (NULL,
					_("Select the drive"));
		g_signal_connect (G_OBJECT (tcs->priv->entry), "changed",
				G_CALLBACK (on_combo_entry_changed), tcs);

		gtk_box_pack_start (GTK_BOX (widget),
				tcs->priv->entry,
				TRUE,       /* expand */
				TRUE,       /* fill */
				0);         /* padding */

		gtk_widget_show_all (tcs->priv->entry);
	} else {
		//FIXME
	}

	return widget;
}

/* Properties */
static void
totem_cd_selection_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec)
{
	TotemCdSelection *tcs;

	g_return_if_fail (TOTEM_IS_CD_SELECTION (object));

	tcs = TOTEM_CD_SELECTION (object);

	switch (property_id)
	{
	case PROP_DEVICE:
		totem_cd_selection_set_device (tcs, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
totem_cd_selection_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec)
{
	TotemCdSelection *tcs;

	g_return_if_fail (TOTEM_IS_CD_SELECTION (object));

	tcs = TOTEM_CD_SELECTION (object);

	switch (property_id)
	{
	case PROP_DEVICE:
		g_value_set_string (value, totem_cd_selection_get_device (tcs));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

void
totem_cd_selection_set_device (TotemCdSelection *tcs, const char *device)
{
	if (tcs->priv->is_entry == TRUE)
	{
		GtkWidget *entry;

		entry = gnome_file_entry_gtk_entry
			(GNOME_FILE_ENTRY (tcs->priv->entry));
		gtk_entry_set_text (GTK_ENTRY (entry), device);
	}
}

const char *
totem_cd_selection_get_device (TotemCdSelection *tcs)
{
	if (tcs->priv->is_entry == TRUE)
	{
		GtkWidget *entry;

		entry = gnome_file_entry_gtk_entry
			(GNOME_FILE_ENTRY (tcs->priv->entry));
		return gtk_entry_get_text (GTK_ENTRY (entry));
	}

	return NULL;
}

#if 0
void
totem_cd_selection_set_speed (TotemCdSelection *tcs, Speeds speed)
{
	g_return_if_fail (tcs != NULL);
	g_return_if_fail (TOTEM_IS_CD_SELECTION (tcs));
	g_return_if_fail (tcs->priv->xine != NULL);

	xine_set_param (tcs->priv->stream, XINE_PARAM_SPEED, speeds[speed]);
}
#endif

