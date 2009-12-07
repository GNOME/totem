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
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include "totem-menu.h"
#include "totem.h"
#include "totem-interface.h"
#include "totem-private.h"
#include "totem-sidebar.h"
#include "totem-statusbar.h"
#include "totem-plugin-manager.h"
#include "bacon-video-widget.h"
#include "totem-uri.h"

#include "totem-profile.h"

#define TOTEM_MAX_RECENT_ITEM_LEN 40

/* Callback functions for GtkBuilder */
G_MODULE_EXPORT void open_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void open_location_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void eject_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void properties_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void play_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void quit_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void preferences_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void fullscreen_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void zoom_1_2_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void zoom_1_1_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void zoom_2_1_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void zoom_in_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void zoom_reset_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void zoom_out_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void next_angle_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void dvd_root_menu_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void dvd_title_menu_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void dvd_audio_menu_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void dvd_angle_menu_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void dvd_chapter_menu_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void next_chapter_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void previous_chapter_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void skip_forward_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void skip_backwards_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void volume_up_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void volume_down_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void contents_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void about_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void plugins_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void repeat_mode_action_callback (GtkToggleAction *action, Totem *totem);
G_MODULE_EXPORT void shuffle_mode_action_callback (GtkToggleAction *action, Totem *totem);
G_MODULE_EXPORT void deinterlace_action_callback (GtkToggleAction *action, Totem *totem);
G_MODULE_EXPORT void show_controls_action_callback (GtkToggleAction *action, Totem *totem);
G_MODULE_EXPORT void show_sidebar_action_callback (GtkToggleAction *action, Totem *totem);
G_MODULE_EXPORT void aspect_ratio_changed_callback (GtkRadioAction *action, GtkRadioAction *current, Totem *totem);
G_MODULE_EXPORT void select_subtitle_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void clear_playlist_action_callback (GtkAction *action, Totem *totem);

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
		const char **paths, const char *prefix, const char *lang, 
		int lang_id, int lang_index, GSList **group)
{
	const char *full_lang;
	char *label;
	char *name;
	GtkAction *action;
	guint i;

	full_lang = totem_lang_get_full (lang);

	if (lang_index > 1) {
		char *num_lang;

		num_lang = g_strdup_printf ("%s #%u",
					    full_lang ? full_lang : lang,
					    lang_index);
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
	for (i = 0; paths[i] != NULL; i++) {
		gtk_ui_manager_add_ui (totem->ui_manager, ui_id,
				       paths[i], name, name, GTK_UI_MANAGER_MENUITEM, FALSE);
	}
	g_free (name);

	return action;
}

static GtkAction *
create_lang_actions (Totem *totem, GtkActionGroup *action_group, guint ui_id,
		const char **paths, const char *prefix, GList *list,
		gboolean is_lang)
{
	GtkAction *action = NULL;
	unsigned int i, *hash_value;
	GList *l;
	GSList *group = NULL;
	GHashTable *lookup;
	char *action_data;

	if (is_lang == FALSE) {
		add_lang_action (totem, action_group, ui_id, paths, prefix,
				_("None"), -2, 0, &group);
	}

	action = add_lang_action (totem, action_group, ui_id, paths, prefix,
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

		add_lang_action (totem, action_group, ui_id, paths, prefix,
				 action_data, i, num + 1, &group);
		g_free (action_data);
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
	const char *paths[3] = { "/tmw-menubar/sound/languages/placeholder", "/totem-main-popup/popup-languages/placeholder", NULL };
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

	if (list != NULL) {
		action = create_lang_actions (totem, totem->languages_action_group,
				totem->languages_ui_id,
				paths,
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
	const char *paths[3] = { "/tmw-menubar/view/subtitles/placeholder", "/totem-main-popup/popup-subtitles/placeholder", NULL };

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


	if (list != NULL) {
		action = create_lang_actions (totem, totem->subtitles_action_group,
				totem->subtitles_ui_id,
				paths,
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
        GtkRecentInfo *recent_info;
        GdkPixbuf *icon;
        GtkWidget *image = NULL;
        gint w, h;

        if (!GTK_IS_MENU_ITEM (proxy))
                return;

        label = GTK_LABEL (GTK_BIN (proxy)->child);

        gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars (label,TOTEM_MAX_RECENT_ITEM_LEN);

        if (!GTK_IS_IMAGE_MENU_ITEM (proxy))
                return;

        recent_info = g_object_get_data (G_OBJECT (action), "recent-info");
        g_assert (recent_info != NULL);

        gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);

        icon = gtk_recent_info_get_icon (recent_info, w);
        if (icon != NULL) {
                image = gtk_image_new_from_pixbuf (icon);
                g_object_unref (icon);
        }

        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (proxy), image);
        gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (proxy), TRUE);
}

static void
on_recent_file_item_activated (GtkAction *action,
                               Totem *totem)
{
	GtkRecentInfo *recent_info;
	const gchar *uri;

	recent_info = g_object_get_data (G_OBJECT (action), "recent-info");
	uri = gtk_recent_info_get_uri (recent_info);

	totem_add_to_playlist_and_play (totem, uri, NULL, FALSE);
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
        GList *items, *totem_items, *l;
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

	/* Remove the non-Totem items */
	totem_items = NULL;
        for (l = items; l && l->data; l = l->next) {
                GtkRecentInfo *info;

                info = (GtkRecentInfo *) l->data;

                if (gtk_recent_info_has_group (info, "Totem")) {
                	gtk_recent_info_ref (info);
                	totem_items = g_list_prepend (totem_items, info);
		}
	}
	g_list_foreach (items, (GFunc) gtk_recent_info_unref, NULL);
        g_list_free (items);

        totem_items = g_list_sort (totem_items, (GCompareFunc) totem_compare_recent_items);

        for (l = totem_items; l && l->data; l = l->next) {
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

        g_list_foreach (totem_items, (GFunc) gtk_recent_info_unref, NULL);
        g_list_free (totem_items);
}

void
totem_setup_recent (Totem *totem)
{
	totem->recent_manager = gtk_recent_manager_get_default ();
	totem->recent_action_group = NULL;
	totem->recent_ui_id = 0;

	g_signal_connect (G_OBJECT (totem->recent_manager), "changed",
			G_CALLBACK (totem_recent_manager_changed_callback),
			totem);

	totem_recent_manager_changed_callback (totem->recent_manager, totem);
}

static void
recent_info_cb (GFile *file,
		GAsyncResult *res,
		Totem *totem)
{
	GtkRecentData data;
	char *groups[] = { NULL, NULL };
	GFileInfo *file_info;
	const char *uri, *display_name;

	memset (&data, 0, sizeof (data));

	file_info = g_file_query_info_finish (file, res, NULL);
	uri = g_object_get_data (G_OBJECT (file), "uri");
	display_name = g_object_get_data (G_OBJECT (file), "display_name");

	/* Probably an unsupported URI scheme */
	if (file_info == NULL) {
		data.display_name = g_strdup (display_name);
		/* Bogus mime-type, we just want it added */
		data.mime_type = g_strdup ("video/x-totem-stream");
		groups[0] = (gchar*) "TotemStreams";
	} else {
		data.mime_type = g_strdup (g_file_info_get_content_type (file_info));
		data.display_name = g_strdup (g_file_info_get_display_name (file_info));
		g_object_unref (file_info);
		groups[0] = (gchar*) "Totem";
	}

	data.app_name = g_strdup (g_get_application_name ());
	data.app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
	data.groups = groups;
	if (gtk_recent_manager_add_full (totem->recent_manager,
					 uri, &data) == FALSE) {
		g_warning ("Couldn't add recent file for '%s'", uri);
	}

	g_free (data.display_name);
	g_free (data.mime_type);
	g_free (data.app_name);
	g_free (data.app_exec);

	g_object_unref (file);
}

void
totem_action_add_recent (Totem *totem, const char *uri, const char *display_name)
{
	GFile *file;

	if (totem_is_special_mrl (uri) != FALSE)
		return;

	file = g_file_new_for_uri (uri);
	g_object_set_data_full (G_OBJECT (file), "uri", g_strdup (uri), g_free);
	g_object_set_data_full (G_OBJECT (file), "display_name", g_strdup (display_name), g_free);
	g_file_query_info_async (file,
				 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
				 G_FILE_QUERY_INFO_NONE, 0, NULL, (GAsyncReadyCallback) recent_info_cb, totem);
}

/* Play Disc menu items */

static void
on_play_disc_activate (GtkAction *action, Totem *totem)
{
	char *device_path;

	device_path = g_object_get_data (G_OBJECT (action), "device_path");
	totem_action_play_media_device (totem, device_path);
}

/* Play DVB menu items */
static void
on_play_dvb_activate (GtkAction *action, Totem *totem)
{
	int adapter;
	char *str;

	adapter = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), "adapter"));
	str = g_strdup_printf ("%d", adapter);
	totem_action_play_media (totem, MEDIA_TYPE_DVB, str);
	g_free (str);
}

static const char *
get_icon_name_for_gicon (GtkIconTheme *theme,
			 GIcon *icon)
{
	const char * const *icon_names;
	const char *icon_name;
	guint j;

	icon_name = NULL;

	if (G_IS_EMBLEMED_ICON (icon) != FALSE) {
		GIcon *new_icon;
		new_icon = g_emblemed_icon_get_icon (G_EMBLEMED_ICON (icon));
		g_object_unref (icon);
		icon = g_object_ref (new_icon);
	}

	if (G_IS_THEMED_ICON (icon)) {
		icon_names = g_themed_icon_get_names (G_THEMED_ICON (icon));

		for (j = 0; icon_names[j] != NULL; j++) {
			icon_name = icon_names[j];
			if (gtk_icon_theme_has_icon (theme, icon_name) != FALSE)
				break;
		}
	}

	return icon_name;
}

static char *
unescape_archive_name (GFile *root)
{
	char *uri;
	guint len;
	char *escape1, *escape2;

	uri = g_file_get_uri (root);

	/* Remove trailing slash */
	len = strlen (uri);
	if (uri[len - 1] == '/')
		uri[len - 1] = '\0';

	/* Unescape the path */
	escape1 = g_uri_unescape_string (uri + strlen ("archive://"), NULL);
	escape2 = g_uri_unescape_string (escape1, NULL);
	g_free (escape1);
	g_free (uri);

	return escape2;
}

static void
add_mount_to_menu (GMount *mount,
		   GtkIconTheme *theme,
		   guint position,
		   Totem *totem)
{
	char *name, *escaped_name, *label;
	GtkAction *action;
	GIcon *icon;
	const char *icon_name;
	char *device_path;

	GVolume *volume;
	GFile *root, *iso;
	char **content_types;
	gboolean has_content;
	guint i;

	/* Check whether we have an archive mount */
	volume = g_mount_get_volume (mount);
	if (volume != NULL) {
		g_object_unref (volume);
		return;
	}

	root = g_mount_get_root (mount);
	if (g_file_has_uri_scheme (root, "archive") == FALSE) {
		g_object_unref (root);
		return;
	}

	/* Check whether it's a DVD or VCD image */
	content_types = g_content_type_guess_for_tree (root);
	if (content_types == NULL ||
	    g_strv_length (content_types) == 0) {
		g_strfreev (content_types);
		g_object_unref (root);
		return;
	}

	has_content = FALSE;
	for (i = 0; content_types[i] != NULL; i++) {
		/* XXX: Keep in sync with mime-type-list.txt */
		if (g_str_equal (content_types[i], "x-content/video-dvd") ||
		    g_str_equal (content_types[i], "x-content/video-vcd") ||
		    g_str_equal (content_types[i], "x-content/video-svcd")) {
			has_content = TRUE;
			break;
		}
	}
	g_strfreev (content_types);

	if (has_content == FALSE) {
		g_object_unref (root);
		return;
	}

	device_path = unescape_archive_name (root);
	g_object_unref (root);

	/* And ensure it's a local path */
	iso = g_file_new_for_uri (device_path);
	g_free (device_path);
	device_path = g_file_get_path (iso);
	g_object_unref (iso);

	/* Work out an icon to display */
	icon = g_mount_get_icon (mount);
	icon_name = get_icon_name_for_gicon (theme, icon);

	/* Get the mount's pretty name for the menu label */
	name = g_mount_get_name (mount);
	g_strstrip (name);
	escaped_name = escape_label_for_menu (name);
	g_free (name);
	/* Translators:
	 * This is not a JPEG image, but a disc image, for example,
	 * an ISO file */
	label = g_strdup_printf (_("Play Image '%s'"), escaped_name);
	g_free (escaped_name);

	name = g_strdup_printf (_("device%d"), position);

	action = gtk_action_new (name, label, NULL, NULL);
	g_object_set (G_OBJECT (action),
		      "icon-name", icon_name, NULL);
	gtk_action_set_always_show_image (action, TRUE);
	gtk_action_group_add_action (totem->devices_action_group, action);
	g_object_unref (action);

	gtk_ui_manager_add_ui (totem->ui_manager, totem->devices_ui_id,
			       "/tmw-menubar/movie/devices-placeholder", name, name,
			       GTK_UI_MANAGER_MENUITEM, FALSE);

	g_free (name);
	g_free (label);
	g_object_unref (icon);

	g_object_set_data_full (G_OBJECT (action),
				"device_path", device_path,
				(GDestroyNotify) g_free);

	g_signal_connect (G_OBJECT (action), "activate",
			  G_CALLBACK (on_play_disc_activate), totem);
}

static void
add_volume_to_menu (GVolume *volume,
		    GDrive *drive,
		    GtkIconTheme *theme,
		    guint position,
		    Totem *totem)
{
	char *name, *escaped_name, *label;
	GtkAction *action;
	gboolean disabled;
	GIcon *icon;
	const char *icon_name;
	char *device_path;
	GtkWidget *menu_item;
	char *menu_item_path;

	disabled = FALSE;

	/* Add devices with blank CDs and audio CDs in them, but disable them */
	if (drive != NULL) {
		device_path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
		if (device_path == NULL)
			return;
	}

	/* Check whether we have a media... */
	if (drive != NULL &&
	    g_drive_has_media (drive) == FALSE) {
		disabled = TRUE;
	} else {
		/* ... Or an audio CD or a blank media */
		GMount *mount;
		GFile *root;

		mount = g_volume_get_mount (volume);
		if (mount != NULL) {
			root = g_mount_get_root (mount);
			g_object_unref (mount);

			if (g_file_has_uri_scheme (root, "burn") != FALSE || g_file_has_uri_scheme (root, "cdda") != FALSE)
				disabled = TRUE;
			g_object_unref (root);
		}
	}

	/* Work out an icon to display */
	icon = g_volume_get_icon (volume);
	icon_name = get_icon_name_for_gicon (theme, icon);

	/* Get the volume's pretty name for the menu label */
	name = g_volume_get_name (volume);
	g_strstrip (name);
	escaped_name = escape_label_for_menu (name);
	g_free (name);
	label = g_strdup_printf (_("Play Disc '%s'"), escaped_name);
	g_free (escaped_name);

	name = g_strdup_printf (_("device%d"), position);

	action = gtk_action_new (name, label, NULL, NULL);
	g_object_set (G_OBJECT (action),
		      "icon-name", icon_name,
		      "sensitive", !disabled, NULL);
	gtk_action_group_add_action (totem->devices_action_group, action);
	g_object_unref (action);

	gtk_ui_manager_add_ui (totem->ui_manager, totem->devices_ui_id,
			       "/tmw-menubar/movie/devices-placeholder", name, name,
			       GTK_UI_MANAGER_MENUITEM, FALSE);

	/* TODO: This can be made cleaner once bug #589842 is fixed */
	menu_item_path = g_strdup_printf ("/tmw-menubar/movie/devices-placeholder/%s", name);
	menu_item = gtk_ui_manager_get_widget (totem->ui_manager, menu_item_path);
	g_free (menu_item_path);

	if (menu_item != NULL)
		gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menu_item), TRUE);

	g_free (name);
	g_free (label);
	g_object_unref (icon);

	if (disabled != FALSE) {
		g_free (device_path);
		return;
	}

	g_object_set_data_full (G_OBJECT (action),
				"device_path", device_path,
				(GDestroyNotify) g_free);

	g_signal_connect (G_OBJECT (action), "activate",
			  G_CALLBACK (on_play_disc_activate), totem);
}

static void
add_drive_to_menu (GDrive *drive,
		   GtkIconTheme *theme,
		   guint position,
		   Totem *totem)
{
	GList *volumes, *i;

	/* FIXME: We used to explicitly check whether it was a CD/DVD drive
	 * Use:
	 * udi = g_volume_get_identifier (i->data, G_VOLUME_IDENTIFIER_KIND_HAL_UDI); */
	if (g_drive_can_eject (drive) == FALSE)
		return;

	/* Repeat for all the drive's volumes */
	volumes = g_drive_get_volumes (drive);

	for (i = volumes; i != NULL; i = i->next) {
		GVolume *volume = i->data;
		add_volume_to_menu (volume, drive, theme, position, totem);
		g_object_unref (volume);
	}

	g_list_free (volumes);
}

static void
update_drive_menu_items (GtkMenuItem *movie_menuitem, Totem *totem)
{
	GList *drives, *mounts, *i;
	GtkIconTheme *theme;
	guint position;

	/* Add any suitable devices to the menu */
	position = 0;

	theme = gtk_icon_theme_get_default ();

	drives = g_volume_monitor_get_connected_drives (totem->monitor);
	for (i = drives; i != NULL; i = i->next) {
		GDrive *drive = i->data;

		position++;
		add_drive_to_menu (drive, theme, position, totem);
		g_object_unref (drive);
	}
	g_list_free (drives);

	/* Look for mounted archives */
	mounts = g_volume_monitor_get_mounts (totem->monitor);
	for (i = mounts; i != NULL; i = i->next) {
		GMount *mount = i->data;

		position++;
		add_mount_to_menu (mount, theme, position, totem);
		g_object_unref (mount);
	}
	g_list_free (mounts);

	totem->drives_changed = FALSE;
}

static void
update_dvb_menu_items (GtkMenuItem *movie_menuitem, Totem *totem)
{
	guint i;

	for (i = 0 ; i < 8 ; i++) {
		char *devicenode;

		devicenode = g_strdup_printf("/dev/dvb/adapter%d/frontend0", i);

		if (g_file_test (devicenode, G_FILE_TEST_EXISTS) != FALSE) {
			char* label, *name, *adapter_name;
			GtkAction* action;

			/* translators: the index of the adapter
			 * DVB Adapter 1 */
			adapter_name = g_strdup_printf (_("DVB Adapter %u"), i);
			/* translators:
			 * Watch TV on 'DVB Adapter 1'
			 * or
			 * Watch TV on 'Hauppauge Nova-T Stick' */
			label = g_strdup_printf (_("Watch TV on \'%s\'"), adapter_name);
			g_free (adapter_name);
			name = g_strdup_printf ("dvbdevice%d", i);
			action = gtk_action_new (name, label, NULL, NULL);

			g_object_set (G_OBJECT(action), "icon-name", "totem-tv", "sensitive", TRUE, NULL);
			gtk_action_group_add_action (totem->devices_action_group, action);
			g_object_unref (action);
			gtk_ui_manager_add_ui (totem->ui_manager, totem->devices_ui_id,
					       "/tmw-menubar/movie/devices-placeholder", name, name,
					       GTK_UI_MANAGER_MENUITEM, FALSE);
			g_object_set_data_full (G_OBJECT (action),
						"adapter", GINT_TO_POINTER (i), NULL);
			g_signal_connect (G_OBJECT (action), "activate",
					  G_CALLBACK (on_play_dvb_activate), totem);
		}
		g_free (devicenode);
	}
}

static void
on_movie_menu_select (GtkMenuItem *movie_menuitem, Totem *totem)
{
	//FIXME we should check whether there's new DVB items
/*	if (totem->drives_changed == FALSE)
		return;
*/
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

	update_drive_menu_items (movie_menuitem, totem);

	/* Check for DVB */
	/* FIXME we should only update if we have an updated as per HAL */
	update_dvb_menu_items (movie_menuitem, totem);

	gtk_ui_manager_ensure_update (totem->ui_manager);
}

static void
on_g_volume_monitor_event (GVolumeMonitor *monitor,
			   gpointer *device,
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
			"volume-added",
			G_CALLBACK (on_g_volume_monitor_event), totem);
	g_signal_connect (G_OBJECT (totem->monitor),
			"volume-removed",
			G_CALLBACK (on_g_volume_monitor_event), totem);
	g_signal_connect (G_OBJECT (totem->monitor),
			"mount-added",
			G_CALLBACK (on_g_volume_monitor_event), totem);
	g_signal_connect (G_OBJECT (totem->monitor),
			"mount-removed",
			G_CALLBACK (on_g_volume_monitor_event), totem);

	totem->drives_changed = TRUE;
}

void
open_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_open (totem);
}

void
open_location_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_open_location (totem);
}

void
eject_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_eject (totem);
}

void
properties_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_show_properties (totem);
}

void
play_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_play_pause (totem);
}

G_GNUC_NORETURN void
quit_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_exit (totem);
}

void
preferences_action_callback (GtkAction *action, Totem *totem)
{
	gtk_widget_show (totem->prefs);
}

void
fullscreen_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_fullscreen_toggle (totem);
}

void
zoom_1_2_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 0.5); 
}

void
zoom_1_1_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 1);
}

void
zoom_2_1_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_set_scale_ratio (totem, 2);
}

void
zoom_in_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_zoom_relative (totem, ZOOM_IN_OFFSET);
}

void
zoom_reset_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_zoom_reset (totem);
}

void
zoom_out_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_zoom_relative (totem, ZOOM_OUT_OFFSET);
}

void
select_subtitle_action_callback (GtkAction *action, Totem *totem)
{
	totem_playlist_select_subtitle_dialog (totem->playlist,
					       TOTEM_PLAYLIST_DIALOG_PLAYING);
} 

void
next_angle_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_next_angle (totem);
}

void
dvd_root_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU);
}

void
dvd_title_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_TITLE_MENU);
}

void
dvd_audio_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_AUDIO_MENU);
}

void
dvd_angle_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ANGLE_MENU);
}

void
dvd_chapter_menu_action_callback (GtkAction *action, Totem *totem)
{
        bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_CHAPTER_MENU);
}

void
next_chapter_action_callback (GtkAction *action, Totem *totem)
{
	TOTEM_PROFILE (totem_action_next (totem));
}

void
previous_chapter_action_callback (GtkAction *action, Totem *totem)
{
	TOTEM_PROFILE (totem_action_previous (totem));
}

void
skip_forward_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_seek_relative (totem, SEEK_FORWARD_OFFSET * 1000);
}

void
skip_backwards_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_seek_relative (totem, SEEK_BACKWARD_OFFSET * 1000);
}

void
volume_up_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_volume_relative (totem, VOLUME_UP_OFFSET);
}

void
volume_down_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_volume_relative (totem, VOLUME_DOWN_OFFSET);
}

void
contents_action_callback (GtkAction *action, Totem *totem)
{
	totem_action_show_help (totem);
}

void
about_action_callback (GtkAction *action, Totem *totem)
{
	char *backend_version, *description;

	const char *authors[] =
	{
		"Bastien Nocera <hadess@hadess.net>",
		"Ronald Bultje <rbultje@ronald.bitfreak.net>",
		"Julien Moutte <julien@moutte.net> (GStreamer backend)",
		"Tim-Philipp M\303\274ller <tim\100centricular\056net> (GStreamer backend)",
		"Philip Withnall <philip@tecnocode.co.uk>",
		NULL
	};
	const char *artists[] = { "Jakub Steiner <jimmac@ximian.com>", NULL };
	#include "../help/totem-docs.h"
	char *license = totem_interface_get_license ();

	backend_version = bacon_video_widget_get_backend_name (totem->bvw);
	/* This lists the back-end type and version, such as
	 * Movie Player using GStreamer 0.10.1 */
	description = g_strdup_printf (_("Movie Player using %s"), backend_version);

	gtk_show_about_dialog (GTK_WINDOW (totem->win),
				     "version", VERSION,
				     "copyright", _("Copyright \xc2\xa9 2002-2009 Bastien Nocera"),
				     "comments", description,
				     "authors", authors,
				     "documenters", documentation_credits,
				     "artists", artists,
				     "translator-credits", _("translator-credits"),
				     "logo-icon-name", "totem",
				     "license", license,
				     "wrap-license", TRUE,
				     "website-label", _("Totem Website"),
				     "website", "http://projects.gnome.org/totem/",
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


void
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

void
repeat_mode_action_callback (GtkToggleAction *action, Totem *totem)
{
	totem_playlist_set_repeat (totem->playlist,
			gtk_toggle_action_get_active (action));
}

void
shuffle_mode_action_callback (GtkToggleAction *action, Totem *totem)
{
	totem_playlist_set_shuffle (totem->playlist,
			gtk_toggle_action_get_active (action));
}

void
deinterlace_action_callback (GtkToggleAction *action, Totem *totem)
{
	gboolean value;

	value = gtk_toggle_action_get_active (action);
	bacon_video_widget_set_deinterlacing (totem->bvw, value);
	gconf_client_set_bool (totem->gc, GCONF_PREFIX"/deinterlace",
			value, NULL);
}

void
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

void
show_sidebar_action_callback (GtkToggleAction *action, Totem *totem)
{
	if (totem_is_fullscreen (totem))
		return;

	totem_sidebar_toggle (totem, gtk_toggle_action_get_active (action));
}

void
aspect_ratio_changed_callback (GtkRadioAction *action, GtkRadioAction *current, Totem *totem)
{
	totem_action_set_aspect_ratio (totem, gtk_radio_action_get_current_value (current));
}

void
clear_playlist_action_callback (GtkAction *action, Totem *totem)
{
	totem_playlist_clear (totem->playlist);
	totem_action_set_mrl (totem, NULL, NULL);
}

/* Show help in status bar when selecting (hovering over) a menu item. */
static void
menu_item_select_cb (GtkMenuItem *proxy, Totem *totem)
{
	GtkAction *action;
	const gchar *message;

	action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (proxy));
	g_return_if_fail (action != NULL);

	message = gtk_action_get_tooltip (action);
	if (message)
		totem_statusbar_push_help (TOTEM_STATUSBAR (totem->statusbar), message);
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy, Totem *totem)
{
	totem_statusbar_pop_help (TOTEM_STATUSBAR (totem->statusbar));
}

static void
setup_action (Totem *totem, GtkAction *action)
{
	GSList *proxies;
	for (proxies = gtk_action_get_proxies (action); proxies != NULL; proxies = proxies->next) {
		if (GTK_IS_MENU_ITEM (proxies->data)) {
			g_signal_connect (proxies->data, "select", G_CALLBACK (menu_item_select_cb), totem);
			g_signal_connect (proxies->data, "deselect", G_CALLBACK (menu_item_deselect_cb), totem);
		}

	}
}

static void
setup_menu_items (Totem *totem)
{
	GList *action_groups;

	/* FIXME: We can remove this once GTK+ bug #574001 is fixed */
	for (action_groups = gtk_ui_manager_get_action_groups (totem->ui_manager);
	     action_groups != NULL; action_groups = action_groups->next) {
		GtkActionGroup *action_group = GTK_ACTION_GROUP (action_groups->data);
		GList *actions;
		for (actions = gtk_action_group_list_actions (action_group); actions != NULL; actions = actions->next) {
			setup_action (totem, GTK_ACTION (actions->data));
		}
	}
}

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
	{ "sidebar", NULL, N_("S_idebar"), "F9", N_("Show or hide the sidebar"), G_CALLBACK (show_sidebar_action_callback), TRUE }
};

static const GtkRadioActionEntry aspect_ratio_entries[] = {
	{ "aspect-ratio-auto", NULL, N_("Auto"), NULL, N_("Sets automatic aspect ratio"), BVW_RATIO_AUTO },
	{ "aspect-ratio-square", NULL, N_("Square"), NULL, N_("Sets square aspect ratio"), BVW_RATIO_SQUARE },
	{ "aspect-ratio-fbt", NULL, N_("4:3 (TV)"), NULL, N_("Sets 4:3 (TV) aspect ratio"), BVW_RATIO_FOURBYTHREE },
	{ "aspect-ratio-anamorphic", NULL, N_("16:9 (Widescreen)"), NULL, N_("Sets 16:9 (Anamorphic) aspect ratio"), BVW_RATIO_ANAMORPHIC },
	{ "aspect-ratio-dvb", NULL, N_("2.11:1 (DVB)"), NULL, N_("Sets 2.11:1 (DVB) aspect ratio"), BVW_RATIO_DVB }
};

void
totem_ui_manager_setup (Totem *totem)
{
	totem->main_action_group = GTK_ACTION_GROUP (gtk_builder_get_object (totem->xml, "main-action-group"));
	totem->zoom_action_group = GTK_ACTION_GROUP (gtk_builder_get_object (totem->xml, "zoom-action-group"));

	/* FIXME: Moving these to GtkBuilder depends on bug #457631 */
	if (gtk_widget_get_direction (totem->win) == GTK_TEXT_DIR_RTL) {
		GtkActionGroup *action_group = GTK_ACTION_GROUP (gtk_builder_get_object (totem->xml, "skip-action-group"));
		GtkAction *action;

		action = gtk_action_group_get_action (action_group, "skip-forward");
		gtk_action_set_accel_path (action, "Left");

		action = gtk_action_group_get_action (action_group, "skip-backwards");
		gtk_action_set_accel_path (action, "Right");
	}

	totem->ui_manager = GTK_UI_MANAGER (gtk_builder_get_object (totem->xml, "totem-ui-manager"));

	setup_menu_items (totem);

	totem->devices_action_group = NULL;
	totem->devices_ui_id = gtk_ui_manager_new_merge_id (totem->ui_manager);
	totem->languages_action_group = NULL;
	totem->languages_ui_id = gtk_ui_manager_new_merge_id (totem->ui_manager);
	totem->subtitles_action_group = NULL;
	totem->subtitles_ui_id = gtk_ui_manager_new_merge_id (totem->ui_manager);
}

