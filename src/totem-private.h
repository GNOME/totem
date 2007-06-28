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

#ifndef __TOTEM_PRIVATE_H__
#define __TOTEM_PRIVATE_H__

#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>

#include "totem-playlist.h"
#include "bacon-message-connection.h"
#include "bacon-video-widget.h"
#include "totem-open-location.h"

#define totem_signal_block_by_data(obj, data) (g_signal_handlers_block_matched (obj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, data))
#define totem_signal_unblock_by_data(obj, data) (g_signal_handlers_unblock_matched (obj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, data))

#define totem_set_sensitivity(xml, name, state)		\
	{							\
		GtkWidget *widget;				\
		widget = glade_xml_get_widget (xml, name);	\
		gtk_widget_set_sensitive (widget, state);	\
	}
#define totem_main_set_sensitivity(name, state) totem_set_sensitivity (totem->xml, name, state)

#define totem_action_set_sensitivity(name, state)					\
	{										\
		GtkAction *action;							\
		action = gtk_action_group_get_action (totem->main_action_group, name);	\
		gtk_action_set_sensitive (action, state);				\
	}

typedef enum {
	TOTEM_CONTROLS_UNDEFINED,
	TOTEM_CONTROLS_VISIBLE,
	TOTEM_CONTROLS_HIDDEN,
	TOTEM_CONTROLS_FULLSCREEN
} ControlsVisibility;

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED
} TotemStates;

struct TotemObject {
	GObject parent;

	/* Control window */
	GladeXML *xml;
	GtkWidget *win;
	BaconVideoWidget *bvw;
	GtkWidget *prefs;
	GtkWidget *statusbar;

	/* UI manager */
	GtkActionGroup *main_action_group;
	GtkActionGroup *zoom_action_group;
	GtkUIManager *ui_manager;

	GtkActionGroup *devices_action_group;
	guint devices_ui_id;

	GtkActionGroup *languages_action_group;
	guint languages_ui_id;

	GtkActionGroup *subtitles_action_group;
	guint subtitles_ui_id;

	/* Plugins */
	GtkWidget *plugins;

	/* Sidebar */
	GtkWidget *sidebar;
	gboolean sidebar_shown;
	int sidebar_w;

	/* Play/Pause */
	GtkWidget *pp_button;
	/* fullscreen Play/Pause */
	GtkWidget *fs_pp_button;

	/* Seek */
	GtkWidget *seek;
	GtkAdjustment *seekadj;
	gboolean seek_lock;
	gboolean seekable;

	/* Volume */
	GtkWidget *volume;
	gfloat prev_volume;
	int volume_first_time;
	gboolean volume_sensitive;

	/* Subtitles/Languages menus */
	GtkWidget *subtitles;
	GtkWidget *languages;
	GList *subtitles_list;
	GList *language_list;

	/* exit fullscreen Popup */
	GtkWidget *exit_popup;

	/* controls management */
	ControlsVisibility controls_visibility;

	/* control fullscreen Popup */
	GtkWidget *control_popup;
	GtkWidget *fs_seek;
	GtkAdjustment *fs_seekadj;
	GtkWidget *tcw_time_label;

	guint popup_timeout;
	gboolean popup_in_progress;
	GdkRectangle fullscreen_rect;

	/* Stream info */
	gint64 stream_length;

	/* recent file stuff */
	GtkRecentManager *recent_manager;
	GtkActionGroup *recent_action_group;
	guint recent_ui_id;

	/* Monitor for playlist unmounts and drives/volumes monitoring */
	GnomeVFSVolumeMonitor *monitor;
	gboolean drives_changed;

	/* session */
	const char *argv0;
	gint64 seek_to;
	guint index;
	gboolean session_restored;

	/* Window State */
	int window_w, window_h;
	gboolean maximised;

	/* other */
	char *mrl;
	TotemPlaylist *playlist;
	GConfClient *gc;
	BaconMessageConnection *conn;
	TotemStates state;
	gboolean cursor_shown;
	TotemOpenLocation *open_location;
};

GtkWidget *totem_volume_create (void);

#define SEEK_FORWARD_OFFSET 60
#define SEEK_BACKWARD_OFFSET -15

#define VOLUME_DOWN_OFFSET -8
#define VOLUME_UP_OFFSET 8

#define ZOOM_IN_OFFSET 1
#define ZOOM_OUT_OFFSET -1

void	totem_action_open			(Totem *totem);
void	totem_action_open_location		(Totem *totem);
void	totem_action_eject			(Totem *totem);
void	totem_action_take_screenshot		(Totem *totem);
void	totem_action_zoom_relative		(Totem *totem, int off_pct);
void	totem_action_zoom_reset			(Totem *totem);
void	totem_action_show_help			(Totem *totem);
void	totem_action_show_properties		(Totem *totem);

void	show_controls				(Totem *totem, gboolean was_fullscreen);

#endif /* __TOTEM_PRIVATE_H__ */
