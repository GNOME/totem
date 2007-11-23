/* 
 * Copyright (C) 2001,2002,2003,2004,2005 Bastien Nocera <hadess@hadess.net>
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

#ifndef __TOTEM_H__
#define __TOTEM_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include "plparse/totem-disc.h"
#include "totem-playlist.h"

#define TOTEM_GCONF_PREFIX "/apps/totem"

G_BEGIN_DECLS

typedef enum {
	TOTEM_REMOTE_COMMAND_UNKNOWN,
	TOTEM_REMOTE_COMMAND_PLAY,
	TOTEM_REMOTE_COMMAND_PAUSE,
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
	TOTEM_REMOTE_COMMAND_TOGGLE_CONTROLS,
	TOTEM_REMOTE_COMMAND_SHOW_PLAYING,
	TOTEM_REMOTE_COMMAND_SHOW_VOLUME,
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
	TOTEM_REMOTE_COMMAND_MUTE
} TotemRemoteCommand;

typedef enum {
	TOTEM_REMOTE_SETTING_SHUFFLE,
	TOTEM_REMOTE_SETTING_REPEAT
} TotemRemoteSetting;

GType totem_remote_command_get_type	(void);
GQuark totem_remote_command_quark	(void);
#define TOTEM_TYPE_REMOTE_COMMAND	(totem_remote_command_get_type())
#define TOTEM_REMOTE_COMMAND		totem_remote_command_quark ()

GType totem_disc_media_type_get_type	(void);
GQuark totem_disc_media_type_quark	(void);
#define TOTEM_TYPE_DISC_MEDIA_TYPE	(totem_disc_media_type_get_type())
#define TOTEM_DISC_MEDIA_TYPE		totem_disc_media_type_quark ()

#define SHOW_PLAYING_NO_TRACKS "NONE"

#define TOTEM_TYPE_OBJECT              (totem_object_get_type ())
#define TOTEM_OBJECT(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), totem_object_get_type (), TotemObject))
#define TOTEM_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), totem_object_get_type (), TotemObjectClass))
#define TOTEM_IS_OBJECT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE (obj, totem_object_get_type ()))
#define TOTEM_IS_OBJECT_CLASS(klass)   (G_CHECK_INSTANCE_GET_CLASS ((klass), totem_object_get_type ()))

typedef struct TotemObject Totem;
typedef struct TotemObject TotemObject;

typedef struct {
	GObjectClass parent_class;

	void (*file_opened)			(Totem *totem, const char *mrl);
	void (*file_closed)			(Totem *totem);
	void (*metadata_updated)		(Totem *totem,
						 const char *artist,
						 const char *title,
						 const char *album);
} TotemObjectClass;

GType	totem_object_get_type			(void);
void    totem_object_plugins_init		(TotemObject *totem);
void    totem_object_plugins_shutdown		(void);
void	totem_file_opened			(TotemObject *totem,
						 const char *mrl);
void	totem_file_closed			(TotemObject *totem);
void	totem_metadata_updated			(TotemObject *totem,
						 const char *artist,
						 const char *title,
						 const char *album);

void	totem_action_exit			(Totem *totem) G_GNUC_NORETURN;
void	totem_action_play			(Totem *totem);
void	totem_action_stop			(Totem *totem);
void	totem_action_play_pause			(Totem *totem);
void	totem_action_pause			(Totem *totem);
void	totem_action_fullscreen_toggle		(Totem *totem);
void	totem_action_fullscreen			(Totem *totem, gboolean state);
void	totem_action_next			(Totem *totem);
void	totem_action_previous			(Totem *totem);
void	totem_action_seek_time			(Totem *totem, gint64 sec);
void	totem_action_seek_relative		(Totem *totem, gint64 offset);
void	totem_action_volume_relative		(Totem *totem, double off_pct);
gboolean totem_action_set_mrl			(Totem *totem,
						 const char *mrl);
void	totem_action_set_mrl_and_play		(Totem *totem,
						 const char *mrl);

gboolean totem_action_set_mrl_with_warning	(Totem *totem,
						 const char *mrl,
						 gboolean warn);

void	totem_action_play_media			(Totem *totem,
						 TotemDiscMediaType type,
						 const char *device);

void	totem_action_toggle_aspect_ratio	(Totem *totem);
void	totem_action_set_aspect_ratio		(Totem *totem, int ratio);
int	totem_action_get_aspect_ratio		(Totem *totem);
void	totem_action_toggle_controls		(Totem *totem);
void	totem_action_next_angle			(Totem *totem);

void	totem_action_set_scale_ratio		(Totem *totem, gfloat ratio);
void    totem_action_error                      (const char *title,
						 const char *reason,
						 Totem *totem);
void    totem_action_play_media_device		(Totem *totem,
						 const char *device);

gboolean totem_is_fullscreen			(Totem *totem);
gboolean totem_is_playing			(Totem *totem);
gboolean totem_is_paused			(Totem *totem);
gboolean totem_is_seekable			(Totem *totem);
GtkWindow *totem_get_main_window		(Totem *totem);
GtkUIManager *totem_get_ui_manager		(Totem *totem);
GtkWidget *totem_get_video_widget		(Totem *totem);

/* Current media information */
char *	totem_get_short_title			(Totem *totem);
gint64	totem_get_current_time			(Totem *totem);

/* Playlist handling */
guint	totem_get_playlist_length		(Totem *totem);
void	totem_action_set_playlist_index		(Totem *totem,
						 guint index);
guint	totem_get_playlist_pos			(Totem *totem);
char *	totem_get_title_at_playlist_pos		(Totem *totem,
						 guint index);
void totem_add_to_playlist_and_play		(Totem *totem,
						 const char *uri,
						 const char *display_name,
						 gboolean add_to_recent);

/* Sidebar handling */
void    totem_add_sidebar_page			(Totem *totem,
						 const char *page_id,
						 const char *title,
						 GtkWidget *main_widget);
void    totem_remove_sidebar_page		(Totem *totem,
						 const char *page_id);

/* Remote actions */
void    totem_action_remote			(Totem *totem,
						 TotemRemoteCommand cmd,
						 const char *url);
void	totem_action_remote_set_setting		(Totem *totem,
						 TotemRemoteSetting setting,
						 gboolean value);
gboolean totem_action_remote_get_setting	(Totem *totem,
						 TotemRemoteSetting setting);

#endif /* __TOTEM_H__ */
