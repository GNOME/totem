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
#include "totem.h"
#include "totem-interface.h"
#include "totem-private.h"
#include "totem-sidebar.h"
#include "totem-plugin-manager.h"
#include "bacon-video-widget.h"

#include "debug.h"

/* Helper function to escape underscores in labels
 * before putting them in menu items */
static char *
escape_label_for_menu (const char *name)
{
	char *new, **a;

	a = g_strsplit (name, "_", -1);
	new = g_strjoinv ("__", a);
	g_strfreev (a);

	return new;
}

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
	const char *ccode_longB, *ccode_longT, *ccode, *lang_name;

	if (!g_str_equal (element_name, "iso_639_entry")
			|| attr_names == NULL
			|| attr_values == NULL)
		return;

	ccode = NULL;
	ccode_longB = NULL;
	ccode_longT = NULL;
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
		} else if (g_str_equal (*attr_names, "iso_639_2B_code")) {
			/* skip if empty */
			if (**attr_values)
			{
				g_return_if_fail (strlen (*attr_values) == 3 || strcmp (*attr_values, "qaa-qtz") == 0);
				ccode_longB = *attr_values;
			}
		} else if (g_str_equal (*attr_names, "iso_639_2T_code")) {
			/* skip if empty */
			if (**attr_values)
			{
				g_return_if_fail (strlen (*attr_values) == 3 || strcmp (*attr_values, "qaa-qtz") == 0);
				ccode_longT = *attr_values;
			}
		} else if (g_str_equal (*attr_names, "name")) {
			lang_name = *attr_values;
		}

		++attr_names;
		++attr_values;
	}

	if (lang_name == NULL)
		return;

	if (ccode != NULL)
	{
		g_hash_table_insert (lang_table,
				g_strdup (ccode),
				g_strdup (lang_name));
	}
	if (ccode_longB != NULL)
	{
		g_hash_table_insert (lang_table,
				g_strdup (ccode_longB),
				g_strdup (lang_name));
	}
	if (ccode_longT != NULL)
	{
		g_hash_table_insert (lang_table,
				g_strdup (ccode_longT),
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
	const char *lang_name;
	int len;

	g_return_val_if_fail (lang != NULL, NULL);

	len = strlen (lang);
	if (len != 2 && len != 3)
		return NULL;
	if (lang_table == NULL)
		totem_lang_table_init ();

	lang_name = (const gchar*) g_hash_table_lookup (lang_table, lang);

	if (lang_name)
		return dgettext ("iso_639", lang_name);

	return NULL;
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
subtitles_changed_callback (GtkRadioAction *action, GtkRadioAction *current,
		Totem *totem)
{
	int rank;

	rank = gtk_radio_action_get_current_value (current);

	bacon_video_widget_set_subtitle (totem->bvw, rank);
}


static void
languages_changed_callback (GtkRadioAction *action, GtkRadioAction *current,
		Totem *totem)
{
	int rank;

	rank = gtk_radio_action_get_current_value (current);

	bacon_video_widget_set_language (totem->bvw, rank);
}

static GtkAction *
add_lang_action (Totem *totem, GtkActionGroup *action_group, guint ui_id,
		const char *path, const char *prefix, const char *lang, 
		int lang_id, int index, GSList **group)
{
	const char *full_lang;
	char *label;
	char *name;
	GtkAction *action;

	full_lang = totem_lang_get_full (lang);

	if (index > 1) {
		char *num_lang;

		num_lang = g_strdup_printf ("%s #%u",
					    full_lang ? full_lang : lang,
					    index);
		label = escape_label_for_menu (num_lang);
		g_free (num_lang);
	} else {
		label = escape_label_for_menu (full_lang ? full_lang : lang);
	}

	name = g_strdup_printf ("%s-%d", prefix, lang_id);

	action = g_object_new (GTK_TYPE_RADIO_ACTION,
			       "name", name,
			       "label", label,
			       "value", lang_id,
			       NULL);
	g_free (label);

	gtk_radio_action_set_group (GTK_RADIO_ACTION (action), *group);
	*group = gtk_radio_action_get_group (GTK_RADIO_ACTION (action));
	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);
	gtk_ui_manager_add_ui (totem->ui_manager, ui_id,
			       path, name, name, GTK_UI_MANAGER_MENUITEM, FALSE);
	g_free (name);

	return action;
}

static GtkAction *
create_lang_actions (Totem *totem, GtkActionGroup *action_group, guint ui_id,
		const char *path, const char *prefix, GList *list,
		gboolean is_lang)
{
	GtkAction *action = NULL;
	unsigned int i, *hash_value;
	GList *l;
	GSList *group = NULL;
	GHashTable *lookup;
	char *action_data;

	if (is_lang == FALSE) {
		add_lang_action (totem, action_group, ui_id, path, prefix,
				_("None"), -2, 0, &group);
	}

	action = add_lang_action (totem, action_group, ui_id, path, prefix,
			_("Auto"), -1, 0, &group);

	i = 0;
	lookup = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

	for (l = list; l != NULL; l = l->next)
	{
		guint num;

		hash_value = g_hash_table_lookup (lookup, l->data);
		if (hash_value == NULL) {
			num = 0;
			action_data = g_strdup (l->data);
			g_hash_table_insert (lookup, l->data, GINT_TO_POINTER (1));
		} else {
			num = GPOINTER_TO_INT (hash_value);
			action_data = g_strdup (l->data);
			g_hash_table_replace (lookup, l->data, GINT_TO_POINTER (num + 1));
		}

		add_lang_action (totem, action_group, ui_id, path, prefix,
				 action_data, i, num + 1, &group);
 		i++;
	}

	g_hash_table_destroy (lookup);

	return action;
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
	GtkAction *action;
	int current;

	/* Remove old UI */
	gtk_ui_manager_remove_ui (totem->ui_manager, totem->languages_ui_id);
	gtk_ui_manager_ensure_update (totem->ui_manager);

	/* Create new ActionGroup */
	if (totem->languages_action_group) {
		gtk_ui_manager_remove_action_group (totem->ui_manager,
				totem->languages_action_group);
		g_object_unref (totem->languages_action_group);
	}
	totem->languages_action_group = gtk_action_group_new ("languages-action-group");
	gtk_ui_manager_insert_action_group (totem->ui_manager,
			totem->languages_action_group, -1);

	if (list != NULL)
	{
		action = create_lang_actions (totem, totem->languages_action_group,
				totem->languages_ui_id,
				"/tmw-menubar/sound/languages/placeholder",
			       	"languages", list, TRUE);
		gtk_ui_manager_ensure_update (totem->ui_manager);

		current = bacon_video_widget_get_language (totem->bvw);
		gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action),
				current);
		g_signal_connect (G_OBJECT (action), "changed",
				G_CALLBACK (languages_changed_callback), totem);
	}

	totem_g_list_deep_free (totem->language_list);
	totem->language_list = list;
}

static void
totem_subtitles_update (Totem *totem, GList *list)
{
	GtkAction *action;
	int current;

	/* Remove old UI */
	gtk_ui_manager_remove_ui (totem->ui_manager, totem->subtitles_ui_id);
	gtk_ui_manager_ensure_update (totem->ui_manager);

	/* Create new ActionGroup */
	if (totem->subtitles_action_group) {
		gtk_ui_manager_remove_action_group (totem->ui_manager,
				totem->subtitles_action_group);
		g_object_unref (totem->subtitles_action_group);
	}
	totem->subtitles_action_group = gtk_action_group_new ("subtitles-action-group");
	gtk_ui_manager_insert_action_group (totem->ui_manager,
			totem->subtitles_action_group, -1);


	if (list != NULL)
	{
		action = create_lang_actions (totem, totem->subtitles_action_group,
				totem->subtitles_ui_id,
				"/tmw-menubar/view/subtitles/placeholder",
			       	"subtitles", list, FALSE);
		gtk_ui_manager_ensure_update (totem->ui_manager);

		current = bacon_video_widget_get_subtitle (totem->bvw);
		gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action),
				current);
		g_signal_connect (G_OBJECT (action), "changed",
				G_CALLBACK (subtitles_changed_callback), totem);
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
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy,
                  gpointer data)
{
        GtkLabel *label;

        if (!GTK_IS_MENU_ITEM (proxy))
                return;

        label = GTK_LABEL (GTK_BIN (proxy)->child);

        gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars (label,TOTEM_MAX_RECENT_ITEM_LEN);
}

static void
on_recent_file_item_activated (GtkAction *action,
                               Totem *totem)
{
	GtkRecentInfo *recent_info;
	const gchar *uri;
	gboolean playlist_changed;
	guint end;

	recent_info = g_object_get_data (G_OBJECT (action), "recent-info");
	uri = gtk_recent_info_get_uri (recent_info);

	totem_signal_block_by_data (totem->playlist, totem);

	end = totem_playlist_get_last (totem->playlist);
	playlist_changed = totem_playlist_add_mrl (totem->playlist, uri, NULL);
	gtk_recent_manager_add_item (totem->recent_manager, uri);

	totem_signal_unblock_by_data (totem->playlist, totem);

	if (playlist_changed)
	{
		char *mrl;

		totem_playlist_set_current (totem->playlist, end + 1);
		mrl = totem_playlist_get_current_mrl (totem->playlist);
		totem_action_set_mrl_and_play (totem, mrl);
		g_free (mrl);
	}
}

static gint
totem_compare_recent_items (GtkRecentInfo *a, GtkRecentInfo *b)
{
	gboolean has_totem_a, has_totem_b;

	has_totem_a = gtk_recent_info_has_group (a, "Totem");
	has_totem_b = gtk_recent_info_has_group (b, "Totem");

	if (has_totem_a && has_totem_b) {
		time_t time_a, time_b;

		time_a = gtk_recent_info_get_modified (a);
		time_b = gtk_recent_info_get_modified (b);

		return (time_b - time_a);
	} else if (has_totem_a) {
		return -1;
	} else if (has_totem_b) {
		return 1;
	}

	return 0;
}

static void
totem_recent_manager_changed_callback (GtkRecentManager *recent_manager, Totem *totem)
{
        GList *items, *l;
        guint n_items = 0;

        if (totem->recent_ui_id != 0) {
                gtk_ui_manager_remove_ui (totem->ui_manager, totem->recent_ui_id);
                gtk_ui_manager_ensure_update (totem->ui_manager);
        }

        if (totem->recent_action_group) {
                gtk_ui_manager_remove_action_group (totem->ui_manager,
                                totem->recent_action_group);
        }

        totem->recent_action_group = gtk_action_group_new ("recent-action-group");
        g_signal_connect (totem->recent_action_group, "connect-proxy",
                          G_CALLBACK (connect_proxy_cb), NULL);
        gtk_ui_manager_insert_action_group (totem->ui_manager,
                        totem->recent_action_group, -1);
        g_object_unref (totem->recent_action_group);

        totem->recent_ui_id = gtk_ui_manager_new_merge_id (totem->ui_manager);
        items = gtk_recent_manager_get_items (recent_manager);
        items = g_list_sort (items, (GCompareFunc) totem_compare_recent_items);

        for (l = items; l && l->data; l = l->next) {
                GtkRecentInfo *info;
                GtkAction     *action;
                char           action_name[32];
                const char    *display_name;
                char          *label;
                char          *escaped_label;

                info = (GtkRecentInfo *) l->data;

                if (!gtk_recent_info_has_group (info, "Totem"))
                        continue;

                g_snprintf (action_name, sizeof (action_name), "RecentFile%u", n_items);

                display_name = gtk_recent_info_get_display_name (info);
                escaped_label = escape_label_for_menu (display_name);

                label = g_strdup_printf ("_%d.  %s", n_items + 1, escaped_label);
                g_free (escaped_label);

                action = gtk_action_new (action_name, label, NULL, NULL);
                g_object_set_data_full (G_OBJECT (action), "recent-info",
                                        gtk_recent_info_ref (info),
                                        (GDestroyNotify) gtk_recent_info_unref);
                g_signal_connect (G_OBJECT (action), "activate",
                                  G_CALLBACK (on_recent_file_item_activated),
                                  totem);

                gtk_action_group_add_action (totem->recent_action_group,
                                            action);
                g_object_unref (action);

                gtk_ui_manager_add_ui (totem->ui_manager, totem->recent_ui_id,
                                      "/tmw-menubar/movie/recent-placeholder",
                                      label, action_name, GTK_UI_MANAGER_MENUITEM,
                                      FALSE);
                g_free (label);

                if (++n_items == 5)
                        break;
        }

        g_list_foreach (items, (GFunc) gtk_recent_info_unref, NULL);
        g_list_free (items);
}

void
totem_setup_recent (Totem *totem)
{
	GdkScreen *screen;
	screen = gtk_widget_get_screen (totem->win);
	totem->recent_manager = gtk_recent_manager_get_for_screen (screen);
	totem->recent_action_group = NULL;
	totem->recent_ui_id = 0;

	g_signal_connect (G_OBJECT (totem->recent_manager), "changed",
			G_CALLBACK (totem_recent_manager_changed_callback),
			totem);

	totem_recent_manager_changed_callback (totem->recent_manager, totem);
}

void
totem_action_add_recent (Totem *totem, const char *filename)
{
	GtkRecentData data;
	char *groups[] = { NULL, NULL };

	data.mime_type = gnome_vfs_get_mime_type (filename);
	if (data.mime_type == NULL) {
		/* No mime-type means warnings, and it breaks when adding
		 * non-gnome-vfs supported URI schemes */
		return;
	}
	data.display_name = NULL;

	if (strstr (filename, "file:///") == NULL) {
		/* It's a URI/stream */
		groups[0] = "TotemStreams";
	} else {
		char *display;

		/* Local files with no mime-type probably don't exist */
		if (data.mime_type == NULL)
			return;

		/* It's a local file */
		display = g_filename_from_uri (filename, NULL, NULL);
		if (display) {
			data.display_name = g_filename_display_basename (display);
			g_free (display);
		}
		groups[0] = "Totem";
	}

	data.description = NULL;
	data.app_name = g_strdup (g_get_application_name ());
	data.app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
	data.groups = groups;
	gtk_recent_manager_add_full (totem->recent_manager,
			filename, &data);

	g_free (data.display_name);
	g_free (data.mime_type);
	g_free (data.app_name);
	g_free (data.app_exec);
}

/* Play Disc menu items */

static void
on_play_disc_activate (GtkAction *action, Totem *totem)
{
	char *device_path;

	device_path = g_object_get_data (G_OBJECT (action), "device_path");
	totem_action_play_media_device (totem, device_path);
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

static char *
my_gnome_vfs_volume_get_mount_path (GnomeVFSVolume *volume)
{
	char *uri, *path;

	uri = gnome_vfs_volume_get_activation_uri (volume);
	path = g_filename_from_uri (uri, NULL, NULL);
	g_free (uri);

	if (path == NULL)
		return gnome_vfs_volume_get_device_path (volume);
	return path;
}

static void
add_device_to_menu (GObject *device, guint position, Totem *totem)
{
	char *name, *escaped_name, *icon_name, *device_path;
	char *label, *activation_uri;
	GtkAction *action;
	gboolean disabled = FALSE;

	/* Add devices with blank CDs and audio CDs in them, but disable them */
	activation_uri = fake_gnome_vfs_device_get_something (device,
		&gnome_vfs_volume_get_activation_uri,
		&gnome_vfs_drive_get_activation_uri);
	if (activation_uri != NULL) {
		if (g_str_has_prefix (activation_uri, "burn://") != FALSE || g_str_has_prefix (activation_uri, "cdda://") != FALSE) {
			disabled = TRUE;
		}
		g_free (activation_uri);
	} else {
		if (GNOME_IS_VFS_DRIVE (device)) {
			device_path = gnome_vfs_drive_get_device_path
				(GNOME_VFS_DRIVE (device));
			disabled = !totem_cd_has_medium (device_path);
			g_free (device_path);
		}
	}

	name = fake_gnome_vfs_device_get_something (device,
		&gnome_vfs_volume_get_display_name,
		&gnome_vfs_drive_get_display_name);
	icon_name = fake_gnome_vfs_device_get_something (device,
		&gnome_vfs_volume_get_icon, &gnome_vfs_drive_get_icon);
	device_path = fake_gnome_vfs_device_get_something (device,
		&my_gnome_vfs_volume_get_mount_path,
		&gnome_vfs_drive_get_device_path);

	g_strstrip (name);
	escaped_name = escape_label_for_menu (name);
	g_free (name);
	label = g_strdup_printf (_("Play Disc '%s'"), escaped_name);
	g_free (escaped_name);

	name = g_strdup_printf (_("device%d"), position);
	action = gtk_action_new (name, label, NULL, NULL);
	g_object_set (G_OBJECT (action), "icon-name", icon_name,
		"sensitive", !disabled, NULL);
	gtk_action_group_add_action (totem->devices_action_group, action);
	g_object_unref (action);
	gtk_ui_manager_add_ui (totem->ui_manager, totem->devices_ui_id,
		"/tmw-menubar/movie/devices-placeholder", name, name,
		GTK_UI_MANAGER_MENUITEM, FALSE);
	g_free (name);
	g_free (label);
	g_free (icon_name);

	g_object_set_data_full (G_OBJECT (action), "device_path",
			device_path, (GDestroyNotify) g_free);
	if (GNOME_IS_VFS_VOLUME (device)) {
		g_object_set_data_full (G_OBJECT (action), "activation_uri",
				gnome_vfs_volume_get_activation_uri (GNOME_VFS_VOLUME (device)),
				(GDestroyNotify) g_free);
	}

	g_signal_connect (G_OBJECT (action), "activate",
			G_CALLBACK (on_play_disc_activate), totem);
}

static void
on_movie_menu_select (GtkMenuItem *movie_menuitem, Totem *totem)
{
	GList *devices, *volumes, *drives, *i;
	guint position;

	if (totem->drives_changed == FALSE)
		return;

	/* Remove old UI */
	gtk_ui_manager_remove_ui (totem->ui_manager, totem->devices_ui_id);
	gtk_ui_manager_ensure_update (totem->ui_manager);

	/* Create new ActionGroup */
	if (totem->devices_action_group) {
		gtk_ui_manager_remove_action_group (totem->ui_manager,
				totem->devices_action_group);
		g_object_unref (totem->devices_action_group);
	}
	totem->devices_action_group = gtk_action_group_new ("devices-action-group");
	gtk_ui_manager_insert_action_group (totem->ui_manager,
			totem->devices_action_group, -1);

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
	position = 0;

	for (i = devices; i != NULL; i = i->next)
	{
		position++;
		add_device_to_menu (i->data, position, totem);
	}
	gtk_ui_manager_ensure_update (totem->ui_manager);

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

	item = gtk_ui_manager_get_widget (totem->ui_manager, "/tmw-menubar/movie");
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

static void
open_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_open (totem);
}

static void
open_location_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_open_location (totem);
}

static void
eject_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_eject (totem);
}

static void
properties_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_show_properties (totem);
}

static void
play_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_play_pause (totem);
}

static void
quit_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_exit (totem);
}

static void
take_screenshot_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_take_screenshot (totem);
}

static void
preferences_action_callback (GtkAction *action, Totem *totem)
{
	gtk_widget_show (totem->prefs);
}

static void
fullscreen_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_fullscreen_toggle (totem);
}

static void
zoom_1_2_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 0.5); 
}

static void
zoom_1_1_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 1);
}

static void
zoom_2_1_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 2);
}

static void
zoom_in_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_zoom_relative (totem, ZOOM_IN_OFFSET);
}

static void
zoom_reset_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_zoom_reset (totem);
}

static void
zoom_out_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_zoom_relative (totem, ZOOM_OUT_OFFSET);
}

static void
next_angle_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_next_angle (totem);
}

static void
dvd_root_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU);
}

static void
dvd_title_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_TITLE_MENU);
}

static void
dvd_audio_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_AUDIO_MENU);
}

static void
dvd_angle_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ANGLE_MENU);
}

static void
dvd_chapter_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_CHAPTER_MENU);
}

static void
next_chapter_action_callback (GtkAction *action, Totem *totem)
{
	TOTEM_PROFILE (totem_action_next (totem));
}

static void
previous_chapter_action_callback (GtkAction *action, Totem *totem)
{
	TOTEM_PROFILE (totem_action_previous (totem));
}

static void
skip_forward_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_seek_relative (totem, SEEK_FORWARD_OFFSET);
}

static void
skip_backwards_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_seek_relative (totem, SEEK_BACKWARD_OFFSET);
}

static void
volume_up_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_volume_relative (totem, VOLUME_UP_OFFSET);
}

static void
volume_down_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_volume_relative (totem, VOLUME_DOWN_OFFSET);
}

static void
contents_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_show_help (totem);
}

static void
about_action_callback (GtkAction *action, Totem *totem)
{
	char *backend_version, *description;
	const char *frontend_type;

	const char *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
		"Ronald Bultje <rbultje@ronald.bitfreak.net>",
		"Julien Moutte <julien@moutte.net> (GStreamer backend)",
		"Tim-Philipp M\303\274ller <tim\100centricular\056net> (GStreamer backend)",
		NULL
	};
	const char *artists[] = { "Jakub Steiner <jimmac@ximian.com>", NULL };
	const char *documenters[] =
	{
		"Chee Bin Hoh <cbhoh@gnome.org>",
		NULL
	};
	char *license = totem_interface_get_license ();

#ifdef HAVE_GTK_ONLY
	frontend_type = N_("GTK+");
#else
	frontend_type = N_("GNOME");
#endif

	backend_version = bacon_video_widget_get_backend_name (totem->bvw);
	/* This lists the back-end and front-end types and versions, such as
	 * Movie Player using GStreamer 0.10.1 and GNOME */
	description = g_strdup_printf (_("Movie Player using %s and %s"),
				       backend_version, _(frontend_type));

	gtk_show_about_dialog (GTK_WINDOW (totem->win),
				     "version", VERSION,
				     "copyright", _("Copyright \xc2\xa9 2002-2006 Bastien Nocera"),
				     "comments", description,
				     "authors", authors,
				     "documenters", documenters,
				     "artists", artists,
				     "translator-credits", _("translator-credits"),
				     "logo-icon-name", "totem",
				     "license", license,
				     "wrap-license", TRUE,
				     "website-label", _("Totem Website"),
				     "website", "http://www.gnome.org/projects/totem/",
				     NULL);

	g_free (backend_version);
	g_free (description);
	g_free (license);
}

static gboolean
totem_plugins_window_delete_cb (GtkWidget *window,
				   GdkEventAny *event,
				   gpointer data)
{
	gtk_widget_hide (window);

	return TRUE;
}

static void
totem_plugins_response_cb (GtkDialog *dialog,
			      int response_id,
			      gpointer data)
{
	if (response_id == GTK_RESPONSE_CLOSE)
		gtk_widget_hide (GTK_WIDGET (dialog));
}


static void
plugins_action_callback (GtkAction *action, Totem *totem)
{
	if (totem->plugins == NULL) {
		GtkWidget *manager;

		totem->plugins = gtk_dialog_new_with_buttons (_("Configure Plugins"),
							      GTK_WINDOW (totem->win),
							      GTK_DIALOG_DESTROY_WITH_PARENT,
							      GTK_STOCK_CLOSE,
							      GTK_RESPONSE_CLOSE,
							      NULL);
		gtk_container_set_border_width (GTK_CONTAINER (totem->plugins), 5);
		gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (totem->plugins)->vbox), 2);
		gtk_dialog_set_has_separator (GTK_DIALOG (totem->plugins), FALSE);

		g_signal_connect_object (G_OBJECT (totem->plugins),
					 "delete_event",
					 G_CALLBACK (totem_plugins_window_delete_cb),
					 NULL, 0);
		g_signal_connect_object (G_OBJECT (totem->plugins),
					 "response",
					 G_CALLBACK (totem_plugins_response_cb),
					 NULL, 0);

		manager = totem_plugin_manager_new ();
		gtk_widget_show_all (GTK_WIDGET (manager));
		gtk_container_add (GTK_CONTAINER (GTK_DIALOG (totem->plugins)->vbox),
				   manager);
	}

	gtk_window_present (GTK_WINDOW (totem->plugins));
}

static void
repeat_mode_action_callback (GtkToggleAction *action, Totem *totem)
{
	totem_playlist_set_repeat (totem->playlist,
			gtk_toggle_action_get_active (action));
}

static void
shuffle_mode_action_callback (GtkToggleAction *action, Totem *totem)
{
	totem_playlist_set_shuffle (totem->playlist,
			gtk_toggle_action_get_active (action));
}

static void
deinterlace_action_callback (GtkToggleAction *action, Totem *totem)
{
	gboolean value;

	value = gtk_toggle_action_get_active (action);
	bacon_video_widget_set_deinterlacing (totem->bvw, value);
	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/deinterlace",
			value, NULL);
}

static void
show_controls_action_callback (GtkToggleAction *action, Totem *totem)
{
	gboolean show;

	show = gtk_toggle_action_get_active (action);

	/* Let's update our controls visibility */
	if (show)
		totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;
	else
		totem->controls_visibility = TOTEM_CONTROLS_HIDDEN;

	show_controls (totem, FALSE);
}

static void
show_sidebar_action_callback (GtkToggleAction *action, Totem *totem)
{
	if (totem_is_fullscreen (totem))
		return;

	totem_sidebar_toggle (totem, gtk_toggle_action_get_active (action));
}

static void
aspect_ratio_changed_callback (GtkRadioAction *action, GtkRadioAction *current, Totem *totem)
{
	totem_action_set_aspect_ratio (totem, gtk_radio_action_get_current_value (current));
}

static void
clear_playlist_action_callback (GtkAction *action, Totem *totem)
{
	totem_playlist_clear (totem->playlist);
	totem_action_set_mrl (totem, NULL);
}

static const GtkActionEntry entries[] = {
	{ "movie-menu", NULL, N_("_Movie") },
	{ "open", GTK_STOCK_OPEN, N_("_Open..."), "<control>O", N_("Open a file"), G_CALLBACK (open_action_callback) },
	{ "open-location", NULL, N_("Open _Location..."), "<control>L", N_("Open a non-local file"), G_CALLBACK (open_location_action_callback) },
	{ "eject", "media-eject", N_("_Eject"), "<control>E", NULL, G_CALLBACK (eject_action_callback) },
	{ "properties", GTK_STOCK_PROPERTIES, N_("_Properties"), "<control>P", NULL, G_CALLBACK (properties_action_callback) },
	{ "play", GTK_STOCK_MEDIA_PLAY, N_("Play / Pa_use"), "P", N_("Play or pause the movie"), G_CALLBACK (play_action_callback) },
	{ "quit", GTK_STOCK_QUIT, N_("_Quit"), "<control>Q", N_("Quit the program"), G_CALLBACK (quit_action_callback) },

	{ "edit-menu", NULL, N_("_Edit") },
	{ "take-screenshot", "camera-photo", N_("Take _Screenshot..."), "<control>S", N_("Take a screenshot"), G_CALLBACK (take_screenshot_action_callback) },
	{ "clear-playlist", NULL, N_("_Clear Playlist"), NULL, N_("Clear playlist"), G_CALLBACK (clear_playlist_action_callback) },
	{ "preferences", GTK_STOCK_PREFERENCES, N_("Prefere_nces"), NULL, NULL, G_CALLBACK (preferences_action_callback) },
	{ "plugins", NULL, N_("Plugins..."), NULL, NULL, G_CALLBACK (plugins_action_callback) },

	{ "view-menu", NULL, N_("_View") },
	{ "fullscreen", "view-fullscreen", N_("_Fullscreen"), "F", N_("Switch to fullscreen"), G_CALLBACK (fullscreen_action_callback) },
	{ "zoom-window-menu", NULL, N_("Fit Window to Movie") },
	{ "zoom-1-2", NULL, N_("_Resize 1:2"), "0", N_("Resize to half the video size"), G_CALLBACK (zoom_1_2_action_callback) },
	{ "zoom-1-1", NULL, N_("Resize _1:1"), "1", N_("Resize to video size"), G_CALLBACK (zoom_1_1_action_callback) },
	{ "zoom-2-1", NULL, N_("Resize _2:1"), "2", N_("Resize to twice the video size"), G_CALLBACK (zoom_2_1_action_callback) },
	{ "aspect-ratio-menu", NULL, N_("_Aspect Ratio") },
	{ "next-angle", NULL, N_("Switch An_gles"), "G", N_("Switch angles"), G_CALLBACK (next_angle_action_callback) },
/*	{ "subtitles-menu", NULL, N_("S_ubtitles") },*/

	{ "go-menu", NULL, N_("_Go") },
	{ "dvd-root-menu", GTK_STOCK_INDEX, N_("_DVD Menu"), "m", N_("Go to the DVD menu"), G_CALLBACK (dvd_root_menu_action_callback) },
	{ "dvd-title-menu", NULL, N_("_Title Menu"), NULL, N_("Go to the title menu"), G_CALLBACK (dvd_title_menu_action_callback) },
	{ "dvd-audio-menu", NULL, N_("A_udio Menu"), NULL, N_("Go to the audio menu"), G_CALLBACK (dvd_audio_menu_action_callback) },
	{ "dvd-angle-menu", NULL, N_("_Angle Menu"), NULL, N_("Go to the angle menu"), G_CALLBACK (dvd_angle_menu_action_callback) },
	{ "dvd-chapter-menu", GTK_STOCK_INDEX, N_("_Chapter Menu"), "c", N_("Go to the chapter menu"), G_CALLBACK (dvd_chapter_menu_action_callback) },
	{ "next-chapter", GTK_STOCK_MEDIA_NEXT, N_("_Next Chapter/Movie"), "n", N_("Next chapter or movie"), G_CALLBACK (next_chapter_action_callback) },
	{ "previous-chapter", GTK_STOCK_MEDIA_PREVIOUS, N_("_Previous Chapter/Movie"), "b", N_("Previous chapter or movie"), G_CALLBACK (previous_chapter_action_callback) },

	{ "sound-menu", NULL, N_("_Sound") },
/*	{ "languages-menu", NULL, N_("_Languages") }, */
	{ "volume-up", "audio-volume-high", N_("Volume _Up"), "Up", N_("Volume up"), G_CALLBACK (volume_up_action_callback) },
	{ "volume-down", "audio-volume-low", N_("Volume _Down"), "Down", N_("Volume down"), G_CALLBACK (volume_down_action_callback) },

	{ "help-menu", NULL, N_("_Help") },
	{ "contents", GTK_STOCK_HELP, N_("_Contents"), "F1", N_("Help contents"), G_CALLBACK (contents_action_callback) },
	{ "about", GTK_STOCK_ABOUT, N_("_About"), NULL, NULL, G_CALLBACK (about_action_callback) }
};

static const GtkActionEntry zoom_entries[] = {
	{ "zoom-in", GTK_STOCK_ZOOM_IN, N_("Zoom In"), "R", N_("Zoom in"), G_CALLBACK (zoom_in_action_callback) },
	{ "zoom-reset", GTK_STOCK_ZOOM_100, N_("Zoom Reset"), NULL, N_("Zoom reset"), G_CALLBACK (zoom_reset_action_callback) },
	{ "zoom-out", GTK_STOCK_ZOOM_OUT, N_("Zoom Out"), "T", N_("Zoom out"), G_CALLBACK (zoom_out_action_callback) }
};

static const GtkActionEntry seek_entries_ltr[] = {
	{ "skip-forward", GTK_STOCK_MEDIA_FORWARD, N_("Skip _Forward"), "Right", N_("Skip forward"), G_CALLBACK (skip_forward_action_callback) },
	{ "skip-backwards", GTK_STOCK_MEDIA_REWIND, N_("Skip _Backwards"), "Left", N_("Skip backwards"), G_CALLBACK (skip_backwards_action_callback) }
};

static const GtkActionEntry seek_entries_rtl[] = {
	{ "skip-forward", GTK_STOCK_MEDIA_FORWARD, N_("Skip _Forward"), "Left", N_("Skip forward"), G_CALLBACK (skip_forward_action_callback) },
	{ "skip-backwards", GTK_STOCK_MEDIA_REWIND, N_("Skip _Backwards"), "Right", N_("Skip backwards"), G_CALLBACK (skip_backwards_action_callback) }
};

static const GtkToggleActionEntry toggle_entries[] = {
	{ "repeat-mode", NULL, N_("_Repeat Mode"), NULL, N_("Set the repeat mode"), G_CALLBACK (repeat_mode_action_callback), FALSE },
	{ "shuffle-mode", NULL, N_("Shuff_le Mode"), NULL, N_("Set the shuffle mode"), G_CALLBACK (shuffle_mode_action_callback), FALSE },
	{ "deinterlace", NULL, N_("_Deinterlace"), "I", N_("Deinterlace"), G_CALLBACK (deinterlace_action_callback), FALSE },
	{ "show-controls", NULL, N_("Show _Controls"), "H", N_("Show controls"), G_CALLBACK (show_controls_action_callback), TRUE },
	{ "sidebar", NULL, N_("_Sidebar"), "F9", N_("Show or hide the sidebar"), G_CALLBACK (show_sidebar_action_callback), TRUE }
};

static const GtkRadioActionEntry aspect_ratio_entries[] = {
	{ "aspect-ratio-auto", NULL, N_("Auto"), NULL, N_("Sets automatic aspect ratio"), BVW_RATIO_AUTO },
	{ "aspect-ratio-square", NULL, N_("Square"), NULL, N_("Sets square aspect ratio"), BVW_RATIO_SQUARE },
	{ "aspect-ratio-fbt", NULL, N_("4:3 (TV)"), NULL, N_("Sets 4:3 (TV) aspect ratio"), BVW_RATIO_FOURBYTHREE },
	{ "aspect-ratio-anamorphic", NULL, N_("16:9 (Widescreen)"), NULL, N_("Sets 16:9 (Anamorphic) aspect ratio"), BVW_RATIO_ANAMORPHIC },
	{ "aspect-ratio-dvb", NULL, N_("2.11:1 (DVB)"), NULL, N_("Sets 2.11:1 (DVB) aspect ratio"), BVW_RATIO_DVB }
};

static void
totem_ui_manager_connect_proxy_callback (GtkUIManager *ui_manager,
		GtkAction *action, GtkWidget *widget, Totem *totem)
{
	GtkRecentInfo *recent_info;
	GdkPixbuf *icon;
	GtkWidget *image;
	gint w, h;

	recent_info = g_object_get_data (G_OBJECT (action), "recent-info");

	if (recent_info == NULL) {
		return;
	}

	if (GTK_IS_IMAGE_MENU_ITEM (widget)) {
		gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
		icon = gtk_recent_info_get_icon (recent_info, w);

		if (icon != NULL) {
			image = gtk_image_new_from_pixbuf (icon);
			gtk_image_menu_item_set_image
				(GTK_IMAGE_MENU_ITEM (widget), image);
		}
	}
}

void
totem_ui_manager_setup (Totem *totem)
{
	char *filename;
	GtkWidget *menubar;
	GtkWidget *menubar_box;
	GtkAction *action;

	totem->main_action_group = gtk_action_group_new ("main-action-group");
	gtk_action_group_set_translation_domain (totem->main_action_group,
			GETTEXT_PACKAGE);
	gtk_action_group_add_actions (totem->main_action_group, entries,
		G_N_ELEMENTS (entries), totem);
	if (gtk_widget_get_direction (totem->win) == GTK_TEXT_DIR_RTL) {
		gtk_action_group_add_actions (totem->main_action_group,
				seek_entries_rtl,
				G_N_ELEMENTS (seek_entries_rtl), totem);
	} else {
		gtk_action_group_add_actions (totem->main_action_group,
				seek_entries_ltr,
				G_N_ELEMENTS (seek_entries_ltr), totem);
	}
	gtk_action_group_add_toggle_actions (totem->main_action_group, 
		toggle_entries, G_N_ELEMENTS (toggle_entries), totem);
	gtk_action_group_add_radio_actions (totem->main_action_group,
		aspect_ratio_entries, G_N_ELEMENTS (aspect_ratio_entries), 0,
		G_CALLBACK (aspect_ratio_changed_callback), totem);

	action = g_object_new (GTK_TYPE_ACTION,
			"name", "subtitles-menu",
			"label", _("S_ubtitles"),
			"hide-if-empty", FALSE, NULL);
	gtk_action_group_add_action (totem->main_action_group, action);
	g_object_unref (action);
	action = g_object_new (GTK_TYPE_ACTION,
			"name", "languages-menu",
			"label", _("_Languages"),
			"hide-if-empty", FALSE, NULL);
	gtk_action_group_add_action (totem->main_action_group, action);
	g_object_unref (action);

	/* Hide help if we're using GTK+ only */
#ifdef HAVE_GTK_ONLY
	action = gtk_action_group_get_action
		(totem->main_action_group, "contents");
	gtk_action_set_visible (action, FALSE);
#endif /* HAVE_GTK_ONLY */

	totem->zoom_action_group = gtk_action_group_new ("zoom-action-group");
	gtk_action_group_set_translation_domain (totem->zoom_action_group,
			GETTEXT_PACKAGE);
	gtk_action_group_add_actions (totem->zoom_action_group, zoom_entries,
		G_N_ELEMENTS (zoom_entries), totem);

	totem->ui_manager = gtk_ui_manager_new ();
	g_signal_connect (G_OBJECT (totem->ui_manager), "connect-proxy",
			G_CALLBACK (totem_ui_manager_connect_proxy_callback),
			totem);
	gtk_ui_manager_insert_action_group (totem->ui_manager,
			totem->main_action_group, 0);
	gtk_ui_manager_insert_action_group (totem->ui_manager,
			totem->zoom_action_group, -1);

	totem->devices_action_group = NULL;
	totem->devices_ui_id = gtk_ui_manager_new_merge_id (totem->ui_manager);
	totem->languages_action_group = NULL;
	totem->languages_ui_id = gtk_ui_manager_new_merge_id
		(totem->ui_manager);
	totem->subtitles_action_group = NULL;
	totem->subtitles_ui_id = gtk_ui_manager_new_merge_id
		(totem->ui_manager);

	gtk_window_add_accel_group (GTK_WINDOW (totem->win),
			gtk_ui_manager_get_accel_group (totem->ui_manager));

	filename = totem_interface_get_full_path ("totem-ui.xml");
	if (gtk_ui_manager_add_ui_from_file (totem->ui_manager,
				filename, NULL) == 0) {
		totem_interface_error_blocking (
			_("Couldn't load the 'ui description' file"),
			_("Make sure that Totem is properly installed."),
			GTK_WINDOW (totem->win));
		totem_action_exit (NULL);
	}
	g_free (filename);

	menubar = gtk_ui_manager_get_widget (totem->ui_manager, "/tmw-menubar");
	menubar_box = glade_xml_get_widget (totem->xml, "tmw_menubar_box");
	gtk_box_pack_start (GTK_BOX (menubar_box), menubar, FALSE, FALSE, 0);
}

