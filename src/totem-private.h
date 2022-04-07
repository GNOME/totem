/* 
 * Copyright (C) 2001-2002 Bastien Nocera <hadess@hadess.net>
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

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

#include "totem-playlist.h"
#include "backend/bacon-video-widget.h"
#include "backend/bacon-time-label.h"
#include "totem-open-location.h"
#include "totem-plugins-engine.h"

typedef enum {
	TOTEM_CONTROLS_UNDEFINED,
	TOTEM_CONTROLS_VISIBLE,
	TOTEM_CONTROLS_FULLSCREEN
} ControlsVisibility;

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED
} TotemStates;

struct _TotemObject {
	GtkApplication parent;

	/* Control window */
	GtkBuilder *xml;
	GtkWidget *win;
	GtkWidget *stack;
	BaconVideoWidget *bvw;
	GtkWidget *bvw_grid;
	GtkWidget *prefs;
	GtkWindow *shortcuts_win;
	GtkWidget *spinner;

	GtkWidget *grilo;

	GtkWidget *play_button;
	BaconTimeLabel *time_label;
	BaconTimeLabel *time_rem_label;
	GtkWidget *header;

	GtkWidget *fullscreen_header;
	GtkWidget *fullscreen_gear_button;
	GtkWidget *fullscreen_subtitles_button;

	/* Plugins */
	GtkWidget *plugins;
	TotemPluginsEngine *engine;

	/* Seek */
	GtkWidget *seek;
	GtkAdjustment *seekadj;
	gboolean seek_lock;
	gboolean seekable;

	/* Volume */
	GtkWidget *volume;
	gboolean volume_sensitive;
	gboolean muted;
	double prev_volume;

	/* Subtitles/Languages menus */
	gboolean updating_menu;

	/* controls management */
	ControlsVisibility controls_visibility;
	gboolean reveal_controls;
	guint transition_timeout_id;
	GHashTable *busy_popup_ht; /* key=reason string, value=gboolean */

	/* Stream info */
	gint64 stream_length;

	/* Monitor for playlist unmounts and drives/volumes monitoring */
	GVolumeMonitor *monitor;
	gboolean drives_changed;

	/* session */
	gboolean pause_start;
	guint save_timeout_id;

	/* Window State */
	int window_w, window_h;
	gboolean maximised;

	/* Toolbar state */
	char *title;
	char *subtitle;
	char *search_string;
	gboolean select_mode;
	GObject *custom_title;
	GtkWidget *fullscreen_button;
	GtkWidget *gear_button;
	GtkWidget *add_button;
	GtkWidget *main_menu_button;
	GtkWidget *subtitles_button;

	char *player_title;

	/* Playlist */
	TotemPlaylist *playlist;
	GSignalGroup *playlist_signals;

	/* other */
	char *mrl;
	char *next_subtitle;
	GSettings *settings;
	TotemStates state;
	TotemOpenLocation *open_location;
	gboolean disable_kbd_shortcuts;
	gboolean has_played_emitted;
};

#define SEEK_FORWARD_OFFSET 60
#define SEEK_BACKWARD_OFFSET -15

#define VOLUME_DOWN_OFFSET (-0.08)
#define VOLUME_UP_OFFSET (0.08)

#define VOLUME_DOWN_SHORT_OFFSET (-0.02)
#define VOLUME_UP_SHORT_OFFSET (0.02)

void	totem_object_open			(Totem *totem);
void	totem_object_open_location		(Totem *totem);
void	totem_object_eject			(Totem *totem);
void	totem_object_show_help			(Totem *totem);
void	totem_object_show_keyboard_shortcuts	(Totem *totem);
void    totem_object_set_mrl			(TotemObject *totem,
						 const char *mrl,
						 const char *subtitle);

void	totem_object_set_fullscreen		(TotemObject *totem, gboolean state);
void	totem_object_set_main_page		(TotemObject *totem,
						 const char  *page_id);
const char * totem_object_get_main_page		(Totem *totem);
void	totem_object_add_items_to_playlist	(TotemObject *totem,
						 GList       *items);
