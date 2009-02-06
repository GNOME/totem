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
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

/**
 * SECTION:totem-object
 * @short_description: main object
 * @stability: Unstable
 * @include: totem-object.h
 *
 * #TotemObject is the core object of Totem; a singleton which controls all Totem's main functions.
 **/

#include "config.h"

#include <glib-object.h>
#include <gtk/gtk.h>

#include "totem.h"
#include "totemobject-marshal.h"
#include "totem-private.h"
#include "totem-plugins-engine.h"
#include "ev-sidebar.h"
#include "totem-playlist.h"
#include "bacon-video-widget.h"

enum {
	PROP_0,
	PROP_FULLSCREEN,
	PROP_PLAYING,
	PROP_STREAM_LENGTH,
	PROP_SEEKABLE,
	PROP_CURRENT_TIME,
	PROP_CURRENT_MRL
};

enum {
	FILE_OPENED,
	FILE_CLOSED,
	METADATA_UPDATED,
	LAST_SIGNAL
};

static void totem_object_set_property		(GObject *object,
						 guint property_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void totem_object_get_property		(GObject *object,
						 guint property_id,
						 GValue *value,
						 GParamSpec *pspec);
static void totem_object_finalize (GObject *totem);

static int totem_table_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(TotemObject, totem_object, G_TYPE_OBJECT)

static void
totem_object_class_init (TotemObjectClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	object_class->set_property = totem_object_set_property;
	object_class->get_property = totem_object_get_property;
	object_class->finalize = totem_object_finalize;

	/**
	 * TotemObject:fullscreen:
	 *
	 * If %TRUE, Totem is in fullscreen mode.
	 **/
	g_object_class_install_property (object_class, PROP_FULLSCREEN,
					 g_param_spec_boolean ("fullscreen", NULL, NULL,
							       FALSE, G_PARAM_READABLE));

	/**
	 * TotemObject:playing:
	 *
	 * If %TRUE, Totem is playing an audio or video file.
	 **/
	g_object_class_install_property (object_class, PROP_PLAYING,
					 g_param_spec_boolean ("playing", NULL, NULL,
							       FALSE, G_PARAM_READABLE));

	/**
	 * TotemObject:stream-length:
	 *
	 * The length of the current stream, in milliseconds.
	 **/
	g_object_class_install_property (object_class, PROP_STREAM_LENGTH,
					 g_param_spec_int64 ("stream-length", NULL, NULL,
							     G_MININT64, G_MAXINT64, 0,
							     G_PARAM_READABLE));

	/**
	 * TotemObject:current-time:
	 *
	 * The player's position (time) in the current stream, in milliseconds.
	 **/
	g_object_class_install_property (object_class, PROP_CURRENT_TIME,
					 g_param_spec_int64 ("current-time", NULL, NULL,
							     G_MININT64, G_MAXINT64, 0,
							     G_PARAM_READABLE));

	/**
	 * TotemObject:seekable:
	 *
	 * If %TRUE, the current stream is seekable.
	 **/
	g_object_class_install_property (object_class, PROP_SEEKABLE,
					 g_param_spec_boolean ("seekable", NULL, NULL,
							       FALSE, G_PARAM_READABLE));

	/**
	 * TotemObject:current-mrl:
	 *
	 * The MRL of the current stream.
	 **/
	g_object_class_install_property (object_class, PROP_CURRENT_MRL,
					 g_param_spec_string ("current-mrl", NULL, NULL,
							      NULL, G_PARAM_READABLE));

	/**
	 * TotemObject::file-opened:
	 * @totem: the #TotemObject which received the signal
	 * @mrl: the MRL of the opened stream
	 *
	 * The ::file-opened signal is emitted when a new stream is opened by Totem.
	 */
	totem_table_signals[FILE_OPENED] =
		g_signal_new ("file-opened",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, file_opened),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * TotemObject::file-closed:
	 * @totem: the #TotemObject which received the signal
	 *
	 * The ::file-closed signal is emitted when Totem closes a stream.
	 */
	totem_table_signals[FILE_CLOSED] =
		g_signal_new ("file-closed",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, file_closed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0, G_TYPE_NONE);

	/**
	 * TotemObject::metadata-updated:
	 * @totem: the #TotemObject which received the signal
	 * @artist: the name of the artist, or %NULL
	 * @title: the stream title, or %NULL
	 * @album: the name of the stream's album, or %NULL
	 * @track_number: the stream's track number
	 *
	 * The ::metadata-updated signal is emitted when the metadata of a stream is updated, typically
	 * when it's being loaded.
	 */
	totem_table_signals[METADATA_UPDATED] =
		g_signal_new ("metadata-updated",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, metadata_updated),
				NULL, NULL,
				totemobject_marshal_VOID__STRING_STRING_STRING_UINT,
				G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);
}

static void
totem_object_init (TotemObject *totem)
{
	//FIXME nothing yet
}

static void
totem_object_finalize (GObject *object)
{

	G_OBJECT_CLASS (totem_object_parent_class)->finalize (object);
}

static void
totem_object_set_property (GObject *object,
			   guint property_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
totem_object_get_property (GObject *object,
			   guint property_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	TotemObject *totem;

	totem = TOTEM_OBJECT (object);

	switch (property_id)
	{
	case PROP_FULLSCREEN:
		g_value_set_boolean (value, totem_is_fullscreen (totem));
		break;
	case PROP_PLAYING:
		g_value_set_boolean (value, totem_is_playing (totem));
		break;
	case PROP_STREAM_LENGTH:
		g_value_set_int64 (value, bacon_video_widget_get_stream_length (totem->bvw));
		break;
	case PROP_CURRENT_TIME:
		g_value_set_int64 (value, bacon_video_widget_get_current_time (totem->bvw));
		break;
	case PROP_SEEKABLE:
		g_value_set_boolean (value, totem_is_seekable (totem));
		break;
	case PROP_CURRENT_MRL:
		g_value_set_string (value, totem->mrl);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

/**
 * totem_object_plugins_init:
 * @totem: a #TotemObject
 *
 * Initialises the plugin engine and activates all the
 * enabled plugins.
 **/
void
totem_object_plugins_init (TotemObject *totem)
{
	totem_plugins_engine_init (totem);
}

/**
 * totem_object_plugins_shutdown:
 *
 * Shuts down the plugin engine and deactivates all the
 * plugins.
 **/
void
totem_object_plugins_shutdown (void)
{
	totem_plugins_engine_shutdown ();
}

/**
 * totem_get_main_window:
 * @totem: a #TotemObject
 *
 * Gets Totem's main window and increments its reference count.
 *
 * Return value: Totem's main window
 **/
GtkWindow *
totem_get_main_window (Totem *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	g_object_ref (G_OBJECT (totem->win));

	return GTK_WINDOW (totem->win);
}

/**
 * totem_get_ui_manager:
 * @totem: a #TotemObject
 *
 * Gets Totem's UI manager, but does not change its reference count.
 *
 * Return value: Totem's UI manager
 **/
GtkUIManager *
totem_get_ui_manager (Totem *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	return totem->ui_manager;
}

/**
 * totem_get_video_widget:
 * @totem: a #TotemObject
 *
 * Gets Totem's video widget and increments its reference count.
 *
 * Return value: Totem's video widget
 **/
GtkWidget *
totem_get_video_widget (Totem *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	g_object_ref (G_OBJECT (totem->bvw));

	return GTK_WIDGET (totem->bvw);
}

/**
 * totem_get_video_widget_backend_name:
 * @totem: a #TotemObject
 *
 * Gets the name string of the backend video widget, typically the video library's
 * version string (e.g. what's returned by gst_version_string()).
 *
 * Return value: the name string of the backend video widget
 **/
char *
totem_get_video_widget_backend_name (Totem *totem)
{
	return bacon_video_widget_get_backend_name (totem->bvw);
}

/**
 * totem_get_current_time:
 * @totem: a #TotemObject
 *
 * Gets the current position's time in the stream as a gint64.
 *
 * Return value: the current position in the stream
 **/
gint64
totem_get_current_time (Totem *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), 0);

	return bacon_video_widget_get_current_time (totem->bvw);
}

/**
 * totem_add_to_playlist_and_play:
 * @totem: a #TotemObject
 * @uri: the URI to add to the playlist
 * @display_name: the display name of the URI
 * @add_to_recent: if %TRUE, add the URI to the recent items list
 *
 * Add @uri to the playlist and play it immediately.
 **/
void
totem_add_to_playlist_and_play (Totem *totem,
				const char *uri,
				const char *display_name,
				gboolean add_to_recent)
{
	gboolean playlist_changed;
	int end;

	totem_signal_block_by_data (totem->playlist, totem);

	playlist_changed = totem_playlist_add_mrl_with_cursor (totem->playlist, uri, display_name);
	if (add_to_recent != FALSE)
		gtk_recent_manager_add_item (totem->recent_manager, uri);
	end = totem_playlist_get_last (totem->playlist);

	totem_signal_unblock_by_data (totem->playlist, totem);

	if (playlist_changed && end != -1) {
		char *mrl, *subtitle;

		subtitle = NULL;
		totem_playlist_set_current (totem->playlist, end);
		mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);
		totem_action_set_mrl_and_play (totem, mrl, subtitle);
		g_free (mrl);
		g_free (subtitle);
	}
}

/**
 * totem_get_current_mrl:
 * @totem: a #TotemObject
 *
 * Get the MRL of the current stream, or %NULL if nothing's playing.
 *
 * Return value: the MRL of the current stream
 **/
char *
totem_get_current_mrl (Totem *totem)
{
	return totem_playlist_get_current_mrl (totem->playlist, NULL);
}

guint
totem_get_playlist_length (Totem *totem)
{
	int last;

	last = totem_playlist_get_last (totem->playlist);
	if (last == -1)
		return 0;
	return last + 1;
}

int
totem_get_playlist_pos (Totem *totem)
{
	return totem_playlist_get_current (totem->playlist);
}

char *
totem_get_title_at_playlist_pos (Totem *totem, guint index)
{
	return totem_playlist_get_title (totem->playlist, index);
}

char *
totem_get_short_title (Totem *totem)
{
	gboolean custom;
	return totem_playlist_get_current_title (totem->playlist, &custom);
}

/**
 * totem_set_current_subtitle:
 * @totem: a #TotemObject
 * @subtitle_uri: the URI of the subtitle file to add
 *
 * Add the @subtitle_uri subtitle file to the playlist, setting it as the subtitle for the current
 * playlist entry.
 **/
void
totem_set_current_subtitle (Totem *totem, const char *subtitle_uri)
{
	totem_playlist_set_current_subtitle (totem->playlist, subtitle_uri);
}

/**
 * totem_add_sidebar_page:
 * @totem: a #TotemObject
 * @page_id: a string used to identify the page
 * @title: the page's title
 * @main_widget: the main widget for the page
 *
 * Adds a sidebar page to Totem's sidebar with the given @page_id.
 * @main_widget is added into the page and shown automatically, while
 * @title is displayed as the page's title in the tab bar.
 **/
void
totem_add_sidebar_page (Totem *totem,
			const char *page_id,
			const char *title,
			GtkWidget *main_widget)
{
	ev_sidebar_add_page (EV_SIDEBAR (totem->sidebar),
			     page_id,
			     title,
			     main_widget);
}

/**
 * totem_remove_sidebar_page:
 * @totem: a #TotemObject
 * @page_id: a string used to identify the page
 *
 * Removes the page identified by @page_id from Totem's sidebar.
 * If @page_id doesn't exist in the sidebar, this function does
 * nothing.
 **/
void
totem_remove_sidebar_page (Totem *totem,
			   const char *page_id)
{
	ev_sidebar_remove_page (EV_SIDEBAR (totem->sidebar),
				page_id);
}

/**
 * totem_file_opened:
 * @totem: a #TotemObject
 * @mrl: the MRL opened
 *
 * Emits the ::file-opened signal on @totem, with the
 * specified @mrl.
 **/
void
totem_file_opened (TotemObject *totem,
		   const char *mrl)
{
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[FILE_OPENED],
		       0, mrl);
}

/**
 * totem_file_closed:
 * @totem: a #TotemObject
 *
 * Emits the ::file-closed signal on @totem.
 **/
void
totem_file_closed (TotemObject *totem)
{
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[FILE_CLOSED],
		       0);

}

/**
 * totem_metadata_updated:
 * @totem: a #TotemObject
 * @artist: the stream's artist
 * @title: the stream's title
 * @album: the stream's album
 *
 * Emits the ::metadata-updated signal on @totem,
 * with the specified stream data.
 **/
void
totem_metadata_updated (TotemObject *totem,
			const char *artist,
			const char *title,
			const char *album,
			guint track_num)
{
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[METADATA_UPDATED],
		       0,
		       artist,
		       title,
		       album,
		       track_num);
}

GQuark
totem_remote_command_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("totem_remote_command");

	return quark;
}

/* This should really be standard. */
#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType
totem_remote_command_get_type (void)
{
	static GType etype = 0;

	if (etype == 0)	{
		static const GEnumValue values[] = {
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_UNKNOWN, "Unknown command"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PLAY, "Play"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PAUSE, "Pause"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PLAYPAUSE, "Play or pause"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_NEXT, "Next file"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PREVIOUS, "Previous file"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SEEK_FORWARD, "Seek forward"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SEEK_BACKWARD, "Seek backward"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_VOLUME_UP, "Volume up"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_VOLUME_DOWN, "Volume down"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_FULLSCREEN, "Fullscreen"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_QUIT, "Quit"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_ENQUEUE, "Enqueue"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_REPLACE, "Replace"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SHOW, "Show"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_TOGGLE_CONTROLS, "Toggle controls"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SHOW_PLAYING, "Show playing"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SHOW_VOLUME, "Show volume"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_UP, "Up"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_DOWN, "Down"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_LEFT, "Left"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_RIGHT, "Right"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SELECT, "Select"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_DVD_MENU, "DVD menu"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_ZOOM_UP, "Zoom up"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_ZOOM_DOWN, "Zoom down"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_EJECT, "Eject"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PLAY_DVD, "Play DVD"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_MUTE, "Mute"),
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TotemRemoteCommand", values);
	}

	return etype;
}

GQuark
totem_disc_media_type_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("totem_disc_media_type");

	return quark;
}

GType
totem_disc_media_type_get_type (void)
{
	static GType etype = 0;

	if (etype == 0)	{
		static const GEnumValue values[] = {
			ENUM_ENTRY (MEDIA_TYPE_ERROR, "Media type error"),
			ENUM_ENTRY (MEDIA_TYPE_DATA, "Data disc"),
			ENUM_ENTRY (MEDIA_TYPE_CDDA, "CDDA disc"),
			ENUM_ENTRY (MEDIA_TYPE_VCD, "VCD"),
			ENUM_ENTRY (MEDIA_TYPE_DVD, "DVD"),
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TotemDiscMediaType", values);
	}

	return etype;
}
