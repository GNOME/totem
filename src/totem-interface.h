/* totem-interface.h

   Copyright (C) 2005,2007 Bastien Nocera <hadess@hadess.net>

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

#ifndef TOTEM_INTERFACE_H
#define TOTEM_INTERFACE_H

#include <gtk/gtk.h>
#include "totem.h"

G_BEGIN_DECLS

char		*totem_interface_get_full_path	(const char *name);
GtkBuilder	*totem_interface_load		(const char *name,
						 gboolean fatal,
						 GtkWindow *parent,
						 gpointer user_data);
GtkBuilder      *totem_interface_load_with_full_path (const char *filename, 
						      gboolean fatal, 
						      GtkWindow *parent,
						      gpointer user_data);
void		 totem_interface_error		(const char *title,
						 const char *reason,
						 GtkWindow *parent);
void		 totem_interface_error_blocking	(const char *title,
						 const char *reason,
						 GtkWindow *parent);
GtkWidget *	 totem_interface_create_header_button (GtkWidget  *header,
						       GtkWidget  *button,
						       const char *icon_name,
						       GtkPackType pack_type);

G_END_DECLS

#endif /* TOTEM_INTERFACE_H */
