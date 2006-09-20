/*
 *  arch-tag: Implementation of Rhythmbox icon loading
 *
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003,2004 Colin Walters <walters@verbum.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "totem-stock-icons.h"
#include "totem-private.h"
#include "video-utils.h"

static GHashTable *icons_table = NULL;

static GdkPixbuf *
totem_get_icon_from_theme (const char *id, GtkIconSize size)
{
	GtkIconTheme *theme;
	GtkIconInfo *icon;
	const char *filename;
	int width, height;
	GdkPixbuf *pixbuf;

	theme = gtk_icon_theme_get_default ();
	gtk_icon_size_lookup (size, &width, &height);
	icon = gtk_icon_theme_lookup_icon (theme, id, width, 0);
	if (icon == NULL) {
		return NULL;
	}

	filename = gtk_icon_info_get_filename (icon);
	pixbuf = gdk_pixbuf_new_from_file_at_size (filename,
			width, height, NULL);
	gtk_icon_info_free (icon);

	return pixbuf;
}

static gboolean
remove_value (gpointer key, GdkPixbuf *pixbuf, Totem *totem)
{
	gdk_pixbuf_unref (pixbuf);
	return TRUE;
}

void
totem_named_icons_dispose (Totem *totem)
{
	if (icons_table != NULL)
	{
		g_hash_table_foreach_remove (icons_table,
				(GHRFunc) remove_value, totem);
		g_hash_table_destroy (icons_table);
		icons_table = NULL;
	}
}

#define PIXBUF_FOR_ID(x) (g_hash_table_lookup(icons_table, x))

static void
totem_default_theme_changed (GtkIconTheme *theme, Totem *totem)
{
	totem_named_icons_dispose (totem);
	totem_named_icons_init (totem, TRUE);
	totem_set_default_icons (totem);
}

static void
set_icon_for_action (Totem *totem, const char *name, const char *icon_id)
{
	GtkAction *action;
	GSList *proxies, *p;

	action = gtk_action_group_get_action (totem->main_action_group, name);

	g_object_set_data (G_OBJECT (action), "pixbuf-icon",
			PIXBUF_FOR_ID(icon_id));
	proxies = g_slist_copy (gtk_action_get_proxies (action));
	for (p = proxies; p != NULL; p = p->next) {
		gtk_action_connect_proxy (action, GTK_WIDGET (p->data));
	}
	g_slist_free (proxies);
}

void
totem_set_default_icons (Totem *totem)
{
	GtkWidget *item;

	/* Screenshot button */
	set_icon_for_action (totem, "take-screenshot", "panel-screenshot");
	set_icon_for_action (totem, "volume-up", "audio-volume-high");
	set_icon_for_action (totem, "volume-down", "audio-volume-low");

	/* Leave fullscreen button */
	item = glade_xml_get_widget (totem->xml,
			"tefw_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			PIXBUF_FOR_ID("stock_leave-fullscreen"));
}

static GdkPixbuf *
totem_get_pixbuf_from_totem_install (const char *filename)
{
	GdkPixbuf *pixbuf;
	char *path, *fn;

	fn = g_strconcat (filename, ".png", NULL);
	path = g_build_filename (DATADIR, "totem", fn, NULL);
	g_free (fn);
	pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	g_free (path);

	return pixbuf;
}

void
totem_named_icons_init (Totem *totem, gboolean refresh)
{
	GtkIconTheme *theme;
	int i;
	char *items[][4] = {
		{ "panel-screenshot", "stock-panel-screenshot", "gnome-screenshot", "applets-screenshooter" },
		{ "stock_leave-fullscreen", GTK_STOCK_QUIT, NULL, NULL },
		{ "audio-volume-high", "stock-volume-high", NULL, NULL },
		{ "audio-volume-low", "stock-volume-low", NULL, NULL },
	};

	if (refresh == FALSE) {
		theme = gtk_icon_theme_get_default ();
		g_signal_connect (G_OBJECT (theme), "changed",
				G_CALLBACK (totem_default_theme_changed),
				totem);
	}

	if (icons_table == NULL) {
		icons_table = g_hash_table_new (g_str_hash, g_str_equal);
	}

	for (i = 0; i < (int) G_N_ELEMENTS (items); i++) {
		GdkPixbuf *pixbuf = NULL;
		int size;
		int j;

		size = G_N_ELEMENTS (items[i]);

		for (j = 0; j < size && items[i][j] != NULL; j++) {
			pixbuf = totem_get_icon_from_theme
				(items[i][j],
				 GTK_ICON_SIZE_BUTTON);
			if (pixbuf != NULL)
				break;
		}

		if (pixbuf == NULL) {
			for (j = 0; j < size && items[i][j] != NULL; j++) {
				pixbuf = totem_get_pixbuf_from_totem_install
					(items[i][j]);
				if (pixbuf != NULL)
					break;
			}
		}

		if (pixbuf == NULL) {
			for (j = 0; j < size && items[i][j] != NULL; j++) {
				pixbuf = gtk_widget_render_icon
					(GTK_WIDGET (totem->win),
					 items[i][j], GTK_ICON_SIZE_BUTTON,
					 NULL);
				if (pixbuf != NULL)
					break;
			}
		}

		if (pixbuf == NULL) {
			g_warning ("Couldn't find themed icon for \"%s\"",
					items[i][0]);
			continue;
		}

		/* The pixbuf will be unref'ed when we destroy the 
		 * hash table */
		g_hash_table_insert (icons_table, (gpointer) items[i][0],
				(gpointer) pixbuf);
	}
}

