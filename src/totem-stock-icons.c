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

void
totem_set_default_icons (Totem *totem)
{
	GtkWidget *item;

	/* Play button */
	item = glade_xml_get_widget (totem->xml,
			"tmw_play_pause_button_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			totem->state == STATE_PLAYING
			? PIXBUF_FOR_ID("stock-media-pause")
			: PIXBUF_FOR_ID("stock-media-play"));
	item = glade_xml_get_widget (totem->xml, "tcw_pp_button_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			totem->state == STATE_PLAYING
			? PIXBUF_FOR_ID("stock-media-pause")
			: PIXBUF_FOR_ID("stock-media-play"));

	/* Previous button */
	item = glade_xml_get_widget (totem->xml, "tmw_previous_button_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			PIXBUF_FOR_ID("stock-media-prev"));
	item = glade_xml_get_widget (totem->xml, "tcw_previous_button_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			PIXBUF_FOR_ID("stock-media-prev"));

	/* Next button */
	item = glade_xml_get_widget (totem->xml, "tmw_next_button_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			PIXBUF_FOR_ID("stock-media-next"));
	item = glade_xml_get_widget (totem->xml, "tcw_next_button_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			PIXBUF_FOR_ID("stock-media-next"));

	/* Screenshot button */
	item = glade_xml_get_widget (totem->xml,
			"tmw_take_screenshot_menu_item_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			PIXBUF_FOR_ID("panel-screenshot"));

	/* Playlist button */
	item = glade_xml_get_widget (totem->xml,
			"tmw_playlist_button_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			PIXBUF_FOR_ID("stock_playlist"));
	item = glade_xml_get_widget (totem->xml,
			"tmw_show_playlist_menu_item_image");
	gtk_image_set_from_pixbuf (GTK_IMAGE (item),
			PIXBUF_FOR_ID("stock_playlist"));
	gtk_window_set_icon (GTK_WINDOW (totem->playlist),
			PIXBUF_FOR_ID("stock_playlist"));
}

GdkPixbuf *
totem_get_named_icon_for_id (const char *id)
{
	return PIXBUF_FOR_ID(id);
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

	struct {
		const char *fn1;
		const char *fn2;
		const char *fn3;
	} items[] = {
		{ "stock-media-play", "stock_media-play", "stock_media_play" },
		{ "stock-media-pause", "stock_media-pause", "stock_media_pause" },
		{ "stock-media-prev", "stock_media-prev", "stock_media_previous" },
		{ "stock-media-next", "stock_media-next", "stock_media_next" },
		{ "panel-screenshot", "stock-panel-screenshot", "gnome-screenshot" },
		{ "stock_playlist", "playlist-24", NULL },
	};

	if (refresh == FALSE) {
		theme = gtk_icon_theme_get_default ();
		g_signal_connect (G_OBJECT (theme), "changed",
				G_CALLBACK (totem_default_theme_changed),
				totem);
	}

	if (icons_table == NULL) {
		icons_table = g_hash_table_new (NULL, g_str_equal);
	}

	for (i = 0; i < (int) G_N_ELEMENTS (items); i++) {
		GdkPixbuf *pixbuf = NULL;

		pixbuf = totem_get_icon_from_theme (items[i].fn1,
				GTK_ICON_SIZE_BUTTON);

		if (pixbuf == NULL && items[i].fn2 != NULL) {
			pixbuf = totem_get_icon_from_theme (items[i].fn2,
					GTK_ICON_SIZE_BUTTON);
		}

		if (pixbuf == NULL && items[i].fn3 != NULL) {
			pixbuf = totem_get_icon_from_theme (items[i].fn3,
					GTK_ICON_SIZE_BUTTON);
		}

		if (pixbuf == NULL && items[i].fn3 != NULL) {
			pixbuf = totem_get_pixbuf_from_totem_install
				(items[i].fn3);
		}

		if (pixbuf == NULL) {
			pixbuf = totem_get_pixbuf_from_totem_install
				(items[i].fn2);
		}

		if (pixbuf == NULL) {
			pixbuf = totem_get_pixbuf_from_totem_install
				(items[i].fn1);
		}

		if (pixbuf == NULL) {
			g_warning ("Couldn't find themed icon for \"%s\"",
					items[i].fn1);
			continue;
		}

		/* The pixbuf will be unref'ed when we destroy the 
		 * hash table */
		g_hash_table_insert (icons_table, (gpointer) items[i].fn1,
				(gpointer) pixbuf);
	}

	/* Switch direction of the Play icon if we're in Right-To-Left */
	if (gtk_widget_get_direction (totem->win) == GTK_TEXT_DIR_RTL)
	{
		totem_pixbuf_mirror (PIXBUF_FOR_ID("stock-media-play"));
		totem_pixbuf_mirror (PIXBUF_FOR_ID("stock-media-next"));
		totem_pixbuf_mirror (PIXBUF_FOR_ID("stock-media-prev"));
	}
}

