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
#define GST_USE_UNSTABLE_API 1
#include <gst/tag/tag.h>
#include <string.h>

#include "totem-menu.h"
#include "totem.h"
#include "totem-interface.h"
#include "totem-private.h"
#include "bacon-video-widget.h"
#include "totem-uri.h"

#include "totem-profile.h"

static void
open_action_cb (GSimpleAction *action,
		GVariant      *parameter,
		gpointer       user_data)
{
	totem_object_open (TOTEM_OBJECT (user_data));
}

static void
open_location_action_cb (GSimpleAction *action,
			 GVariant      *parameter,
			 gpointer       user_data)
{
	totem_object_open_location (TOTEM_OBJECT (user_data));
}

static void
preferences_action_cb (GSimpleAction *action,
		       GVariant      *parameter,
		       gpointer       user_data)
{
	gtk_widget_show (TOTEM_OBJECT (user_data)->prefs);
}

static void
fullscreen_change_state (GSimpleAction *action,
			 GVariant      *value,
			 gpointer       user_data)
{
	gboolean param;

	param = g_variant_get_boolean (value);
	totem_object_set_fullscreen (TOTEM_OBJECT (user_data), param);

	g_simple_action_set_state (action, value);
}

static void
set_subtitle_action_change_state (GSimpleAction *action,
				  GVariant      *value,
				  gpointer       user_data)
{
	int rank;

	rank = g_variant_get_int32 (value);
	if (!TOTEM_OBJECT (user_data)->updating_menu)
		bacon_video_widget_set_subtitle (TOTEM_OBJECT (user_data)->bvw, rank);

	g_simple_action_set_state (action, value);
}

static void
set_language_action_change_state (GSimpleAction *action,
				  GVariant      *value,
				  gpointer       user_data)
{
	int rank;

	rank = g_variant_get_int32 (value);
	if (!TOTEM_OBJECT (user_data)->updating_menu)
		bacon_video_widget_set_language (TOTEM_OBJECT (user_data)->bvw, rank);

	g_simple_action_set_state (action, value);
}

static void
aspect_ratio_change_state (GSimpleAction *action,
			   GVariant      *value,
			   gpointer       user_data)
{
	BvwAspectRatio ratio;

	ratio = g_variant_get_int32 (value);
	bacon_video_widget_set_aspect_ratio (TOTEM_OBJECT (user_data)->bvw, ratio);

	g_simple_action_set_state (action, value);
}

static void
zoom_action_change_state (GSimpleAction *action,
			  GVariant      *value,
			  gpointer       user_data)
{
	gboolean expand;

	expand = g_variant_get_boolean (value);
	bacon_video_widget_set_zoom (TOTEM_OBJECT (user_data)->bvw,
				     expand ? BVW_ZOOM_EXPAND : BVW_ZOOM_NONE);

	g_simple_action_set_state (action, value);
}

static void
repeat_change_state (GSimpleAction *action,
		     GVariant      *value,
		     gpointer       user_data)
{
	gboolean param;

	param = g_variant_get_boolean (value);
	totem_playlist_set_repeat (TOTEM_OBJECT (user_data)->playlist, param);

	g_simple_action_set_state (action, value);
}

static void
toggle_action_cb (GSimpleAction *action,
		  GVariant      *parameter,
		  gpointer       user_data)
{
	GVariant *state;

	state = g_action_get_state (G_ACTION (action));
	g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
	g_variant_unref (state);
}

static void
list_action_cb (GSimpleAction *action,
		GVariant      *parameter,
		gpointer       user_data)
{
	g_action_change_state (G_ACTION (action), parameter);
}

static void
help_action_cb (GSimpleAction *action,
		GVariant      *parameter,
		gpointer       user_data)
{
	totem_object_show_help (TOTEM_OBJECT (user_data));
}

static void
keyboard_shortcuts_action_cb (GSimpleAction *action,
			      GVariant      *parameter,
			      gpointer       user_data)
{
	totem_object_show_keyboard_shortcuts (TOTEM_OBJECT (user_data));
}

static void
quit_action_cb (GSimpleAction *action,
		GVariant      *parameter,
		gpointer       user_data)
{
	totem_object_exit (TOTEM_OBJECT (user_data));
}

static void
dvd_root_menu_action_cb (GSimpleAction *action,
			 GVariant      *parameter,
			 gpointer       user_data)
{
        bacon_video_widget_dvd_event (TOTEM_OBJECT (user_data)->bvw, BVW_DVD_ROOT_MENU);
}

static void
dvd_title_menu_action_cb (GSimpleAction *action,
			  GVariant      *parameter,
			  gpointer       user_data)
{
        bacon_video_widget_dvd_event (TOTEM_OBJECT (user_data)->bvw, BVW_DVD_TITLE_MENU);
}

static void
dvd_audio_menu_action_cb (GSimpleAction *action,
			  GVariant      *parameter,
			  gpointer       user_data)
{
        bacon_video_widget_dvd_event (TOTEM_OBJECT (user_data)->bvw, BVW_DVD_AUDIO_MENU);
}

static void
dvd_angle_menu_action_cb (GSimpleAction *action,
			  GVariant      *parameter,
			  gpointer       user_data)
{
        bacon_video_widget_dvd_event (TOTEM_OBJECT (user_data)->bvw, BVW_DVD_ANGLE_MENU);
}

static void
dvd_chapter_menu_action_cb (GSimpleAction *action,
			    GVariant      *parameter,
			    gpointer       user_data)
{
        bacon_video_widget_dvd_event (TOTEM_OBJECT (user_data)->bvw, BVW_DVD_CHAPTER_MENU);
}

static void
next_angle_action_cb (GSimpleAction *action,
		      GVariant      *parameter,
		      gpointer       user_data)
{
        totem_object_next_angle (TOTEM_OBJECT (user_data));
}

static void
eject_action_cb (GSimpleAction *action,
		 GVariant      *parameter,
		 gpointer       user_data)
{
	totem_object_eject (TOTEM_OBJECT (user_data));
}

static void
select_subtitle_action_cb (GSimpleAction *action,
			   GVariant      *parameter,
			   gpointer       user_data)
{
	totem_playlist_select_subtitle_dialog (TOTEM_OBJECT (user_data)->playlist,
					       TOTEM_PLAYLIST_DIALOG_PLAYING);
}

static void
play_action_cb (GSimpleAction *action,
		GVariant      *parameter,
		gpointer       user_data)
{
	totem_object_play_pause (TOTEM_OBJECT (user_data));
}

static void
next_chapter_action_cb (GSimpleAction *action,
			GVariant      *parameter,
			gpointer       user_data)
{
	TOTEM_PROFILE (totem_object_seek_next (TOTEM_OBJECT (user_data)));
}

static void
previous_chapter_action_cb (GSimpleAction *action,
			    GVariant      *parameter,
			    gpointer       user_data)
{
	TOTEM_PROFILE (totem_object_seek_previous (TOTEM_OBJECT (user_data)));
}

static void
remote_command_cb (GSimpleAction *action,
		   GVariant      *parameter,
		   gpointer       user_data)
{
	TotemObject *totem;
	TotemRemoteCommand cmd;
	const char *url;

	totem = TOTEM_OBJECT (user_data);

	g_application_activate (G_APPLICATION (totem));

	g_variant_get (parameter, "(i&s)", &cmd, &url);

	if (url && *url == '\0')
		totem_object_remote_command (totem, cmd, NULL);
	else
		totem_object_remote_command (totem, cmd, url);
}

static GActionEntry app_entries[] = {
	/* Main app menu */
	{ "open", open_action_cb, NULL, NULL, NULL },
	{ "open-location", open_location_action_cb, NULL, NULL, NULL },
	{ "fullscreen", toggle_action_cb, NULL, "false", fullscreen_change_state },
	{ "preferences", preferences_action_cb, NULL, NULL, NULL },
	{ "repeat", toggle_action_cb, NULL, "false", repeat_change_state },
	{ "shortcuts", keyboard_shortcuts_action_cb, NULL, NULL, NULL },
	{ "help", help_action_cb, NULL, NULL, NULL },
	{ "quit", quit_action_cb, NULL, NULL, NULL },

	/* "Go" menu */
	{ "dvd-root-menu", dvd_root_menu_action_cb, NULL, NULL, NULL },
	{ "dvd-title-menu", dvd_title_menu_action_cb, NULL, NULL, NULL },
	{ "dvd-audio-menu", dvd_audio_menu_action_cb, NULL, NULL, NULL },
	{ "dvd-angle-menu", dvd_angle_menu_action_cb, NULL, NULL, NULL },
	{ "dvd-chapter-menu", dvd_chapter_menu_action_cb, NULL, NULL, NULL },

	/* Cogwheel menu */
	{ "select-subtitle", select_subtitle_action_cb, NULL, NULL, NULL },
	{ "set-subtitle", list_action_cb, "i", "-1", set_subtitle_action_change_state },
	{ "set-language", list_action_cb, "i", "-1", set_language_action_change_state },
	{ "aspect-ratio", list_action_cb, "i", "0", aspect_ratio_change_state },
	{ "zoom", toggle_action_cb, NULL, "false", zoom_action_change_state },
	{ "next-angle", next_angle_action_cb, NULL, NULL, NULL },
	{ "eject", eject_action_cb, NULL, NULL, NULL },

	/* Navigation popup */
	{ "play", play_action_cb, NULL, NULL, NULL },
	{ "next-chapter", next_chapter_action_cb, NULL, NULL, NULL },
	{ "previous-chapter", previous_chapter_action_cb, NULL, NULL, NULL },

	/* Remote command handling */
	{ "remote-command", remote_command_cb, "(is)", NULL, NULL },
};

void
totem_app_actions_setup (Totem *totem)
{
	g_action_map_add_action_entries (G_ACTION_MAP (totem), app_entries, G_N_ELEMENTS (app_entries), totem);
}

void
totem_app_menu_setup (Totem *totem)
{
	char *accels[] = { NULL, NULL };
	const char * const shortcuts_accels[] = {
		"<Ctrl>H",
		"<Ctrl>question",
		"<Ctrl>F1",
		NULL
	};

	/* FIXME: https://gitlab.gnome.org/GNOME/glib/issues/700 */
	accels[0] = "<Primary>G";
	gtk_application_set_accels_for_action (GTK_APPLICATION (totem), "app.next-angle", (const char * const *) accels);
	accels[0] = "<Primary>M";
	gtk_application_set_accels_for_action (GTK_APPLICATION (totem), "app.root-menu", (const char * const *) accels);
	accels[0] = "<Primary>E";
	gtk_application_set_accels_for_action (GTK_APPLICATION (totem), "app.eject", (const char * const *) accels);
	gtk_application_set_accels_for_action (GTK_APPLICATION (totem), "app.shortcuts", shortcuts_accels);
	gtk_window_set_application (GTK_WINDOW (totem->win), GTK_APPLICATION (totem));
}

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

/* Subtitle and language menus */
static char *
bvw_lang_info_to_id (BvwLangInfo *info)
{
	return g_strdup_printf ("%s-%s", info->language, info->codec);
}

static int
hash_table_num_instances (GHashTable *ht,
			  const char *key)
{
	gpointer value;

	value = g_hash_table_lookup (ht, key);
	if (!value)
		return 0;
	return GPOINTER_TO_INT (value);
}

static const char *
get_language_name_no_und (const char   *lang,
			  BvwTrackType  track_type)
{
	const char *ret;

	ret =  gst_tag_get_language_name (lang);
	if (ret != NULL)
		return ret;

	switch (track_type) {
	case BVW_TRACK_TYPE_AUDIO:
		return _("Audio Track");
		break;
	case BVW_TRACK_TYPE_SUBTITLE:
		return _("Subtitle");
		break;
	case BVW_TRACK_TYPE_VIDEO:
		g_assert_not_reached ();
	}

	return NULL;
}

GList *
bvw_lang_info_to_menu_labels (GList        *langs,
			      BvwTrackType  track_type)
{
	GList *l, *ret;
	GHashTable *lang_table, *lang_codec_table, *printed_table;

	lang_table = g_hash_table_new (g_str_hash, g_str_equal);
	lang_codec_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* Populate the hash tables */
	for (l = langs; l != NULL; l = l->next) {
		BvwLangInfo *info = l->data;
		int num;
		char *id;

		num = hash_table_num_instances (lang_table, info->language);
		num++;
		g_hash_table_insert (lang_table,
				     (gpointer) info->language,
				     GINT_TO_POINTER (num));

		id = bvw_lang_info_to_id (info);
		num = hash_table_num_instances (lang_codec_table, id);
		num++;
		g_hash_table_insert (lang_codec_table,
				     id,
				     GINT_TO_POINTER (num));
	}

	ret = NULL;
	printed_table = g_hash_table_new (g_str_hash, g_str_equal);
	for (l = langs; l != NULL; l = l->next) {
		BvwLangInfo *info = l->data;
		int num;
		char *str;

		num = hash_table_num_instances (lang_table, info->language);
		g_assert (num >= 1);
		if (num > 1) {
			char *id;

			id = bvw_lang_info_to_id (info);
			num = hash_table_num_instances (lang_codec_table, id);
			if (num > 1) {
				num = hash_table_num_instances (printed_table, info->language);
				num++;
				g_hash_table_insert (printed_table,
						     (gpointer) info->language,
						     GINT_TO_POINTER (num));

				str = g_strdup_printf ("%s #%d",
						       get_language_name_no_und (info->language, track_type),
						       num);
			} else {
				str = g_strdup_printf ("%s — %s",
						       get_language_name_no_und (info->language, track_type),
						       info->codec);
			}
			g_free (id);
		} else {
			str = g_strdup (get_language_name_no_und (info->language, track_type));
		}

		ret = g_list_prepend (ret, str);
	}

	g_hash_table_destroy (printed_table);
	g_hash_table_destroy (lang_codec_table);
	g_hash_table_destroy (lang_table);

	return g_list_reverse (ret);
}

static void
add_lang_item (GMenu      *menu,
	       const char *label,
	       const char *action,
	       int         target)
{
	GMenuItem *item;

	item = g_menu_item_new (label, NULL);
	g_menu_item_set_action_and_target_value (item, action, g_variant_new_int32 (target));
	g_menu_append_item (G_MENU (menu), item);
}

static void
add_lang_action (GMenu *menu,
		 const char *action,
		 const char *label,
		 int lang_id)
{
	char *escaped_label;

	escaped_label = escape_label_for_menu (label);
	add_lang_item (menu, escaped_label, action, lang_id);
	g_free (escaped_label);
}

static void
create_lang_actions (GMenu        *menu,
		     const char   *action,
		     GList        *list,
		     BvwTrackType  track_type)
{
	GList *ui_list, *l;
	guint i;

	if (track_type == BVW_TRACK_TYPE_SUBTITLE) {
		/* Translators: an entry in the "Subtitles" menu, used to choose the subtitle language of a DVD */
		add_lang_action (menu, action, _("None"), -2);
	}

	/* Translators: an entry in the "Languages" menu, used to choose the audio language of a DVD */
	add_lang_action (menu, action, C_("Language", "Auto"), -1);

	ui_list = bvw_lang_info_to_menu_labels (list, track_type);

	for (l = ui_list, i = 0; l != NULL; l = l->next, i++)
		add_lang_action (menu, action, l->data, i);

	g_list_free_full (ui_list, (GDestroyNotify) g_free);
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
	while (o != NULL && n != NULL && retval != FALSE) {
		BvwLangInfo *info_o, *info_n;

		info_o = o->data;
		info_n = n->data;
		if (g_strcmp0 (info_o->language, info_n->language) != 0)
			retval = FALSE;
		if (g_strcmp0 (info_o->codec, info_n->codec) != 0)
			retval = FALSE;
                o = g_list_next (o);
                n = g_list_next (n);
	}

	return retval;
}

static void
totem_languages_update (Totem *totem, GList *list)
{
	GAction *action;
	int current;

	/* Remove old UI */
	totem_object_empty_menu_section (totem, "languages-placeholder");

	if (list != NULL) {
		GMenu *menu;
		menu = totem_object_get_menu_section (totem, "languages-placeholder");
		create_lang_actions (menu, "app.set-language", list, BVW_TRACK_TYPE_AUDIO);
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (totem), "set-language");
	totem->updating_menu = TRUE;
	current = bacon_video_widget_get_language (totem->bvw);
	g_action_change_state (action, g_variant_new_int32 (current));
	totem->updating_menu = FALSE;

	g_list_free_full (totem->languages_list, (GDestroyNotify) bacon_video_widget_lang_info_free);
	totem->languages_list = list;
}

static void
totem_subtitles_update (Totem *totem, GList *list)
{
	GAction *action;
	int current;

	/* Remove old UI */
	totem_object_empty_menu_section (totem, "subtitles-placeholder");

	if (list != NULL) {
		GMenu *menu;
		menu = totem_object_get_menu_section (totem, "subtitles-placeholder");
		create_lang_actions (menu, "app.set-subtitle", list, BVW_TRACK_TYPE_SUBTITLE);
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (totem), "set-subtitle");
	totem->updating_menu = TRUE;
	current = bacon_video_widget_get_subtitle (totem->bvw);
	g_action_change_state (action, g_variant_new_int32 (current));
	totem->updating_menu = FALSE;

	g_list_free_full (totem->subtitles_list, (GDestroyNotify) bacon_video_widget_lang_info_free);
	totem->subtitles_list = list;
}

void
totem_sublang_update (Totem *totem)
{
	GList *list;

	list = bacon_video_widget_get_languages (totem->bvw);
	if (totem_sublang_equal_lists (totem->languages_list, list) == TRUE) {
		g_list_free_full (list, (GDestroyNotify) bacon_video_widget_lang_info_free);
	} else {
		totem_languages_update (totem, list);
	}

	list = bacon_video_widget_get_subtitles (totem->bvw);
	if (totem_sublang_equal_lists (totem->subtitles_list, list) == TRUE) {
		g_list_free_full (list, (GDestroyNotify) bacon_video_widget_lang_info_free);
	} else {
		totem_subtitles_update (totem, list);
	}
}

void
totem_sublang_exit (Totem *totem)
{
	g_list_free_full (totem->subtitles_list, (GDestroyNotify) bacon_video_widget_lang_info_free);
	g_list_free_full (totem->languages_list, (GDestroyNotify) bacon_video_widget_lang_info_free);
}
