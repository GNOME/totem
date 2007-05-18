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

static void
cb_resize (Totem * totem)
{
	GValue gvalue_size = { 0, };
	gint handle_size;
	GtkWidget *pane;
	int w, h;

	w = totem->win->allocation.width;
	h = totem->win->allocation.height;

	g_value_init (&gvalue_size, G_TYPE_INT);
	pane = glade_xml_get_widget (totem->xml, "tmw_main_pane");
	gtk_widget_style_get_property(pane, "handle-size", &gvalue_size);
	handle_size = g_value_get_int (&gvalue_size);
	
	if (totem->sidebar_shown) {
		w += totem->sidebar->allocation.width + handle_size;
	} else {
		w -= totem->sidebar->allocation.width + handle_size;
	}

	if (w > 0 && h > 0)
		gtk_window_resize (GTK_WINDOW (totem->win), w, h);
}

void
totem_sidebar_toggle (Totem *totem, gboolean state)
{
	GtkAction *action;

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (totem->sidebar)) == state)
		return;

	if (state != FALSE)
		gtk_widget_show (GTK_WIDGET (totem->sidebar));
	else
		gtk_widget_hide (GTK_WIDGET (totem->sidebar));

	action = gtk_action_group_get_action (totem->main_action_group, "sidebar");
	totem_signal_block_by_data (G_OBJECT (action), totem);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), state);
	totem_signal_unblock_by_data (G_OBJECT (action), totem);

	gconf_client_set_bool (totem->gc,
				GCONF_PREFIX"/sidebar_shown",
				state,
				NULL);
	totem->sidebar_shown = state;
	cb_resize(totem);
}

static void
toggle_sidebar_from_sidebar (GtkWidget *playlist, Totem *totem)
{
	totem_sidebar_toggle (totem, FALSE);
}

gboolean
totem_sidebar_is_visible (Totem *totem)
{
	return totem->sidebar_shown;
}

void
totem_sidebar_setup (Totem *totem, gboolean visible, const char *page_id)
{
	GtkWidget *item;
	GtkAction *action;

	item = glade_xml_get_widget (totem->xml, "tmw_main_pane");
	totem->sidebar = ev_sidebar_new ();
	ev_sidebar_add_page (EV_SIDEBAR (totem->sidebar),
			"playlist", _("Playlist"),
			GTK_WIDGET (totem->playlist));
	if (page_id != NULL) {
		ev_sidebar_set_current_page (EV_SIDEBAR (totem->sidebar),
				page_id);
	} else {
		ev_sidebar_set_current_page (EV_SIDEBAR (totem->sidebar),
				"playlist");
	}
	gtk_paned_pack2 (GTK_PANED (item), totem->sidebar, FALSE, FALSE);

	totem->sidebar_shown = visible;

	action = gtk_action_group_get_action (totem->main_action_group,
			"sidebar");

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	/* Signals */
	g_signal_connect (G_OBJECT (totem->sidebar), "closed",
			G_CALLBACK (toggle_sidebar_from_sidebar), totem);

	gtk_widget_show_all (totem->sidebar);
	gtk_widget_realize (totem->sidebar);

	if (!visible)
		gtk_widget_hide (totem->sidebar);
}

const char *
totem_sidebar_get_current_page (Totem *totem)
{
	return ev_sidebar_get_current_page (EV_SIDEBAR (totem->sidebar));
}

void
totem_sidebar_set_current_page (Totem *totem, const char *name)
{
	ev_sidebar_set_current_page (EV_SIDEBAR (totem->sidebar), name);
	totem_sidebar_toggle (totem, TRUE);
}

