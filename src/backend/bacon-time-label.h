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

#ifndef BACON_TIME_LABEL_H
#define BACON_TIME_LABEL_H

#include <gtk/gtk.h>

#define BACON_TYPE_TIME_LABEL            (bacon_time_label_get_type ())
#define BACON_TIME_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), BACON_TYPE_TIME_LABEL, BaconTimeLabel))
#define BACON_TIME_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), BACON_TYPE_TIME_LABEL, BaconTimeLabelClass))
#define BACON_IS_TIME_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BACON_TYPE_TIME_LABEL))
#define BACON_IS_TIME_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), BACON_TYPE_TIME_LABEL))

typedef struct BaconTimeLabel	      BaconTimeLabel;
typedef struct BaconTimeLabelClass    BaconTimeLabelClass;
typedef struct _BaconTimeLabelPrivate BaconTimeLabelPrivate;

struct BaconTimeLabel {
	GtkLabel parent;
	BaconTimeLabelPrivate *priv;
};

struct BaconTimeLabelClass {
	GtkLabelClass parent_class;
};

G_MODULE_EXPORT GType bacon_time_label_get_type (void);
GtkWidget *bacon_time_label_new                 (void);
void       bacon_time_label_set_time            (BaconTimeLabel *label,
                                                 gint64          time,
                                                 gint64          length);
void
bacon_time_label_set_remaining                  (BaconTimeLabel *label,
                                                 gboolean        remaining);

#endif /* BACON_TIME_LABEL_H */
