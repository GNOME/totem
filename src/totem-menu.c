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
#include "totem-sidebar.h"
#include "bacon-video-widget.h"
#include "totem-uri.h"

#include "totem-profile.h"

/* Callback functions for GtkBuilder */
G_MODULE_EXPORT void play_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void next_chapter_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void previous_chapter_action_callback (GtkAction *action, Totem *totem);
G_MODULE_EXPORT void show_sidebar_action_callback (GtkToggleAction *action, Totem *totem);
G_MODULE_EXPORT void clear_playlist_action_callback (GtkAction *action, Totem *totem);

static void
open_action_cb (GSimpleAction *action,
		GVariant      *parameter,
		gpointer       user_data)
{
	totem_action_open (TOTEM_OBJECT (user_data));
}

static void
open_location_action_cb (GSimpleAction *action,
			 GVariant      *parameter,
			 gpointer       user_data)
{
	totem_action_open_location (TOTEM_OBJECT (user_data));
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
	totem_action_fullscreen (TOTEM_OBJECT (user_data), param);

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
aspect_ratio_action_cb (GSimpleAction *action,
			GVariant      *parameter,
			gpointer       user_data)
{
	g_action_change_state (G_ACTION (action), parameter);
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
shuffle_change_state (GSimpleAction *action,
		      GVariant      *value,
		      gpointer       user_data)
{
	gboolean param;

	param = g_variant_get_boolean (value);
	totem_playlist_set_shuffle (TOTEM_OBJECT (user_data)->playlist, param);

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
help_action_cb (GSimpleAction *action,
		GVariant      *parameter,
		gpointer       user_data)
{
	totem_action_show_help (TOTEM_OBJECT (user_data));
}

static void
quit_action_cb (GSimpleAction *action,
		GVariant      *parameter,
		gpointer       user_data)
{
	totem_object_action_exit (TOTEM_OBJECT (user_data));
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
        totem_object_action_next_angle (TOTEM_OBJECT (user_data));
}

static void
properties_action_cb (GSimpleAction *action,
		      GVariant      *parameter,
		      gpointer       user_data)
{
        totem_action_show_properties (TOTEM_OBJECT (user_data));
}

static void
eject_action_cb (GSimpleAction *action,
		 GVariant      *parameter,
		 gpointer       user_data)
{
	totem_action_eject (TOTEM_OBJECT (user_data));
}

static void
select_subtitle_action_cb (GSimpleAction *action,
			   GVariant      *parameter,
			   gpointer       user_data)
{
	totem_playlist_select_subtitle_dialog (TOTEM_OBJECT (user_data)->playlist,
					       TOTEM_PLAYLIST_DIALOG_PLAYING);
}

static GActionEntry app_entries[] = {
	/* Main app menu */
	{ "open", open_action_cb, NULL, NULL, NULL },
	{ "open-location", open_location_action_cb, NULL, NULL, NULL },
	{ "fullscreen", toggle_action_cb, NULL, "false", fullscreen_change_state },
	{ "preferences", preferences_action_cb, NULL, NULL, NULL },
	{ "shuffle", toggle_action_cb, NULL, "false", shuffle_change_state },
	{ "repeat", toggle_action_cb, NULL, "false", repeat_change_state },
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
	{ "aspect-ratio", aspect_ratio_action_cb, "i", "0", aspect_ratio_change_state },
	{ "zoom", toggle_action_cb, NULL, "false", zoom_action_change_state },
	{ "next-angle", next_angle_action_cb, NULL, NULL, NULL },
	{ "properties", properties_action_cb, NULL, NULL, NULL },
	{ "eject", eject_action_cb, NULL, NULL, NULL },
};

void
totem_app_menu_setup (Totem *totem)
{
	GMenuModel *appmenu;

	g_action_map_add_action_entries (G_ACTION_MAP (totem), app_entries, G_N_ELEMENTS (app_entries), totem);

	appmenu = (GMenuModel *)gtk_builder_get_object (totem->xml, "appmenu");
	gtk_application_set_app_menu (GTK_APPLICATION (totem), appmenu);

	/* FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=700085 */
	gtk_application_add_accelerator (GTK_APPLICATION (totem), "<Primary>G", "app.next-angle", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (totem), "<Primary>M", "app.root-menu", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (totem), "<Primary>P", "app.properties", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (totem), "<Primary>E", "app.eject", NULL);

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
		int lang_id, int lang_index, GSList **group)
{
	const char *full_lang;
	char *label;
	char *name;
	GtkAction *action;

	full_lang = gst_tag_get_language_name (lang);

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
		                /* Translators: an entry in the "Languages" menu, used to choose the audio language of a DVD */
				_("None"), -2, 0, &group);
	}

	action = add_lang_action (totem, action_group, ui_id, path, prefix,
	                          /* Translators: an entry in the "Languages" menu, used to choose the audio language of a DVD */
	                          C_("Language", "Auto"), -1, 0, &group);

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
	const char *path = "/tmw-menubar/sound/languages/placeholder";
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
				path,
				"languages", list, TRUE);
		gtk_ui_manager_ensure_update (totem->ui_manager);

		current = bacon_video_widget_get_language (totem->bvw);
		gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action),
				current);
		g_signal_connect (G_OBJECT (action), "changed",
				G_CALLBACK (languages_changed_callback), totem);
	}

	g_list_free_full (totem->language_list, g_free);
	totem->language_list = list;
}

static void
totem_subtitles_update (Totem *totem, GList *list)
{
	GtkAction *action;
	int current;
	const char *path = "/tmw-menubar/view/subtitles/placeholder";

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
				path,
				"subtitles", list, FALSE);
		gtk_ui_manager_ensure_update (totem->ui_manager);

		current = bacon_video_widget_get_subtitle (totem->bvw);
		gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action),
				current);
		g_signal_connect (G_OBJECT (action), "changed",
				G_CALLBACK (subtitles_changed_callback), totem);
	}

	g_list_free_full (totem->subtitles_list, g_free);
	totem->subtitles_list = list;
}

void
totem_sublang_update (Totem *totem)
{
	GList *list;

	list = bacon_video_widget_get_languages (totem->bvw);
	if (totem_sublang_equal_lists (totem->language_list, list) == TRUE) {
		g_list_free_full (list, g_free);
	} else {
		totem_languages_update (totem, list);
	}

	list = bacon_video_widget_get_subtitles (totem->bvw);
	if (totem_sublang_equal_lists (totem->subtitles_list, list) == TRUE) {
		g_list_free_full (list, g_free);
	} else {
		totem_subtitles_update (totem, list);
	}
}

void
totem_sublang_exit (Totem *totem)
{
	g_list_free_full (totem->subtitles_list, g_free);
	g_list_free_full (totem->language_list, g_free);
}

void
play_action_callback (GtkAction *action, Totem *totem)
{
	totem_object_action_play_pause (totem);
}

void
next_chapter_action_callback (GtkAction *action, Totem *totem)
{
	TOTEM_PROFILE (totem_object_action_next (totem));
}

void
previous_chapter_action_callback (GtkAction *action, Totem *totem)
{
	TOTEM_PROFILE (totem_object_action_previous (totem));
}

void
show_sidebar_action_callback (GtkToggleAction *action, Totem *totem)
{
	if (totem_object_is_fullscreen (totem))
		return;

	totem_sidebar_toggle (totem, gtk_toggle_action_get_active (action));
}

void
clear_playlist_action_callback (GtkAction *action, Totem *totem)
{
	totem_playlist_clear (totem->playlist);
	totem_action_set_mrl (totem, NULL, NULL);
}

void
totem_ui_manager_setup (Totem *totem)
{
	totem->main_action_group = GTK_ACTION_GROUP (gtk_builder_get_object (totem->xml, "main-action-group"));

	totem->ui_manager = GTK_UI_MANAGER (gtk_builder_get_object (totem->xml, "totem-ui-manager"));

	totem->languages_action_group = NULL;
	totem->languages_ui_id = gtk_ui_manager_new_merge_id (totem->ui_manager);
	totem->subtitles_action_group = NULL;
	totem->subtitles_ui_id = gtk_ui_manager_new_merge_id (totem->ui_manager);
}

