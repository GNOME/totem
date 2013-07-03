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
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <glib/gi18n.h>

#include "totem.h"
#include "totem-sidebar.h"
#include "totem-private.h"

static void
cb_resize (Totem * totem)
{
	GValue gvalue_size = { 0, };
	gint handle_size;
	GtkWidget *pane;
	GtkAllocation allocation;
	int w, h;

	gtk_widget_get_allocation (totem->win, &allocation);
	w = allocation.width;
	h = allocation.height;

	g_value_init (&gvalue_size, G_TYPE_INT);
	pane = GTK_WIDGET (gtk_builder_get_object (totem->xml, "tmw_main_pane"));
	gtk_widget_style_get_property (pane, "handle-size", &gvalue_size);
	handle_size = g_value_get_int (&gvalue_size);
	g_value_unset (&gvalue_size);

	gtk_widget_get_allocation (totem->sidebar, &allocation);
	if (totem->sidebar_shown) {
		w += allocation.width + handle_size;
	} else {
		w -= allocation.width + handle_size;
	}

	if (w > 0 && h > 0)
		gtk_window_resize (GTK_WINDOW (totem->win), w, h);
}

void
totem_sidebar_toggle (Totem *totem, gboolean state)
{
	if (gtk_widget_get_visible (GTK_WIDGET (totem->sidebar)) == state)
		return;

	if (state != FALSE)
		gtk_widget_show (GTK_WIDGET (totem->sidebar));
	else
		gtk_widget_hide (GTK_WIDGET (totem->sidebar));

	totem->sidebar_shown = state;
	cb_resize(totem);
}

gboolean
totem_sidebar_is_visible (Totem *totem)
{
	return totem->sidebar_shown;
}

static gboolean
has_popup (void)
{
	GList *list, *l;
	gboolean retval = FALSE;

	list = gtk_window_list_toplevels ();
	for (l = list; l != NULL; l = l->next) {
		GtkWindow *window = GTK_WINDOW (l->data);
		if (gtk_widget_get_visible (GTK_WIDGET (window)) && gtk_window_get_window_type (window) == GTK_WINDOW_POPUP) {
			retval = TRUE;
			break;
		}
	}
	g_list_free (list);
	return retval;
}

gboolean
totem_sidebar_is_focused (Totem *totem, gboolean *handles_kbd)
{
	GtkWidget *focused;

	if (handles_kbd != NULL)
		*handles_kbd = has_popup ();

	focused = gtk_window_get_focus (GTK_WINDOW (totem->win));
	if (focused == NULL)
		return FALSE;
	if (gtk_widget_is_ancestor (focused, GTK_WIDGET (totem->sidebar)) != FALSE)
		return TRUE;

	return FALSE;
}

void
totem_sidebar_setup (Totem *totem, gboolean visible)
{
	GtkPaned *item;

	item = GTK_PANED (gtk_builder_get_object (totem->xml, "tmw_main_pane"));
	totem->sidebar = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (totem->sidebar), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (totem->sidebar), FALSE);

	totem_sidebar_add_page (totem, "playlist", _("Playlist"), NULL, GTK_WIDGET (totem->playlist));
	gtk_paned_pack2 (item, totem->sidebar, FALSE, FALSE);

	totem->sidebar_shown = visible;

	gtk_widget_show_all (totem->sidebar);
	gtk_widget_realize (totem->sidebar);

	if (!visible)
		gtk_widget_hide (totem->sidebar);
}

void
totem_sidebar_add_page (Totem *totem,
			const char *page_id,
			const char *label,
			const char *accelerator,
			GtkWidget *main_widget)
{
	g_return_if_fail (page_id != NULL);
	g_return_if_fail (GTK_IS_WIDGET (main_widget));

	g_object_set_data_full (G_OBJECT (main_widget), "sidebar-name",
				g_strdup (page_id), g_free);

	gtk_notebook_append_page (GTK_NOTEBOOK (totem->sidebar),
				  main_widget,
				  NULL);
}

static int
get_page_num_for_name (Totem *totem,
		       const char *page_id)
{
	int num_pages, i;

	if (page_id == NULL)
		return -1;

	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (totem->sidebar));
	for (i = 0; i < num_pages; i++) {
		GtkWidget *widget;
		const char *name;

		widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (totem->sidebar), i);

		name = g_object_get_data (G_OBJECT (widget), "sidebar-name");
		if (g_strcmp0 (name, page_id) == 0)
			return i;
	}

	return -1;
}

void
totem_sidebar_remove_page (Totem *totem,
			   const char *page_id)
{
	int page_num;

	page_num = get_page_num_for_name (totem, page_id);

	if (page_num == -1) {
		g_warning ("Tried to remove sidebar page '%s' but it does not exist", page_id);
		return;
	}

	gtk_notebook_remove_page (GTK_NOTEBOOK (totem->sidebar), page_num);
}

char *
totem_sidebar_get_current_page (Totem *totem)
{
	int current_page;
	GtkWidget *widget;

	if (totem->sidebar == NULL)
		return NULL;

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (totem->sidebar));
	widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (totem->sidebar), current_page);

	return g_strdup (g_object_get_data (G_OBJECT (widget), "sidebar-name"));
}

void
totem_sidebar_set_current_page (Totem *totem,
				const char *page_id,
				gboolean force_visible)
{
	int page_num;

	if (page_id == NULL)
		return;

	page_num = get_page_num_for_name (totem, page_id);

	if (page_num == -1) {
		g_warning ("Tried to set sidebar page '%s' but it does not exist", page_id);
		return;
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (totem->sidebar), page_num);

	if (force_visible != FALSE)
		totem_sidebar_toggle (totem, TRUE);
}

