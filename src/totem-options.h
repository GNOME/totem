/* totem-options.h

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

#ifndef TOTEM_OPTIONS_H
#define TOTEM_OPTIONS_H

#include <popt.h>

#include "totem.h"
#include "bacon-message-connection.h"

G_BEGIN_DECLS

poptOption totem_options_get_options (void);

void totem_options_process_early (GConfClient *gc, int argc, char **argv);
void totem_options_process_late (Totem *totem, int *argc, char ***argv);

void totem_options_process_for_server (BaconMessageConnection *conn,
		int argc, char **argv);

G_END_DECLS

#endif /* TOTEM_SKIPTO_H */
