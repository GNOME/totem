/*
 * Time label widget
 *
 * Copyright (C) 2004-2013 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <gtk/gtk.h>

#define BACON_TYPE_TIME_LABEL            (bacon_time_label_get_type ())
G_DECLARE_FINAL_TYPE(BaconTimeLabel, bacon_time_label, BACON, TIME_LABEL, GtkBin)

G_MODULE_EXPORT GType bacon_time_label_get_type (void);
GtkWidget *bacon_time_label_new                 (void);
const char *bacon_time_label_get_label          (BaconTimeLabel *label);
void       bacon_time_label_set_time            (BaconTimeLabel *label,
                                                 gint64          time_msecs,
                                                 gint64          length_msecs);
void       bacon_time_label_reset               (BaconTimeLabel *label);

void       bacon_time_label_set_remaining       (BaconTimeLabel *label,
                                                 gboolean        remaining);
void       bacon_time_label_set_show_msecs      (BaconTimeLabel *label,
                                                 gboolean        show_msecs);
