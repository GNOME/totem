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

#include "config.h"

#include "bacon-time-label.h"
#include <glib/gi18n.h>
#include "totem-time-helpers.h"

struct _BaconTimeLabelPrivate {
	gint64 time;
	gint64 length;
	gboolean remaining;
};

G_DEFINE_TYPE (BaconTimeLabel, bacon_time_label, GTK_TYPE_LABEL)
#define BACON_TIME_LABEL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), BACON_TYPE_TIME_LABEL, BaconTimeLabelPrivate))

enum {
	PROP_0,
	PROP_REMAINING
};

static void
bacon_time_label_init (BaconTimeLabel *label)
{
	char *time_string;
	label->priv = G_TYPE_INSTANCE_GET_PRIVATE (label, BACON_TYPE_TIME_LABEL, BaconTimeLabelPrivate);

	time_string = totem_time_to_string (0, FALSE, FALSE);
	gtk_label_set_text (GTK_LABEL (label), time_string);
	g_free (time_string);

	label->priv->time = 0;
	label->priv->length = -1;
	label->priv->remaining = FALSE;
}

GtkWidget *
bacon_time_label_new (void)
{
	return GTK_WIDGET (g_object_new (BACON_TYPE_TIME_LABEL, NULL));
}

static void
bacon_time_label_set_property (GObject      *object,
			       guint         property_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	switch (property_id) {
	case PROP_REMAINING:
		bacon_time_label_set_remaining (BACON_TIME_LABEL (object), g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
bacon_time_label_class_init (BaconTimeLabelClass *klass)
{
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (BaconTimeLabelPrivate));

	object_class = (GObjectClass *) klass;
	object_class->set_property = bacon_time_label_set_property;

	g_object_class_install_property (object_class, PROP_REMAINING,
					 g_param_spec_boolean ("remaining", "Remaining",
							       "Whether to show a remaining time.", FALSE,
							       G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}

static void
update_label_text (BaconTimeLabel *label)
{
	char *label_str;
	gboolean force_hour = FALSE;
	gint64 _time, length;

	_time = label->priv->time;
	length = label->priv->length;

	if (length > 60 * 60 * 1000)
		force_hour = TRUE;

	if (length <= 0) {
		if (!label->priv->remaining) {
			label_str = totem_time_to_string (_time, FALSE, force_hour);
		} else {
			/* translators: Unknown remaining time */
			label_str = g_strdup (_("--:--"));
		}
	} else {
		if (!label->priv->remaining) {
			/* Elapsed */
			label_str = totem_time_to_string (_time, FALSE, force_hour);
		} else {
			/* Remaining */
			label_str = totem_time_to_string (length - _time, TRUE, force_hour);
		}
	}

	gtk_label_set_text (GTK_LABEL (label), label_str);
	g_free (label_str);
}

void
bacon_time_label_set_time (BaconTimeLabel *label,
			   gint64          _time,
			   gint64          length)
{
	g_return_if_fail (BACON_IS_TIME_LABEL (label));

	if (!label->priv->remaining) {
		if (_time / 1000 == label->priv->time / 1000)
			return;
	} else {
		if (_time / 1000 == label->priv->time / 1000 &&
		    length / 1000 == label->priv->length / 1000)
		return;
	}

	label->priv->time = _time;
	label->priv->length = length;

	update_label_text (label);
}

void
bacon_time_label_set_remaining (BaconTimeLabel *label,
				gboolean        remaining)
{
	g_return_if_fail (BACON_IS_TIME_LABEL (label));

	label->priv->remaining = remaining;
	update_label_text (label);
}
