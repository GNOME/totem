/* totem-uri.h

   Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>

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

#ifndef TOTEM_URI_H
#define TOTEM_URI_H

#include "totem.h"
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

char*		totem_create_full_path	(const char *path);
gboolean	totem_is_media		(const char *uri);
gboolean	totem_playing_dvd	(const char *uri);
gboolean	totem_is_block_device	(const char *uri);
void		totem_setup_file_monitoring (Totem *totem);
void		totem_setup_file_filters (void);
void		totem_destroy_file_filters (void);
char*		totem_uri_get_subtitle_uri (const char *uri);
char*		totem_uri_escape_for_display (const char *uri);
GSList*		totem_add_files		(GtkWindow *parent,
					 const char *path);

G_END_DECLS

#endif /* TOTEM_URI_H */
