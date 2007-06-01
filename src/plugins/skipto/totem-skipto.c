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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include "totem-skipto.h"
#include "totem.h"
#include "totem-plugin.h"
#include "totem-uri.h"
#include "video-utils.h"
#include "bacon-video-widget.h"

static GObjectClass *parent_class = NULL;
static void totem_skipto_class_init	(TotemSkiptoClass *class);
static void totem_skipto_init		(TotemSkipto *ggo);
static void totem_skipto_finalize	(GObject *object);

struct TotemSkiptoPrivate {
	GladeXML *xml;
	GtkWidget *label;
	GtkWidget *spinbutton;
	gint64 time;
	Totem *totem;
};

TOTEM_PLUGIN_DEFINE_TYPE (TotemSkipto, totem_skipto, GTK_TYPE_DIALOG)

static void
totem_skipto_class_init (TotemSkiptoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (TotemSkiptoPrivate));

	object_class->finalize = totem_skipto_finalize;
}

static void
totem_skipto_response_cb (GtkDialog *dialog, gint response_id, gpointer data)
{
	TotemSkipto *skipto;

	skipto = TOTEM_SKIPTO (dialog);
	gtk_spin_button_update (GTK_SPIN_BUTTON (skipto->priv->spinbutton));
}

static void
totem_skipto_init (TotemSkipto *skipto)
{
	skipto->priv = G_TYPE_INSTANCE_GET_PRIVATE (skipto, TOTEM_TYPE_SKIPTO, TotemSkiptoPrivate);

	g_signal_connect (skipto, "response",
				G_CALLBACK (totem_skipto_response_cb), NULL);
}

static void
totem_skipto_finalize (GObject *object)
{
	g_return_if_fail (object != NULL);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
totem_skipto_update_range (TotemSkipto *skipto, gint64 time)
{
	g_return_if_fail (TOTEM_IS_SKIPTO (skipto));

	if (time == skipto->priv->time)
		return;

	gtk_spin_button_set_range (GTK_SPIN_BUTTON (skipto->priv->spinbutton),
			0, (gdouble) time / 1000);
	skipto->priv->time = time;
}

gint64
totem_skipto_get_range (TotemSkipto *skipto)
{
	gint64 time;

	g_return_val_if_fail (TOTEM_IS_SKIPTO (skipto), 0);

	time = gtk_spin_button_get_value (GTK_SPIN_BUTTON (skipto->priv->spinbutton)) * 1000;

	return time;
}

void
totem_skipto_set_seekable (TotemSkipto *skipto, gboolean seekable)
{
	g_return_if_fail (TOTEM_IS_SKIPTO (skipto));

	gtk_dialog_set_response_sensitive (GTK_DIALOG (skipto),
			GTK_RESPONSE_OK, seekable);
}

void
totem_skipto_set_current (TotemSkipto *skipto, gint64 time)
{
	g_return_if_fail (TOTEM_IS_SKIPTO (skipto));

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (skipto->priv->spinbutton),
			(gdouble) (time / 1000));
}

static void
spin_button_activate_cb (GtkEntry *entry, TotemSkipto *skipto)
{
	gtk_dialog_response (GTK_DIALOG (skipto), GTK_RESPONSE_OK);
}

static void
spin_button_value_changed_cb (GtkSpinButton *spinbutton, TotemSkipto *skipto)
{
	int sec;
	char *str;

	sec = (int) gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinbutton));
	str = totem_time_to_string_text (sec * 1000);
	gtk_label_set_text (GTK_LABEL (skipto->priv->label), str);
	g_free (str);
}

GtkWidget*
totem_skipto_new (const char *glade_filename, Totem *totem)
{
	TotemSkipto *skipto;
	GtkWidget *container;

	g_return_val_if_fail (glade_filename != NULL, NULL);

	skipto = TOTEM_SKIPTO (g_object_new (TOTEM_TYPE_SKIPTO, NULL));

	skipto->priv->totem = totem;
	skipto->priv->xml = glade_xml_new (glade_filename,
					   "tstw_skip_vbox", NULL);

	if (skipto->priv->xml == NULL) {
		g_object_unref (skipto);
		return NULL;
	}
	skipto->priv->label = glade_xml_get_widget
		(skipto->priv->xml, "tstw_position_label");
	skipto->priv->spinbutton = glade_xml_get_widget
		(skipto->priv->xml, "tstw_skip_spinbutton");

	gtk_window_set_title (GTK_WINDOW (skipto), _("Skip to"));
	gtk_dialog_set_has_separator (GTK_DIALOG (skipto), FALSE);
	gtk_dialog_add_buttons (GTK_DIALOG (skipto),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);

	/* Skipto dialog */
	g_signal_connect (G_OBJECT (skipto->priv->spinbutton), "value-changed",
			G_CALLBACK (spin_button_value_changed_cb), skipto);
	g_signal_connect_after (G_OBJECT (skipto->priv->spinbutton), "activate",
			G_CALLBACK (spin_button_activate_cb), skipto);
	g_signal_connect (G_OBJECT (skipto), "delete-event",
			G_CALLBACK (gtk_widget_destroy), skipto);

	container = glade_xml_get_widget (skipto->priv->xml,
			"tstw_skip_vbox");
	gtk_container_set_border_width (GTK_CONTAINER (skipto), 5);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (skipto)->vbox),
			container,
			TRUE,       /* expand */
			TRUE,       /* fill */
			0);         /* padding */

	gtk_window_set_transient_for (GTK_WINDOW (skipto),
			totem_get_main_window (totem));

	gtk_widget_show_all (GTK_WIDGET (skipto));

	return GTK_WIDGET (skipto);
}
