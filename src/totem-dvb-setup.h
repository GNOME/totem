/* totem-dvb-setup.h

   Copyright (C) 2004-2005 Bastien Nocera <hadess@hadess.net>

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

#ifndef TOTEM_DVB_SETUP_H
#define TOTEM_DVB_SETUP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

enum {
	TOTEM_DVB_SETUP_MISSING,
	TOTEM_DVB_SETUP_STARTED_OK,
	TOTEM_DVB_SETUP_CRASHED,
	TOTEM_DVB_SETUP_FAILURE,
	TOTEM_DVB_SETUP_SUCCESS
};

typedef void (*TotemDvbSetupResultFunc) (int result, const char *device, gpointer user_data);

int totem_dvb_setup_device (const char *device,
			    GtkWindow *parent,
			    TotemDvbSetupResultFunc func,
			    gpointer user_data);

G_END_DECLS

#endif /* TOTEM_DVB_SETUP_H */
