/* 
 * Copyright (C) 2002 Bastien Nocera <hadess@hadess.net>
 *
 * bacon-v4l-selection.c
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

#include "bacon-v4l-selection.h"
#include "video-dev.h"

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

struct BaconV4lSelectionPrivate {
	GList *cdroms;
};

static void bacon_v4l_selection_init (BaconV4lSelection *bvs);

static void bacon_v4l_selection_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec);
static void bacon_v4l_selection_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec);

static void bacon_v4l_selection_finalize (GObject *object);

static GtkWidgetClass *parent_class = NULL;

static int bvs_table_signals[LAST_SIGNAL] = { 0 };

static VideoDev *
get_video_device (BaconV4lSelection *bvs, int nr)
{
	GList *item;

	item = g_list_nth (bvs->priv->cdroms, nr);
	if (item == NULL)
		return NULL;
	else
		return item->data;
}

G_DEFINE_TYPE(BaconV4lSelection, bacon_v4l_selection, GTK_TYPE_COMBO_BOX)

static void
bacon_v4l_selection_class_init (BaconV4lSelectionClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_combo_box_get_type ());

	/* GObject */
	object_class->set_property = bacon_v4l_selection_set_property;
	object_class->get_property = bacon_v4l_selection_get_property;
	object_class->finalize = bacon_v4l_selection_finalize;

	/* Properties */
	g_object_class_install_property (object_class, PROP_DEVICE,
			g_param_spec_string ("device", NULL, NULL,
				FALSE, G_PARAM_READWRITE));

	/* Signals */
	bvs_table_signals[DEVICE_CHANGED] =
		g_signal_new ("device-changed",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (BaconV4lSelectionClass,
					device_changed),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
bacon_v4l_selection_init (BaconV4lSelection *bvs)
{
	bvs->priv = g_new0 (BaconV4lSelectionPrivate, 1);
}

static void
bacon_v4l_selection_finalize (GObject *object)
{
	GList *l;
	BaconV4lSelection *bvs = (BaconV4lSelection *) object;

	g_return_if_fail (bvs != NULL);
	g_return_if_fail (BACON_IS_V4L_SELECTION (bvs));

	l = bvs->priv->cdroms;
	while (l != NULL)
	{
		VideoDev *cdrom = l->data;

		l = g_list_remove (l, cdrom);
		video_dev_free (cdrom);
	}

	g_free (bvs->priv);
	bvs->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
combo_device_changed (GtkComboBox *combo, gpointer user_data)
{
	BaconV4lSelection *bvs = (BaconV4lSelection *) user_data;
	VideoDev *drive;
	int i;

	i = gtk_combo_box_get_active (combo);
	drive = get_video_device (bvs, i);

	g_signal_emit (G_OBJECT (bvs),
		       bvs_table_signals[DEVICE_CHANGED],
		       0, drive->device);
}

static void
cdrom_combo_box (BaconV4lSelection *bvs)
{
	GList *l;
	VideoDev *cdrom;

	bvs->priv->cdroms = scan_for_video_devices ();

	for (l = bvs->priv->cdroms; l != NULL; l = l->next)
	{
		cdrom = l->data;

		if (cdrom->display_name == NULL) {
			g_warning ("cdrom->display_name != NULL failed");
		}

		gtk_combo_box_append_text (GTK_COMBO_BOX (bvs),
				cdrom->display_name
				? cdrom->display_name : _("Unnamed CDROM"));
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (bvs), 0);

	if (bvs->priv->cdroms == NULL) {
		gtk_widget_set_sensitive (GTK_WIDGET (bvs), FALSE);
	}
}

GtkWidget *
bacon_v4l_selection_new (void)
{
	GtkWidget *widget;
	BaconV4lSelection *bvs;
	GtkCellRenderer *cell;
	GtkListStore *store;

	widget = GTK_WIDGET
		(g_object_new (bacon_v4l_selection_get_type (), NULL));

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (widget),
			GTK_TREE_MODEL (store));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), cell,
			"text", 0,
			NULL);

	bvs = BACON_V4L_SELECTION (widget);
	cdrom_combo_box (bvs);

	g_signal_connect (G_OBJECT (bvs), "changed",
			G_CALLBACK (combo_device_changed), bvs);

	return widget;
}

/* Properties */
static void
bacon_v4l_selection_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec)
{
	BaconV4lSelection *bvs;

	g_return_if_fail (BACON_IS_V4L_SELECTION (object));

	bvs = BACON_V4L_SELECTION (object);

	switch (property_id)
	{
	case PROP_DEVICE:
		bacon_v4l_selection_set_device (bvs, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
bacon_v4l_selection_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec)
{
	BaconV4lSelection *bvs;

	g_return_if_fail (BACON_IS_V4L_SELECTION (object));

	bvs = BACON_V4L_SELECTION (object);

	switch (property_id)
	{
	case PROP_DEVICE:
		g_value_set_string (value, bacon_v4l_selection_get_device (bvs));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

const char *
bacon_v4l_selection_get_default_device (BaconV4lSelection *bvs)
{
	GList *l;
	VideoDev *drive;

	g_return_val_if_fail (bvs != NULL, "/dev/video0");
	g_return_val_if_fail (BACON_IS_V4L_SELECTION (bvs), "/dev/video0");

	l = bvs->priv->cdroms;
	if (bvs->priv->cdroms == NULL)
		return "/dev/video0";

	drive = l->data;

	return drive->device;
}

void
bacon_v4l_selection_set_device (BaconV4lSelection *bvs, const char *device)
{
	GList *l;
	VideoDev *drive;
	gboolean found;
	int i;

	found = FALSE;
	i = -1;

	g_return_if_fail (bvs != NULL);
	g_return_if_fail (BACON_IS_V4L_SELECTION (bvs));

	for (l = bvs->priv->cdroms; l != NULL && found == FALSE;
			l = l->next)
	{
		i++;

		drive = l->data;

		if (strcmp (drive->device, device) == 0)
			found = TRUE;
	}

	if (found)
	{
		gtk_combo_box_set_active (GTK_COMBO_BOX (bvs), i);
	} else {
		/* If the device doesn't exist, set it back to
		 * the default */
		gtk_combo_box_set_active (GTK_COMBO_BOX (bvs), 0);

		drive = get_video_device (bvs, 0);

		if (drive == NULL)
			return;

		g_signal_emit (G_OBJECT (bvs),
				bvs_table_signals [DEVICE_CHANGED],
				0, drive->device);
	}
}

const char *
bacon_v4l_selection_get_device (BaconV4lSelection *bvs)
{
	VideoDev *drive;
	int i;

	g_return_val_if_fail (bvs != NULL, NULL);
	g_return_val_if_fail (BACON_IS_V4L_SELECTION (bvs), NULL);

	i = gtk_combo_box_get_active (GTK_COMBO_BOX (bvs));
	drive = get_video_device (bvs, i);

	return drive ? drive->device : NULL;
}

