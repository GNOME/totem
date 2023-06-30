/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-3-or-later
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
