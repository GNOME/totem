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
totem_prefs_set_show_visuals (Totem *totem, gboolean value, gboolean warn)
{
	GtkWidget *item;

	gconf_client_set_bool (totem->gc,
			GCONF_PREFIX"/show_vfx", value, NULL);

	item = glade_xml_get_widget (totem->xml, "tpw_visuals_type_label");
	gtk_widget_set_sensitive (item, value);
	item = glade_xml_get_widget (totem->xml,
			"tpw_visuals_type_optionmenu");
	gtk_widget_set_sensitive (item, value);
	item = glade_xml_get_widget (totem->xml, "tpw_visuals_size_label");
	gtk_widget_set_sensitive (item, value);
	item = glade_xml_get_widget (totem->xml,
			"tpw_visuals_size_optionmenu");
	gtk_widget_set_sensitive (item, value);

	if (warn == FALSE)
	{
		bacon_video_widget_set_show_visuals
			(BACON_VIDEO_WIDGET (totem->bvw), value);
		return;
	}

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
on_checkbutton2_toggled (GtkToggleButton *togglebutton, Totem *totem)
{
	GtkWidget *item;
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

	totem_prefs_set_show_visuals (totem, value, TRUE);
}

static void
on_tvout_toggled (GtkToggleButton *togglebutton, Totem *totem)
{
	TvOutType type;
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	if (value == FALSE)
		return;

	type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (togglebutton),
				"tvout_type"));
	value = bacon_video_widget_set_tv_out
		(BACON_VIDEO_WIDGET (totem->bvw), type);

	if (value == TRUE)
		totem_action_error (_("Switching on or off this type of TV-Out requires a restart to take effect."), totem);
}

static void
on_deinterlace1_activate (GtkCheckMenuItem *checkmenuitem, Totem *totem)
{
	gboolean value;

	value = gtk_check_menu_item_get_active (checkmenuitem);
	bacon_video_widget_set_deinterlacing (totem->bvw, value);
	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/deinterlace",
			value, NULL);
}

static void
deinterlace_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, Totem *totem)
{
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "tmw_deinterlace_menu_item");
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_deinterlace1_activate, totem);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
			gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/deinterlace", NULL));

	g_signal_connect (G_OBJECT (item),  "activate",
			G_CALLBACK (on_deinterlace1_activate), totem);
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

	item = glade_xml_get_widget (totem->xml, "tpw_display_checkbutton");
	g_signal_handlers_disconnect_by_func (G_OBJECT (item),
			on_checkbutton1_toggled, totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item),
			gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/auto_resize", NULL));

	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton1_toggled), totem);
}

static void
show_vfx_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, Totem *totem)
{
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "tpw_visuals_checkbutton");
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

	item = glade_xml_get_widget (totem->xml, "tpw_device_combo");
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

static void
visual_menu_changed (GtkOptionMenu *option_menu, Totem *totem)
{
	GList *list;
	const char *old_name;
	char *name;
	int i;

	i = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
	list = bacon_video_widget_get_visuals_list (totem->bvw);
	name = g_list_nth_data (list, i);

	old_name = gconf_client_get_string (totem->gc,
			GCONF_PREFIX"/visual", NULL);

	if (old_name == NULL || strcmp (old_name, name) != 0)
	{
		gconf_client_set_string (totem->gc, GCONF_PREFIX"/visual",
				name, NULL);

		if (bacon_video_widget_set_visuals (totem->bvw, name) == TRUE)
			totem_action_error (_("Changing the visuals effect type will require a restart to take effect."), totem);
	}
}

static void
visual_quality_menu_changed (GtkOptionMenu *option_menu, Totem *totem)
{
	int i;

	i = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
	gconf_client_set_int (totem->gc,
			GCONF_PREFIX"/visual_quality", i, NULL);
	bacon_video_widget_set_visuals_quality (totem->bvw, i);
}

GtkWidget *
bacon_cd_selection_create (void)
{
	GtkWidget *widget;

	widget = bacon_cd_selection_new ();
	gtk_widget_show (widget);

	return widget;
}

static void
brightness_changed (GtkRange *range, Totem *totem)
{
	gdouble i;

	i = gtk_range_get_value (range);
	bacon_video_widget_set_video_property (totem->bvw,
			BVW_VIDEO_BRIGHTNESS, (int) i);
}

static void
contrast_changed (GtkRange *range, Totem *totem)
{
	gdouble i;

	i = gtk_range_get_value (range);
	bacon_video_widget_set_video_property (totem->bvw,
			BVW_VIDEO_CONTRAST, (int) i);
}

static void
audio_out_menu_changed (GtkOptionMenu *option_menu, Totem *totem)
{
	BaconVideoWidgetAudioOutType audio_out;
	audio_out = gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
	bacon_video_widget_set_audio_out_type (totem->bvw, audio_out);
}

void
totem_setup_preferences (Totem *totem)
{
	GtkWidget *item, *menu;
	const char *mediadev;
	gboolean show_visuals, auto_resize, is_local, deinterlace;
	int connection_speed, i, found;
	char *path, *visual;
	GList *list, *l;
	BaconVideoWidgetAudioOutType audio_out;

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

	totem->prefs = glade_xml_get_widget (totem->xml, "totem_preferences_window");

	g_signal_connect (G_OBJECT (totem->prefs),
			"response", G_CALLBACK (hide_prefs), (gpointer) totem);
	g_signal_connect (G_OBJECT (totem->prefs), "delete-event",
			G_CALLBACK (hide_prefs), (gpointer) totem);

	/* Auto-resize */
	auto_resize = gconf_client_get_bool (totem->gc,
			GCONF_PREFIX"/auto_resize", NULL);
	item = glade_xml_get_widget (totem->xml, "tpw_display_checkbutton");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), auto_resize);
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton1_toggled), totem);
	bacon_video_widget_set_auto_resize
		(BACON_VIDEO_WIDGET (totem->bvw), auto_resize);

	/* Media device */
	item = glade_xml_get_widget (totem->xml, "tpw_device_combo");
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

	/* Connection Speed */
	connection_speed = bacon_video_widget_get_connection_speed (totem->bvw);
	item = glade_xml_get_widget (totem->xml, "tpw_speed_optionmenu");
	gtk_option_menu_set_history (GTK_OPTION_MENU (item),
			connection_speed);
	g_signal_connect (item, "changed",
			G_CALLBACK (option_menu_connection_changed), totem);

	/* Proprietary plugins */
	item = glade_xml_get_widget (totem->xml, "tpw_plugins_button");
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_button1_clicked), totem);
	path = g_build_path (G_DIR_SEPARATOR_S,
			g_get_home_dir (), PROPRIETARY_PLUGINS, NULL);
	if (g_file_test (path, G_FILE_TEST_IS_DIR) == FALSE)
		mkdir (path, 0775);
	bacon_video_widget_set_proprietary_plugins_path (totem->bvw, path);
	g_free (path);

	/* Enable visuals */
	item = glade_xml_get_widget (totem->xml, "tpw_visuals_checkbutton");
	show_visuals = gconf_client_get_bool (totem->gc,
			GCONF_PREFIX"/show_vfx", NULL);
	if (is_local == FALSE && show_visuals == TRUE)
		show_visuals = ask_show_visuals (totem);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (item), show_visuals);
	totem_prefs_set_show_visuals (totem, show_visuals, FALSE);
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_checkbutton2_toggled), totem);

	/* Visuals list */
	list = bacon_video_widget_get_visuals_list (totem->bvw);
	menu = gtk_menu_new ();
	gtk_widget_show (menu);

	visual = gconf_client_get_string (totem->gc,
			GCONF_PREFIX"/visual", NULL);
	if (visual == NULL || strcmp (visual, "") == 0)
		visual = g_strdup ("goom");

	i = 0;
	for (l = list; l != NULL; l = l->next)
	{
		const char *name = l->data;

		item = gtk_menu_item_new_with_label (name);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		if (strcmp (name, visual) == 0)
			found = i;

		i++;
	}

	/* Visualisation quality */
	i = gconf_client_get_int (totem->gc,
			GCONF_PREFIX"/visual_quality", NULL);
	bacon_video_widget_set_visuals_quality (totem->bvw, i);
	item = glade_xml_get_widget (totem->xml, "tpw_visuals_size_optionmenu");
	gtk_option_menu_set_history (GTK_OPTION_MENU (item), i);
	g_signal_connect (G_OBJECT (item), "changed",
			G_CALLBACK (visual_quality_menu_changed), totem);

	item = glade_xml_get_widget (totem->xml, "tpw_visuals_type_optionmenu");
	gtk_option_menu_set_menu (GTK_OPTION_MENU (item), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (item), found);
	g_signal_connect (G_OBJECT (item), "changed",
			G_CALLBACK (visual_menu_changed), totem);

	/* Brightness */
	item = glade_xml_get_widget (totem->xml, "tpw_bright_scale");
	i = bacon_video_widget_get_video_property (totem->bvw,
			BVW_VIDEO_BRIGHTNESS);
	gtk_range_set_value (GTK_RANGE (item), (gdouble) i);
	g_signal_connect (G_OBJECT (item), "value-changed",
			G_CALLBACK (brightness_changed), totem);

	/* Contrast */
	item = glade_xml_get_widget (totem->xml, "tpw_contrast_scale");
	i = bacon_video_widget_get_video_property (totem->bvw,
			BVW_VIDEO_CONTRAST);
	gtk_range_set_value (GTK_RANGE (item), (gdouble) i);
	g_signal_connect (G_OBJECT (item), "value-changed",
			G_CALLBACK (contrast_changed), totem);

	/* Sound output type */
	item = glade_xml_get_widget (totem->xml, "tpw_sound_output_optionmenu");
	audio_out = bacon_video_widget_get_audio_out_type (totem->bvw);
	gtk_option_menu_set_history (GTK_OPTION_MENU (item), audio_out);
	g_signal_connect (G_OBJECT (item), "changed",
			G_CALLBACK (audio_out_menu_changed), totem);

	/* This one is for the deinterlacing menu, not really our dialog
	 * but we do it anyway */
	item = glade_xml_get_widget (totem->xml, "tmw_deinterlace_menu_item");
	deinterlace = gconf_client_get_bool (totem->gc,
			GCONF_PREFIX"/deinterlace", NULL);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
				deinterlace);
	bacon_video_widget_set_deinterlacing (totem->bvw, deinterlace);
	gconf_client_notify_add (totem->gc, GCONF_PREFIX"/deinterlace",
			(GConfClientNotifyFunc) deinterlace_changed_cb,
			totem, NULL, NULL);
	g_signal_connect (G_OBJECT (item), "activate",
			G_CALLBACK (on_deinterlace1_activate), totem);
}

void
totem_preferences_tvout_setup (Totem *totem)
{
	GtkWidget *item;
	TvOutType type;
	const char *name;

	type = bacon_video_widget_get_tv_out (totem->bvw);
	switch (type)
	{
	case TV_OUT_NONE:
		name = "tpw_notvout_radio_button";
		break;
	case TV_OUT_TVMODE:
		name = "tpw_tvoutmode_radio_button";
		break;
	case TV_OUT_DXR3:
		name = "tpw_dxr3tvout_radio_button";
		break;
	default:
		g_assert_not_reached ();
	}

	item = glade_xml_get_widget (totem->xml, name);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);

	item = glade_xml_get_widget (totem->xml, "tpw_notvout_radio_button");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_tvout_toggled), totem);
	g_object_set_data (G_OBJECT (item), "tvout_type",
			GINT_TO_POINTER (TV_OUT_NONE));
	item = glade_xml_get_widget (totem->xml, "tpw_tvoutmode_radio_button");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_tvout_toggled), totem);
	g_object_set_data (G_OBJECT (item), "tvout_type",
			GINT_TO_POINTER (TV_OUT_TVMODE));
	item = glade_xml_get_widget (totem->xml, "tpw_dxr3tvout_radio_button");
	g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_tvout_toggled), totem);
	g_object_set_data (G_OBJECT (item), "tvout_type",
			GINT_TO_POINTER (TV_OUT_DXR3));
}

void
totem_preferences_visuals_setup (Totem *totem)
{
	char *visual;

	visual = gconf_client_get_string (totem->gc,
			GCONF_PREFIX"/visual", NULL);
	if (visual == NULL || strcmp (visual, "") == 0)
		visual = g_strdup ("goom");

	bacon_video_widget_set_visuals (totem->bvw, visual);
}
