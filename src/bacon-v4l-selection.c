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

#include <config.h>

#include <string.h>
#ifndef HAVE_GTK_ONLY
#include <gnome.h>
#else
#include <gtk/gtk.h>
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
#else
#include <libgnome/gnome-i18n.h>
#endif /* HAVE_GTK_ONLY */

#include "bacon-v4l-selection.h"

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
	gboolean is_entry;
	GtkWidget *widget;
	GList *devs;
};


static void bacon_v4l_selection_class_init (BaconV4lSelectionClass *klass);
static void bacon_v4l_selection_instance_init (BaconV4lSelection *bvs);

static void bacon_v4l_selection_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec);
static void bacon_v4l_selection_get_property (GObject *object, guint property_id,
		GValue *value, GParamSpec *pspec);

static void bacon_v4l_selection_realize (GtkWidget *widget);
static void bacon_v4l_selection_unrealize (GtkWidget *widget);
static void bacon_v4l_selection_finalize (GObject *object);

static GtkWidgetClass *parent_class = NULL;

static int bvs_table_signals[LAST_SIGNAL] = { 0 };

static VideoDev *
get_drive (BaconV4lSelection *bvs, int nr)
{
	GList *item;

	item = g_list_nth (bvs->priv->devs, nr);
	if (item == NULL)
		return NULL;
	else
		return item->data;
}


GtkType
bacon_v4l_selection_get_type (void)
{
	static GtkType bacon_v4l_selection_type = 0;

	if (!bacon_v4l_selection_type) {
		static const GTypeInfo bacon_v4l_selection_info = {
			sizeof (BaconV4lSelectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) bacon_v4l_selection_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (BaconV4lSelection),
			0 /* n_preallocs */,
			(GInstanceInitFunc) bacon_v4l_selection_instance_init,
			NULL
		};

		bacon_v4l_selection_type = g_type_register_static
			(GTK_TYPE_VBOX,
			 "BaconV4lSelection", &bacon_v4l_selection_info,
			 (GTypeFlags)0);
	}

	return bacon_v4l_selection_type;
}

static void
bacon_v4l_selection_class_init (BaconV4lSelectionClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;

	parent_class = gtk_type_class (gtk_vbox_get_type ());

	/* GtkWidget */
	widget_class->realize = bacon_v4l_selection_realize;
	widget_class->unrealize = bacon_v4l_selection_unrealize;

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
bacon_v4l_selection_instance_init (BaconV4lSelection *bvs)
{
	bvs->priv = g_new0 (BaconV4lSelectionPrivate, 1);

#if defined (__linux__) || defined (__FreeBSD__)
	bvs->priv->is_entry = FALSE;
#else
	bvs->priv->is_entry = TRUE;
#endif

	bvs->priv->devs = NULL;
}

static void
bacon_v4l_selection_realize (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->realize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);
	}
}

static void
bacon_v4l_selection_unrealize (GtkWidget *widget)
{
	if (GTK_WIDGET_CLASS (parent_class)->unrealize != NULL) {
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
	}
}

static void
bacon_v4l_selection_finalize (GObject *object)
{
	GList *l;

	BaconV4lSelection *bvs = (BaconV4lSelection *) object;
	G_OBJECT_CLASS (parent_class)->finalize (object);

	gtk_widget_destroy (bvs->priv->widget);

	l = bvs->priv->devs;
	while (l != NULL)
	{
		VideoDev *dev = l->data;

		video_dev_free (dev);
		l = g_list_remove (l, dev);
		g_free (dev);
	}

	bvs->priv = NULL;
	bvs = NULL;
}

static void
option_menu_device_changed (GtkOptionMenu *option_menu, gpointer user_data)
{
	BaconV4lSelection *bvs = (BaconV4lSelection *) user_data;
	VideoDev *drive;
	int i;

	i = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
	drive = get_drive (bvs, i);

	g_signal_emit (G_OBJECT (bvs),
			bvs_table_signals[DEVICE_CHANGED],
			0, drive->device);
}

static GtkWidget *
video_dev_option_menu (BaconV4lSelection *bvs)
{
	GList *l;
	GtkWidget *option_menu, *menu, *item;
	VideoDev *dev;

	bvs->priv->devs = scan_for_video_devices ();

	menu = gtk_menu_new();
	gtk_widget_show(menu);

	option_menu = gtk_option_menu_new ();

	for (l = bvs->priv->devs; l != NULL; l = l->next)
	{
		dev = l->data;

		if (dev->display_name == NULL)
			g_warning ("dev->display_name != NULL failed");
		item = gtk_menu_item_new_with_label (dev->display_name
				? dev->display_name : _("Unnamed Video Device"));
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}
	gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 0);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

	if (bvs->priv->devs == NULL)
		gtk_widget_set_sensitive (option_menu, FALSE);

	return option_menu;
}

#ifndef HAVE_GTK_ONLY
static void
on_combo_entry_changed (GnomeFileEntry *entry, gpointer user_data)
{
	BaconV4lSelection *bvs = (BaconV4lSelection *) user_data;
	const char *str;
	GtkWidget *widget;

	widget = gnome_file_entry_gtk_entry (entry);
	str = gtk_entry_get_text (GTK_ENTRY (widget));

	g_signal_emit (G_OBJECT (bvs),
			bvs_table_signals[DEVICE_CHANGED],
			0, str);
}
#endif /* !HAVE_GTK_ONLY */

GtkWidget *
bacon_v4l_selection_new (void)
{
	GtkWidget *widget;
	BaconV4lSelection *bvs;

	widget = GTK_WIDGET
		(g_object_new (bacon_v4l_selection_get_type (), NULL));
	bvs = BACON_V4L_SELECTION (widget);

#ifndef HAVE_GTK_ONLY
	if (bvs->priv->is_entry)
	{
		bvs->priv->widget = gnome_file_entry_new (NULL,
					_("Select the drive"));
		g_signal_connect (G_OBJECT (bvs->priv->widget), "changed",
				G_CALLBACK (on_combo_entry_changed), bvs);

		gtk_box_pack_start (GTK_BOX (widget),
				bvs->priv->widget,
				TRUE,       /* expand */
				TRUE,       /* fill */
				0);         /* padding */
	} else
#endif /* !HAVE_GTK_ONLY */
	{
		bvs->priv->widget = video_dev_option_menu (bvs);

		g_signal_connect (bvs->priv->widget, "changed",
				(GCallback)option_menu_device_changed, bvs);

		gtk_box_pack_start (GTK_BOX (widget),
				bvs->priv->widget,
				TRUE,       /* expand */
				TRUE,       /* fill */
				0);         /* padding */
	}

	gtk_widget_show_all (bvs->priv->widget);

	return widget;
}

/* Properties */
static void
bacon_v4l_selection_set_property (GObject *object, guint property_id,
		const GValue *value, GParamSpec *pspec)
{
	BaconV4lSelection *bvs;

	g_return_if_fail (BACON_IS_CD_SELECTION (object));

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

	g_return_if_fail (BACON_IS_CD_SELECTION (object));

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

	l = bvs->priv->devs;
	if (bvs->priv->devs == NULL)
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

	g_return_if_fail (bvs != NULL);
	g_return_if_fail (BACON_IS_CD_SELECTION (bvs));

#ifndef HAVE_GTK_ONLY
	if (bvs->priv->is_entry != FALSE)
	{
		GtkWidget *entry;

		entry = gnome_file_entry_gtk_entry
			(GNOME_FILE_ENTRY (bvs->priv->widget));
		gtk_entry_set_text (GTK_ENTRY (entry), device);
	} else
#endif /* !HAVE_GTK_ONLY */
	{
		i = -1;
		found = FALSE;

		for (l = bvs->priv->devs; l != NULL && found == FALSE;
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
					(bvs->priv->widget), i);
		} else {
			/* If the device doesn't exist, set it back to
			 * the default */
			gtk_option_menu_set_history (GTK_OPTION_MENU
					(bvs->priv->widget), 0);

			drive = get_drive (bvs, 0);

			if (drive == NULL)
				return;

			g_signal_emit (G_OBJECT (bvs),
					bvs_table_signals [DEVICE_CHANGED],
					0, drive->device);
		}
			
	}
}

const char *
bacon_v4l_selection_get_device (BaconV4lSelection *bvs)
{
	VideoDev *drive;
	int i;

	g_return_val_if_fail (bvs != NULL, NULL);
	g_return_val_if_fail (BACON_IS_CD_SELECTION (bvs), NULL);

#ifndef HAVE_GTK_ONLY
	if (bvs->priv->is_entry != FALSE)
	{
		GtkWidget *entry;

		entry = gnome_file_entry_gtk_entry
			(GNOME_FILE_ENTRY (bvs->priv->widget));
		return gtk_entry_get_text (GTK_ENTRY (entry));
	} else
#endif /* !HAVE_GTK_ONLY */
	{
		i = gtk_option_menu_get_history (GTK_OPTION_MENU
				(bvs->priv->widget));
		drive = get_drive (bvs, i);

		return drive ? drive->device : NULL;
	}

	return NULL;
}

const VideoDev *
bacon_v4l_selection_get_video_device (BaconV4lSelection *bvs)
{
	VideoDev *drive;
	int i;

	g_return_val_if_fail (bvs != NULL, NULL);
	g_return_val_if_fail (BACON_IS_CD_SELECTION (bvs), NULL);

	if (bvs->priv->is_entry != FALSE)
	{
		return NULL;
	} else {
		i = gtk_option_menu_get_history (GTK_OPTION_MENU
				(bvs->priv->widget));
		drive = get_drive (bvs, i);

		return drive;
	}

	return NULL;
}

