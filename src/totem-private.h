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

#ifndef __TOTEM_PRIVATE_H__
#define __TOTEM_PRIVATE_H__

#include <gtk/gtk.h>
#include <gio/gio.h>

#include "totem-playlist.h"
#include "backend/bacon-video-widget.h"
#include "backend/bacon-time-label.h"
#include "totem-open-location.h"
#include "totem-plugins-engine.h"

#define totem_signal_block_by_data(obj, data) (g_signal_handlers_block_matched (obj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, data))
#define totem_signal_unblock_by_data(obj, data) (g_signal_handlers_unblock_matched (obj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, data))

#define totem_set_sensitivity(xml, name, state)					\
	{									\
		GtkWidget *widget;						\
		widget = GTK_WIDGET (gtk_builder_get_object (xml, name));	\
		gtk_widget_set_sensitive (widget, state);			\
	}
#define totem_controls_set_sensitivity(name, state) gtk_widget_set_sensitive (g_object_get_data (totem->controls, name), state)

#define totem_object_set_sensitivity(name, state)					\
	{										\
		GtkAction *__action;							\
		__action = gtk_action_group_get_action (totem->main_action_group, name);\
		gtk_action_set_sensitive (__action, state);				\
	}

#define totem_object_set_sensitivity2(name, state)					\
	{										\
		GAction *__action;							\
		__action = g_action_map_lookup_action (G_ACTION_MAP (totem), name);	\
		g_simple_action_set_enabled (G_SIMPLE_ACTION (__action), state);	\
	}

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
	GtkWidget *prefs;
	GtkBuilder *prefs_xml;

	GtkWidget *grilo;

	GObject *controls;
	BaconTimeLabel *time_label;
	BaconTimeLabel *time_rem_label;
	GtkWidget *header;

	/* UI manager */
	GtkActionGroup *main_action_group;
	GtkUIManager *ui_manager;

	GtkActionGroup *languages_action_group;
	guint languages_ui_id;

	GtkActionGroup *subtitles_action_group;
	guint subtitles_ui_id;

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
	GList *subtitles_list;
	GList *languages_list;

	/* controls management */
	ControlsVisibility controls_visibility;

	/* Stream info */
	gint64 stream_length;

	/* Monitor for playlist unmounts and drives/volumes monitoring */
	GVolumeMonitor *monitor;
	gboolean drives_changed;

	/* session */
	gint64 seek_to_start;
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

	char *player_title;

	/* other */
	char *mrl;
	TotemPlaylist *playlist;
	GSettings *settings;
	TotemStates state;
	TotemOpenLocation *open_location;
	gboolean disable_kbd_shortcuts;
	gboolean has_played_emitted;
};

GtkWidget *totem_volume_create (void);

#define SEEK_FORWARD_OFFSET 60
#define SEEK_BACKWARD_OFFSET -15

#define VOLUME_DOWN_OFFSET (-0.08)
#define VOLUME_UP_OFFSET (0.08)

#define VOLUME_DOWN_SHORT_OFFSET (-0.02)
#define VOLUME_UP_SHORT_OFFSET (0.02)

#define ZOOM_IN_OFFSET 0.01
#define ZOOM_OUT_OFFSET -0.01

void	totem_object_open			(Totem *totem);
void	totem_object_open_location		(Totem *totem);
void	totem_object_eject			(Totem *totem);
void	totem_object_set_zoom			(Totem *totem, gboolean zoom);
void	totem_object_show_help			(Totem *totem);
void	totem_object_show_properties		(Totem *totem);
void    totem_object_set_mrl			(TotemObject *totem,
						 const char *mrl,
						 const char *subtitle);
gboolean totem_object_open_files		(Totem *totem, char **list);
G_GNUC_NORETURN void totem_object_show_error_and_exit (const char *title, const char *reason, Totem *totem);

void	show_controls				(Totem *totem, gboolean was_fullscreen);

void	totem_setup_window			(Totem *totem);
void	totem_callback_connect			(Totem *totem);
void	playlist_widget_setup			(Totem *totem);
void	grilo_widget_setup			(Totem *totem);
void	video_widget_create			(Totem *totem);
void    totem_object_plugins_init		(TotemObject *totem);
void    totem_object_plugins_shutdown		(TotemObject *totem);
void	totem_object_set_fullscreen		(TotemObject *totem, gboolean state);
void	totem_object_set_volume_relative	(TotemObject *totem, double off_pct);
void	totem_object_volume_toggle_mute		(TotemObject *totem);
void	totem_object_set_main_page		(TotemObject *totem,
						 const char  *page_id);
const char * totem_object_get_main_page		(Totem *totem);

/* Signal emission */
void	totem_file_has_played			(TotemObject *totem,
						 const char *mrl);

#endif /* __TOTEM_PRIVATE_H__ */
