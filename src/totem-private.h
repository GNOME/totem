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
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 */

#ifndef __TOTEM_PRIVATE_H__
#define __TOTEM_PRIVATE_H__

#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>

#include "totem-remote.h"
#include "totem-scrsaver.h"
#include "egg-recent-model.h"
#include "egg-recent-view-gtk.h"
#include "totem-playlist.h"
#include "bacon-message-connection.h"
#include "bacon-video-widget.h"
#include "totem-skipto.h"

#define totem_signal_block_by_data(obj, data) (g_signal_handlers_block_matched (obj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, data))
#define totem_signal_unblock_by_data(obj, data) (g_signal_handlers_unblock_matched (obj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, data))

#define totem_set_sensitivity(xml, name, state)		\
	{							\
		GtkWidget *widget;				\
		widget = glade_xml_get_widget (xml, name);	\
		gtk_widget_set_sensitive (widget, state);	\
	}
#define totem_main_set_sensitivity(name, state) totem_set_sensitivity (totem->xml, name, state)
#define totem_popup_set_sensitivity(name, state) totem_set_sensitivity (totem->xml_popup, name, state)

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
	GladeXML *xml_popup;
	GtkWidget *win;
	BaconVideoWidget *bvw;
	GtkWidget *prefs;
	GtkWidget *statusbar;
	GtkWidget *sidebar;
	gboolean sidebar_shown;

	/* Separate Dialogs */
	GtkWidget *properties;
	TotemSkipto *skipto;
	GtkWidget *about;

	/* Play/Pause */
	GtkWidget *pp_button;
	/* fullscreen Play/Pause */
	GtkWidget *fs_pp_button;
	GtkTooltips *tooltip;

	/* Seek */
	GtkWidget *seek;
	GtkAdjustment *seekadj;
	gboolean seek_lock;
	gboolean seekable;

	/* Volume */
	GtkWidget *volume;
	gboolean vol_lock;
	gboolean vol_fs_lock;
	gfloat prev_volume;
	int volume_first_time;

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

	/* Monitor for playlist unmounts and drives/volumes monitoring */
	GnomeVFSVolumeMonitor *monitor;
	gboolean drives_changed;

	/* session */
	const char *argv0;
	gint64 seek_to;
	guint index;

	/* other */
	char *mrl;
	TotemPlaylist *playlist;
	GConfClient *gc;
	TotemRemote *remote;
	BaconMessageConnection *conn;
	TotemStates state;
};

GtkWidget *totem_volume_create (void);

#endif /* __TOTEM_PRIVATE_H__ */
