/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001,2002,2003 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "bacon-video-widget-enums.h"
#include "totem.h"
#include "totem-private.h"
#include "totem-preferences-dialog.h"
#include "totem-interface.h"
#include "totem-subtitle-encoding.h"
#include "totem-plugins-engine.h"
#include "totem-preferences-plugin-row.h"

struct _TotemPreferencesDialog {
	HdyPreferencesWindow parent_instance;

	Totem *totem;

	GtkRange *tpw_bright_scale;
	GtkAdjustment *tpw_bright_adjustment;
	GtkRange *tpw_contrast_scale;
	GtkAdjustment *tpw_contrast_adjustment;
	GtkRange *tpw_hue_scale;
	GtkAdjustment *tpw_hue_adjustment;
	GtkRange *tpw_saturation_scale;
	GtkAdjustment *tpw_saturation_adjustment;

	GtkFontChooser *font_sel_button;
	GtkCheckButton *tpw_auto_subtitles_checkbutton;
	GtkWidget *tpw_bright_contr_vbox;
	GtkCheckButton *tpw_no_deinterlace_checkbutton;
	GtkCheckButton *tpw_no_hardware_acceleration;
	GtkComboBox *tpw_sound_output_combobox;
	GtkComboBox *subtitle_encoding_combo;

	GtkListBox *tpw_plugins_list;
};

G_DEFINE_TYPE (TotemPreferencesDialog, totem_preferences_dialog, HDY_TYPE_PREFERENCES_WINDOW)

static void
disable_kbd_shortcuts_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	totem->disable_kbd_shortcuts = g_settings_get_boolean (totem->settings, "disable-keyboard-shortcuts");
}

static void
tpw_color_reset_clicked_cb (GtkButton *button, TotemPreferencesDialog *prefs)
{
	gtk_range_set_value (prefs->tpw_bright_scale, 65535/2);
	gtk_range_set_value (prefs->tpw_contrast_scale, 65535/2);
	gtk_range_set_value (prefs->tpw_hue_scale, 65535/2);
	gtk_range_set_value (prefs->tpw_saturation_scale, 65535/2);
}

static void
font_set_cb (GtkFontButton * fb, TotemPreferencesDialog * prefs)
{
	Totem *totem = prefs->totem;
	gchar *font;

	font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (fb));
	g_settings_set_string (totem->settings, "subtitle-font", font);
	g_free (font);
}

static void
encoding_set_cb (GtkComboBox *cb, TotemPreferencesDialog *prefs)
{
	Totem *totem = prefs->totem;
	const gchar *encoding;

	encoding = totem_subtitle_encoding_get_selected (cb);
	if (encoding)
		g_settings_set_string (totem->settings, "subtitle-encoding", encoding);
}

static void
font_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	TotemPreferencesDialog *prefs = TOTEM_PREFERENCES_DIALOG (totem->prefs);
	gchar *font;

	font = g_settings_get_string (settings, "subtitle-font");
	gtk_font_chooser_set_font (prefs->font_sel_button, font);
	bacon_video_widget_set_subtitle_font (totem->bvw, font);
	g_free (font);
}

static void
encoding_changed_cb (GSettings *settings, const gchar *key, TotemObject *totem)
{
	TotemPreferencesDialog *prefs = TOTEM_PREFERENCES_DIALOG (totem->prefs);
	gchar *encoding;

	encoding = g_settings_get_string (settings, "subtitle-encoding");
	totem_subtitle_encoding_set (prefs->subtitle_encoding_combo, encoding);
	bacon_video_widget_set_subtitle_encoding (totem->bvw, encoding);
	g_free (encoding);
}

static gboolean
int_enum_get_mapping (GValue *value, GVariant *variant, GEnumClass *enum_class)
{
	GEnumValue *enum_value;
	const gchar *nick;

	g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), FALSE);

	nick = g_variant_get_string (variant, NULL);
	enum_value = g_enum_get_value_by_nick (enum_class, nick);

	if (enum_value == NULL)
		return FALSE;

	g_value_set_int (value, enum_value->value);

	return TRUE;
}

static GVariant *
int_enum_set_mapping (const GValue *value, const GVariantType *expected_type, GEnumClass *enum_class)
{
	GEnumValue *enum_value;

	g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), NULL);

	enum_value = g_enum_get_value (enum_class, g_value_get_int (value));

	if (enum_value == NULL)
		return NULL;

	return g_variant_new_string (enum_value->value_nick);
}

static int
totems_plugins_sort_cb (GtkListBoxRow *row_a,
                        GtkListBoxRow *row_b,
                        gpointer       data)
{
	const char *name_a = hdy_preferences_row_get_title (HDY_PREFERENCES_ROW (row_a));
	const char *name_b = hdy_preferences_row_get_title (HDY_PREFERENCES_ROW (row_b));

	return g_utf8_collate (name_a, name_b);
}

enum {
	PROP_0,
	PROP_TOTEM,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
totem_preferences_dialog_get_property (GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
	TotemPreferencesDialog *self = TOTEM_PREFERENCES_DIALOG (object);

	switch (prop_id)
	  {
	  case PROP_TOTEM:
		  g_value_set_object (value, self->totem);
		  break;
	  default:
		  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	  }
}

static void
totem_preferences_dialog_set_property (GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
	TotemPreferencesDialog *self = TOTEM_PREFERENCES_DIALOG (object);

	switch (prop_id)
	  {
	  case PROP_TOTEM:
		  g_assert (self->totem == NULL);
		  self->totem = g_value_get_object (value);
		  break;
	  default:
		  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	  }
}

static void
totem_preferences_dialog_constructed (GObject *object)
{
	TotemPreferencesDialog *prefs = TOTEM_PREFERENCES_DIALOG (object);
	g_autoptr(TotemPluginsEngine) engine = NULL;
	g_autoptr(GtkWidget) bvw = NULL;
	PeasEngine *peas_engine = NULL;
	TotemObject *totem;
	guint i, hidden;
	char *font, *encoding;

	G_OBJECT_CLASS (totem_preferences_dialog_parent_class)->constructed (object);

	g_assert (prefs->totem != NULL);
	totem = prefs->totem;

	bvw = totem_object_get_video_widget (totem);

        g_signal_connect (prefs, "destroy", G_CALLBACK (gtk_widget_destroyed), &totem->prefs);

	/* Disable deinterlacing */
	g_settings_bind (totem->settings, "disable-deinterlacing",
			 prefs->tpw_no_deinterlace_checkbutton, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (totem->settings, "disable-deinterlacing", bvw, "deinterlacing",
	                 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY | G_SETTINGS_BIND_INVERT_BOOLEAN);

	/* Disable hardware acceleration */
	g_settings_bind (totem->settings, "force-software-decoders",
			 prefs->tpw_no_hardware_acceleration, "active", G_SETTINGS_BIND_DEFAULT);

	/* Auto-load subtitles */
	g_settings_bind (totem->settings, "autoload-subtitles",
			 prefs->tpw_auto_subtitles_checkbutton, "active", G_SETTINGS_BIND_DEFAULT);

	/* Brightness and all */
	struct {
		GtkRange *range;
		BvwVideoProperty prop;
		const gchar *key;
		GtkAdjustment *adjustment;
	} props[4] = {
		{ prefs->tpw_contrast_scale, BVW_VIDEO_CONTRAST, "contrast", prefs->tpw_contrast_adjustment },
		{ prefs->tpw_saturation_scale, BVW_VIDEO_SATURATION, "saturation", prefs->tpw_saturation_adjustment },
		{ prefs->tpw_bright_scale, BVW_VIDEO_BRIGHTNESS, "brightness", prefs->tpw_bright_adjustment },
		{ prefs->tpw_hue_scale, BVW_VIDEO_HUE, "hue", prefs->tpw_hue_adjustment }
	};

	hidden = 0;
	for (i = 0; i < G_N_ELEMENTS (props); i++) {
		int prop_value;

		g_settings_bind (totem->settings, props[i].key, props[i].adjustment, "value", G_SETTINGS_BIND_DEFAULT);
		g_settings_bind (totem->settings, props[i].key, bvw, props[i].key, G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

		prop_value = bacon_video_widget_get_video_property (totem->bvw, props[i].prop);
		if (prop_value < 0) {
			/* The property's unsupported, so hide the widget and its label */
			gtk_range_set_value (props[i].range, (gdouble) 65535/2);
			gtk_widget_hide (GTK_WIDGET (props[i].range));
			hidden++;
		}
	}

	/* If all the properties have been hidden, hide their section box */
	gtk_widget_set_visible (prefs->tpw_bright_contr_vbox, hidden < G_N_ELEMENTS (props));

	/* Sound output type */
	g_settings_bind (totem->settings, "audio-output-type", bvw, "audio-output-type",
	                 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);
	g_settings_bind_with_mapping (totem->settings, "audio-output-type",
				      prefs->tpw_sound_output_combobox, "active", G_SETTINGS_BIND_DEFAULT,
	                              (GSettingsBindGetMapping) int_enum_get_mapping, (GSettingsBindSetMapping) int_enum_set_mapping,
	                              g_type_class_ref (BVW_TYPE_AUDIO_OUTPUT_TYPE), (GDestroyNotify) g_type_class_unref);

	/* Subtitle font selection */
	font = g_settings_get_string (totem->settings, "subtitle-font");
	if (*font != '\0') {
		gtk_font_chooser_set_font (prefs->font_sel_button, font);
		bacon_video_widget_set_subtitle_font (totem->bvw, font);
	}
	g_free (font);
	g_signal_connect (totem->settings, "changed::subtitle-font", (GCallback) font_changed_cb, totem);

	/* Subtitle encoding selection */
	totem_subtitle_encoding_init (prefs->subtitle_encoding_combo);
	encoding = g_settings_get_string (totem->settings, "subtitle-encoding");
	/* Make sure the default is UTF-8 */
	if (*encoding == '\0') {
		g_free (encoding);
		encoding = g_strdup ("UTF-8");
	}
	totem_subtitle_encoding_set (prefs->subtitle_encoding_combo, encoding);
	if (encoding && strcasecmp (encoding, "") != 0) {
		bacon_video_widget_set_subtitle_encoding (totem->bvw, encoding);
	}
	g_free (encoding);
	g_signal_connect (totem->settings, "changed::subtitle-encoding", (GCallback) encoding_changed_cb, totem);

	/* Disable keyboard shortcuts */
	totem->disable_kbd_shortcuts = g_settings_get_boolean (totem->settings, "disable-keyboard-shortcuts");
	g_signal_connect (totem->settings, "changed::disable-keyboard-shortcuts", (GCallback) disable_kbd_shortcuts_changed_cb, totem);

	/* Plugins */
	gtk_list_box_set_sort_func (prefs->tpw_plugins_list, totems_plugins_sort_cb, NULL, NULL);

	engine = totem_plugins_engine_get_default (totem);
	peas_engine = totem_plugins_engine_get_engine (engine);

	for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (peas_engine)); i++) {
		g_autoptr(GError) error = NULL;
		g_autoptr(PeasPluginInfo) plugin_info = NULL;
		const char *plugin_name;
		GtkWidget *plugin_row;

		plugin_info = PEAS_PLUGIN_INFO (g_list_model_get_item (G_LIST_MODEL (peas_engine), i));
		plugin_name = peas_plugin_info_get_name (plugin_info);

		if (!peas_plugin_info_is_available (plugin_info, &error)) {
			g_warning ("The plugin %s is not available : %s", plugin_name, error ? error->message : "no message error");
			continue;
		}

		if (peas_plugin_info_is_hidden (plugin_info) ||
		    peas_plugin_info_is_builtin (plugin_info))
			continue;

		plugin_row = totem_preferences_plugin_row_new (plugin_info);

		hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (plugin_row), plugin_name);
		hdy_expander_row_set_subtitle (HDY_EXPANDER_ROW (plugin_row), peas_plugin_info_get_description (plugin_info));

		gtk_container_add (GTK_CONTAINER (prefs->tpw_plugins_list), plugin_row);
	}
}

static void
totem_preferences_dialog_class_init (TotemPreferencesDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = totem_preferences_dialog_get_property;
	object_class->set_property = totem_preferences_dialog_set_property;
	object_class->constructed = totem_preferences_dialog_constructed;

	properties[PROP_TOTEM] = g_param_spec_object ("totem", "Totem object", "Totem object",
						      TOTEM_TYPE_OBJECT,
						      G_PARAM_READWRITE |
						      G_PARAM_CONSTRUCT_ONLY |
						      G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/totem/ui/totem-preferences-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, font_sel_button);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_auto_subtitles_checkbutton);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_bright_contr_vbox);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_bright_adjustment);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_bright_scale);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_contrast_adjustment);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_contrast_scale);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_hue_adjustment);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_hue_scale);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_no_deinterlace_checkbutton);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_no_hardware_acceleration);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_saturation_adjustment);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_saturation_scale);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_sound_output_combobox);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, subtitle_encoding_combo);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_plugins_list);

	gtk_widget_class_bind_template_callback (widget_class, encoding_set_cb);
	gtk_widget_class_bind_template_callback (widget_class, font_set_cb);
	gtk_widget_class_bind_template_callback (widget_class, tpw_color_reset_clicked_cb);
}

static void
totem_preferences_dialog_init (TotemPreferencesDialog *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
totem_preferences_dialog_new (Totem *totem)
{
	return  g_object_new (TOTEM_TYPE_PREFERENCES_DIALOG,
			      "totem", totem,
			      NULL);
}
