/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "totem.h"
#include "totem-private.h"
#include "totem-preferences.h"
#include "totem-interface.h"
#include "video-utils.h"
#include "totem-subtitle-encoding.h"
#include "totem-plugins-engine.h"

/* Callback functions for GtkBuilder */
G_MODULE_EXPORT void checkbutton1_toggled_cb (GtkToggleButton *togglebutton, Totem *totem);
G_MODULE_EXPORT void checkbutton2_toggled_cb (GtkToggleButton *togglebutton, Totem *totem);
G_MODULE_EXPORT void checkbutton3_toggled_cb (GtkToggleButton *togglebutton, Totem *totem);
G_MODULE_EXPORT void audio_screensaver_button_toggled_cb (GtkToggleButton *togglebutton, Totem *totem);
G_MODULE_EXPORT void no_deinterlace_toggled_cb (GtkToggleButton *togglebutton, Totem *totem);
G_MODULE_EXPORT void remember_position_checkbutton_toggled_cb (GtkToggleButton *togglebutton, Totem *totem);
G_MODULE_EXPORT void connection_combobox_changed (GtkComboBox *combobox, Totem *totem);
G_MODULE_EXPORT void visual_menu_changed (GtkComboBox *combobox, Totem *totem);
G_MODULE_EXPORT void visual_quality_menu_changed (GtkComboBox *combobox, Totem *totem);
G_MODULE_EXPORT void brightness_changed (GtkRange *range, Totem *totem);
G_MODULE_EXPORT void contrast_changed (GtkRange *range, Totem *totem);
G_MODULE_EXPORT void saturation_changed (GtkRange *range, Totem *totem);
G_MODULE_EXPORT void hue_changed (GtkRange *range, Totem *totem);
G_MODULE_EXPORT void tpw_color_reset_clicked_cb (GtkButton *button, Totem *totem);
G_MODULE_EXPORT void audio_out_menu_changed (GtkComboBox *combobox, Totem *totem);
G_MODULE_EXPORT void font_set_cb (GtkFontButton * fb, Totem * totem);
G_MODULE_EXPORT void encoding_set_cb (GtkComboBox *cb, Totem *totem);
G_MODULE_EXPORT void auto_chapters_toggled_cb (GtkToggleButton *togglebutton, Totem *totem);

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
				_("Enable visual effects?"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("It seems you are running Totem remotely.\n"
						    "Are you sure you want to enable the visual "
						    "effects?"));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
			GTK_RESPONSE_NO);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return (answer == GTK_RESPONSE_YES ? TRUE : FALSE);
}

void
checkbutton1_toggled_cb (GtkToggleButton *togglebutton, Totem *totem)
{
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	g_settings_set_boolean (totem->settings, "auto-resize", value);
	bacon_video_widget_set_auto_resize
		(BACON_VIDEO_WIDGET (totem->bvw), value);
}

static void
totem_prefs_set_show_visuals (Totem *totem, gboolean value)
{
	GtkWidget *item;

	g_settings_set_boolean (totem->settings, "show-vfx", value);

	item = GTK_WIDGET (gtk_builder_get_object (totem->xml, "tpw_visuals_type_label"));
	gtk_widget_set_sensitive (item, value);
	item = GTK_WIDGET (gtk_builder_get_object (totem->xml,
			"tpw_visuals_type_combobox"));
	gtk_widget_set_sensitive (item, value);
	item = GTK_WIDGET (gtk_builder_get_object (totem->xml, "tpw_visuals_size_label"));
	gtk_widget_set_sensitive (item, value);
	item = GTK_WIDGET (gtk_builder_get_object (totem->xml,
			"tpw_visuals_size_combobox"));
	gtk_widget_set_sensitive (item, value);

	bacon_video_widget_set_show_visualizations
		(BACON_VIDEO_WIDGET (totem->bvw), value);
}

void
checkbutton2_toggled_cb (GtkToggleButton *togglebutton, Totem *totem)
{
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);

	if (value != FALSE && totem_display_is_local () == FALSE)
	{
		if (ask_show_visuals (totem) == FALSE)
		{
			g_settings_set_boolean (totem->settings, "show-vfx", FALSE);
			gtk_toggle_button_set_active (togglebutton, FALSE);
			return;
		}
	}

	totem_prefs_set_show_visuals (totem, value);
}

void
checkbutton3_toggled_cb (GtkToggleButton *togglebutton, Totem *totem)
{
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	g_settings_set_boolean (totem->settings, "autoload-subtitles", value);
	totem->autoload_subs = value;
}

void
audio_screensaver_button_toggled_cb (GtkToggleButton *togglebutton, Totem *totem)
{
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);
	g_settings_set_boolean (totem->settings, "lock-screensaver-on-audio", value);
}

void
no_deinterlace_toggled_cb (GtkToggleButton *togglebutton, Totem *totem)
{
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);

	bacon_video_widget_set_deinterlacing (totem->bvw, !value);
	g_settings_set_boolean (totem->settings, "disable-deinterlacing", value);
}

static void
no_deinterlace_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	GObject *button;
	gboolean value;

	button = gtk_builder_get_object (totem->xml, "tpw_no_deinterlace_checkbutton");

	g_signal_handlers_block_matched (button,
					 G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, totem);

	value = g_settings_get_boolean (totem->settings, "disable-deinterlacing");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), value);
	bacon_video_widget_set_deinterlacing (totem->bvw, !value);

	g_signal_handlers_unblock_matched (button,
					   G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, totem);
}

void
remember_position_checkbutton_toggled_cb (GtkToggleButton *togglebutton, Totem *totem)
{
	gboolean value;

	value = gtk_toggle_button_get_active (togglebutton);

	g_settings_set_boolean (totem->settings, "remember-position", value);
	totem->remember_position = value;
}

static void
remember_position_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	GObject *item;

	item = gtk_builder_get_object (totem->xml, "tpw_remember_position_checkbutton");
	g_signal_handlers_block_by_func (item, remember_position_checkbutton_toggled_cb,
					 totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), g_settings_get_boolean (settings, "remember-position"));

	g_signal_handlers_unblock_by_func (item, remember_position_checkbutton_toggled_cb,
					   totem);
}

static void
auto_resize_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	GObject *item;

	item = gtk_builder_get_object (totem->xml, "tpw_display_checkbutton");
	g_signal_handlers_disconnect_by_func (item,
			checkbutton1_toggled_cb, totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), g_settings_get_boolean (settings, "auto-resize"));

	g_signal_connect (item, "toggled",
			G_CALLBACK (checkbutton1_toggled_cb), totem);
}

static void
show_vfx_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	GObject *item;

	item = gtk_builder_get_object (totem->xml, "tpw_visuals_checkbutton");
	g_signal_handlers_disconnect_by_func (item,
			checkbutton2_toggled_cb, totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), g_settings_get_boolean (totem->settings, "show-vfx"));

	g_signal_connect (item, "toggled",
			G_CALLBACK (checkbutton2_toggled_cb), totem);
}

static void
disable_kbd_shortcuts_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	totem->disable_kbd_shortcuts = g_settings_get_boolean (totem->settings, "disable-keyboard-shortcuts");
}

static void
lock_screensaver_on_audio_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	GObject *item, *radio;
	gboolean value;

	item = gtk_builder_get_object (totem->xml, "tpw_audio_toggle_button");
	g_signal_handlers_disconnect_by_func (item,
					      audio_screensaver_button_toggled_cb, totem);

	value = g_settings_get_boolean (totem->settings, "lock-screensaver-on-audio");
	if (value != FALSE) {
		radio = gtk_builder_get_object (totem->xml, "tpw_audio_toggle_button");
	} else {
		radio = gtk_builder_get_object (totem->xml, "tpw_video_toggle_button");
	}
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);

	g_signal_connect (item, "toggled",
			  G_CALLBACK (audio_screensaver_button_toggled_cb), totem);
}

static void
autoload_subtitles_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	GObject *item;

	item = gtk_builder_get_object (totem->xml, "tpw_auto_subtitles_checkbutton");
	g_signal_handlers_disconnect_by_func (item,
			checkbutton3_toggled_cb, totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), g_settings_get_boolean (totem->settings, "autoload-subtitles"));

	g_signal_connect (item, "toggled",
			G_CALLBACK (checkbutton3_toggled_cb), totem);
}

static void
autoload_chapters_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	GObject *item;

	item = gtk_builder_get_object (totem->xml, "tpw_auto_chapters_checkbutton");
	g_signal_handlers_disconnect_by_func (item,
					      auto_chapters_toggled_cb, totem);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), g_settings_get_boolean (totem->settings, "autoload-chapters"));

	g_signal_connect (item, "toggled",
			  G_CALLBACK (auto_chapters_toggled_cb), totem);
}

void
connection_combobox_changed (GtkComboBox *combobox, Totem *totem)
{
	int i;

	i = gtk_combo_box_get_active (combobox);
	bacon_video_widget_set_connection_speed
		(BACON_VIDEO_WIDGET (totem->bvw), i);
}

void
visual_menu_changed (GtkComboBox *combobox, Totem *totem)
{
	GList *list;
	char *old_name, *name;
	int i;

	i = gtk_combo_box_get_active (combobox);
	list = bacon_video_widget_get_visualization_list (totem->bvw);
	name = g_list_nth_data (list, i);

	old_name = g_settings_get_string (totem->settings, "visual");

	if (old_name == NULL || strcmp (old_name, name) != 0)
	{
		g_settings_set_string (totem->settings, "visual", name);

		bacon_video_widget_set_visualization (totem->bvw, name);
	}

	g_free (old_name);
}

void
visual_quality_menu_changed (GtkComboBox *combobox, Totem *totem)
{
	int i;

	i = gtk_combo_box_get_active (combobox);
	g_settings_set_int (totem->settings, "visual-quality", i);
	bacon_video_widget_set_visualization_quality (totem->bvw, i);
}

void
brightness_changed (GtkRange *range, Totem *totem)
{
	gdouble i;

	i = gtk_range_get_value (range);
	bacon_video_widget_set_video_property (totem->bvw,
			BVW_VIDEO_BRIGHTNESS, (int) i);
}

void
contrast_changed (GtkRange *range, Totem *totem)
{
	gdouble i;

	i = gtk_range_get_value (range);
	bacon_video_widget_set_video_property (totem->bvw,
			BVW_VIDEO_CONTRAST, (int) i);
}

void
saturation_changed (GtkRange *range, Totem *totem)
{
	gdouble i;

	i = gtk_range_get_value (range);
	bacon_video_widget_set_video_property (totem->bvw,
			BVW_VIDEO_SATURATION, (int) i);
}

void
hue_changed (GtkRange *range, Totem *totem)
{
	gdouble i;

	i = gtk_range_get_value (range);
	bacon_video_widget_set_video_property (totem->bvw,
			BVW_VIDEO_HUE, (int) i);
}

void
tpw_color_reset_clicked_cb (GtkButton *button, Totem *totem)
{
	guint i;
	const char *scales[] = {
		"tpw_bright_scale",
		"tpw_contrast_scale",
		"tpw_saturation_scale",
		"tpw_hue_scale"
	};

	for (i = 0; i < G_N_ELEMENTS (scales); i++) {
		GtkRange *item;
		item = GTK_RANGE (gtk_builder_get_object (totem->xml, scales[i]));
		gtk_range_set_value (item, 65535/2);
	}
}

void
audio_out_menu_changed (GtkComboBox *combobox, Totem *totem)
{
	BvwAudioOutType audio_out;

	audio_out = gtk_combo_box_get_active (combobox);
	bacon_video_widget_set_audio_out_type (totem->bvw, audio_out);
}

void
font_set_cb (GtkFontButton * fb, Totem * totem)
{
	const gchar *font;

	font = gtk_font_button_get_font_name (fb);
	g_settings_set_string (totem->settings, "subtitle-font", font);
}

void
encoding_set_cb (GtkComboBox *cb, Totem *totem)
{
	const gchar *encoding;

	encoding = totem_subtitle_encoding_get_selected (cb);
	if (encoding)
		g_settings_set_string (totem->settings, "subtitle-encoding", encoding);
}

static void
font_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	gchar *font;
	GtkFontButton *item;

	item = GTK_FONT_BUTTON (gtk_builder_get_object (totem->xml, "font_sel_button"));
	font = g_settings_get_string (settings, "subtitle-font");
	gtk_font_button_set_font_name (item, font);
	bacon_video_widget_set_subtitle_font (totem->bvw, font);
	g_free (font);
}

static void
encoding_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	gchar *encoding;
	GtkComboBox *item;

	item = GTK_COMBO_BOX (gtk_builder_get_object (totem->xml, "subtitle_encoding_combo"));
	encoding = g_settings_get_string (settings, "subtitle-encoding");
	totem_subtitle_encoding_set (item, encoding);
	bacon_video_widget_set_subtitle_encoding (totem->bvw, encoding);
	g_free (encoding);
}

void
auto_chapters_toggled_cb (GtkToggleButton *togglebutton, Totem *totem)
{
	g_settings_set_boolean (totem->settings, "autoload-chapters", gtk_toggle_button_get_active (togglebutton));
}

void
totem_setup_preferences (Totem *totem)
{
	GtkWidget *menu, *content_area;
	gboolean show_visuals, auto_resize, is_local, no_deinterlace, lock_screensaver_on_audio, auto_chapters;
	int connection_speed;
	guint i, hidden;
	char *visual, *font, *encoding;
	GList *list, *l;
	BvwAudioOutType audio_out;
	GObject *item;

	static struct {
		const char *name;
		BvwVideoProperty prop;
		const char *label;
	} props[4] = {
		{ "tpw_contrast_scale", BVW_VIDEO_CONTRAST, "tpw_contrast_label" },
		{ "tpw_saturation_scale", BVW_VIDEO_SATURATION, "tpw_saturation_label" },
		{ "tpw_bright_scale", BVW_VIDEO_BRIGHTNESS, "tpw_brightness_label" },
		{ "tpw_hue_scale", BVW_VIDEO_HUE, "tpw_hue_label" }
	};

	g_return_if_fail (totem->settings != NULL);

	is_local = totem_display_is_local ();

	g_signal_connect (totem->settings, "changed::auto-resize", (GCallback) auto_resize_changed_cb, totem);

	/* Work-around builder dialogue not parenting properly for
	 * On top windows */
	item = gtk_builder_get_object (totem->xml, "tpw_notebook");
	totem->prefs = gtk_dialog_new_with_buttons (_("Preferences"),
			GTK_WINDOW (totem->win),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE,
			GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (totem->prefs), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (totem->prefs), 5);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (totem->prefs));
	gtk_box_set_spacing (GTK_BOX (content_area), 2);
	gtk_widget_reparent (GTK_WIDGET (item), content_area);
	gtk_widget_show_all (content_area);
	item = gtk_builder_get_object (totem->xml, "totem_preferences_window");
	gtk_widget_destroy (GTK_WIDGET (item));

	g_signal_connect (G_OBJECT (totem->prefs), "response",
			G_CALLBACK (gtk_widget_hide), NULL);
	g_signal_connect (G_OBJECT (totem->prefs), "delete-event",
			G_CALLBACK (gtk_widget_hide_on_delete), NULL);
        g_signal_connect (totem->prefs, "destroy",
                          G_CALLBACK (gtk_widget_destroyed), &totem->prefs);

	/* Remember position */
	totem->remember_position = g_settings_get_boolean (totem->settings, "remember-position");
	item = gtk_builder_get_object (totem->xml, "tpw_remember_position_checkbutton");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), totem->remember_position);
	g_signal_connect (totem->settings, "changed::remember-position", (GCallback) remember_position_changed_cb, totem);

	/* Auto-resize */
	auto_resize = g_settings_get_boolean (totem->settings, "auto-resize");
	item = gtk_builder_get_object (totem->xml, "tpw_display_checkbutton");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), auto_resize);
	bacon_video_widget_set_auto_resize
		(BACON_VIDEO_WIDGET (totem->bvw), auto_resize);

	/* Screensaver audio locking */
	lock_screensaver_on_audio = g_settings_get_boolean (totem->settings, "lock-screensaver-on-audio");
	if (lock_screensaver_on_audio != FALSE)
		item = gtk_builder_get_object (totem->xml, "tpw_audio_toggle_button");
	else
		item = gtk_builder_get_object (totem->xml, "tpw_video_toggle_button");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), TRUE);
	g_signal_connect (totem->settings, "changed::lock-screensaver-on-audio", (GCallback) lock_screensaver_on_audio_changed_cb, totem);

	/* Disable deinterlacing */
	item = gtk_builder_get_object (totem->xml, "tpw_no_deinterlace_checkbutton");
	no_deinterlace = g_settings_get_boolean (totem->settings, "disable-deinterlacing");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), no_deinterlace);
	bacon_video_widget_set_deinterlacing (totem->bvw, !no_deinterlace);
	g_signal_connect (totem->settings, "changed::disable-deinterlacing", (GCallback) no_deinterlace_changed_cb, totem);

	/* Connection Speed */
	connection_speed = bacon_video_widget_get_connection_speed (totem->bvw);
	item = gtk_builder_get_object (totem->xml, "tpw_speed_combobox");
	gtk_combo_box_set_active (GTK_COMBO_BOX (item), connection_speed);

	/* Enable visuals */
	item = gtk_builder_get_object (totem->xml, "tpw_visuals_checkbutton");
	show_visuals = g_settings_get_boolean (totem->settings, "show-vfx");
	if (is_local == FALSE && show_visuals != FALSE)
		show_visuals = ask_show_visuals (totem);

	g_signal_handlers_disconnect_by_func (item, checkbutton2_toggled_cb, totem);
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (item), show_visuals);
	totem_prefs_set_show_visuals (totem, show_visuals);
	g_signal_connect (item, "toggled", G_CALLBACK (checkbutton2_toggled_cb), totem);

	g_signal_connect (totem->settings, "changed::show-vfx", (GCallback) show_vfx_changed_cb, totem);

	/* Auto-load subtitles */
	item = gtk_builder_get_object (totem->xml, "tpw_auto_subtitles_checkbutton");
	totem->autoload_subs = g_settings_get_boolean (totem->settings, "autoload-subtitles");

	g_signal_handlers_disconnect_by_func (item, checkbutton3_toggled_cb, totem);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), totem->autoload_subs);
	g_signal_connect (item, "toggled", G_CALLBACK (checkbutton3_toggled_cb), totem);

	g_signal_connect (totem->settings, "changed::autoload-subtitles", (GCallback) autoload_subtitles_changed_cb, totem);

	/* Auto-load external chapters */
	item = gtk_builder_get_object (totem->xml, "tpw_auto_chapters_checkbutton");
	auto_chapters = g_settings_get_boolean (totem->settings, "autoload-chapters");

	g_signal_handlers_disconnect_by_func (item, auto_chapters_toggled_cb, totem);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), auto_chapters);
	g_signal_connect (item, "toggled", G_CALLBACK (auto_chapters_toggled_cb), totem);

	g_signal_connect (totem->settings, "changed::autoload-chapters", (GCallback) autoload_chapters_changed_cb, totem);

	/* Visuals list */
	list = bacon_video_widget_get_visualization_list (totem->bvw);
	menu = gtk_menu_new ();
	gtk_widget_show (menu);

	visual = g_settings_get_string (totem->settings, "visual");
	if (visual == NULL || strcmp (visual, "") == 0) {
		g_free (visual);
		visual = g_strdup ("goom");
	}

	item = gtk_builder_get_object (totem->xml, "tpw_visuals_type_combobox");

	i = 0;
	for (l = list; l != NULL; l = l->next) {
		const char *name = l->data;

		gtk_combo_box_append_text (GTK_COMBO_BOX (item), name);

		if (strcmp (name, visual) == 0)
			gtk_combo_box_set_active (GTK_COMBO_BOX (item), i);

		i++;
	}
	g_free (visual);

	/* Visualisation quality */
	i = g_settings_get_int (totem->settings, "visual-quality");
	bacon_video_widget_set_visualization_quality (totem->bvw, i);
	item = gtk_builder_get_object (totem->xml, "tpw_visuals_size_combobox");
	gtk_combo_box_set_active (GTK_COMBO_BOX (item), i);

	/* Brightness and all */
	hidden = 0;
	for (i = 0; i < G_N_ELEMENTS (props); i++) {
		int prop_value;
		item = gtk_builder_get_object (totem->xml, props[i].name);
		prop_value = bacon_video_widget_get_video_property (totem->bvw,
							       props[i].prop);
		if (prop_value >= 0)
			gtk_range_set_value (GTK_RANGE (item), (gdouble) prop_value);
		else {
			gtk_range_set_value (GTK_RANGE (item), (gdouble) 65535/2);
			gtk_widget_hide (GTK_WIDGET (item));
			item = gtk_builder_get_object (totem->xml, props[i].label);
			gtk_widget_hide (GTK_WIDGET (item));
			hidden++;
		}
	}

	if (hidden == G_N_ELEMENTS (props)) {
		item = gtk_builder_get_object (totem->xml, "tpw_bright_contr_vbox");
		gtk_widget_hide (GTK_WIDGET (item));
	}

	/* Sound output type */
	item = gtk_builder_get_object (totem->xml, "tpw_sound_output_combobox");
	audio_out = bacon_video_widget_get_audio_out_type (totem->bvw);
	gtk_combo_box_set_active (GTK_COMBO_BOX (item), audio_out);

	/* Subtitle font selection */
	item = gtk_builder_get_object (totem->xml, "font_sel_button");
	gtk_font_button_set_title (GTK_FONT_BUTTON (item),
				   _("Select Subtitle Font"));
	font = g_settings_get_string (totem->settings, "subtitle-font");
	if (font && strcmp (font, "") != 0) {
		gtk_font_button_set_font_name (GTK_FONT_BUTTON (item), font);
		bacon_video_widget_set_subtitle_font (totem->bvw, font);
	}
	g_free (font);
	g_signal_connect (totem->settings, "changed::subtitle-font", (GCallback) font_changed_cb, totem);

	/* Subtitle encoding selection */
	item = gtk_builder_get_object (totem->xml, "subtitle_encoding_combo");
	totem_subtitle_encoding_init (GTK_COMBO_BOX (item));
	encoding = g_settings_get_string (totem->settings, "subtitle-encoding");
	/* Make sure the default is UTF-8 */
	if (encoding == NULL || *encoding == '\0') {
		g_free (encoding);
		encoding = g_strdup ("UTF-8");
	}
	totem_subtitle_encoding_set (GTK_COMBO_BOX(item), encoding);
	if (encoding && strcasecmp (encoding, "") != 0) {
		bacon_video_widget_set_subtitle_encoding (totem->bvw, encoding);
	}
	g_free (encoding);
	g_signal_connect (totem->settings, "changed::subtitle-encoding", (GCallback) encoding_changed_cb, totem);

	/* Disable keyboard shortcuts */
	totem->disable_kbd_shortcuts = g_settings_get_boolean (totem->settings, "disable-keyboard-shortcuts");
	g_signal_connect (totem->settings, "changed::disable-keyboard-shortcuts", (GCallback) disable_kbd_shortcuts_changed_cb, totem);
}

void
totem_preferences_visuals_setup (Totem *totem)
{
	char *visual;

	visual = g_settings_get_string (totem->settings, "visual");
	if (visual == NULL || strcmp (visual, "") == 0) {
		g_free (visual);
		visual = g_strdup ("goom");
	}

	bacon_video_widget_set_visualization (totem->bvw, visual);
	g_free (visual);
}
