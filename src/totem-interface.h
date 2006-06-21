/* totem-interface.h

   Copyright (C) 2005 Bastien Nocera <hadess@hadess.net>

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

#ifndef TOTEM_INTERFACE_H
#define TOTEM_INTERFACE_H

#include <glade/glade.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

GdkPixbuf	*totem_interface_load_pixbuf	(const char *name);
char		*totem_interface_get_full_path	(const char *name);
GladeXML	*totem_interface_load		(const char *name,
						 const char *display_name,
						 gboolean fatal,
						 GtkWindow *parent);
GladeXML	*totem_interface_load_with_root (const char *name,
						 const char *root_widget,
						 const char *display_name,
						 gboolean fatal,
						 GtkWindow *parent);
void		 totem_interface_error		(const char *title,
						 const char *reason,
						 GtkWindow *parent);
void		 totem_interface_error_blocking	(const char *title,
						 const char *reason,
						 GtkWindow *parent);
void		 totem_interface_set_transient_for (GtkWindow *window,
						    GtkWindow *parent);

G_END_DECLS

#endif /* TOTEM_INTERFACE_H */
