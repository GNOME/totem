/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 * Author: Bastien Nocera <hadess@hadess.net>, Philip Withnall <philip@tecnocode.co.uk>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "totem-dirs.h"
#include "totem-skipto.h"
#include "totem-uri.h"
#include "totem-time-entry.h"
#include "backend/bacon-video-widget.h"

static void totem_skipto_dispose	(GObject *object);

struct TotemSkiptoPrivate {
	GtkBuilder *xml;
	GtkWidget *time_entry;
	GtkLabel *seconds_label;
	GtkAdjustment *adj;
	gint64 time;
	Totem *totem;
	gpointer class_ref;
};

G_DEFINE_TYPE_WITH_PRIVATE (TotemSkipto, totem_skipto, GTK_TYPE_DIALOG)

#define WID(x) (gtk_builder_get_object (skipto->priv->xml, x))

static void
totem_skipto_class_init (TotemSkiptoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = totem_skipto_dispose;
}

static void
totem_skipto_response_cb (GtkDialog *dialog, gint response_id, gpointer data)
{
	TotemSkipto *skipto;

	skipto = TOTEM_SKIPTO (dialog);
	gtk_spin_button_update (GTK_SPIN_BUTTON (skipto->priv->time_entry));
}

static void
totem_skipto_init (TotemSkipto *skipto)
{
	skipto->priv = totem_skipto_get_instance_private (skipto);

	gtk_dialog_set_default_response (GTK_DIALOG (skipto), GTK_RESPONSE_OK);
	g_signal_connect (skipto, "response",
				G_CALLBACK (totem_skipto_response_cb), NULL);
}

static void
totem_skipto_dispose (GObject *object)
{
	TotemSkipto *skipto;

	skipto = TOTEM_SKIPTO (object);
	if (skipto->priv) {
		g_clear_object (&skipto->priv->xml);
		skipto->priv->adj = NULL;
		skipto->priv->time_entry = NULL;
		skipto->priv->seconds_label = NULL;

		if (skipto->priv->class_ref != NULL) {
			g_type_class_unref (skipto->priv->class_ref);
			skipto->priv->class_ref = NULL;
		}
	}

	G_OBJECT_CLASS (totem_skipto_parent_class)->dispose (object);
}

void
totem_skipto_update_range (TotemSkipto *skipto, gint64 _time)
{
	g_return_if_fail (TOTEM_IS_SKIPTO (skipto));

	if (_time == skipto->priv->time)
		return;

	gtk_spin_button_set_range (GTK_SPIN_BUTTON (skipto->priv->time_entry),
			0, (gdouble) _time / 1000);
	skipto->priv->time = _time;
}

gint64
totem_skipto_get_range (TotemSkipto *skipto)
{
	gint64 _time;

	g_return_val_if_fail (TOTEM_IS_SKIPTO (skipto), 0);

	_time = gtk_spin_button_get_value (GTK_SPIN_BUTTON (skipto->priv->time_entry)) * 1000;

	return _time;
}

void
totem_skipto_set_seekable (TotemSkipto *skipto, gboolean seekable)
{
	g_return_if_fail (TOTEM_IS_SKIPTO (skipto));

	gtk_dialog_set_response_sensitive (GTK_DIALOG (skipto),
			GTK_RESPONSE_OK, seekable);
}

void
totem_skipto_set_current (TotemSkipto *skipto, gint64 _time)
{
	g_return_if_fail (TOTEM_IS_SKIPTO (skipto));

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (skipto->priv->time_entry),
			(gdouble) (_time / 1000));
}

static void
time_entry_activate_cb (GtkEntry *entry, TotemSkipto *skipto)
{
	gtk_dialog_response (GTK_DIALOG (skipto), GTK_RESPONSE_OK);
}

static void
tstw_adjustment_value_changed_cb (GtkAdjustment *adjustment, TotemSkipto *skipto)
{
	int value;

	g_return_if_fail (TOTEM_IS_SKIPTO (skipto));

	if (skipto->priv->seconds_label == NULL)
		return;

	value = (int) gtk_adjustment_get_value (adjustment);

	/* Update the "seconds" label so that it always has the correct singular/plural form */
	/* Translators: label for the seconds selector in the "Skip to" dialogue */
	gtk_label_set_label (skipto->priv->seconds_label, ngettext ("second", "seconds", value));
}

GtkWidget *
totem_skipto_new (TotemObject *totem)
{
	TotemSkipto *skipto;
	GtkWidget *container;
	guint label_length;

	skipto = TOTEM_SKIPTO (g_object_new (TOTEM_TYPE_SKIPTO, NULL));
	skipto->priv->class_ref = g_type_class_ref (TOTEM_TYPE_TIME_ENTRY);

	skipto->priv->totem = totem;
	skipto->priv->xml = totem_plugin_load_interface ("skipto",
							 "skipto.ui", TRUE,
							 NULL, skipto);

	if (skipto->priv->xml == NULL) {
		g_object_unref (skipto);
		return NULL;
	}

	skipto->priv->adj = GTK_ADJUSTMENT (WID("tstw_skip_adjustment"));
	g_signal_connect (skipto->priv->adj, "value-changed",
			  G_CALLBACK (tstw_adjustment_value_changed_cb), skipto);

	skipto->priv->time_entry = GTK_WIDGET (WID ("tstw_skip_time_entry"));
	g_signal_connect (G_OBJECT (skipto->priv->time_entry), "activate",
			  G_CALLBACK (time_entry_activate_cb), skipto);

	skipto->priv->seconds_label = GTK_LABEL (WID ("tstw_seconds_label"));

	/* Fix the label width at the maximum necessary for the plural labels, to prevent it changing size when we change the spinner value */
	/* Translators: you should translate this string to a number (written in digits) which corresponds to the longer character length of the
	 * translations for "second" and "seconds", as translated elsewhere in this file. For example, in English, "second" is 6 characters long and
	 * "seconds" is 7 characters long, so this string should be translated to "7". See: bgo#639398 */
	label_length = strtoul (C_("Skip To label length", "7"), NULL, 10);
	gtk_label_set_width_chars (skipto->priv->seconds_label, label_length);

	/* Set the initial "seconds" label */
	tstw_adjustment_value_changed_cb (skipto->priv->adj, skipto);

	gtk_window_set_title (GTK_WINDOW (skipto), _("Skip To"));
	gtk_dialog_add_buttons (GTK_DIALOG (skipto),
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Skip To"), GTK_RESPONSE_OK,
				NULL);

	/* Skipto dialog */
	g_signal_connect (G_OBJECT (skipto), "delete-event",
			  G_CALLBACK (gtk_widget_destroy), skipto);

	container = GTK_WIDGET (gtk_builder_get_object (skipto->priv->xml,
				"tstw_skip_vbox"));
	gtk_container_set_border_width (GTK_CONTAINER (skipto), 5);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (skipto))),
			    container,
			    TRUE,       /* expand */
			    TRUE,       /* fill */
			    0);         /* padding */

	gtk_window_set_transient_for (GTK_WINDOW (skipto),
				      totem_object_get_main_window (totem));

	gtk_widget_show_all (GTK_WIDGET (skipto));

	return GTK_WIDGET (skipto);
}
