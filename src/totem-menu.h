/* totem-menu.h

   Copyright (C) 2004-2005 Bastien Nocera <hadess@hadess.net>

   SPDX-License-Identifier: GPL-3-or-later

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
