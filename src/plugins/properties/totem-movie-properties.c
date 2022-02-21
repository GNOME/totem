/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
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
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * See license_change file for details.
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>
#include <bacon-video-widget-properties.h>

#include "totem-plugin.h"
#include "totem.h"
#include "bacon-video-widget.h"

#define TOTEM_TYPE_MOVIE_PROPERTIES_PLUGIN		(totem_movie_properties_plugin_get_type ())
#define TOTEM_MOVIE_PROPERTIES_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_MOVIE_PROPERTIES_PLUGIN, TotemMoviePropertiesPlugin))

typedef struct {
	PeasExtensionBase parent;

	GtkWidget     *props;
	GtkWidget     *dialog;
	guint          handler_id_stream_length;
	guint          handler_id_main_page;
	GSimpleAction *props_action;
} TotemMoviePropertiesPlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_MOVIE_PROPERTIES_PLUGIN,
		      TotemMoviePropertiesPlugin,
		      totem_movie_properties_plugin)

/* used in update_properties_from_bvw() */
#define UPDATE_FROM_STRING(type, name) \
	do { \
		const char *temp; \
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw), \
						 type, &value); \
		if ((temp = g_value_get_string (&value)) != NULL) { \
			bacon_video_widget_properties_set_label (props, name, \
								 temp); \
		} \
		g_value_unset (&value); \
	} while (0)

#define UPDATE_FROM_INT(type, name, format, empty) \
	do { \
		char *temp; \
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw), \
						 type, &value); \
		if (g_value_get_int (&value) != 0) \
			temp = g_strdup_printf (gettext (format), \
					g_value_get_int (&value)); \
		else \
			temp = g_strdup (empty); \
		bacon_video_widget_properties_set_label (props, name, temp); \
		g_free (temp); \
		g_value_unset (&value); \
	} while (0)

#define UPDATE_FROM_INT2(type1, type2, name, format) \
	do { \
		int x, y; \
		char *temp; \
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw), \
						 type1, &value); \
		x = g_value_get_int (&value); \
		g_value_unset (&value); \
		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw), \
						 type2, &value); \
		y = g_value_get_int (&value); \
		g_value_unset (&value); \
		temp = g_strdup_printf (gettext (format), x, y); \
		bacon_video_widget_properties_set_label (props, name, temp); \
		g_free (temp); \
	} while (0)

static void
update_properties_from_bvw (BaconVideoWidgetProperties *props,
				      GtkWidget *widget)
{
	GValue value = { 0, };
	gboolean has_video, has_audio;
	BaconVideoWidget *bvw;

	g_return_if_fail (BACON_IS_VIDEO_WIDGET_PROPERTIES (props));
	g_return_if_fail (BACON_IS_VIDEO_WIDGET (widget));

	bvw = BACON_VIDEO_WIDGET (widget);

	/* General */
	UPDATE_FROM_STRING (BVW_INFO_TITLE, "title");
	UPDATE_FROM_STRING (BVW_INFO_ARTIST, "artist");
	UPDATE_FROM_STRING (BVW_INFO_ALBUM, "album");
	UPDATE_FROM_STRING (BVW_INFO_YEAR, "year");
	UPDATE_FROM_STRING (BVW_INFO_COMMENT, "comment");
	UPDATE_FROM_STRING (BVW_INFO_CONTAINER, "container");

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
					 BVW_INFO_DURATION, &value);
	bacon_video_widget_properties_set_duration (props,
						    g_value_get_int (&value) * 1000);
	g_value_unset (&value);

	/* Types */
	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
					 BVW_INFO_HAS_VIDEO, &value);
	has_video = g_value_get_boolean (&value);
	g_value_unset (&value);

	bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw),
					 BVW_INFO_HAS_AUDIO, &value);
	has_audio = g_value_get_boolean (&value);
	g_value_unset (&value);

	bacon_video_widget_properties_set_has_type (props, has_video, has_audio);

	/* Video */
	if (has_video != FALSE)
	{
		UPDATE_FROM_INT2 (BVW_INFO_DIMENSION_X, BVW_INFO_DIMENSION_Y,
				  "dimensions", N_("%d × %d"));
		UPDATE_FROM_STRING (BVW_INFO_VIDEO_CODEC, "vcodec");
		UPDATE_FROM_INT (BVW_INFO_VIDEO_BITRATE, "video_bitrate",
				 N_("%d kbps"), C_("Stream bit rate", "N/A"));

		bacon_video_widget_get_metadata (BACON_VIDEO_WIDGET (bvw), BVW_INFO_FPS, &value);
		bacon_video_widget_properties_set_framerate (props, g_value_get_float (&value));
		g_value_unset (&value);
	}

	/* Audio */
	if (has_audio != FALSE)
	{
		UPDATE_FROM_INT (BVW_INFO_AUDIO_BITRATE, "audio_bitrate",
				 N_("%d kbps"), C_("Stream bit rate", "N/A"));
		UPDATE_FROM_STRING (BVW_INFO_AUDIO_CODEC, "acodec");
		UPDATE_FROM_INT (BVW_INFO_AUDIO_SAMPLE_RATE, "samplerate",
				N_("%d Hz"), C_("Sample rate", "N/A"));
		UPDATE_FROM_STRING (BVW_INFO_AUDIO_CHANNELS, "channels");
	}

#undef UPDATE_FROM_STRING
#undef UPDATE_FROM_INT
#undef UPDATE_FROM_INT2
}

static void
main_page_notify_cb (TotemObject                *totem,
		     GParamSpec                 *arg1,
		     TotemMoviePropertiesPlugin *pi)
{
	char *main_page;

	g_object_get (G_OBJECT (totem), "main-page", &main_page, NULL);
	if (g_strcmp0 (main_page, "player") == 0)
		gtk_widget_hide (pi->dialog);
	g_free (main_page);
}

static void
stream_length_notify_cb (TotemObject *totem,
			 GParamSpec *arg1,
			 TotemMoviePropertiesPlugin *plugin)
{
	gint64 stream_length;

	g_object_get (G_OBJECT (totem),
		      "stream-length", &stream_length,
		      NULL);

	bacon_video_widget_properties_set_duration
		(BACON_VIDEO_WIDGET_PROPERTIES (plugin->props),
		 stream_length);
}

static void
totem_movie_properties_plugin_file_opened (TotemObject *totem,
					   const char *mrl,
					   TotemMoviePropertiesPlugin *plugin)
{
	GtkWidget *bvw;

	bvw = totem_object_get_video_widget (totem);
	update_properties_from_bvw
		(BACON_VIDEO_WIDGET_PROPERTIES (plugin->props), bvw);
	g_object_unref (bvw);
	gtk_widget_set_sensitive (plugin->props, TRUE);
}

static void
totem_movie_properties_plugin_file_closed (TotemObject *totem,
					   TotemMoviePropertiesPlugin *plugin)
{
        /* Reset the properties and wait for the signal*/
        bacon_video_widget_properties_reset
		(BACON_VIDEO_WIDGET_PROPERTIES (plugin->props));
	gtk_widget_set_sensitive (plugin->props, FALSE);
}

static void
totem_movie_properties_plugin_metadata_updated (TotemObject *totem,
						const char *artist, 
						const char *title, 
						const char *album,
						guint track_num,
						TotemMoviePropertiesPlugin *plugin)
{
	GtkWidget *bvw;

	bvw = totem_object_get_video_widget (totem);
	update_properties_from_bvw
		(BACON_VIDEO_WIDGET_PROPERTIES (plugin->props), bvw);
	g_object_unref (bvw);
}

static void
properties_action_cb (GSimpleAction              *simple,
		      GVariant                   *parameter,
		      TotemMoviePropertiesPlugin *pi)
{
	TotemObject *totem;
	char *main_page;

	totem = g_object_get_data (G_OBJECT (pi), "object");
	g_object_get (G_OBJECT (totem), "main-page", &main_page, NULL);
	if (g_strcmp0 (main_page, "player") == 0)
		gtk_widget_show (pi->dialog);
	g_free (main_page);
}

static void
impl_activate (PeasActivatable *plugin)
{
	TotemMoviePropertiesPlugin *pi;
	TotemObject *totem;
	GtkWindow *parent;
	GMenu *menu;
	GMenuItem *item;
	const char * const accels[] = { "<Primary>p", "<Primary>i", "View", NULL };

	pi = TOTEM_MOVIE_PROPERTIES_PLUGIN (plugin);
	totem = g_object_get_data (G_OBJECT (plugin), "object");

	pi->props = bacon_video_widget_properties_new ();
	gtk_widget_show (pi->props);
	gtk_widget_set_sensitive (pi->props, FALSE);

	parent = totem_object_get_main_window (totem);
	pi->dialog = gtk_dialog_new_with_buttons (_("Properties"),
							parent,
							GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
							NULL,
							GTK_RESPONSE_CLOSE,
							NULL);
	g_object_unref (parent);
	g_signal_connect (pi->dialog, "delete-event",
			  G_CALLBACK (gtk_widget_hide_on_delete), NULL);
	g_signal_connect (pi->dialog, "response",
			  G_CALLBACK (gtk_widget_hide_on_delete), NULL);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (pi->dialog))),
			   pi->props);

	/* Properties action */
	pi->props_action = g_simple_action_new ("properties", NULL);
	g_signal_connect (G_OBJECT (pi->props_action), "activate",
			  G_CALLBACK (properties_action_cb), pi);
	g_action_map_add_action (G_ACTION_MAP (totem), G_ACTION (pi->props_action));
	gtk_application_set_accels_for_action (GTK_APPLICATION (totem),
					       "app.properties",
					       accels);

	/* Install the menu */
	menu = totem_object_get_menu_section (totem, "properties-placeholder");
	item = g_menu_item_new (_("_Properties"), "app.properties");
	g_menu_item_set_attribute (item, "accel", "s", "<Primary>p");
	g_menu_append_item (G_MENU (menu), item);
	g_object_unref (item);

	g_signal_connect (G_OBJECT (totem),
			  "file-opened",
			  G_CALLBACK (totem_movie_properties_plugin_file_opened),
			  plugin);
	g_signal_connect (G_OBJECT (totem),
			  "file-closed",
			  G_CALLBACK (totem_movie_properties_plugin_file_closed),
			  plugin);
	g_signal_connect (G_OBJECT (totem),
			  "metadata-updated",
			  G_CALLBACK (totem_movie_properties_plugin_metadata_updated),
			  plugin);
	pi->handler_id_stream_length = g_signal_connect (G_OBJECT (totem),
							       "notify::stream-length",
							       G_CALLBACK (stream_length_notify_cb),
							       plugin);
	pi->handler_id_main_page = g_signal_connect (G_OBJECT (totem),
							   "notify::main-page",
							   G_CALLBACK (main_page_notify_cb),
							   plugin);
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemMoviePropertiesPlugin *pi;
	TotemObject *totem;
	const char * const accels[] = { NULL };

	pi = TOTEM_MOVIE_PROPERTIES_PLUGIN (plugin);
	totem = g_object_get_data (G_OBJECT (plugin), "object");

	g_signal_handler_disconnect (G_OBJECT (totem), pi->handler_id_stream_length);
	g_signal_handler_disconnect (G_OBJECT (totem), pi->handler_id_main_page);
	g_signal_handlers_disconnect_by_func (G_OBJECT (totem),
					      totem_movie_properties_plugin_metadata_updated,
					      plugin);
	g_signal_handlers_disconnect_by_func (G_OBJECT (totem),
					      totem_movie_properties_plugin_file_opened,
					      plugin);
	g_signal_handlers_disconnect_by_func (G_OBJECT (totem),
					      totem_movie_properties_plugin_file_closed,
					      plugin);
	pi->handler_id_stream_length = 0;
	pi->handler_id_main_page = 0;

	gtk_application_set_accels_for_action (GTK_APPLICATION (totem),
					       "app.properties",
					       accels);
	totem_object_empty_menu_section (totem, "properties-placeholder");
}
