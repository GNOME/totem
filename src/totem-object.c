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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#include "config.h"

#include <glib-object.h>
#include <gtk/gtkwindow.h>

#include "totem.h"
#include "totemobject-marshal.h"
#include "totem-private.h"
#include "totem-plugins-engine.h"
#include "ev-sidebar.h"

enum {
	PROP_0,
	PROP_FULLSCREEN,
	PROP_PLAYING,
	PROP_STREAM_LENGTH,
	PROP_SEEKABLE,
	PROP_CURRENT_TIME,
	PROP_ERROR_SHOWN
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

	g_object_class_install_property (object_class, PROP_FULLSCREEN,
					 g_param_spec_boolean ("fullscreen", NULL, NULL,
							       FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_PLAYING,
					 g_param_spec_boolean ("playing", NULL, NULL,
							       FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_STREAM_LENGTH,
					 g_param_spec_int64 ("stream-length", NULL, NULL,
							     G_MININT64, G_MAXINT64, 0,
							     G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_CURRENT_TIME,
					 g_param_spec_int64 ("current-time", NULL, NULL,
							     G_MININT64, G_MAXINT64, 0,
							     G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_SEEKABLE,
					 g_param_spec_boolean ("seekable", NULL, NULL,
							       FALSE, G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_ERROR_SHOWN,
					 g_param_spec_boolean ("error-shown", NULL, NULL,
							       FALSE, G_PARAM_READABLE));

	totem_table_signals[FILE_OPENED] =
		g_signal_new ("file-opened",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, file_opened),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);

	totem_table_signals[FILE_CLOSED] =
		g_signal_new ("file-closed",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, file_closed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0, G_TYPE_NONE);

	totem_table_signals[METADATA_UPDATED] =
		g_signal_new ("metadata-updated",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, metadata_updated),
				NULL, NULL,
				totemobject_marshal_VOID__STRING_STRING_STRING,
				G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
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
	case PROP_ERROR_SHOWN:
		//g_value_set_boolean (value, XXX);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

void
totem_object_plugins_init (TotemObject *totem)
{
	totem_plugins_engine_init (totem);
}

void
totem_object_plugins_shutdown (void)
{
	totem_plugins_engine_shutdown ();
}

GtkWindow *
totem_get_main_window (Totem *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	g_object_ref (G_OBJECT (totem->win));

	return GTK_WINDOW (totem->win);
}

GtkUIManager *
totem_get_ui_manager (Totem *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	return totem->ui_manager;
}

GtkWidget *
totem_get_video_widget (Totem *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	g_object_ref (G_OBJECT (totem->bvw));

	return GTK_WIDGET (totem->bvw);
}

gint64
totem_get_current_time (Totem *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), 0);

	return bacon_video_widget_get_current_time (totem->bvw);
}

guint
totem_get_playlist_length (Totem *totem)
{
	return totem_playlist_get_last (totem->playlist) + 1;
}

guint
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

void
totem_remove_sidebar_page (Totem *totem,
			   const char *page_id)
{
	ev_sidebar_remove_page (EV_SIDEBAR (totem->sidebar),
				page_id);
}

void
totem_file_opened (TotemObject *totem,
		   const char *mrl)
{
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[FILE_OPENED],
		       0, mrl);
}

void
totem_file_closed (TotemObject *totem)
{
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[FILE_CLOSED],
		       0);

}

void
totem_metadata_updated (TotemObject *totem,
			const char *artist,
			const char *title,
			const char *album)
{
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[METADATA_UPDATED],
		       0,
		       artist,
		       title,
		       album);
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
			{ 0, 0, 0 }
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
			{ 0, 0, 0 }
		};

		etype = g_enum_register_static ("TotemDiscMediaType", values);
	}

	return etype;
}
