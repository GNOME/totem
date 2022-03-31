/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#pragma once

#include <glib-object.h>

#include <gtk/gtk.h>

#define TOTEM_TYPE_SEARCH_ENTRY totem_search_entry_get_type()
G_DECLARE_FINAL_TYPE(TotemSearchEntry, totem_search_entry, TOTEM, SEARCH_ENTRY, GtkBox)

GType totem_search_entry_get_type              (void) G_GNUC_CONST;
TotemSearchEntry *totem_search_entry_new       (void);
void totem_search_entry_add_source             (TotemSearchEntry *entry,
						const gchar      *id,
						const gchar      *label,
						int               priority);
void totem_search_entry_remove_source          (TotemSearchEntry *self,
						const gchar      *id);

const char *totem_search_entry_get_selected_id (TotemSearchEntry *self);
gboolean    totem_search_entry_set_selected_id (TotemSearchEntry *self,
						const char       *id);

const char *totem_search_entry_get_text        (TotemSearchEntry *self);
GtkEntry   *totem_search_entry_get_entry       (TotemSearchEntry *self);
