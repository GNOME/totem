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

struct _BaconTimeLabel {
	GtkLabel parent;

	gint64 time;
	gint64 length;
	gboolean remaining;
	gboolean show_msecs;
};

G_DEFINE_TYPE(BaconTimeLabel, bacon_time_label, GTK_TYPE_LABEL)

enum {
	PROP_0,
	PROP_REMAINING,
	PROP_SHOW_MSECS
};

static void
bacon_time_label_init (BaconTimeLabel *label)
{
	char *time_string;
	PangoAttrList *attrs;

	time_string = totem_time_to_string (0, TOTEM_TIME_FLAG_NONE);
	gtk_label_set_text (GTK_LABEL (label), time_string);
	g_free (time_string);

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_font_features_new ("tnum=1"));
	gtk_label_set_attributes (GTK_LABEL (label), attrs);
	pango_attr_list_unref (attrs);

	label->time = 0;
	label->length = -1;
	label->remaining = FALSE;
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
	case PROP_SHOW_MSECS:
		bacon_time_label_set_show_msecs (BACON_TIME_LABEL (object), g_value_get_boolean (value));
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

	object_class = (GObjectClass *) klass;
	object_class->set_property = bacon_time_label_set_property;

	g_object_class_install_property (object_class, PROP_REMAINING,
					 g_param_spec_boolean ("remaining", "Remaining",
							       "Whether to show a remaining time.", FALSE,
							       G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class, PROP_SHOW_MSECS,
					 g_param_spec_boolean ("show-msecs", "Show milliseconds",
							       "Whether to show milliseconds.", FALSE,
							       G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}

static void
update_label_text (BaconTimeLabel *label)
{
	g_autofree char *label_str = NULL;
	gint64 _time, length;
	TotemTimeFlag flags;

	_time = label->time;
	length = label->length;

	flags = label->remaining ? TOTEM_TIME_FLAG_REMAINING : TOTEM_TIME_FLAG_NONE;
	if (length > 60 * 60 * 1000)
		flags |= TOTEM_TIME_FLAG_FORCE_HOUR;
	flags |= label->show_msecs ? TOTEM_TIME_FLAG_MSECS : TOTEM_TIME_FLAG_NONE;

	if (length <= 0 || _time > length)
		label_str = totem_time_to_string (label->remaining ? -1 : _time, flags);
	else
		label_str = totem_time_to_string (label->remaining ? length - _time : _time, flags);

	gtk_label_set_text (GTK_LABEL (label), label_str);
}

void
bacon_time_label_set_time (BaconTimeLabel *label,
			   gint64          _time,
			   gint64          length)
{
	g_return_if_fail (BACON_IS_TIME_LABEL (label));

	if (label->length == -1 &&
	    length == -1)
		return;

	if (!label->show_msecs) {
		if (_time / 1000 == label->time / 1000 &&
		    length / 1000 == label->length / 1000)
			return;
	}

	label->time = _time;
	label->length = length;

	update_label_text (label);
}

void
bacon_time_label_reset (BaconTimeLabel *label)
{
	g_return_if_fail (BACON_IS_TIME_LABEL (label));

	bacon_time_label_set_show_msecs (label, FALSE);
	bacon_time_label_set_time (label, 0, 0);
}

void
bacon_time_label_set_remaining (BaconTimeLabel *label,
				gboolean        remaining)
{
	g_return_if_fail (BACON_IS_TIME_LABEL (label));

	label->remaining = remaining;
	update_label_text (label);
}

void
bacon_time_label_set_show_msecs (BaconTimeLabel *label,
				 gboolean        show_msecs)
{
	g_return_if_fail (BACON_IS_TIME_LABEL (label));

	if (show_msecs != label->show_msecs) {
		label->show_msecs = show_msecs;
		update_label_text (label);
	}
}
