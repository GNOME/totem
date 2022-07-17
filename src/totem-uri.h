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
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include "totem.h"
#include <gtk/gtk.h>
#include <gio/gio.h>

const char *	totem_dot_dir			(void);
const char *	totem_data_dot_dir		(void);
char *		totem_create_full_path		(const char *path);
GMount *	totem_get_mount_for_media	(const char *uri);
gboolean	totem_playing_dvd		(const char *uri);
gboolean	totem_uri_is_subtitle		(const char *uri);
gboolean	totem_is_special_mrl		(const char *uri);
gboolean	totem_is_block_device		(const char *uri);
void		totem_setup_file_monitoring	(Totem *totem);
void		totem_setup_file_filters	(void);
void		totem_destroy_file_filters	(void);
char *		totem_uri_escape_for_display	(const char *uri);
GSList *	totem_add_files			(GtkWindow *parent,
						 const char *path);
char *		totem_add_subtitle		(GtkWindow *parent,
						 const char *uri);
