/* 
 * Copyright (C) 2001,2002,2003 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>
#include "totem-private.h"
#include "bacon-cd-selection.h"

#include "debug.h"

static void on_preferences1_activate (GtkButton *button, gpointer user_data);
static void on_checkbutton1_toggled (GtkToggleButton *togglebutton,
		gpointer user_data);
static void on_checkbutton2_toggled (GtkToggleButton *togglebutton,
		gpointer user_data);
static void on_combo_entry1_changed (BaconCdSelection *bcs, char *device,
		gpointer user_data);
static void auto_resize_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data);
static void show_vfx_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data);
static void mediadev_changed_cb (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, gpointer user_data);
GtkWidget * bacon_cd_selection_create (void);
void totem_setup_preferences (Totem *totem);

