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
 */

#ifndef __TOTEM_PRIVATE_H__
#define __TOTEM_PRIVATE_H__

#include <glade/glade.h>
#include <glib/gi18n.h>
#include "totem-remote.h"
#include "scrsaver.h"
#include "egg-recent-model.h"
#include "egg-recent-view-gtk.h"
#include "totem-playlist.h"
#include "bacon-message-connection.h"
#include "bacon-video-widget.h"

typedef enum {
	TOTEM_CONTROLS_VISIBLE,
	TOTEM_CONTROLS_HIDDEN,
	TOTEM_CONTROLS_FULLSCREEN
} ControlsVisibility;

typedef enum {
	STATE_PLAYING,
	STATE_PAUSED,
	STATE_STOPPED
} TotemStates;

struct Totem {
	/* Control window */
	GladeXML *xml;
	GtkWidget *win;
	BaconVideoWidget *bvw;
	GtkWidget *prefs;
	GtkWidget *properties;
	GtkWidget *statusbar;

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
	GtkAdjustment *voladj;
	gboolean vol_lock;
	gboolean vol_fs_lock;
	gfloat prev_volume;
	int volume_first_time;

	/* Subtitles/Languages menus */
	GtkWidget *subtitles;
	GtkWidget *languages;

	/* exit fullscreen Popup */
	GtkWidget *exit_popup;

	/* controls management */
	ControlsVisibility controls_visibility;

	/* control fullscreen Popup */
	GtkWidget *control_popup;
	GtkWidget *fs_seek;
	GtkAdjustment *fs_seekadj;
	GtkWidget *fs_volume;
	GtkAdjustment *fs_voladj;
	GtkWidget *tcw_time_label;

	guint popup_timeout;
	gboolean popup_in_progress;
	GdkRectangle fullscreen_rect;

	TotemScrsaver *scr;

	/* recent file stuff */
	EggRecentModel *recent_model;
	EggRecentViewGtk *recent_view;

	/* other */
	char *mrl;
	TotemPlaylist *playlist;
	GConfClient *gc;
	TotemRemote *remote;
	BaconMessageConnection *conn;
	guint32 keypress_time;
	TotemStates state;
};

#endif /* __TOTEM_PRIVATE_H__ */
