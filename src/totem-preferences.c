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
#include <gtk/gtkmessagedialog.h>
#include <string.h>

#include "totem.h"
#include "totem-private.h"
#include "totem-preferences.h"
#include "bacon-cd-selection.h"

#include "debug.h"

#define PROPRIETARY_PLUGINS ".gnome2"G_DIR_SEPARATOR_S"totem-addons"

static gboolean
totem_display_is_local (Totem *totem)
{
	const char *name, *work;
	int display, screen;

	name = gdk_display_get_name (gdk_display_get_default ());
	if (name == NULL)
		return TRUE;

	work = strstr (name, ":");
	if (work == NULL)
		return TRUE;

	/* Get to the character after the colon */
	work++;
	if (work == NULL)
		return TRUE;

	if (sscanf (work, "%d.%d", &display, &screen) != 2)
		return TRUE;

	if (display < 10)
		return TRUE;

	return FALSE;
}

static void
hide_prefs (GtkWidget *widget, int trash, Totem *totem)
{
	gtk_widget_hide (totem->prefs);
}

static gboolean
ask_show_visuals (Totem *totem)
{
	GtkWidget *dialog;
	int answer;

	dialog =
		gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_YES_NO,
				_("It seems you are running Totem remotely.\n"
				"Are you sure you want to enable the visual "
				"effects?"));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
			GTK_RESPONSE_NO);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return (answer == GTK_RESPONSE_YES ? TRUE : FALSE);
}

static void
on_checkbutton1_toggled (GtkToggleButton *togglebutton, Totem *totem)
{
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/auto_resize",
			value, NULL);
	bacon_video_widget_set_auto_resize
		(BACON_VIDEO_WIDGET (totem->bvw), value);
}

static void              
on_checkbutton2_toggled (GtkToggleButton *togglebutton, Totem *totem)
{                               
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);

	if (value == TRUE && totem_display_is_local (totem) == FALSE)
	{
		if (ask_show_visuals (totem) == FALSE)
		{
			gconf_client_set_bool (totem->gc,
					GCONF_PREFIX"/show_vfx", FALSE, NULL);
			gtk_toggle_button_set_active (togglebutton, FALSE);
			return;
		}
	}

	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/show_vfx", value, NULL);
	if (bacon_video_widget_set_show_visuals
		(BACON_VIDEO_WIDGET (totem->bvw), value) == FALSE)
	{
		totem_action_error (_("The change of this setting will only "
					"take effect for the next movie, or "
					"when Totem is restarted"),
				totem);
	}
}

static void
on_combo_entry1_changed (BaconCdSelection *bcs, char *device, Totem *totem)
{
	const char *str;

	str = bacon_cd_selection_get_device (bcs);
	gconf_client_set_string (totem->gc, GCONF_PREFIX"/mediadev", str, NULL);
	bacon_video_widget_set_media_device
		(BACON_VIDEO_WIDGET (totem->bvw), str);
}

static void
auto_resize_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, Totem *totem)
{
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
		GConfEntry *entry, Totem *totem)
{
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
		GConfEntry *entry, Totem *totem)
{
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
option_menu_connection_changed (GtkOptionMenu *option_menu, Totem *totem)
{
	int i;

	i = gtk_option_menu_get_history (option_menu);
	bacon_video_widget_set_connection_speed
		(BACON_VIDEO_WIDGET (totem->bvw), i);
}

static void
on_button1_clicked (GtkButton *button, Totem *totem)
{
	GError *err = NULL;
	char *path, *cmd;

	path = path = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), PROPRIETARY_PLUGINS, NULL);
	if (g_file_test (path, G_FILE_TEST_IS_DIR) == FALSE)
		mkdir (path, 0775);

	cmd = g_strdup_printf ("nautilus --no-default-window %s", path);
	g_free (path);

	if (g_spawn_command_line_async (cmd, &err) == FALSE)
	{
		char *msg;

		msg = g_strdup_printf ("Totem could not start the file manager\nReason: %s.", err->message);
		totem_action_error (msg, totem);
		g_free (msg);
		g_error_free (err);
	}

	g_free (cmd);
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
	gboolean show_visuals, auto_resize, is_local;
	int connection_speed;
	char *path;

	g_return_if_fail (totem->gc != NULL);

	is_local = totem_display_is_local (totem);

	gconf_client_add_dir (totem->gc, "/apps/totem",
			GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/auto_resize",
			(GConfClientNotifyFunc) auto_resize_changed_cb,
			totem, NULL, NULL);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/show_vfx",
			(GConfClientNotifyFunc) show_vfx_changed_cb,
			totem, NULL, NULL);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/mediadev",
			(GConfClientNotifyFunc) mediadev_changed_cb,
			totem, NULL, NULL);

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
	if (is_local == FALSE && show_visuals == TRUE)
		show_visuals = ask_show_visuals (totem);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (item), show_visuals);
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

	item = glade_xml_get_widget (totem->xml, "button1");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_button1_clicked), totem);
	path = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), PROPRIETARY_PLUGINS, NULL);
	bacon_video_widget_set_proprietary_plugins_path
		(totem->bvw, path);
	g_free (path);
}

