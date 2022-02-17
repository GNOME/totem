/*
 * Time label widget
 *
 * Copyright (C) 2004-2013 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <gtk/gtk.h>

#define BACON_TYPE_TIME_LABEL            (bacon_time_label_get_type ())
G_DECLARE_FINAL_TYPE(BaconTimeLabel, bacon_time_label, BACON, TIME_LABEL, GtkLabel)

G_MODULE_EXPORT GType bacon_time_label_get_type (void);
GtkWidget *bacon_time_label_new                 (void);
void       bacon_time_label_set_time            (BaconTimeLabel *label,
                                                 gint64          time_msecs,
                                                 gint64          length_msecs);
void       bacon_time_label_reset               (BaconTimeLabel *label);

void       bacon_time_label_set_remaining       (BaconTimeLabel *label,
                                                 gboolean        remaining);
void       bacon_time_label_set_show_msecs      (BaconTimeLabel *label,
                                                 gboolean        show_msecs);
