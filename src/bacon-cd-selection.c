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

#include <string.h>
#ifndef HAVE_GTK_ONLY
#include <gnome.h>
#else
#include <gtk/gtkmenu.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmenuitem.h>
#endif /* !HAVE_GTK_ONLY */

#ifdef HAVE_GTK_ONLY
#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(GETTEXT_PACKAGE,String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif /* gettext_noop */
#else
#define _(String) (String)
#define N_(String) (String)
#endif /* ENABLE_NLS */
#endif /* HAVE_GTK_ONLY */

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
};

struct BaconCdSelectionPrivate {
	gboolean is_entry;
	GtkWidget *widget;
	GList *cdroms;
};


static void bacon_cd_selection_class_init (BaconCdSelectionClass *klass);
static void bacon_cd_selection_instance_init (BaconCdSelection *bcs);

static void bacon_cd_selection_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec);
static void bacon_cd_selection_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec);

static void bacon_cd_selection_realize (GtkWidget *widget);
static void bacon_cd_selection_unrealize (GtkWidget *widget);
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


GtkType
bacon_cd_selection_get_type (void)
{
	static GtkType bacon_cd_selection_type = 0;

	if (!bacon_cd_selection_type) {
		static const GTypeInfo bacon_cd_selection_info = {
			sizeof (BaconCdSelectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) bacon_cd_selection_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (BaconCdSelection),
			0 /* n_preallocs */,
			(GInstanceInitFunc) bacon_cd_selection_instance_init,
			NULL
		};

		bacon_cd_selection_type = g_type_register_static
			(GTK_TYPE_VBOX,
			 "BaconCdSelection", &bacon_cd_selection_info,
			 (GTypeFlags)0);
	}

	return bacon_cd_selection_type;
}

static void
bacon_cd_selection_class_init (BaconCdSelectionClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_vbox_get_type ());

	/* GtkWidget */
	widget_class->realize = bacon_cd_selection_realize;
	widget_class->unrealize = bacon_cd_selection_unrealize;

	/* GObject */
	object_class->set_property = bacon_cd_selection_set_property;
	object_class->get_property = bacon_cd_selection_get_property;
	object_class->finalize = bacon_cd_selection_finalize;

	/* Properties */
	g_object_class_install_property (object_class, PROP_DEVICE,
			g_param_spec_string ("device", NULL, NULL,
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
bacon_cd_selection_instance_init (BaconCdSelection *bcs)
{
	bcs->priv = g_new0 (BaconCdSelectionPrivate, 1);

#if defined (__linux__) || defined (__FreeBSD__)
	bcs->priv->is_entry = FALSE;
#else
	bcs->priv->is_entry = TRUE;
#endif

	bcs->priv->cdroms = NULL;
}

static void
bacon_cd_selection_realize (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->realize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
	}
}

static void
bacon_cd_selection_unrealize (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->unrealize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
	}
}

static void
bacon_cd_selection_finalize (GObject *object)
{
	GList *l;

	BaconCdSelection *bcs = (BaconCdSelection *) object;
	G_OBJECT_CLASS (parent_class)->finalize (object);

	gtk_widget_destroy (bcs->priv->widget);

	l = bcs->priv->cdroms;
	while (l != NULL)
	{
		CDDrive *cdrom = l->data;

		cd_drive_free (cdrom);
		l = g_list_remove (l, cdrom);
		g_free (cdrom);
	}

	bcs->priv = NULL;
	bcs = NULL;
}

static void
option_menu_device_changed (GtkOptionMenu *option_menu, gpointer user_data)
{
	BaconCdSelection *bcs = (BaconCdSelection *) user_data;
	CDDrive *drive;
	int i;

	i = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
	drive = get_drive (bcs, i);

	g_signal_emit (G_OBJECT (bcs),
			bcs_table_signals[DEVICE_CHANGED],
			0, drive->device);
}

static GtkWidget *
cdrom_option_menu (BaconCdSelection *bcs)
{
	GList *l;
	GtkWidget *option_menu, *menu, *item;
	CDDrive *cdrom;

	bcs->priv->cdroms = scan_for_cdroms (FALSE, FALSE);

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	option_menu = gtk_option_menu_new ();

	for (l = bcs->priv->cdroms; l != NULL; l = l->next)
	{
		cdrom = l->data;

		if (cdrom->display_name == NULL)
			g_warning ("cdrom->display_name != NULL failed");
		item = gtk_menu_item_new_with_label (cdrom->display_name
				? cdrom->display_name : _("Unnamed CDROM"));
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}
	gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 0);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

	if (bcs->priv->cdroms == NULL)
		gtk_widget_set_sensitive (option_menu, FALSE);

	return option_menu;
}

#ifndef HAVE_GTK_ONLY
static void
on_combo_entry_changed (GnomeFileEntry *entry, gpointer user_data)
{
	BaconCdSelection *bcs = (BaconCdSelection *) user_data;
	const char *str;
	GtkWidget *widget;

	widget = gnome_file_entry_gtk_entry (entry);
	str = gtk_entry_get_text (GTK_ENTRY (widget));

	g_signal_emit (G_OBJECT (bcs),
			bcs_table_signals[DEVICE_CHANGED],
			0, str);
}
#endif /* !HAVE_GTK_ONLY */

GtkWidget *
bacon_cd_selection_new (void)
{
	GtkWidget *widget;
	BaconCdSelection *bcs;

	widget = GTK_WIDGET
		(g_object_new (bacon_cd_selection_get_type (), NULL));
	bcs = BACON_CD_SELECTION (widget);

#ifndef HAVE_GTK_ONLY
	if (bcs->priv->is_entry)
	{
		bcs->priv->widget = gnome_file_entry_new (NULL,
					_("Select the drive"));
		g_object_set (G_OBJECT (bcs->priv->widget),
				"use_filechooser", TRUE, NULL);
		g_signal_connect (G_OBJECT (bcs->priv->widget), "changed",
				G_CALLBACK (on_combo_entry_changed), bcs);

		gtk_box_pack_start (GTK_BOX (widget),
				bcs->priv->widget,
				TRUE,       /* expand */
				TRUE,       /* fill */
				0);         /* padding */
	} else
#endif /* !HAVE_GTK_ONLY */
	{
		bcs->priv->widget = cdrom_option_menu (bcs);

		g_signal_connect (bcs->priv->widget, "changed",
				(GCallback)option_menu_device_changed, bcs);

		gtk_box_pack_start (GTK_BOX (widget),
				bcs->priv->widget,
				TRUE,       /* expand */
				TRUE,       /* fill */
				0);         /* padding */
	}

	gtk_widget_show_all (bcs->priv->widget);

	return widget;
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
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

const char *
bacon_cd_selection_get_default_device (BaconCdSelection *bcs)
{
	GList *l;
	CDDrive *drive;

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

	g_return_if_fail (bcs != NULL);
	g_return_if_fail (BACON_IS_CD_SELECTION (bcs));

#ifndef HAVE_GTK_ONLY
	if (bcs->priv->is_entry != FALSE)
	{
		GtkWidget *entry;

		entry = gnome_file_entry_gtk_entry
			(GNOME_FILE_ENTRY (bcs->priv->widget));
		gtk_entry_set_text (GTK_ENTRY (entry), device);
	} else
#endif /* !HAVE_GTK_ONLY */
	{
		i = -1;
		found = FALSE;

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
			gtk_option_menu_set_history (GTK_OPTION_MENU
					(bcs->priv->widget), i);
		} else {
			/* If the device doesn't exist, set it back to
			 * the default */
			gtk_option_menu_set_history (GTK_OPTION_MENU
					(bcs->priv->widget), 0);

			drive = get_drive (bcs, 0);

			if (drive == NULL)
				return;

			g_signal_emit (G_OBJECT (bcs),
					bcs_table_signals [DEVICE_CHANGED],
					0, drive->device);
		}
			
	}
}

const char *
bacon_cd_selection_get_device (BaconCdSelection *bcs)
{
	CDDrive *drive;
	int i;

	g_return_val_if_fail (bcs != NULL, NULL);
	g_return_val_if_fail (BACON_IS_CD_SELECTION (bcs), NULL);

#ifndef HAVE_GTK_ONLY
	if (bcs->priv->is_entry != FALSE)
	{
		GtkWidget *entry;

		entry = gnome_file_entry_gtk_entry
			(GNOME_FILE_ENTRY (bcs->priv->widget));
		return gtk_entry_get_text (GTK_ENTRY (entry));
	} else
#endif /* !HAVE_GTK_ONLY */
	{
		i = gtk_option_menu_get_history (GTK_OPTION_MENU
				(bcs->priv->widget));
		drive = get_drive (bcs, i);

		return drive ? drive->device : NULL;
	}

	return NULL;
}

const CDDrive *
bacon_cd_selection_get_cdrom (BaconCdSelection *bcs)
{
	CDDrive *drive;
	int i;

	g_return_val_if_fail (bcs != NULL, NULL);
	g_return_val_if_fail (BACON_IS_CD_SELECTION (bcs), NULL);

	if (bcs->priv->is_entry != FALSE)
	{
		return NULL;
	} else {
		i = gtk_option_menu_get_history (GTK_OPTION_MENU
				(bcs->priv->widget));
		drive = get_drive (bcs, i);

		return drive;
	}

	return NULL;
}

