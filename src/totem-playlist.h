/* totem-playlist.h: Simple playlist dialog

   Copyright (C) 2002, 2003, 2004, 2005 Bastien Nocera <hadess@hadess.net>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#ifndef TOTEM_PLAYLIST_H
#define TOTEM_PLAYLIST_H

#include <gtk/gtkvbox.h>
#include <libgnomevfs/gnome-vfs-volume.h>

#include "plparse/totem-pl-parser.h"

G_BEGIN_DECLS

#define TOTEM_TYPE_PLAYLIST            (totem_playlist_get_type ())
#define TOTEM_PLAYLIST(obj)            (GTK_CHECK_CAST ((obj), TOTEM_TYPE_PLAYLIST, TotemPlaylist))
#define TOTEM_PLAYLIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_PLAYLIST, TotemPlaylistClass))
#define TOTEM_IS_PLAYLIST(obj)         (GTK_CHECK_TYPE ((obj), TOTEM_TYPE_PLAYLIST))
#define TOTEM_IS_PLAYLIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_PLAYLIST))

typedef enum {
	TOTEM_PLAYLIST_STATUS_NONE,
	TOTEM_PLAYLIST_STATUS_PLAYING,
	TOTEM_PLAYLIST_STATUS_PAUSED
} TotemPlaylistStatus;

typedef enum {
	TOTEM_PLAYLIST_DIRECTION_NEXT,
	TOTEM_PLAYLIST_DIRECTION_PREVIOUS
} TotemPlaylistDirection;

typedef struct TotemPlaylist	       TotemPlaylist;
typedef struct TotemPlaylistClass      TotemPlaylistClass;
typedef struct TotemPlaylistPrivate    TotemPlaylistPrivate;

typedef void (*TotemPlaylistForeachFunc) (TotemPlaylist *playlist,
					  const gchar   *filename,
					  const gchar   *uri,
					  gpointer       user_data);

struct TotemPlaylist {
	GtkVBox parent;
	TotemPlaylistPrivate *_priv;
};

struct TotemPlaylistClass {
	GtkVBoxClass parent_class;

	void (*changed) (TotemPlaylist *playlist);
	void (*item_activated) (TotemPlaylist *playlist);
	void (*active_name_changed) (TotemPlaylist *playlist);
	void (*current_removed) (TotemPlaylist *playlist);
	void (*repeat_toggled) (TotemPlaylist *playlist, gboolean repeat);
	void (*shuffle_toggled) (TotemPlaylist *playlist, gboolean toggled);
	void (*item_added) (TotemPlaylist *playlist, const gchar *filename, const gchar *uri);
	void (*item_removed) (TotemPlaylist *playlist, const gchar *filename, const gchar *uri);
};

GtkType    totem_playlist_get_type (void);
GtkWidget *totem_playlist_new      (void);

/* The application is responsible for checking that the mrl is correct
 * @display_name is if you have a preferred display string for the mrl,
 * NULL otherwise
 */
gboolean totem_playlist_add_mrl  (TotemPlaylist *playlist,
				  const char *mrl,
				  const char *display_name);
gboolean totem_playlist_add_mrl_with_cursor (TotemPlaylist *playlist,
					     const char *mrl,
					     const char *display_name);

void totem_playlist_save_current_playlist (TotemPlaylist *playlist,
					   const char *output);
void totem_playlist_save_current_playlist_ext (TotemPlaylist *playlist,
					   const char *output, TotemPlParserType type);

/* totem_playlist_clear doesn't emit the current_removed signal, even if it does
 * because the caller should know what to do after it's done with clearing */
gboolean   totem_playlist_clear (TotemPlaylist *playlist);
void       totem_playlist_clear_with_gnome_vfs_volume (TotemPlaylist *playlist,
						       GnomeVFSVolume *volume);
char      *totem_playlist_get_current_mrl (TotemPlaylist *playlist);
char      *totem_playlist_get_current_title (TotemPlaylist *playlist,
					     gboolean *custom);
char      *totem_playlist_get_title (TotemPlaylist *playlist,
				     guint index);

gboolean   totem_playlist_get_current_metadata (TotemPlaylist *playlist,
						char         **artist,
						char         **title,
						char         **album);
gboolean   totem_playlist_set_title (TotemPlaylist *playlist,
				     const char *title,
				     gboolean force);

#define    totem_playlist_has_direction(playlist, direction) (direction == TOTEM_PLAYLIST_DIRECTION_NEXT ? totem_playlist_has_next_mrl (playlist) : totem_playlist_has_previous_mrl (playlist))
gboolean   totem_playlist_has_previous_mrl (TotemPlaylist *playlist);
gboolean   totem_playlist_has_next_mrl (TotemPlaylist *playlist);

#define    totem_playlist_set_direction(playlist, direction) (direction == TOTEM_PLAYLIST_DIRECTION_NEXT ? totem_playlist_set_next (playlist) : totem_playlist_set_previous (playlist))
void       totem_playlist_set_previous (TotemPlaylist *playlist);
void       totem_playlist_set_next (TotemPlaylist *playlist);

gboolean   totem_playlist_get_repeat (TotemPlaylist *playlist);
void       totem_playlist_set_repeat (TotemPlaylist *playlist, gboolean repeat);

gboolean   totem_playlist_get_shuffle (TotemPlaylist *playlist);
void       totem_playlist_set_shuffle (TotemPlaylist *playlist,
				       gboolean shuffle);

gboolean   totem_playlist_set_playing (TotemPlaylist *playlist, TotemPlaylistStatus state);
TotemPlaylistStatus totem_playlist_get_playing (TotemPlaylist *playlist);

void       totem_playlist_set_at_start (TotemPlaylist *playlist);
void       totem_playlist_set_at_end (TotemPlaylist *playlist);

guint      totem_playlist_get_current (TotemPlaylist *playlist);
guint      totem_playlist_get_last (TotemPlaylist *playlist);
void       totem_playlist_set_current (TotemPlaylist *playlist, guint index);

void       totem_playlist_foreach (TotemPlaylist *playlist,
				   TotemPlaylistForeachFunc callback,
				   gpointer user_data);

G_END_DECLS

#endif /* TOTEM_PLAYLIST_H */
