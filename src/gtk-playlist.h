/* gtk-playlist.h: Simple playlist dialog

   Copyright (C) 2002 Bastien Nocera <hadess@hadess.net>

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

#ifndef GTK_PLAYLIST_H
#define GTK_PLAYLIST_H

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define GTK_TYPE_PLAYLIST            (gtk_playlist_get_type ())
#define GTK_PLAYLIST(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_PLAYLIST, GtkPlaylist))
#define GTK_PLAYLIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_PLAYLIST, GtkPlaylistClass))
#define GTK_IS_PLAYLIST(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_PLAYLIST))
#define GTK_IS_PLAYLIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PLAYLIST))

typedef struct GtkPlaylist	      GtkPlaylist;
typedef struct GtkPlaylistClass	      GtkPlaylistClass;
typedef struct GtkPlaylistPrivate     GtkPlaylistPrivate;

struct GtkPlaylist {
	GtkDialog parent;
	GtkPlaylistPrivate *_priv;
};

struct GtkPlaylistClass {
	GtkDialogClass parent_class;

	void (*changed) (GtkPlaylist *playlist);
	void (*current_removed) (GtkPlaylist *playlist);
};

GtkType    gtk_playlist_get_type (void);
GtkWidget *gtk_playlist_new      (void);

/* The application is responsible for checking that the mrl is correct
 * Handles directories, m3u playlists, and shoutcast playlists
 * @display_name is if you have a preferred display string for the mrl,
 * NULL otherwise
 */
gboolean   gtk_playlist_add_mrl  (GtkPlaylist *playlist, const char *mrl,
		const char *display_name);

/* gtk_playlist_clear doesn't emit the current_removed signal, even if it does
 * because the caller should know what to do after it's done with clearing */
void       gtk_playlist_clear (GtkPlaylist *playlist);
char      *gtk_playlist_get_current_mrl (GtkPlaylist *playlist);

gboolean   gtk_playlist_has_previous_mrl (GtkPlaylist *playlist);
gboolean   gtk_playlist_has_next_mrl (GtkPlaylist *playlist);

void       gtk_playlist_set_previous (GtkPlaylist *playlist);
void       gtk_playlist_set_next (GtkPlaylist *playlist);

gboolean   gtk_playlist_set_title (GtkPlaylist *playlist, const gchar *title);

gboolean   gtk_playlist_set_playing (GtkPlaylist *playlist, gboolean state);

void       gtk_playlist_set_at_start (GtkPlaylist *playlist);

gchar *     gtk_playlist_mrl_to_title (const gchar *mrl);

G_END_DECLS

#endif /* GTK_PLAYLIST_H */
