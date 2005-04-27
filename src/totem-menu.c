/* totem-menu.c

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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <string.h>

#include "totem-menu.h"
#include "totem-private.h"
#include "bacon-video-widget.h"
#include "egg-recent-view.h"

/* ISO-639 helpers */
static GHashTable *lang_table;

static void
totem_lang_table_free (void)
{
	g_hash_table_destroy (lang_table);
	lang_table = NULL;
}

static void
totem_lang_table_parse_start_tag (GMarkupParseContext *ctx,
		const gchar         *element_name,
		const gchar        **attr_names,
		const gchar        **attr_values,
		gpointer             data,
		GError             **error)
{
	const char *ccode, *lang_name;

	if (!g_str_equal (element_name, "iso_639_entry")
			|| attr_names == NULL
			|| attr_values == NULL)
		return;

	ccode = NULL;
	lang_name = NULL;

	while (*attr_names && *attr_values)
	{
		if (g_str_equal (*attr_names, "iso_639_1_code"))
		{
			/* skip if empty */
			if (**attr_values)
			{
				g_return_if_fail (strlen (*attr_values) == 2);
				ccode = *attr_values;
			}
		}
		else if (g_str_equal (*attr_names, "name"))
		{
			lang_name = *attr_values;
		}

		++attr_names;
		++attr_values;
	}

	if (ccode && lang_name)
	{
		/* th_log ("lang_table: added %s => %s\n", ccode, lang_name); */
		g_hash_table_insert (lang_table,
				g_strdup (ccode),
				g_strdup (lang_name));
	}
}

#define ISO_CODES_DATADIR ISO_CODES_PREFIX"/share/xml/iso-codes"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX"/share/locale"

static void
totem_lang_table_init (void)
{
	GError *err = NULL;
	char *buf;
	gsize buf_len;

	lang_table = g_hash_table_new_full
		(g_str_hash, g_str_equal, g_free, g_free);

	g_atexit (totem_lang_table_free);

	bindtextdomain ("iso_639", ISO_CODES_LOCALESDIR);
	bind_textdomain_codeset ("iso_639", "UTF-8");

	if (g_file_get_contents (ISO_CODES_DATADIR "/iso_639.xml",
				&buf, &buf_len, &err))
	{
		GMarkupParseContext *ctx;
		GMarkupParser parser =
		{ totem_lang_table_parse_start_tag, NULL, NULL, NULL, NULL };

		ctx = g_markup_parse_context_new (&parser, 0, NULL, NULL);

		if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err))
		{
			g_warning ("Failed to parse '%s': %s\n",
					ISO_CODES_DATADIR"/iso_639.xml",
					err->message);
			g_error_free (err);
		}

		g_markup_parse_context_free (ctx);
		g_free (buf);
	} else {
		g_warning ("Failed to load '%s': %s\n",
				ISO_CODES_DATADIR"/iso_639.xml", err->message);
		g_error_free (err);
	}
}

static const char *
totem_lang_get_full (const char *lang)
{
	static gchar  lang_code[3];
	const gchar  *lang_name;

	g_return_val_if_fail (lang != NULL, NULL);

	if (strlen (lang) != 2)
		return NULL;
	if (lang_table == NULL)
		totem_lang_table_init ();

	lang_name = (const gchar*) g_hash_table_lookup (lang_table, lang);

	if (lang_name)
		return dgettext ("iso_639", lang_name);

	g_return_val_if_fail (g_utf8_validate (lang, -1, NULL), NULL);

	/* if we have a country code but no language name,
	 *          *  at least return the country code - better than
	 *                   *  nothing or 'Default language' */
	g_snprintf (lang_code, sizeof (lang_code), "%s", lang);

	return lang_code;
}

/* Subtitle and language menus */
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
	const char *full_lang;

	full_lang = totem_lang_get_full (lang);
	item = gtk_radio_menu_item_new_with_label (group,
			full_lang ? full_lang : lang);

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

static gboolean
totem_sublang_equal_lists (GList *orig, GList *new)
{
	GList *o, *n;
	gboolean retval;

	if ((orig == NULL && new != NULL) || (orig != NULL && new == NULL))
		return FALSE;
	if (orig == NULL && new == NULL)
		return TRUE;

	if (g_list_length (orig) != g_list_length (new))
		return FALSE;

	retval = TRUE;
	o = orig;
	n = new;
	while (o != NULL && n != NULL && retval != FALSE)
	{
		if (g_str_equal (o->data, n->data) == FALSE)
			retval = FALSE;
                o = g_list_next (o);
                n = g_list_next (n);
	}

	return retval;
}

static void
totem_languages_update (Totem *totem, GList *list)
{
	GtkWidget *item, *submenu;
	GtkWidget *lang_menu;
	int current;

	lang_menu = NULL;

	if (list != NULL)
	{
		current = bacon_video_widget_get_language (totem->bvw);
		lang_menu = create_submenu (totem, list, current, TRUE);
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

	totem_g_list_deep_free (totem->language_list);
	totem->language_list = list;
}

static void
totem_subtitles_update (Totem *totem, GList *list)
{
	GtkWidget *item, *submenu;
	GtkWidget *sub_menu;
	int current;

	sub_menu = NULL;

	if (list != NULL)
	{
		current = bacon_video_widget_get_subtitle (totem->bvw);
		sub_menu = create_submenu (totem, list, current, FALSE);
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

	totem_g_list_deep_free (totem->subtitles_list);
	totem->subtitles_list = list;
}

void
totem_sublang_update (Totem *totem)
{
	GList *list;

	list = bacon_video_widget_get_languages (totem->bvw);
	if (totem_sublang_equal_lists (totem->language_list, list) == TRUE) {
		totem_g_list_deep_free (list);
	} else {
		totem_languages_update (totem, list);
	}

	list = bacon_video_widget_get_subtitles (totem->bvw);
	if (totem_sublang_equal_lists (totem->subtitles_list, list) == TRUE) {
		totem_g_list_deep_free (list);
	} else {
		totem_subtitles_update (totem, list);
	}
}

void
totem_sublang_exit (Totem *totem)
{
	totem_g_list_deep_free (totem->subtitles_list);
	totem_g_list_deep_free (totem->language_list);
}

/* Recent files */
static void
on_recent_file_activate (EggRecentViewGtk *view, EggRecentItem *item,
                         Totem *totem)
{
	char *uri;
	gboolean playlist_changed;
	guint end;

	uri = egg_recent_item_get_uri (item);

	totem_signal_block_by_data (totem->playlist, totem);

	end = totem_playlist_get_last (totem->playlist);
	playlist_changed = totem_playlist_add_mrl (totem->playlist, uri, NULL);
	egg_recent_model_add_full (totem->recent_model, item);

	totem_signal_unblock_by_data (totem->playlist, totem);

	if (playlist_changed)
	{
		char *mrl;

		totem_playlist_set_current (totem->playlist, end + 1);
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	}

	g_free (uri);
}

static void
totem_remove_deleted_recent (Totem *totem)
{
	GList *files, *l;

	files = egg_recent_model_get_list (totem->recent_model);
	for (l = files; l != NULL; l = l->next) {
		EggRecentItem *item = l->data;
		const char *txturi;
		GnomeVFSURI *uri;

		txturi = egg_recent_item_peek_uri (item);
		if (txturi == NULL)
			continue;

		if (g_str_has_prefix (txturi, "file:") == FALSE)
			continue;

		uri = gnome_vfs_uri_new (txturi);
		if (gnome_vfs_uri_exists (uri) == FALSE) {
			egg_recent_model_delete (totem->recent_model, txturi);
		}
		gnome_vfs_uri_unref (uri);
	}
}

void
totem_setup_recent (Totem *totem)
{
	GtkWidget *menu_item;
	GtkWidget *menu;

	menu_item = glade_xml_get_widget (totem->xml, "tmw_menu_item_movie");
	menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu_item));
	menu_item = glade_xml_get_widget (totem->xml,
			"tmw_menu_recent_separator");

	/* it would be better if we just filtered by mime-type, but there
	 * doesn't seem to be an easy way to figure out which mime-types we
	 * can handle */
	totem->recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);

	totem->recent_view = egg_recent_view_gtk_new (menu, menu_item);
	egg_recent_view_gtk_show_icons (EGG_RECENT_VIEW_GTK
			(totem->recent_view), FALSE);
	egg_recent_model_set_limit (totem->recent_model, 5);
	egg_recent_view_set_model (EGG_RECENT_VIEW (totem->recent_view),
			totem->recent_model);
	egg_recent_model_set_filter_groups (totem->recent_model,
			"Totem", NULL);
	egg_recent_view_gtk_set_trailing_sep (totem->recent_view, TRUE);

	totem_remove_deleted_recent (totem);

	g_signal_connect (totem->recent_view, "activate",
			G_CALLBACK (on_recent_file_activate), totem);
}

void
totem_action_add_recent (Totem *totem, const char *filename)
{
	EggRecentItem *item;

	if (strstr (filename, "file:///") == NULL)
		return;

	item = egg_recent_item_new_from_uri (filename);
	egg_recent_item_add_group (item, "Totem");
	egg_recent_model_add_full (totem->recent_model, item);
}

/* Play Disc menu items */

static void
on_play_disc_activate (GtkMenuItem *menu_item, Totem *totem)
{
	MediaType type;
	GError *error = NULL;
	char *device_path;

	device_path = g_object_get_data (G_OBJECT (menu_item), "device_path");

	type = totem_cd_detect_type (device_path, &error);
	switch (type) {
		case MEDIA_TYPE_ERROR:
			totem_action_error ("Failed to play Audio/Video Disc",
					    error ? error->message : "Reason unknown",
					    totem);
			return;
		case MEDIA_TYPE_DATA:
			/* Maybe set default location to the mountpoint of
			 * this device?... */
			{
				GtkWidget *item;

				item = glade_xml_get_widget
					(totem->xml, "tmw_open_menu_item");
				gtk_menu_item_activate (GTK_MENU_ITEM (item));
			}
			return;
		case MEDIA_TYPE_DVD:
		case MEDIA_TYPE_VCD:
		case MEDIA_TYPE_CDDA:
			bacon_video_widget_set_media_device
				(BACON_VIDEO_WIDGET (totem->bvw), device_path);
			totem_action_play_media (totem, type);
			break;
		default:
			g_assert_not_reached ();
	}

	g_free (device_path);
}

/* A GnomeVFSDrive and GnomeVFSVolume share many similar methods, but do not
   share a base class other than GObject. */
static char *
fake_gnome_vfs_device_get_something (GObject *device,
		char *(*volume_function) (GnomeVFSVolume *),
		char *(*drive_function) (GnomeVFSDrive *)) {
        if (GNOME_IS_VFS_VOLUME (device)) {
                return (*volume_function) (GNOME_VFS_VOLUME (device));
        } else if (GNOME_IS_VFS_DRIVE (device)) {
                return (*drive_function) (GNOME_VFS_DRIVE (device));
        } else {
                g_warning ("neither a GnomeVFSVolume or a GnomeVFSDrive");
                return NULL;
        }
}

static void
add_device_to_menu (GObject *device, GtkMenu *menu, gint position, Totem *totem)
{
	char *name, *icon_name, *device_path, *label;
	GtkWidget *menu_item, *icon;

	name = fake_gnome_vfs_device_get_something (device,
		&gnome_vfs_volume_get_display_name,
		&gnome_vfs_drive_get_display_name);
	icon_name = fake_gnome_vfs_device_get_something (device,
		&gnome_vfs_volume_get_icon, &gnome_vfs_drive_get_icon);
	device_path = fake_gnome_vfs_device_get_something (device,
		&gnome_vfs_volume_get_device_path,
		&gnome_vfs_drive_get_device_path);

	label = g_strdup_printf (_("Play Disc '%s'"), name);
	g_free (name);
	menu_item = gtk_image_menu_item_new_with_label (label);
	g_free (label);

	icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
	g_free (icon_name);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), icon);

	g_object_set_data_full (G_OBJECT (menu_item), "device_path",
			g_strdup (device_path), (GDestroyNotify) g_free);
	g_free (device_path);
	g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_play_disc_activate), totem);

	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menu_item, position);
	gtk_widget_show (menu_item);
}

static void
on_movie_menu_select (GtkMenuItem *movie_menuitem, Totem *totem)
{
	GtkMenu *menu;
	GtkMenuItem *openloc_item;
	GtkSeparatorMenuItem *sep_item;

	GList *menuitems;
	gint position;

	GList *devices, *volumes, *drives, *i;

	if (totem->drives_changed == FALSE)
		return;

	menu = GTK_MENU (glade_xml_get_widget (totem->xml, "tmw_menu_movie"));
	g_assert (menu != NULL);

	openloc_item = GTK_MENU_ITEM (glade_xml_get_widget (totem->xml,
				"tmw_open_location_menu_item"));
	sep_item = GTK_SEPARATOR_MENU_ITEM (glade_xml_get_widget (totem->xml,
				"tmw_menu_movie_separator_1"));

	/* remove existing menu entries */
	menuitems = gtk_container_get_children (GTK_CONTAINER (menu));

	i = g_list_find (menuitems, openloc_item)->next;
	while (i != g_list_find (menuitems, sep_item))
	{
		gtk_container_remove (GTK_CONTAINER (menu), i->data);
		gtk_widget_destroy (i->data);

		i = i->next;
	}

	/* Create a list of suitable devices */
	devices = NULL;

	volumes = gnome_vfs_volume_monitor_get_mounted_volumes
		(totem->monitor);
	for (i = volumes; i != NULL; i = i->next) {
		if (gnome_vfs_volume_get_device_type (i->data) != GNOME_VFS_DEVICE_TYPE_CDROM)
			continue;

		gnome_vfs_volume_ref (i->data);
		devices = g_list_append (devices, i->data);
	}
	gnome_vfs_drive_volume_list_free (volumes);

	drives = gnome_vfs_volume_monitor_get_connected_drives (totem->monitor);
	for (i = drives; i != NULL; i = i->next) {
		if (gnome_vfs_drive_get_device_type (i->data) != GNOME_VFS_DEVICE_TYPE_CDROM)
			continue;
		else if (gnome_vfs_drive_is_mounted (i->data))
			continue;

		gnome_vfs_volume_ref (i->data);
		devices = g_list_append (devices, i->data);
	}
	gnome_vfs_drive_volume_list_free (drives);

	/* Add the devices to the menu */
	position = g_list_index (menuitems, openloc_item);
	g_list_free (menuitems);

	for (i = devices; i != NULL; i = i->next)
	{
		position++;
		add_device_to_menu (i->data, GTK_MENU (menu), position, totem);
	}

	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);
}

static void
on_gnome_vfs_monitor_event (GnomeVFSVolumeMonitor *monitor,
		GnomeVFSDrive *drive,
		Totem *totem)
{
	totem->drives_changed = TRUE;
}

void
totem_setup_play_disc (Totem *totem)
{
	GtkWidget *item;

	item = glade_xml_get_widget (totem->xml, "tmw_menu_item_movie");
	g_signal_connect (G_OBJECT (item), "select",
			G_CALLBACK (on_movie_menu_select), totem);

	g_signal_connect (G_OBJECT (totem->monitor),
			"drive-connected",
			G_CALLBACK (on_gnome_vfs_monitor_event), totem);
	g_signal_connect (G_OBJECT (totem->monitor),
			"drive-disconnected",
			G_CALLBACK (on_gnome_vfs_monitor_event), totem);
	g_signal_connect (G_OBJECT (totem->monitor),
			"volume-mounted",
			G_CALLBACK (on_gnome_vfs_monitor_event), totem);
	g_signal_connect (G_OBJECT (totem->monitor),
			"volume-unmounted",
			G_CALLBACK (on_gnome_vfs_monitor_event), totem);

	totem->drives_changed = TRUE;
}

