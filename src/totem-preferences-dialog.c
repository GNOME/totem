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
#include <libpeas-gtk/peas-gtk-plugin-manager.h>

#include "bacon-video-widget-enums.h"
#include "totem.h"
#include "totem-private.h"
#include "totem-preferences-dialog.h"
#include "totem-interface.h"
#include "totem-subtitle-encoding.h"
#include "totem-plugins-engine.h"

struct _TotemPreferencesDialog {
	GtkDialog parent_instance;

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
	GtkButton *tpw_plugins_button;
	GtkComboBox *tpw_sound_output_combobox;
	GtkComboBox *subtitle_encoding_combo;
};

G_DEFINE_TYPE (TotemPreferencesDialog, totem_preferences_dialog, GTK_TYPE_DIALOG)

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

static gboolean
totem_plugins_window_delete_cb (GtkWidget *window,
				   GdkEventAny *event,
				   gpointer data)
{
	gtk_widget_hide (window);

	return TRUE;
}

static void
totem_plugins_response_cb (GtkDialog *dialog,
			      int response_id,
			      gpointer data)
{
	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
plugin_button_clicked_cb (GtkButton *button,
			  Totem     *totem)
{
	if (totem->plugins == NULL) {
		GtkWidget *manager;

		totem->plugins = gtk_dialog_new_with_buttons (_("Configure Plugins"),
							      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
							      GTK_DIALOG_DESTROY_WITH_PARENT,
							      _("_Close"),
							      GTK_RESPONSE_CLOSE,
							      NULL);
		gtk_window_set_modal (GTK_WINDOW (totem->plugins), TRUE);
		gtk_container_set_border_width (GTK_CONTAINER (totem->plugins), 5);
		gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (totem->plugins))), 2);

		g_signal_connect_object (G_OBJECT (totem->plugins),
					 "delete_event",
					 G_CALLBACK (totem_plugins_window_delete_cb),
					 NULL, 0);
		g_signal_connect_object (G_OBJECT (totem->plugins),
					 "response",
					 G_CALLBACK (totem_plugins_response_cb),
					 NULL, 0);

		manager = peas_gtk_plugin_manager_new (NULL);
		gtk_widget_show_all (GTK_WIDGET (manager));
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (totem->plugins))),
				    manager, TRUE, TRUE, 0);
		gtk_window_set_default_size (GTK_WINDOW (totem->plugins), 600, 400);
	}

	gtk_window_present (GTK_WINDOW (totem->plugins));
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
	TotemObject *totem;
	GtkWidget *bvw;
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

	/* Auto-load subtitles */
	g_settings_bind (totem->settings, "autoload-subtitles",
			 prefs->tpw_auto_subtitles_checkbutton, "active", G_SETTINGS_BIND_DEFAULT);

	/* Plugins button */
	g_signal_connect (prefs->tpw_plugins_button, "clicked",
			  G_CALLBACK (plugin_button_clicked_cb), totem);

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

	g_object_unref (bvw);
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
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_plugins_button);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_saturation_adjustment);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_saturation_scale);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, tpw_sound_output_combobox);
	gtk_widget_class_bind_template_child (widget_class, TotemPreferencesDialog, subtitle_encoding_combo);

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
			      "use-header-bar", 1,
			      NULL);
}
