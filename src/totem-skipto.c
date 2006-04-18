/* totem-skipto.c

   Copyright (C) 2004 Bastien Nocera

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"
#include "totem-skipto.h"
#include "video-utils.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "debug.h"

struct TotemSkiptoPrivate
{
	GladeXML *xml;
	GtkWidget *label;
	GtkWidget *spinbutton;
	gint64 time;
};

static GtkWidgetClass *parent_class = NULL;

static void totem_skipto_class_init (TotemSkiptoClass *class);
static void totem_skipto_init       (TotemSkipto      *skipto);

G_DEFINE_TYPE(TotemSkipto, totem_skipto, GTK_TYPE_DIALOG)

static void
totem_skipto_init (TotemSkipto *skipto)
{
	skipto->_priv = g_new0 (TotemSkiptoPrivate, 1);
	gtk_container_set_border_width (GTK_CONTAINER (skipto), 5);
}

static void
totem_skipto_finalize (GObject *object)
{
	g_return_if_fail (object != NULL);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

void
totem_skipto_update_range (TotemSkipto *skipto, gint64 time)
{
	g_return_if_fail (TOTEM_IS_SKIPTO (skipto));

	if (time == skipto->_priv->time)
		return;

	gtk_spin_button_set_range (GTK_SPIN_BUTTON (skipto->_priv->spinbutton),
			0, (gdouble) time / 1000);
	skipto->_priv->time = time;
}

gint64
totem_skipto_get_range (TotemSkipto *skipto)
{
	gint64 time;

	g_return_val_if_fail (TOTEM_IS_SKIPTO (skipto), 0);

	time = gtk_spin_button_get_value (GTK_SPIN_BUTTON (skipto->_priv->spinbutton)) * 1000;

	return time;
}

void
totem_skipto_set_seekable (TotemSkipto *skipto, gboolean seekable)
{
	g_return_if_fail (TOTEM_IS_SKIPTO (skipto));

	gtk_dialog_set_response_sensitive (GTK_DIALOG (skipto),
			GTK_RESPONSE_OK, seekable);
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
	gtk_label_set_text (GTK_LABEL (skipto->_priv->label), str);
	g_free (str);
}

GtkWidget*
totem_skipto_new (const char *glade_filename)
{
	TotemSkipto *skipto;
	GtkWidget *container, *item;

	g_return_val_if_fail (glade_filename != NULL, NULL);

	skipto = TOTEM_SKIPTO (g_object_new (GTK_TYPE_SKIPTO, NULL));

	skipto->_priv->xml = glade_xml_new (glade_filename, "tstw_skip_vbox", NULL);
	if (skipto->_priv->xml == NULL)
	{
		totem_skipto_finalize (G_OBJECT (skipto));
		return NULL;
	}
	skipto->_priv->label = glade_xml_get_widget
		(skipto->_priv->xml, "tstw_position_label");
	skipto->_priv->spinbutton = glade_xml_get_widget
		(skipto->_priv->xml, "tstw_skip_spinbutton");

	gtk_window_set_title (GTK_WINDOW (skipto), _("Skip to"));
	gtk_dialog_set_has_separator (GTK_DIALOG (skipto), FALSE);
	gtk_dialog_add_buttons (GTK_DIALOG (skipto),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);

	/* Skipto dialog */
	item = glade_xml_get_widget (skipto->_priv->xml,
			"totem_skipto_window");
	g_signal_connect (G_OBJECT (skipto->_priv->spinbutton), "value-changed",
			G_CALLBACK (spin_button_value_changed_cb), skipto);
	g_signal_connect (G_OBJECT (skipto->_priv->spinbutton), "activate",
			G_CALLBACK (spin_button_activate_cb), skipto);

	container = glade_xml_get_widget (skipto->_priv->xml,
			"tstw_skip_vbox");
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (skipto)->vbox),
			container,
			TRUE,       /* expand */
			TRUE,       /* fill */
			0);         /* padding */

	gtk_widget_show_all (GTK_DIALOG (skipto)->vbox);

	return GTK_WIDGET (skipto);
}

static void
totem_skipto_class_init (TotemSkiptoClass *klass)
{
	parent_class = gtk_type_class (gtk_dialog_get_type ());

	G_OBJECT_CLASS (klass)->finalize = totem_skipto_finalize;
}

