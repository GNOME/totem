/* totem-menu.h

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

#pragma once

#include "totem.h"
#include "bacon-video-widget.h"

void totem_app_menu_setup (Totem *totem);
void totem_app_actions_setup (Totem *totem);

void totem_subtitles_menu_update (Totem *totem);
void totem_languages_menu_update (Totem *totem);

/* For test use only */
typedef struct {
	char *label;
	int id;
} MenuItem;

GList *bvw_lang_info_to_menu_labels (GList        *langs,
				     BvwTrackType  track_type);
void free_menu_item (MenuItem *item);
