/* totem-sublang.c

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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade.h>

#include "totem-sublang.h"
#include "totem-private.h"
#include "bacon-video-widget.h"

static void
totem_g_list_deep_free (GList *list)
{
	GList *l;

	for (l = list; l != NULL; l = l->next)
		g_free (l->data);
	g_list_free (list);
}

static void
on_sub_activate (GtkButton *button, Totem *totem)
{
	int rank;
	gboolean is_set;

	rank = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "rank"));
	is_set = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (button));

	if (is_set != FALSE)
		bacon_video_widget_set_subtitle (totem->bvw, rank);
}

static void
on_lang_activate (GtkButton *button, Totem *totem)
{
	int rank;
	gboolean is_set;

	rank = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "rank"));
	is_set = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (button));

	if (is_set != FALSE)
		bacon_video_widget_set_language (totem->bvw, rank);
}

static GSList*
add_item_to_menu (Totem *totem, GtkWidget *menu, const char *lang,
		int current_lang, int selection, gboolean is_lang,
		GSList *group)
{
	GtkWidget *item;

	item = gtk_radio_menu_item_new_with_label (group, lang);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
			current_lang == selection ? TRUE : FALSE);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	g_object_set_data (G_OBJECT (item), "rank",
			GINT_TO_POINTER (selection));

	if (is_lang == FALSE)
		g_signal_connect (G_OBJECT (item), "activate",
				G_CALLBACK (on_sub_activate), totem);
	else
		g_signal_connect (G_OBJECT (item), "activate",
				G_CALLBACK (on_lang_activate), totem);

	return gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
}

static GtkWidget*
create_submenu (Totem *totem, GList *list, int current, gboolean is_lang)
{
	GtkWidget *menu = NULL;
	int i;
	GList *l;
	GSList *group = NULL;

	if (list == NULL)
	{
		menu = gtk_menu_new ();
		if (is_lang == FALSE)
		{
			group = add_item_to_menu (totem, menu, _("None"),
					current, -2, is_lang, group);
		}

		group = add_item_to_menu (totem, menu, _("Auto"),
				current, -1, is_lang, group);

		gtk_widget_show (menu);

		return menu;
	}

	i = 0;

	for (l = list; l != NULL; l = l->next)
	{
		if (menu == NULL)
		{
			menu = gtk_menu_new ();
			if (is_lang == FALSE)
			{
				group = add_item_to_menu (totem, menu,
						_("None"), current, -2,
						is_lang, group);
			}
			group = add_item_to_menu (totem, menu, _("Auto"),
					current, -1, is_lang, group);
		}

		group = add_item_to_menu (totem, menu, l->data, current,
				i, is_lang, group);
		i++;
	}

	gtk_widget_show (menu);

	return menu;
}

void
totem_sublang_update (Totem *totem)
{
	GtkWidget *item, *submenu;
	GtkWidget *lang_menu, *sub_menu;
	GList *list;
	int current;

	lang_menu = NULL;
	sub_menu = NULL;

	list = bacon_video_widget_get_languages (totem->bvw);
	if (list != NULL)
	{
		current = bacon_video_widget_get_language (totem->bvw);
		lang_menu = create_submenu (totem, list, current, TRUE);
		totem_g_list_deep_free (list);
	}

	list = bacon_video_widget_get_subtitles (totem->bvw);
	if (list != NULL)
	{
		current = bacon_video_widget_get_subtitle (totem->bvw);
		sub_menu = create_submenu (totem, list, current, FALSE);
		totem_g_list_deep_free (list);
	}

	/* Subtitles */
	item = glade_xml_get_widget (totem->xml, "tmw_subtitles_menu_item");
	submenu = glade_xml_get_widget (totem->xml, "tmw_menu_subtitles");
	if (sub_menu == NULL)
	{
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
	} else {
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), sub_menu);
		totem->subtitles = sub_menu;
	}

	/* Languages */
	item = glade_xml_get_widget (totem->xml, "tmw_languages_menu_item");
	submenu = glade_xml_get_widget (totem->xml, "tmw_menu_languages");
	if (lang_menu == NULL)
	{
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
	} else {
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), lang_menu);
		totem->languages = lang_menu;
	}
}

