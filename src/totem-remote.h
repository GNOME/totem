/*
 *  Copyright (C) 2002 James Willcox  <jwillcox@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
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

#include <glib.h>
#include <glib-object.h>

#ifndef __TOTEM_REMOTE_H
#define __TOTEM_REMOTE_H

G_BEGIN_DECLS

#define TOTEM_TYPE_REMOTE         (totem_remote_get_type ())
#define TOTEM_REMOTE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_REMOTE, TotemRemote))
#define TOTEM_REMOTE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_REMOTE, TotemRemoteClass))
#define TOTEM_IS_REMOTE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_REMOTE))
#define TOTEM_IS_REMOTE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_REMOTE))
#define TOTEM_REMOTE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_REMOTE, TotemRemoteClass))

typedef struct _TotemRemote TotemRemote;

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
} TotemRemoteCommand;

#define SHOW_PLAYING_NO_TRACKS "NONE"

typedef struct
{
	GObjectClass parent_class;

	void (* button_pressed) (TotemRemote *remote, TotemRemoteCommand cmd);
} TotemRemoteClass;

GType		 totem_remote_get_type   (void);

TotemRemote	*totem_remote_new (void);

G_END_DECLS

#endif /* __TOTEM_REMOTE_H */
