/* 
 * Copyright (C) 2002 Bastien Nocera <hadess@hadess.net>
 *
 * bacon-cd-selection.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>

#include <gtk/gtkmenu.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkcellrenderertext.h>

#include "bacon-cd-selection.h"
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
	PROP_FILE_IMAGE,
};

struct BaconCdSelectionPrivate {
	GList *cdroms;
	gboolean have_file_image;
};

static void bacon_cd_selection_init (BaconCdSelection *bcs);

static void bacon_cd_selection_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec);
static void bacon_cd_selection_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec);

static void bacon_cd_selection_finalize (GObject *object);

static GtkWidgetClass *parent_class = NULL;

static int bcs_table_signals[LAST_SIGNAL] = { 0 };

static CDDrive *
get_drive (BaconCdSelection *bcs, int nr)
{
	GList *item;

	item = g_list_nth (bcs->priv->cdroms, nr);
	if (item == NULL)
		return NULL;
	else
		return item->data;
}

G_DEFINE_TYPE(BaconCdSelection, bacon_cd_selection, GTK_TYPE_COMBO_BOX)

static void
bacon_cd_selection_class_init (BaconCdSelectionClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_combo_box_get_type ());

	/* GObject */
	object_class->set_property = bacon_cd_selection_set_property;
	object_class->get_property = bacon_cd_selection_get_property;
	object_class->finalize = bacon_cd_selection_finalize;

	/* Properties */
	g_object_class_install_property (object_class, PROP_DEVICE,
			g_param_spec_string ("device", NULL, NULL,
				FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_FILE_IMAGE,
			g_param_spec_boolean ("file-image", NULL, NULL,
				FALSE, G_PARAM_READWRITE));

	/* Signals */
	bcs_table_signals[DEVICE_CHANGED] =
		g_signal_new ("device-changed",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconCdSelectionClass,
					device_changed),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
bacon_cd_selection_init (BaconCdSelection *bcs)
{
	bcs->priv = g_new0 (BaconCdSelectionPrivate, 1);
}

static void
bacon_cd_selection_finalize (GObject *object)
{
	GList *l;
	BaconCdSelection *bcs = (BaconCdSelection *) object;

	g_return_if_fail (bcs != NULL);
	g_return_if_fail (BACON_IS_CD_SELECTION (bcs));

	l = bcs->priv->cdroms;
	while (l != NULL)
	{
		CDDrive *cdrom = l->data;

		l = g_list_remove (l, cdrom);
		cd_drive_free (cdrom);
	}

	g_free (bcs->priv);
	bcs->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
combo_device_changed (GtkComboBox *combo, gpointer user_data)
{
	BaconCdSelection *bcs = (BaconCdSelection *) user_data;
	CDDrive *drive;
	int i;

	i = gtk_combo_box_get_active (combo);
	drive = get_drive (bcs, i);

	g_signal_emit (G_OBJECT (bcs),
		       bcs_table_signals[DEVICE_CHANGED],
		       0, drive->device);
}

static void
cdrom_combo_box (BaconCdSelection *bcs)
{
	GList *l;
	CDDrive *cdrom;

	bcs->priv->cdroms = scan_for_cdroms (FALSE, FALSE);

	for (l = bcs->priv->cdroms; l != NULL; l = l->next)
	{
		cdrom = l->data;

		if (cdrom->display_name == NULL) {
			g_warning ("cdrom->display_name != NULL failed");
		}

		gtk_combo_box_append_text (GTK_COMBO_BOX (bcs),
				cdrom->display_name
				? cdrom->display_name : _("Unnamed CDROM"));
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (bcs), 0);

	if (bcs->priv->cdroms == NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET (bcs), FALSE);
	}
}

GtkWidget *
bacon_cd_selection_new (void)
{
	GtkWidget *widget;
	BaconCdSelection *bcs;
	GtkCellRenderer *cell;
	GtkListStore *store;

	widget = GTK_WIDGET
		(g_object_new (bacon_cd_selection_get_type (), NULL));

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (widget),
			GTK_TREE_MODEL (store));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), cell,
			"text", 0,
			NULL);

	bcs = BACON_CD_SELECTION (widget);
	cdrom_combo_box (bcs);

	g_signal_connect (G_OBJECT (bcs), "changed",
			G_CALLBACK (combo_device_changed), bcs);

	return widget;
}

static void
bacon_cd_selection_set_have_file_image (BaconCdSelection *bcs,
		gboolean have_file_image)
{
	CDDrive *cdrom;

	g_return_if_fail (bcs != NULL);
	g_return_if_fail (BACON_IS_CD_SELECTION (bcs));

	if (bcs->priv->have_file_image == have_file_image) {
		return;
	}

	if (have_file_image == FALSE)
	{
		GList *item;
		int index;

		index = g_list_length (bcs->priv->cdroms);
		gtk_combo_box_remove_text (GTK_COMBO_BOX (bcs), index);

		item = g_list_last (bcs->priv->cdroms);
		cdrom = (CDDrive *)item->data;
		cd_drive_free (cdrom);
		bcs->priv->cdroms = g_list_delete_link
			(bcs->priv->cdroms, item);
	} else {
		cdrom = cd_drive_get_file_image ();
		gtk_combo_box_append_text (GTK_COMBO_BOX (bcs),
				cdrom->display_name);
		bcs->priv->cdroms = g_list_append (bcs->priv->cdroms, cdrom);
	}

	bcs->priv->have_file_image = have_file_image;
}

/* Properties */
static void
bacon_cd_selection_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec)
{
	BaconCdSelection *bcs;

	g_return_if_fail (BACON_IS_CD_SELECTION (object));

	bcs = BACON_CD_SELECTION (object);

	switch (property_id)
	{
	case PROP_DEVICE:
		bacon_cd_selection_set_device (bcs, g_value_get_string (value));
		break;
	case PROP_FILE_IMAGE:
		bacon_cd_selection_set_have_file_image (bcs,
				g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
bacon_cd_selection_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec)
{
	BaconCdSelection *bcs;

	g_return_if_fail (BACON_IS_CD_SELECTION (object));

	bcs = BACON_CD_SELECTION (object);

	switch (property_id)
	{
	case PROP_DEVICE:
		g_value_set_string (value, bacon_cd_selection_get_device (bcs));
		break;
	case PROP_FILE_IMAGE:
		g_value_set_boolean (value, bcs->priv->have_file_image);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

const char *
bacon_cd_selection_get_default_device (BaconCdSelection *bcs)
{
	GList *l;
	CDDrive *drive;

	g_return_val_if_fail (bcs != NULL, "/dev/cdrom");
	g_return_val_if_fail (BACON_IS_CD_SELECTION (bcs), "/dev/cdrom");

	l = bcs->priv->cdroms;
	if (bcs->priv->cdroms == NULL)
		return "/dev/cdrom";

	drive = l->data;

	return drive->device;
}

void
bacon_cd_selection_set_device (BaconCdSelection *bcs, const char *device)
{
	GList *l;
	CDDrive *drive;
	gboolean found;
	int i;

	found = FALSE;
	i = -1;

	g_return_if_fail (bcs != NULL);
	g_return_if_fail (BACON_IS_CD_SELECTION (bcs));

	for (l = bcs->priv->cdroms; l != NULL && found == FALSE;
			l = l->next)
	{
		i++;

		drive = l->data;

		if (strcmp (drive->device, device) == 0)
			found = TRUE;
	}

	if (found)
	{
		gtk_combo_box_set_active (GTK_COMBO_BOX (bcs), i);
	} else {
		/* If the device doesn't exist, set it back to
		 * the default */
		gtk_combo_box_set_active (GTK_COMBO_BOX (bcs), 0);

		drive = get_drive (bcs, 0);

		if (drive == NULL)
			return;

		g_signal_emit (G_OBJECT (bcs),
				bcs_table_signals [DEVICE_CHANGED],
				0, drive->device);
	}
}

const char *
bacon_cd_selection_get_device (BaconCdSelection *bcs)
{
	CDDrive *drive;
	int i;

	g_return_val_if_fail (bcs != NULL, NULL);
	g_return_val_if_fail (BACON_IS_CD_SELECTION (bcs), NULL);

	i = gtk_combo_box_get_active (GTK_COMBO_BOX (bcs));
	drive = get_drive (bcs, i);

	return drive ? drive->device : NULL;
}

const CDDrive *
bacon_cd_selection_get_cdrom (BaconCdSelection *bcs)
{
	CDDrive *drive;
	int i;

	g_return_val_if_fail (bcs != NULL, NULL);
	g_return_val_if_fail (BACON_IS_CD_SELECTION (bcs), NULL);

	i = gtk_combo_box_get_active (GTK_COMBO_BOX (bcs));
	drive = get_drive (bcs, i);

	return drive;
}

