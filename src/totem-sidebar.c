/* totem-sidebar.c

   Copyright (C) 2004-2005 Bastien Nocera

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

#include <glib/gi18n.h>

#include "totem.h"
#include "totem-sidebar.h"
#include "totem-private.h"
#include "ev-sidebar.h"

void
totem_sidebar_toggle (Totem *totem)
{
	GtkWidget *toggle;
	gboolean state;

	toggle = glade_xml_get_widget (totem->xml, "tmw_sidebar_button");

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), !state);
}

static void
on_sidebar_button_toggled (GtkToggleButton *button, Totem *totem)
{
	GtkWidget *item;
	gboolean state;

	state = gtk_toggle_button_get_active (button);
	if (state != FALSE)
		gtk_widget_show (GTK_WIDGET (totem->sidebar));
	else
		gtk_widget_hide (GTK_WIDGET (totem->sidebar));

	item = glade_xml_get_widget (totem->xml, "tmw_sidebar_menu_item");
	totem_signal_block_by_data (G_OBJECT (item), totem);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), state);
	totem_signal_unblock_by_data (G_OBJECT (item), totem);

	gconf_client_set_bool (totem->gc,
				GCONF_PREFIX"/sidebar_shown",
				state,
				NULL);
	totem->sidebar_shown = state;
}

static void
on_show_sidebar1_activate (GtkButton *button, Totem *totem)
{
	totem_sidebar_toggle (totem);
}

static void
toggle_sidebar_from_sidebar (GtkWidget *playlist, Totem *totem)
{
	GtkWidget *button;

	button = glade_xml_get_widget (totem->xml, "tmw_sidebar_button");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}

gboolean
totem_sidebar_is_visible (Totem *totem)
{
	return totem->sidebar_shown;
}

void
totem_sidebar_setup (Totem *totem)
{
	GtkWidget *item, *item2;
	gboolean visible;

	item = glade_xml_get_widget (totem->xml, "tmw_main_pane");
	totem->sidebar = ev_sidebar_new ();
	ev_sidebar_add_page (EV_SIDEBAR (totem->sidebar),
			"playlist", _("Playlist"),
			GTK_WIDGET (totem->playlist));
	gtk_paned_pack2 (GTK_PANED (item), totem->sidebar, TRUE, FALSE);

	visible = gconf_client_get_bool (totem->gc,
				GCONF_PREFIX"/sidebar_shown",
				NULL);
	totem->sidebar_shown = visible;

	item = glade_xml_get_widget (totem->xml, "tmw_sidebar_button");
	item2 = glade_xml_get_widget (totem->xml, "tmw_sidebar_menu_item");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item), visible);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item2), visible);

	if (visible != FALSE)
		gtk_widget_show_all (totem->sidebar);
	else
		gtk_widget_hide (totem->sidebar);

	/* Signals */
	g_signal_connect (G_OBJECT (item), "clicked",
			G_CALLBACK (on_sidebar_button_toggled), totem);
	g_signal_connect (G_OBJECT (totem->sidebar), "closed",
			G_CALLBACK (toggle_sidebar_from_sidebar), totem);
	g_signal_connect (G_OBJECT (item2), "activate",
			G_CALLBACK (on_show_sidebar1_activate), totem);

	gtk_widget_show_all (GTK_WIDGET (totem->playlist));
}

