/*
 * Copyright (C) 2001,2002,2003,2004,2005 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

/**
 * TOTEM_GSETTINGS_SCHEMA:
 *
 * The GSettings schema under which all Totem settings are stored.
 **/
#define TOTEM_GSETTINGS_SCHEMA "org.gnome.totem"

/**
 * TotemRemoteCommand:
 * @TOTEM_REMOTE_COMMAND_UNKNOWN: unknown command
 * @TOTEM_REMOTE_COMMAND_PLAY: play the current stream
 * @TOTEM_REMOTE_COMMAND_PAUSE: pause the current stream
 * @TOTEM_REMOTE_COMMAND_STOP: stop playing the current stream
 * @TOTEM_REMOTE_COMMAND_PLAYPAUSE: toggle play/pause on the current stream
 * @TOTEM_REMOTE_COMMAND_NEXT: play the next playlist item
 * @TOTEM_REMOTE_COMMAND_PREVIOUS: play the previous playlist item
 * @TOTEM_REMOTE_COMMAND_SEEK_FORWARD: seek forwards in the current stream
 * @TOTEM_REMOTE_COMMAND_SEEK_BACKWARD: seek backwards in the current stream
 * @TOTEM_REMOTE_COMMAND_VOLUME_UP: increase the volume
 * @TOTEM_REMOTE_COMMAND_VOLUME_DOWN: decrease the volume
 * @TOTEM_REMOTE_COMMAND_FULLSCREEN: toggle fullscreen mode
 * @TOTEM_REMOTE_COMMAND_QUIT: quit the instance of Totem
 * @TOTEM_REMOTE_COMMAND_ENQUEUE: enqueue a new playlist item
 * @TOTEM_REMOTE_COMMAND_REPLACE: replace an item in the playlist
 * @TOTEM_REMOTE_COMMAND_SHOW: show the Totem instance
 * @TOTEM_REMOTE_COMMAND_UP: go up (DVD controls)
 * @TOTEM_REMOTE_COMMAND_DOWN: go down (DVD controls)
 * @TOTEM_REMOTE_COMMAND_LEFT: go left (DVD controls)
 * @TOTEM_REMOTE_COMMAND_RIGHT: go right (DVD controls)
 * @TOTEM_REMOTE_COMMAND_SELECT: select the current item (DVD controls)
 * @TOTEM_REMOTE_COMMAND_DVD_MENU: go to the DVD menu
 * @TOTEM_REMOTE_COMMAND_ZOOM_UP: increase the zoom level
 * @TOTEM_REMOTE_COMMAND_ZOOM_DOWN: decrease the zoom level
 * @TOTEM_REMOTE_COMMAND_EJECT: eject the current disc
 * @TOTEM_REMOTE_COMMAND_PLAY_DVD: play a DVD in a drive
 * @TOTEM_REMOTE_COMMAND_MUTE: toggle mute
 * @TOTEM_REMOTE_COMMAND_TOGGLE_ASPECT: toggle the aspect ratio
 *
 * Represents a command which can be sent to a running Totem instance remotely.
 **/
typedef enum {
	TOTEM_REMOTE_COMMAND_UNKNOWN = 0,
	TOTEM_REMOTE_COMMAND_PLAY,
	TOTEM_REMOTE_COMMAND_PAUSE,
	TOTEM_REMOTE_COMMAND_STOP,
	TOTEM_REMOTE_COMMAND_PLAYPAUSE,
	TOTEM_REMOTE_COMMAND_NEXT,
	TOTEM_REMOTE_COMMAND_PREVIOUS,
	TOTEM_REMOTE_COMMAND_SEEK_FORWARD,
	TOTEM_REMOTE_COMMAND_SEEK_BACKWARD,
	TOTEM_REMOTE_COMMAND_VOLUME_UP,
	TOTEM_REMOTE_COMMAND_VOLUME_DOWN,
	TOTEM_REMOTE_COMMAND_FULLSCREEN,
	TOTEM_REMOTE_COMMAND_QUIT,
	TOTEM_REMOTE_COMMAND_ENQUEUE,
	TOTEM_REMOTE_COMMAND_REPLACE,
	TOTEM_REMOTE_COMMAND_SHOW,
	TOTEM_REMOTE_COMMAND_UP,
	TOTEM_REMOTE_COMMAND_DOWN,
	TOTEM_REMOTE_COMMAND_LEFT,
	TOTEM_REMOTE_COMMAND_RIGHT,
	TOTEM_REMOTE_COMMAND_SELECT,
	TOTEM_REMOTE_COMMAND_DVD_MENU,
	TOTEM_REMOTE_COMMAND_ZOOM_UP,
	TOTEM_REMOTE_COMMAND_ZOOM_DOWN,
	TOTEM_REMOTE_COMMAND_EJECT,
	TOTEM_REMOTE_COMMAND_PLAY_DVD,
	TOTEM_REMOTE_COMMAND_MUTE,
	TOTEM_REMOTE_COMMAND_TOGGLE_ASPECT
} TotemRemoteCommand;

/**
 * TotemRemoteSetting:
 * @TOTEM_REMOTE_SETTING_REPEAT: whether repeat is enabled
 *
 * Represents a boolean setting or preference on a remote Totem instance.
 **/
typedef enum {
	TOTEM_REMOTE_SETTING_REPEAT
} TotemRemoteSetting;

GType totem_remote_command_get_type	(void);
GQuark totem_remote_command_quark	(void);
#define TOTEM_TYPE_REMOTE_COMMAND	(totem_remote_command_get_type())
#define TOTEM_REMOTE_COMMAND		totem_remote_command_quark ()

GType totem_remote_setting_get_type	(void);
GQuark totem_remote_setting_quark	(void);
#define TOTEM_TYPE_REMOTE_SETTING	(totem_remote_setting_get_type())
#define TOTEM_REMOTE_SETTING		totem_remote_setting_quark ()

/**
 * Totem:
 *
 * The #Totem object is a handy synonym for #TotemObject, and the two can be used interchangably.
 **/
typedef struct _TotemObject Totem;

/**
 * TotemObject:
 *
 * All the fields in the #TotemObject structure are private and should never be accessed directly.
 **/
#define TOTEM_TYPE_OBJECT              (totem_object_get_type ())
G_DECLARE_FINAL_TYPE(TotemObject, totem_object, TOTEM, OBJECT, GtkApplication)

void	totem_object_exit			(TotemObject *totem) G_GNUC_NORETURN;
void	totem_object_play			(TotemObject *totem);
void	totem_object_stop			(TotemObject *totem);
void	totem_object_play_pause			(TotemObject *totem);
void	totem_object_pause			(TotemObject *totem);
gboolean totem_object_can_seek_next		(TotemObject *totem);
void	totem_object_seek_next			(TotemObject *totem);
gboolean totem_object_can_seek_previous		(TotemObject *totem);
void	totem_object_seek_previous		(TotemObject *totem);
void	totem_object_seek_time			(TotemObject *totem, gint64 msec, gboolean accurate);
void	totem_object_seek_relative		(TotemObject *totem, gint64 offset, gboolean accurate);
double	totem_object_get_volume			(TotemObject *totem);
void	totem_object_set_volume			(TotemObject *totem, double volume);

void	totem_object_next_angle			(TotemObject *totem);

void    totem_object_show_error			(TotemObject *totem,
						 const char *title,
						 const char *reason);

gboolean totem_object_is_fullscreen		(TotemObject *totem);
gboolean totem_object_is_playing		(TotemObject *totem);
gboolean totem_object_is_paused			(TotemObject *totem);
gboolean totem_object_is_seekable		(TotemObject *totem);
GtkWindow *totem_object_get_main_window		(TotemObject *totem);
GMenu *totem_object_get_menu_section		(TotemObject *totem,
						 const char  *id);
void totem_object_empty_menu_section		(TotemObject *totem,
						 const char  *id);

float		totem_object_get_rate		(TotemObject *totem);
gboolean	totem_object_set_rate		(TotemObject *totem, float rate);

GtkWidget *totem_object_get_video_widget	(TotemObject *totem);

/* Database handling */
void	totem_object_add_to_view		(TotemObject *totem,
						 GFile       *file,
						 const char  *title);

/* Current media information */
char *	totem_object_get_short_title		(TotemObject *totem);
gint64	totem_object_get_current_time		(TotemObject *totem);

/* Playlist handling */
guint	totem_object_get_playlist_length	(TotemObject *totem);
int	totem_object_get_playlist_pos		(TotemObject *totem);
char *	totem_object_get_title_at_playlist_pos	(TotemObject *totem,
						 guint playlist_index);
void	totem_object_clear_playlist		(TotemObject *totem);
void	totem_object_add_to_playlist		(TotemObject *totem,
						 const char  *uri,
						 const char  *display_name,
						 gboolean     play);
char *  totem_object_get_current_mrl		(TotemObject *totem);
void	totem_object_set_current_subtitle	(TotemObject *totem,
						 const char *subtitle_uri);
/* Remote actions */
void    totem_object_remote_command		(TotemObject *totem,
						 TotemRemoteCommand cmd,
						 const char *url);
void	totem_object_remote_set_setting		(TotemObject *totem,
						 TotemRemoteSetting setting,
						 gboolean value);
gboolean totem_object_remote_get_setting	(TotemObject *totem,
						 TotemRemoteSetting setting);

const gchar * const *totem_object_get_supported_content_types (void);
const gchar * const *totem_object_get_supported_uri_schemes (void);
