/* 
 * Copyright (C) 2001,2002,2003 Bastien Nocera <hadess@hadess.net>
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
 */

#include <config.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "totem.h"
#include "totem-private.h"
#include "totem-preferences.h"
#include "bacon-cd-selection.h"

#include "debug.h"

static void
hide_prefs (GtkWidget *widget, int trash, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;

	gtk_widget_hide (totem->prefs);
}

static void
on_checkbutton1_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/auto_resize",
			value, NULL);
	bacon_video_widget_set_auto_resize
		(BACON_VIDEO_WIDGET (totem->bvw), value);
}

static void              
on_checkbutton2_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{                               
	Totem *totem = (Totem *)user_data;
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/show_vfx", value, NULL);
	if (bacon_video_widget_set_show_visuals
		(BACON_VIDEO_WIDGET (totem->bvw), value) == FALSE)
	{
		totem_action_error (_("The change of this setting will only "
					"take effect for the next movie, or "
					"when Totem is restarted"),
				GTK_WINDOW (totem->win));
	}
}

static void
on_combo_entry1_changed (BaconCdSelection *bcs, char *device,
		gpointer user_data)
{
	Totem *totem = (Totem *)user_data;
	const char *str;

	str = bacon_cd_selection_get_device (bcs);
	gconf_client_set_string (totem->gc, GCONF_PREFIX"/mediadev", str, NULL);
	bacon_video_widget_set_media_device
		(BACON_VIDEO_WIDGET (totem->bvw), str);
}

static void
auto_resize_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "checkbutton1");
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_checkbutton1_toggled, totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item),
			gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/auto_resize", NULL));
}

static void
show_vfx_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "checkbutton2");
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_checkbutton2_toggled, totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item),
			gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/show_vfx", NULL));

	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton2_toggled), totem);
}

static void
mediadev_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	GtkWidget *item;
	char *mediadev;

	item = glade_xml_get_widget (totem->xml, "custom3");
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_combo_entry1_changed, totem);

	mediadev = gconf_client_get_string (totem->gc,
			GCONF_PREFIX"/mediadev", NULL);

	if (mediadev == NULL || strcmp (mediadev, "") == 0)
		mediadev = g_strdup ("/dev/cdrom");

	bacon_cd_selection_set_device (BACON_CD_SELECTION (item), mediadev);
	bacon_video_widget_set_media_device
		(BACON_VIDEO_WIDGET (totem->bvw), mediadev);

	g_signal_connect (G_OBJECT (item), "device-changed",
			G_CALLBACK (on_combo_entry1_changed), totem);
}

static void
option_menu_connection_changed (GtkOptionMenu *option_menu, gpointer user_data)
{
	Totem *totem = (Totem *) user_data;
	int i;

	i = gtk_option_menu_get_history (option_menu);
	bacon_video_widget_set_connection_speed
		(BACON_VIDEO_WIDGET (totem->bvw), i);
}

GtkWidget *
bacon_cd_selection_create (void)
{
	GtkWidget *widget;

	widget = bacon_cd_selection_new ();
	gtk_widget_show (widget);

	return widget;
}

void
totem_setup_preferences (Totem *totem)
{
	GtkWidget *item;
	const char *mediadev;
	gboolean show_visuals, auto_resize;
	int connection_speed;

	g_return_if_fail (totem->gc != NULL);

	gconf_client_add_dir (totem->gc, "/apps/totem",
			GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/auto_resize",
			auto_resize_changed_cb, totem, NULL, NULL);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/show_vfx",
			show_vfx_changed_cb, totem, NULL, NULL);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/mediadev",
			mediadev_changed_cb, totem, NULL, NULL);

	totem->prefs = glade_xml_get_widget (totem->xml, "dialog1");

	g_signal_connect (G_OBJECT (totem->prefs),
			"response", G_CALLBACK (hide_prefs), (gpointer) totem);
	g_signal_connect (G_OBJECT (totem->prefs), "delete-event",
			G_CALLBACK (hide_prefs), (gpointer) totem);

	auto_resize = gconf_client_get_bool (totem->gc,
			GCONF_PREFIX"/auto_resize", NULL);
	item = glade_xml_get_widget (totem->xml, "checkbutton1");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), auto_resize);
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton1_toggled), totem);
	bacon_video_widget_set_auto_resize
		(BACON_VIDEO_WIDGET (totem->bvw), auto_resize);

	item = glade_xml_get_widget (totem->xml, "checkbutton2");
	show_visuals = gconf_client_get_bool (totem->gc,
			GCONF_PREFIX"/show_vfx", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), show_visuals);
	bacon_video_widget_set_show_visuals
		(BACON_VIDEO_WIDGET (totem->bvw), show_visuals);
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton2_toggled), totem);

	item = glade_xml_get_widget (totem->xml, "custom3");
	mediadev = gconf_client_get_string
		(totem->gc, GCONF_PREFIX"/mediadev", NULL);
	if (mediadev == NULL || (strcmp (mediadev, "") == 0)
			|| (strcmp (mediadev, "auto") == 0))
	{
		mediadev = bacon_cd_selection_get_default_device
			(BACON_CD_SELECTION (item));
		gconf_client_set_string (totem->gc, GCONF_PREFIX"/mediadev",
				mediadev, NULL);
		bacon_video_widget_set_media_device
			(BACON_VIDEO_WIDGET (totem->bvw), mediadev);
	} else {
		bacon_video_widget_set_media_device
			(BACON_VIDEO_WIDGET (totem->bvw), mediadev);
	}

	bacon_cd_selection_set_device (BACON_CD_SELECTION (item),
			gconf_client_get_string
			(totem->gc, GCONF_PREFIX"/mediadev", NULL));
	g_signal_connect (G_OBJECT (item), "device-changed",
			G_CALLBACK (on_combo_entry1_changed), totem);

	connection_speed = bacon_video_widget_get_connection_speed (totem->bvw);
	item = glade_xml_get_widget (totem->xml, "optionmenu1");
	gtk_option_menu_set_history (GTK_OPTION_MENU (item),
			connection_speed);
	g_signal_connect (item, "changed",
			(GCallback)option_menu_connection_changed, totem);
}

