/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-ellipsizing-label.h: Subclass of GtkLabel that ellipsizes the text.

   Copyright (C) 2001 Eazel, Inc.

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

   Author: John Sullivan <sullivan@eazel.com>,
 */

#ifndef GTK_PLAYLIST_H
#define GTK_PLAYLIST_H

#include <gtk/gtkdialog.h>

#define GTK_TYPE_PLAYLIST            (gtk_playlist_get_type ())
#define GTK_PLAYLIST(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_PLAYLIST, GtkPlaylist))
#define GTK_PLAYLIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_PLAYLIST, GtkPlaylistClass))
#define GTK_IS_PLAYLIST(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_PLAYLIST))
#define GTK_IS_PLAYLIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PLAYLIST))

typedef struct GtkPlaylist	      GtkPlaylist;
typedef struct GtkPlaylistClass	      GtkPlaylistClass;
typedef struct GtkPlaylistDetails     GtkPlaylistDetails;

struct GtkPlaylist {
	GtkDialog parent;
	GtkPlaylistDetails *details;
};

struct GtkPlaylistClass {
	GtkDialogClass parent_class;
};

GtkType    gtk_playlist_get_type (void);
GtkWidget *gtk_playlist_new      (GtkWindow *parent);

/* The application is responsible for checking that the mrl is correct */
gboolean   gtk_playlist_add_mrl  (GtkPlaylist *playlist, char *mrl);
char      *gtk_playlist_get_current_mrl (GtkPlaylist *playlist);

gboolean   gtk_playlist_has_previous_mrl (GtkPlaylist *playlist);
gboolean   gtk_playlist_has_next_mrl (GtkPlaylist *playlist);

#endif /* GTK_PLAYLIST_H */
