/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

/**
 * SECTION:totem-object
 * @short_description: main Totem object
 * @stability: Unstable
 * @include: totem.h
 *
 * #TotemObject is the core object of Totem; a singleton which controls all Totem's main functions.
 **/

#include "config.h"

#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#include <libgd/gd.h>

#include <string.h>

#include "totem.h"
#include "totem-private.h"
#include "totem-options.h"
#include "totem-plugins-engine.h"
#include "totem-playlist.h"
#include "totem-grilo.h"
#include "bacon-video-widget.h"
#include "bacon-time-label.h"
#include "totem-menu.h"
#include "totem-uri.h"
#include "totem-interface.h"
#include "totem-preferences.h"
#include "totem-session.h"
#include "totem-main-toolbar.h"

#define WANT_MIME_TYPES 1
#include "totem-mime-types.h"
#include "totem-uri-schemes.h"

#define REWIND_OR_PREVIOUS 4000

#define SEEK_FORWARD_SHORT_OFFSET 15
#define SEEK_BACKWARD_SHORT_OFFSET -5

#define SEEK_FORWARD_LONG_OFFSET 10*60
#define SEEK_BACKWARD_LONG_OFFSET -3*60

#define DEFAULT_WINDOW_W 650
#define DEFAULT_WINDOW_H 500

#define TOTEM_SESSION_SAVE_TIMEOUT 10 /* seconds */

/* casts are to shut gcc up */
static const GtkTargetEntry target_table[] = {
	{ (gchar*) "text/uri-list", 0, 0 },
	{ (gchar*) "_NETSCAPE_URL", 0, 1 }
};

static gboolean totem_object_open_files_list (TotemObject *totem, GSList *list);
static void update_buttons (TotemObject *totem);
static void update_fill (TotemObject *totem, gdouble level);
static void update_media_menu_items (TotemObject *totem);
static void playlist_changed_cb (GtkWidget *playlist, TotemObject *totem);
static void play_pause_set_label (TotemObject *totem, TotemStates state);
static void totem_object_set_mrl_and_play (TotemObject *totem, const char *mrl, const char *subtitle);

/* Callback functions for GtkBuilder */
G_MODULE_EXPORT gboolean main_window_destroy_cb (GtkWidget *widget, GdkEvent *event, TotemObject *totem);
G_MODULE_EXPORT gboolean window_state_event_cb (GtkWidget *window, GdkEventWindowState *event, TotemObject *totem);
G_MODULE_EXPORT gboolean seek_slider_pressed_cb (GtkWidget *widget, GdkEventButton *event, TotemObject *totem);
G_MODULE_EXPORT void seek_slider_changed_cb (GtkAdjustment *adj, TotemObject *totem);
G_MODULE_EXPORT gboolean seek_slider_released_cb (GtkWidget *widget, GdkEventButton *event, TotemObject *totem);
G_MODULE_EXPORT gboolean window_key_press_event_cb (GtkWidget *win, GdkEventKey *event, TotemObject *totem);

enum {
	PROP_0,
	PROP_FULLSCREEN,
	PROP_PLAYING,
	PROP_STREAM_LENGTH,
	PROP_SEEKABLE,
	PROP_CURRENT_TIME,
	PROP_CURRENT_MRL,
	PROP_CURRENT_CONTENT_TYPE,
	PROP_CURRENT_DISPLAY_NAME,
	PROP_MAIN_PAGE
};

enum {
	FILE_OPENED,
	FILE_CLOSED,
	FILE_HAS_PLAYED,
	METADATA_UPDATED,
	GET_USER_AGENT,
	GET_TEXT_SUBTITLE,
	LAST_SIGNAL
};

static void totem_object_get_property		(GObject *object,
						 guint property_id,
						 GValue *value,
						 GParamSpec *pspec);
static void totem_object_finalize (GObject *totem);

static int totem_table_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(TotemObject, totem_object, GTK_TYPE_APPLICATION)

static void
totem_object_app_open (GApplication  *application,
		       GFile        **files,
		       gint           n_files,
		       const char    *hint)
{
	GSList *slist = NULL;
	Totem *totem = TOTEM_OBJECT (application);
	int i;

	optionstate.had_filenames = (n_files > 0);

	g_application_activate (application);
	gtk_window_present_with_time (GTK_WINDOW (totem->win),
				      gtk_get_current_event_time ());

	totem_object_set_main_page (TOTEM_OBJECT (application), "player");

	for (i = 0 ; i < n_files; i++)
		slist = g_slist_prepend (slist, g_file_get_uri (files[i]));

	slist = g_slist_reverse (slist);
	totem_object_open_files_list (TOTEM_OBJECT (application), slist);
	g_slist_free_full (slist, g_free);
}

static void
totem_object_app_activate (GApplication *app)
{
	Totem *totem;

	totem = TOTEM_OBJECT (app);

	/* Already init'ed? */
	if (totem->xml != NULL)
		return;

	/* Main window */
	totem->xml = totem_interface_load ("totem.ui", TRUE, NULL, totem);
	if (totem->xml == NULL)
		totem_object_exit (NULL);

	totem->win = GTK_WIDGET (gtk_builder_get_object (totem->xml, "totem_main_window"));

	/* Menubar */
	totem->stack = GTK_WIDGET (gtk_builder_get_object (totem->xml, "tmw_main_stack"));

	/* The playlist widget */
	playlist_widget_setup (totem);

	/* The rest of the widgets */
	totem->state = STATE_STOPPED;

	totem->seek_lock = FALSE;

	totem_setup_file_monitoring (totem);
	totem_setup_file_filters ();
	totem_app_menu_setup (totem);
	/* totem_callback_connect (totem); XXX we do this later now, so it might look ugly for a short while */

	totem_setup_window (totem);

	/* Show ! (again) the video widget this time. */
	video_widget_create (totem);
	grilo_widget_setup (totem);

	/* Show ! */
	gtk_widget_show (totem->win);
	g_application_mark_busy (G_APPLICATION (totem));

	totem->controls_visibility = TOTEM_CONTROLS_UNDEFINED;

	totem->seek = g_object_get_data (totem->controls, "seek_scale");
	totem->seekadj = gtk_range_get_adjustment (GTK_RANGE (totem->seek));
	totem->volume = g_object_get_data (totem->controls, "volume_button");
	totem->time_label = g_object_get_data (totem->controls, "time_label");
	totem->time_rem_label = g_object_get_data (totem->controls, "time_rem_label");
	totem->pause_start = optionstate.pause;

	totem_callback_connect (totem);

	gtk_widget_grab_focus (GTK_WIDGET (totem->bvw));

	/* The prefs after the video widget is connected */
	totem->prefs_xml = totem_interface_load ("preferences.ui", TRUE, NULL, totem);
	totem->prefs = GTK_WIDGET (gtk_builder_get_object (totem->prefs_xml, "totem_preferences_window"));

	gtk_window_set_modal (GTK_WINDOW (totem->prefs), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (totem->prefs), GTK_WINDOW(totem->win));

	totem_setup_preferences (totem);

	/* Initialise all the plugins, and set the default page, in case
	 * it comes from a plugin */
	totem_object_plugins_init (totem);

	/* We're only supposed to be called from totem_object_app_handle_local_options()
	 * and totem_object_app_open() */
	g_assert (optionstate.filenames == NULL);

	if (!optionstate.had_filenames) {
		if (totem_session_try_restore (totem) == FALSE) {
			totem_object_set_main_page (totem, "grilo");
			totem_object_set_mrl (totem, NULL, NULL);
		} else {
			totem_object_set_main_page (totem, "player");
		}
	} else {
		totem_object_set_main_page (totem, "player");
	}

	optionstate.had_filenames = FALSE;

	if (optionstate.fullscreen != FALSE) {
		if (g_strcmp0 (totem_object_get_main_page (totem), "player") == 0)
			totem_object_set_fullscreen (totem, TRUE);
	}

	/* Set the logo at the last minute so we won't try to show it before a video */
	bacon_video_widget_set_logo (totem->bvw, "org.gnome.Totem");

	g_application_unmark_busy (G_APPLICATION (totem));

	gtk_window_set_application (GTK_WINDOW (totem->win), GTK_APPLICATION (totem));
}

static int
totem_object_app_handle_local_options (GApplication *application,
				       GVariantDict *options)
{
	GError *error = NULL;

	if (!g_application_register (application, NULL, &error)) {
		g_warning ("Failed to register application: %s", error->message);
		g_error_free (error);
		return 1;
	}
	totem_options_process_for_server (TOTEM_OBJECT (application), &optionstate);
	return 0;
}

static gboolean
accumulator_first_non_null_wins (GSignalInvocationHint *ihint,
				 GValue *return_accu,
				 const GValue *handler_return,
				 gpointer data)
{
	const gchar *str;

	str = g_value_get_string (handler_return);
	if (str == NULL)
		return TRUE;
	g_value_set_string (return_accu, str);

	return FALSE;
}

static void
totem_object_class_init (TotemObjectClass *klass)
{
	GObjectClass *object_class;
	GApplicationClass *app_class;

	object_class = (GObjectClass *) klass;
	app_class = (GApplicationClass *) klass;

	object_class->get_property = totem_object_get_property;
	object_class->finalize = totem_object_finalize;

	app_class->activate = totem_object_app_activate;
	app_class->open = totem_object_app_open;
	app_class->handle_local_options = totem_object_app_handle_local_options;

	/**
	 * TotemObject:fullscreen:
	 *
	 * If %TRUE, Totem is in fullscreen mode.
	 **/
	g_object_class_install_property (object_class, PROP_FULLSCREEN,
					 g_param_spec_boolean ("fullscreen", "Fullscreen?", "Whether Totem is in fullscreen mode.",
							       FALSE, G_PARAM_READABLE));

	/**
	 * TotemObject:playing:
	 *
	 * If %TRUE, Totem is playing an audio or video file.
	 **/
	g_object_class_install_property (object_class, PROP_PLAYING,
					 g_param_spec_boolean ("playing", "Playing?", "Whether Totem is currently playing a file.",
							       FALSE, G_PARAM_READABLE));

	/**
	 * TotemObject:stream-length:
	 *
	 * The length of the current stream, in milliseconds.
	 **/
	g_object_class_install_property (object_class, PROP_STREAM_LENGTH,
					 g_param_spec_int64 ("stream-length", "Stream length", "The length of the current stream.",
							     G_MININT64, G_MAXINT64, 0,
							     G_PARAM_READABLE));

	/**
	 * TotemObject:current-time:
	 *
	 * The player's position (time) in the current stream, in milliseconds.
	 **/
	g_object_class_install_property (object_class, PROP_CURRENT_TIME,
					 g_param_spec_int64 ("current-time", "Current time", "The player's position (time) in the current stream.",
							     G_MININT64, G_MAXINT64, 0,
							     G_PARAM_READABLE));

	/**
	 * TotemObject:seekable:
	 *
	 * If %TRUE, the current stream is seekable.
	 **/
	g_object_class_install_property (object_class, PROP_SEEKABLE,
					 g_param_spec_boolean ("seekable", "Seekable?", "Whether the current stream is seekable.",
							       FALSE, G_PARAM_READABLE));

	/**
	 * TotemObject:current-mrl:
	 *
	 * The MRL of the current stream.
	 **/
	g_object_class_install_property (object_class, PROP_CURRENT_MRL,
					 g_param_spec_string ("current-mrl", "Current MRL", "The MRL of the current stream.",
							      NULL, G_PARAM_READABLE));

	/**
	 * TotemObject:current-content-type:
	 *
	 * The content-type of the current stream.
	 **/
	g_object_class_install_property (object_class, PROP_CURRENT_CONTENT_TYPE,
					 g_param_spec_string ("current-content-type",
							      "Current stream's content-type",
							      "Current stream's content-type.",
							      NULL, G_PARAM_READABLE));

	/**
	 * TotemObject:current-display-name:
	 *
	 * The display name of the current stream.
	 **/
	g_object_class_install_property (object_class, PROP_CURRENT_DISPLAY_NAME,
					 g_param_spec_string ("current-display-name",
							      "Current stream's display name",
							      "Current stream's display name.",
							      NULL, G_PARAM_READABLE));

	/**
	 * TotemObject:main-page:
	 *
	 * The name of the current main page (usually "grilo", or "player").
	 **/
	g_object_class_install_property (object_class, PROP_MAIN_PAGE,
					 g_param_spec_string ("main-page",
							      "Current main page",
							      "Current main page.",
							      NULL, G_PARAM_READABLE));

	/**
	 * TotemObject::file-opened:
	 * @totem: the #TotemObject which received the signal
	 * @mrl: the MRL of the opened stream
	 *
	 * The #TotemObject::file-opened signal is emitted when a new stream is opened by Totem.
	 */
	totem_table_signals[FILE_OPENED] =
		g_signal_new ("file-opened",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, file_opened),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * TotemObject::file-has-played:
	 * @totem: the #TotemObject which received the signal
	 * @mrl: the MRL of the opened stream
	 *
	 * The #TotemObject::file-has-played signal is emitted when a new stream has started playing in Totem.
	 */
	totem_table_signals[FILE_HAS_PLAYED] =
		g_signal_new ("file-has-played",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, file_has_played),
				NULL, NULL,
				g_cclosure_marshal_VOID__STRING,
				G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * TotemObject::file-closed:
	 * @totem: the #TotemObject which received the signal
	 *
	 * The #TotemObject::file-closed signal is emitted when Totem closes a stream.
	 */
	totem_table_signals[FILE_CLOSED] =
		g_signal_new ("file-closed",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, file_closed),
				NULL, NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0, G_TYPE_NONE);

	/**
	 * TotemObject::metadata-updated:
	 * @totem: the #TotemObject which received the signal
	 * @artist: the name of the artist, or %NULL
	 * @title: the stream title, or %NULL
	 * @album: the name of the stream's album, or %NULL
	 * @track_number: the stream's track number
	 *
	 * The #TotemObject::metadata-updated signal is emitted when the metadata of a stream is updated, typically
	 * when it's being loaded.
	 */
	totem_table_signals[METADATA_UPDATED] =
		g_signal_new ("metadata-updated",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_LAST,
				G_STRUCT_OFFSET (TotemObjectClass, metadata_updated),
				NULL, NULL,
	                        g_cclosure_marshal_generic,
				G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);

	/**
	 * TotemObject::get-user-agent:
	 * @totem: the #TotemObject which received the signal
	 * @mrl: the MRL of the opened stream
	 *
	 * The #TotemObject::get-user-agent signal is emitted before opening a stream, so that plugins
	 * have the opportunity to return the user-agent to be set.
	 *
	 * Return value: allocated string representing the user-agent to use for @mrl
	 */
	totem_table_signals[GET_USER_AGENT] =
		g_signal_new ("get-user-agent",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemObjectClass, get_user_agent),
			      accumulator_first_non_null_wins, NULL,
	                      g_cclosure_marshal_generic,
			      G_TYPE_STRING, 1, G_TYPE_STRING);

	/**
	 * TotemObject::get-text-subtitle:
	 * @totem: the #TotemObject which received the signal
	 * @mrl: the MRL of the opened stream
	 *
	 * The #TotemObject::get-text-subtitle signal is emitted before opening a stream, so that plugins
	 * have the opportunity to detect or download text subtitles for the stream if necessary.
	 *
	 * Return value: allocated string representing the URI of the subtitle to use for @mrl
	 */
	totem_table_signals[GET_TEXT_SUBTITLE] =
		g_signal_new ("get-text-subtitle",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemObjectClass, get_text_subtitle),
			      accumulator_first_non_null_wins, NULL,
	                      g_cclosure_marshal_generic,
			      G_TYPE_STRING, 1, G_TYPE_STRING);
}

static void
totem_object_init (TotemObject *totem)
{
	GtkSettings *gtk_settings;

	if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
		g_warning ("gtk-clutter failed to initialise, expect problems from here on.");

	gtk_settings = gtk_settings_get_default ();
	g_object_set (G_OBJECT (gtk_settings), "gtk-application-prefer-dark-theme", TRUE, NULL);

	totem->settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);

	g_application_add_main_option_entries (G_APPLICATION (totem), all_options);
	g_application_add_option_group (G_APPLICATION (totem), bacon_video_widget_get_option_group ());

	totem_app_actions_setup (totem);
}

static void
totem_object_finalize (GObject *object)
{
	TotemObject *totem = TOTEM_OBJECT (object);

	g_clear_pointer (&totem->title, g_free);
	g_clear_pointer (&totem->subtitle, g_free);
	g_clear_pointer (&totem->search_string, g_free);
	g_clear_pointer (&totem->player_title, g_free);
	g_clear_object (&totem->custom_title);

	G_OBJECT_CLASS (totem_object_parent_class)->finalize (object);
}

static void
totem_object_get_property (GObject *object,
			   guint property_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	TotemObject *totem;

	totem = TOTEM_OBJECT (object);

	switch (property_id)
	{
	case PROP_FULLSCREEN:
		g_value_set_boolean (value, totem_object_is_fullscreen (totem));
		break;
	case PROP_PLAYING:
		g_value_set_boolean (value, totem_object_is_playing (totem));
		break;
	case PROP_STREAM_LENGTH:
		g_value_set_int64 (value, bacon_video_widget_get_stream_length (totem->bvw));
		break;
	case PROP_CURRENT_TIME:
		g_value_set_int64 (value, bacon_video_widget_get_current_time (totem->bvw));
		break;
	case PROP_SEEKABLE:
		g_value_set_boolean (value, totem_object_is_seekable (totem));
		break;
	case PROP_CURRENT_MRL:
		g_value_set_string (value, totem->mrl);
		break;
	case PROP_CURRENT_CONTENT_TYPE:
		g_value_take_string (value, totem_playlist_get_current_content_type (totem->playlist));
		break;
	case PROP_CURRENT_DISPLAY_NAME:
		g_value_take_string (value, totem_playlist_get_current_title (totem->playlist));
		break;
	case PROP_MAIN_PAGE:
		g_value_set_string (value, totem_object_get_main_page (totem));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

/**
 * totem_object_plugins_init:
 * @totem: a #TotemObject
 *
 * Initialises the plugin engine and activates all the
 * enabled plugins.
 **/
void
totem_object_plugins_init (TotemObject *totem)
{
	if (totem->engine == NULL)
		totem->engine = totem_plugins_engine_get_default (totem);
}

/**
 * totem_object_plugins_shutdown:
 * @totem: a #TotemObject
 *
 * Shuts down the plugin engine and deactivates all the
 * plugins.
 **/
void
totem_object_plugins_shutdown (TotemObject *totem)
{
	if (totem->engine)
		totem_plugins_engine_shut_down (totem->engine);
	g_clear_object (&totem->engine);
}

/**
 * totem_object_get_main_window:
 * @totem: a #TotemObject
 *
 * Gets Totem's main window and increments its reference count.
 *
 * Return value: (transfer full): Totem's main window
 **/
GtkWindow *
totem_object_get_main_window (TotemObject *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	g_object_ref (G_OBJECT (totem->win));

	return GTK_WINDOW (totem->win);
}

/**
 * totem_object_get_menu_section:
 * @totem: a #TotemObject
 * @id: the ID for the menu section to look up
 *
 * Get the #GMenu of the given @id from the main Totem #GtkBuilder file.
 *
 * Return value: (transfer none) (nullable): a #GMenu or %NULL on failure
 **/
GMenu *
totem_object_get_menu_section (TotemObject *totem,
			       const char  *id)
{
	GObject *object;
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	object = gtk_builder_get_object (totem->xml, id);
	if (object == NULL || !G_IS_MENU (object))
		return NULL;

	return G_MENU (object);
}

/**
 * totem_object_empty_menu_section:
 * @totem: a #TotemObject
 * @id: the ID for the menu section to empty
 *
 * Empty the GMenu section pointed to by @id, and remove any
 * related actions. Note that menu items with specific target
 * will not have the associated action removed.
 **/
void
totem_object_empty_menu_section (TotemObject *totem,
				 const char  *id)
{
	GMenu *menu;

	g_return_if_fail (TOTEM_IS_OBJECT (totem));

	menu = G_MENU (gtk_builder_get_object (totem->xml, id));
	g_return_if_fail (menu != NULL);

	while (g_menu_model_get_n_items (G_MENU_MODEL (menu)) > 0) {
		const char *action;
		g_menu_model_get_item_attribute (G_MENU_MODEL (menu), 0, G_MENU_ATTRIBUTE_ACTION, "s", &action);
		if (g_str_has_prefix (action, "app.")) {
			GVariant *target;

			target = g_menu_model_get_item_attribute_value (G_MENU_MODEL (menu), 0, G_MENU_ATTRIBUTE_TARGET, NULL);

			/* Don't remove actions that have a specific target */
			if (target == NULL)
				g_action_map_remove_action (G_ACTION_MAP (totem), action + strlen ("app."));
			else
				g_variant_unref (target);
		}
		g_menu_remove (G_MENU (menu), 0);
	}
}

/**
 * totem_object_get_video_widget:
 * @totem: a #TotemObject
 *
 * Gets Totem's video widget and increments its reference count.
 *
 * Return value: (transfer full): Totem's video widget
 **/
GtkWidget *
totem_object_get_video_widget (TotemObject *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), NULL);

	g_object_ref (G_OBJECT (totem->bvw));

	return GTK_WIDGET (totem->bvw);
}

/**
 * totem_object_get_current_time:
 * @totem: a #TotemObject
 *
 * Gets the current position's time in the stream as a gint64.
 *
 * Return value: the current position in the stream
 **/
gint64
totem_object_get_current_time (TotemObject *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), 0);

	return bacon_video_widget_get_current_time (totem->bvw);
}

static void
add_items_to_playlist_and_play_cb (TotemPlaylist *playlist, GAsyncResult *async_result, TotemObject *totem)
{
	char *mrl, *subtitle;

	/* totem_playlist_add_mrls_finish() never returns an error */
	totem_playlist_add_mrls_finish (playlist, async_result, NULL);

	totem_signal_unblock_by_data (playlist, totem);

	/* And start playback */
	mrl = totem_playlist_get_current_mrl (playlist, &subtitle);
	totem_object_set_mrl_and_play (totem, mrl, subtitle);
	g_free (mrl);
	g_free (subtitle);
}

typedef struct {
	TotemObject *totem;
	gchar *uri;
	gchar *display_name;
	gboolean play;
} AddToPlaylistData;

static void
add_to_playlist_and_play_cb (TotemPlaylist *playlist, GAsyncResult *async_result, AddToPlaylistData *data)
{
	int end = -1;
	gboolean playlist_changed;
	GError *error = NULL;

	playlist_changed = totem_playlist_add_mrl_finish (playlist, async_result, &error);

	if (playlist_changed == FALSE && error != NULL) {
		/* FIXME: Crappy dialogue */
		totem_object_show_error (data->totem, "", error->message);
		g_error_free (error);
	}

	if (data->play)
		end = totem_playlist_get_last (playlist);

	totem_signal_unblock_by_data (playlist, data->totem);

	if (data->play && playlist_changed && end != -1) {
		char *mrl, *subtitle;

		subtitle = NULL;
		totem_playlist_set_current (playlist, end);
		mrl = totem_playlist_get_current_mrl (playlist, &subtitle);
		totem_object_set_mrl_and_play (data->totem, mrl, subtitle);
		g_free (mrl);
		g_free (subtitle);
	}

	/* Free the closure data */
	g_object_unref (data->totem);
	g_free (data->uri);
	g_free (data->display_name);
	g_slice_free (AddToPlaylistData, data);
}

static gboolean
save_session_timeout_cb (Totem *totem)
{
	totem_session_save (totem);
	return TRUE;
}

static void
setup_save_timeout_cb (Totem    *totem,
		       gboolean  enable)
{
	if (enable && totem->save_timeout_id == 0) {
		totem->save_timeout_id = g_timeout_add_seconds (TOTEM_SESSION_SAVE_TIMEOUT,
								(GSourceFunc) save_session_timeout_cb,
								totem);
		g_source_set_name_by_id (totem->save_timeout_id, "[totem] save_session_timeout_cb");
	} else if (totem->save_timeout_id > 0) {
		g_source_remove (totem->save_timeout_id);
		totem->save_timeout_id = 0;
	}
}

/**
 * totem_object_add_to_playlist:
 * @totem: a #TotemObject
 * @uri: the URI to add to the playlist
 * @display_name: the display name of the URI
 * @play: whether to play the added item
 *
 * Add @uri to the playlist and play it immediately.
 **/
void
totem_object_add_to_playlist (TotemObject *totem,
			      const char  *uri,
			      const char  *display_name,
			      gboolean     play)
{
	AddToPlaylistData *data;

	/* Block all signals from the playlist until we're finished. They're unblocked in the callback, add_to_playlist_and_play_cb.
	 * There are no concurrency issues here, since blocking the signals multiple times should require them to be unblocked the
	 * same number of times before they fire again. */
	totem_signal_block_by_data (totem->playlist, totem);

	data = g_slice_new (AddToPlaylistData);
	data->totem = g_object_ref (totem);
	data->uri = g_strdup (uri);
	data->display_name = g_strdup (display_name);
	data->play = play;

	totem_playlist_add_mrl (totem->playlist, uri, display_name, TRUE,
	                        NULL, (GAsyncReadyCallback) add_to_playlist_and_play_cb, data);
}

/**
 * totem_object_add_items_to_playlist:
 * @totem: a #TotemObject
 * @items: a #GList of #TotemPlaylistMrlData
 *
 * Add @items to the playlist and play them immediately.
 * This function takes ownership of both the list and its elements when
 * called, so don't free either after calling
 * totem_object_add_items_to_playlist().
 **/
void
totem_object_add_items_to_playlist (TotemObject *totem,
				    GList       *items)
{
	/* Block all signals from the playlist until we're finished. They're unblocked in the callback, add_to_playlist_and_play_cb.
	 * There are no concurrency issues here, since blocking the signals multiple times should require them to be unblocked the
	 * same number of times before they fire again. */
	totem_signal_block_by_data (totem->playlist, totem);

	totem_playlist_add_mrls (totem->playlist, items, TRUE, NULL,
				 (GAsyncReadyCallback) add_items_to_playlist_and_play_cb, totem);
}

/**
 * totem_object_clear_playlist:
 * @totem: a #TotemObject
 *
 * Empties the current playlist.
 **/
void
totem_object_clear_playlist (TotemObject *totem)
{
	totem_playlist_clear (totem->playlist);
}

/**
 * totem_object_get_current_mrl:
 * @totem: a #TotemObject
 *
 * Get the MRL of the current stream, or %NULL if nothing's playing.
 * Free with g_free().
 *
 * Return value: a newly-allocated string containing the MRL of the current stream
 **/
char *
totem_object_get_current_mrl (TotemObject *totem)
{
	return totem_playlist_get_current_mrl (totem->playlist, NULL);
}

/**
 * totem_object_get_playlist_length:
 * @totem: a #TotemObject
 *
 * Returns the length of the current playlist.
 *
 * Return value: the playlist length
 **/
guint
totem_object_get_playlist_length (TotemObject *totem)
{
	int last;

	last = totem_playlist_get_last (totem->playlist);
	if (last == -1)
		return 0;
	return last + 1;
}

/**
 * totem_object_get_playlist_pos:
 * @totem: a #TotemObject
 *
 * Returns the <code class="literal">0</code>-based index of the current entry in the playlist. If
 * there is no current entry in the playlist, <code class="literal">-1</code> is returned.
 *
 * Return value: the index of the current playlist entry, or <code class="literal">-1</code>
 **/
int
totem_object_get_playlist_pos (TotemObject *totem)
{
	return totem_playlist_get_current (totem->playlist);
}

/**
 * totem_object_get_title_at_playlist_pos:
 * @totem: a #TotemObject
 * @playlist_index: the <code class="literal">0</code>-based entry index
 *
 * Gets the title of the playlist entry at @index.
 *
 * Return value: the entry title at @index, or %NULL; free with g_free()
 **/
char *
totem_object_get_title_at_playlist_pos (TotemObject *totem, guint playlist_index)
{
	return totem_playlist_get_title (totem->playlist, playlist_index);
}

/**
 * totem_object_get_short_title:
 * @totem: a #TotemObject
 *
 * Gets the title of the current entry in the playlist.
 *
 * Return value: the current entry's title, or %NULL; free with g_free()
 **/
char *
totem_object_get_short_title (TotemObject *totem)
{
	return totem_playlist_get_current_title (totem->playlist);
}

/**
 * totem_object_add_to_view:
 * @totem: a #TotemObject
 * @file: a #GFile representing a media
 * @title: a title for the media, or %NULL
 *
 * Adds a local media file to the main view.
 *
 **/
void
totem_object_add_to_view (TotemObject *totem,
			  GFile       *file,
			  const char  *title)
{
	char *uri;

	uri = g_file_get_uri (file);
	if (!totem_grilo_add_item_to_recent (TOTEM_GRILO (totem->grilo),
					     uri, title, FALSE)) {
		g_warning ("Failed to add '%s' to view", uri);
	}
	g_free (uri);
}

/**
 * totem_object_set_current_subtitle:
 * @totem: a #TotemObject
 * @subtitle_uri: the URI of the subtitle file to add
 *
 * Add the @subtitle_uri subtitle file to the playlist, setting it as the subtitle for the current
 * playlist entry.
 **/
void
totem_object_set_current_subtitle (TotemObject *totem, const char *subtitle_uri)
{
	totem_playlist_set_current_subtitle (totem->playlist, subtitle_uri);
}

void
totem_object_set_main_page (TotemObject *totem,
			    const char  *page_id)
{
	if (g_strcmp0 (page_id, gtk_stack_get_visible_child_name (GTK_STACK (totem->stack))) == 0) {
		if (g_strcmp0 (page_id, "grilo") == 0)
			totem_grilo_start (TOTEM_GRILO (totem->grilo));
		else
			totem_grilo_pause (TOTEM_GRILO (totem->grilo));
		return;
	}

	gtk_stack_set_visible_child_full (GTK_STACK (totem->stack), page_id, GTK_STACK_TRANSITION_TYPE_NONE);

	if (g_strcmp0 (page_id, "player") == 0) {
		totem_grilo_pause (TOTEM_GRILO (totem->grilo));
		g_object_get (totem->header,
			      "title", &totem->title,
			      "subtitle", &totem->subtitle,
			      "search-string", &totem->search_string,
			      "select-mode", &totem->select_mode,
			      "custom-title", &totem->custom_title,
			      NULL);
		g_object_set (totem->header,
			      "show-back-button", TRUE,
			      "show-select-button", FALSE,
			      "show-search-button", FALSE,
			      "title", totem->player_title,
			      "subtitle", NULL,
			      "search-string", NULL,
			      "select-mode", FALSE,
			      "custom-title", NULL,
			      NULL);
		gtk_widget_show (totem->fullscreen_button);
		gtk_widget_show (totem->gear_button);
		gtk_widget_hide (totem->add_button);
		gtk_widget_hide (totem->main_menu_button);
		bacon_video_widget_show_popup (totem->bvw);
	} else if (g_strcmp0 (page_id, "grilo") == 0) {
		totem_grilo_start (TOTEM_GRILO (totem->grilo));
		g_object_set (totem->header,
			      "show-back-button", totem_grilo_get_show_back_button (TOTEM_GRILO (totem->grilo)),
			      "show-select-button", TRUE,
			      "show-search-button", TRUE,
			      "title", totem->title,
			      "subtitle", totem->subtitle,
			      "search-string", totem->search_string,
			      "select-mode", totem->select_mode,
			      "custom-title", totem->custom_title,
			      NULL);
		g_clear_pointer (&totem->title, g_free);
		g_clear_pointer (&totem->subtitle, g_free);
		g_clear_pointer (&totem->search_string, g_free);
		g_clear_pointer (&totem->player_title, g_free);
		g_clear_object (&totem->custom_title);
		gtk_widget_show (totem->main_menu_button);
		gtk_widget_hide (totem->fullscreen_button);
		gtk_widget_hide (totem->gear_button);
		if (totem_grilo_get_current_page (TOTEM_GRILO (totem->grilo)) == TOTEM_GRILO_PAGE_RECENT)
			gtk_widget_show (totem->add_button);
		totem_grilo_start (TOTEM_GRILO (totem->grilo));
	}

	g_object_notify (G_OBJECT (totem), "main-page");
}

/**
 * totem_object_get_main_page:
 * @totem: a #TotemObject
 *
 * Gets the identifier for the current page in Totem's
 * main view.
 *
 * Return value: identifier for current page
 */
const char *
totem_object_get_main_page (Totem *totem)
{
	return gtk_stack_get_visible_child_name (GTK_STACK (totem->stack));
}

/*
 * emit_file_opened:
 * @totem: a #TotemObject
 * @mrl: the MRL opened
 *
 * Emits the #TotemObject::file-opened signal on @totem, with the
 * specified @mrl.
 **/
static void
emit_file_opened (TotemObject *totem,
		   const char *mrl)
{
	totem_session_save (totem);
	setup_save_timeout_cb (totem, TRUE);
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[FILE_OPENED],
		       0, mrl);
}

/*
 * emit_file_closed:
 * @totem: a #TotemObject
 *
 * Emits the #TotemObject::file-closed signal on @totem.
 **/
static void
emit_file_closed (TotemObject *totem)
{
	setup_save_timeout_cb (totem, FALSE);
	totem_session_save (totem);
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[FILE_CLOSED],
		       0);
}

/**
 * totem_file_has_played:
 * @totem: a #TotemObject
 *
 * Emits the #TotemObject::file-played signal on @totem.
 **/
void
totem_file_has_played (TotemObject *totem,
		       const char  *mrl)
{
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[FILE_HAS_PLAYED],
		       0, mrl);
}

/*
 * emit_metadata_updated:
 * @totem: a #TotemObject
 * @artist: the stream's artist, or %NULL
 * @title: the stream's title, or %NULL
 * @album: the stream's album, or %NULL
 * @track_num: the track number of the stream
 *
 * Emits the #TotemObject::metadata-updated signal on @totem,
 * with the specified stream data.
 **/
static void
emit_metadata_updated (TotemObject *totem,
			const char *artist,
			const char *title,
			const char *album,
			guint track_num)
{
	g_signal_emit (G_OBJECT (totem),
		       totem_table_signals[METADATA_UPDATED],
		       0,
		       artist,
		       title,
		       album,
		       track_num);
}

GQuark
totem_remote_command_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("totem_remote_command");

	return quark;
}

/* This should really be standard. */
#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType
totem_remote_command_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_UNKNOWN, "unknown"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PLAY, "play"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PAUSE, "pause"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_STOP, "stop"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PLAYPAUSE, "play-pause"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_NEXT, "next"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PREVIOUS, "previous"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SEEK_FORWARD, "seek-forward"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SEEK_BACKWARD, "seek-backward"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_VOLUME_UP, "volume-up"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_VOLUME_DOWN, "volume-down"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_FULLSCREEN, "fullscreen"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_QUIT, "quit"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_ENQUEUE, "enqueue"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_REPLACE, "replace"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SHOW, "show"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_UP, "up"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_DOWN, "down"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_LEFT, "left"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_RIGHT, "right"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_SELECT, "select"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_DVD_MENU, "dvd-menu"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_ZOOM_UP, "zoom-up"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_ZOOM_DOWN, "zoom-down"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_EJECT, "eject"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_PLAY_DVD, "play-dvd"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_MUTE, "mute"),
			ENUM_ENTRY (TOTEM_REMOTE_COMMAND_TOGGLE_ASPECT, "toggle-aspect-ratio"),
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TotemRemoteCommand", values);
	}

	return etype;
}

GQuark
totem_remote_setting_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("totem_remote_setting");

	return quark;
}

GType
totem_remote_setting_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			ENUM_ENTRY (TOTEM_REMOTE_SETTING_REPEAT, "repeat"),
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("TotemRemoteSetting", values);
	}

	return etype;
}

static void
reset_seek_status (TotemObject *totem)
{
	/* Release the lock and reset everything so that we
	 * avoid being "stuck" seeking on errors */

	if (totem->seek_lock != FALSE) {
		totem->seek_lock = FALSE;
		bacon_video_widget_unmark_popup_busy (totem->bvw, "seek started");
		bacon_video_widget_seek (totem->bvw, 0, NULL);
		bacon_video_widget_stop (totem->bvw);
		play_pause_set_label (totem, STATE_STOPPED);
	}
}

/**
 * totem_object_show_error:
 * @totem: a #TotemObject
 * @title: the error dialog title
 * @reason: the error dialog text
 *
 * Displays a non-blocking error dialog with the
 * given @title and @reason.
 **/
void
totem_object_show_error (TotemObject *totem, const char *title, const char *reason)
{
	reset_seek_status (totem);
	totem_interface_error (title, reason,
			GTK_WINDOW (totem->win));
}

G_GNUC_NORETURN void
totem_object_show_error_and_exit (const char *title,
		const char *reason, TotemObject *totem)
{
	reset_seek_status (totem);
	totem_interface_error_blocking (title, reason,
			GTK_WINDOW (totem->win));
	totem_object_exit (totem);
}

static void
totem_object_save_size (TotemObject *totem)
{
	if (totem->bvw == NULL)
		return;

	if (totem_object_is_fullscreen (totem) != FALSE)
		return;

	/* Save the size of the video widget */
	gtk_window_get_size (GTK_WINDOW (totem->win), &totem->window_w, &totem->window_h);
}

static void
totem_object_save_state (TotemObject *totem)
{
	GKeyFile *keyfile;
	char *contents, *filename;

	if (totem->win == NULL)
		return;
	if (totem->window_w == 0
	    || totem->window_h == 0)
		return;

	keyfile = g_key_file_new ();
	g_key_file_set_integer (keyfile, "State",
				"window_w", totem->window_w);
	g_key_file_set_integer (keyfile, "State",
			"window_h", totem->window_h);
	g_key_file_set_boolean (keyfile, "State",
			"maximised", totem->maximised);

	contents = g_key_file_to_data (keyfile, NULL, NULL);
	g_key_file_free (keyfile);
	filename = g_build_filename (totem_dot_dir (), "state.ini", NULL);
	g_file_set_contents (filename, contents, -1, NULL);

	g_free (filename);
	g_free (contents);
}

/**
 * totem_object_exit:
 * @totem: a #TotemObject
 *
 * Closes Totem.
 **/
void
totem_object_exit (TotemObject *totem)
{
	GdkDisplay *display = NULL;

	/* Shut down the plugins first, allowing them to display modal dialogues (etc.) without threat of being killed from another thread */
	if (totem != NULL && totem->engine != NULL)
		totem_object_plugins_shutdown (totem);

	if (gtk_main_level () > 0)
		gtk_main_quit ();

	if (totem == NULL)
		exit (0);

	if (totem->bvw)
		totem_object_save_size (totem);

	if (totem->win != NULL) {
		gtk_widget_hide (totem->win);
		display = gtk_widget_get_display (totem->win);
	}

	if (totem->prefs != NULL)
		gtk_widget_hide (totem->prefs);

	if (display != NULL)
		gdk_display_sync (display);

	setup_save_timeout_cb (totem, FALSE);
	totem_session_cleanup (totem);

	if (totem->bvw)
		bacon_video_widget_close (totem->bvw);

	totem_object_save_state (totem);

	totem_sublang_exit (totem);
	totem_destroy_file_filters ();

	g_clear_object (&totem->settings);

	if (totem->win)
		gtk_widget_destroy (GTK_WIDGET (totem->win));

	g_object_unref (totem);

	exit (0);
}

G_GNUC_NORETURN gboolean
main_window_destroy_cb (GtkWidget *widget, GdkEvent *event, TotemObject *totem)
{
	totem_object_exit (totem);
}

static void
play_pause_set_label (TotemObject *totem, TotemStates state)
{
	GtkWidget *image;
	const char *id, *tip;

	if (state == totem->state)
		return;

	switch (state)
	{
	case STATE_PLAYING:
		id = "media-playback-pause-symbolic";
		tip = N_("Pause");
		totem_playlist_set_playing (totem->playlist, TOTEM_PLAYLIST_STATUS_PLAYING);
		break;
	case STATE_PAUSED:
		id = "media-playback-start-symbolic";
		tip = N_("Play");
		totem_playlist_set_playing (totem->playlist, TOTEM_PLAYLIST_STATUS_PAUSED);
		break;
	case STATE_STOPPED:
		bacon_time_label_set_time (totem->time_label,
					   0, 0);
		bacon_time_label_set_time (totem->time_rem_label,
					   0, 0);
		id = "media-playback-start-symbolic";
		totem_playlist_set_playing (totem->playlist, TOTEM_PLAYLIST_STATUS_NONE);
		tip = N_("Play");
		break;
	default:
		g_assert_not_reached ();
		return;
	}

	gtk_widget_set_tooltip_text (totem->play_button, _(tip));
	image = gtk_button_get_image (GTK_BUTTON (totem->play_button));
	gtk_image_set_from_icon_name (GTK_IMAGE (image), id, GTK_ICON_SIZE_MENU);

	totem->state = state;

	g_object_notify (G_OBJECT (totem), "playing");
}

void
totem_object_eject (TotemObject *totem)
{
	GMount *mount;

	mount = totem_get_mount_for_media (totem->mrl);
	if (mount == NULL)
		return;

	g_clear_pointer (&totem->mrl, g_free);
	bacon_video_widget_close (totem->bvw);
	emit_file_closed (totem);
	totem->has_played_emitted = FALSE;

	/* The volume monitoring will take care of removing the items */
	g_mount_eject_with_operation (mount, G_MOUNT_UNMOUNT_NONE, NULL, NULL, NULL, NULL);
	g_object_unref (mount);
}

/**
 * totem_object_play:
 * @totem: a #TotemObject
 *
 * Plays the current stream. If Totem is already playing, it continues
 * to play. If the stream cannot be played, and error dialog is displayed.
 **/
void
totem_object_play (TotemObject *totem)
{
	GError *err = NULL;
	int retval;
	char *msg, *disp;

	if (totem->mrl == NULL)
		return;

	if (bacon_video_widget_is_playing (totem->bvw) != FALSE)
		return;

	retval = bacon_video_widget_play (totem->bvw,  &err);
	play_pause_set_label (totem, retval ? STATE_PLAYING : STATE_STOPPED);

	if (retval != FALSE) {
		if (totem->has_played_emitted == FALSE) {
			totem_file_has_played (totem, totem->mrl);
			totem->has_played_emitted = TRUE;
		}
		return;
	}

	disp = totem_uri_escape_for_display (totem->mrl);
	msg = g_strdup_printf(_("Totem could not play “%s”."), disp);
	g_free (disp);

	totem_object_show_error (totem, msg, err->message);
	bacon_video_widget_stop (totem->bvw);
	play_pause_set_label (totem, STATE_STOPPED);
	g_free (msg);
	g_error_free (err);
}

static void
totem_object_seek (TotemObject *totem, double pos)
{
	GError *err = NULL;
	int retval;

	if (totem->mrl == NULL)
		return;
	if (bacon_video_widget_is_seekable (totem->bvw) == FALSE)
		return;

	retval = bacon_video_widget_seek (totem->bvw, pos, &err);

	if (retval == FALSE)
	{
		char *msg, *disp;

		disp = totem_uri_escape_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play “%s”."), disp);
		g_free (disp);

		reset_seek_status (totem);

		totem_object_show_error (totem, msg, err->message);
		g_free (msg);
		g_error_free (err);
	}
}

static void
totem_object_set_mrl_and_play (TotemObject *totem, const char *mrl, const char *subtitle)
{
	totem_object_set_mrl (totem, mrl, subtitle);
	totem_object_play (totem);
}

static gboolean
totem_object_open_dialog (TotemObject *totem, const char *path)
{
	GSList *filenames, *l;

	filenames = totem_add_files (GTK_WINDOW (totem->win), path);

	if (filenames == NULL)
		return FALSE;

	for (l = filenames; l != NULL; l = l->next) {
		char *uri = l->data;

		totem_grilo_add_item_to_recent (TOTEM_GRILO (totem->grilo), uri, NULL, FALSE);
		g_free (uri);
	}
	g_slist_free (filenames);

	return TRUE;
}

/**
 * totem_object_play_pause:
 * @totem: a #TotemObject
 *
 * Gets the current MRL from the playlist and attempts to play it.
 * If the stream is already playing, playback is paused.
 **/
void
totem_object_play_pause (TotemObject *totem)
{
	if (totem->mrl == NULL) {
		char *mrl, *subtitle;

		/* Try to pull an mrl from the playlist */
		mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);
		if (mrl == NULL) {
			play_pause_set_label (totem, STATE_STOPPED);
			return;
		} else {
			totem_object_set_mrl_and_play (totem, mrl, subtitle);
			g_free (mrl);
			g_free (subtitle);
			return;
		}
	}

	if (bacon_video_widget_is_playing (totem->bvw) == FALSE) {
		if (bacon_video_widget_play (totem->bvw, NULL) != FALSE &&
		    totem->has_played_emitted == FALSE) {
			totem_file_has_played (totem, totem->mrl);
			totem->has_played_emitted = TRUE;
		}
		play_pause_set_label (totem, STATE_PLAYING);
	} else {
		bacon_video_widget_pause (totem->bvw);
		play_pause_set_label (totem, STATE_PAUSED);
	}
}

/**
 * totem_object_stop:
 * @totem: a #TotemObject
 *
 * Stops playback, and sets the playlist back at the start.
 */
void
totem_object_stop (TotemObject *totem)
{
	char *mrl, *subtitle;

	totem_playlist_set_at_start (totem->playlist);
	update_buttons (totem);
	bacon_video_widget_stop (totem->bvw);
	play_pause_set_label (totem, STATE_STOPPED);
	mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);
	if (mrl != NULL) {
		totem_object_set_mrl (totem, mrl, subtitle);
		bacon_video_widget_pause (totem->bvw);
		g_free (mrl);
		g_free (subtitle);
	}
}

/**
 * totem_object_pause:
 * @totem: a #TotemObject
 *
 * Pauses the current stream. If Totem is already paused, it continues
 * to be paused.
 **/
void
totem_object_pause (TotemObject *totem)
{
	if (bacon_video_widget_is_playing (totem->bvw) != FALSE) {
		bacon_video_widget_pause (totem->bvw);
		play_pause_set_label (totem, STATE_PAUSED);
	}
}

gboolean
window_state_event_cb (GtkWidget           *window,
		       GdkEventWindowState *event,
		       TotemObject         *totem)
{
	GAction *action;

	totem->maximised = !!(event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);

	if ((event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) == 0)
		return FALSE;

	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
		if (totem->controls_visibility != TOTEM_CONTROLS_UNDEFINED)
			totem_object_save_size (totem);

		totem->controls_visibility = TOTEM_CONTROLS_FULLSCREEN;
		show_controls (totem, FALSE);
	} else {
		totem->controls_visibility = TOTEM_CONTROLS_VISIBLE;
		show_controls (totem, TRUE);
	}

	bacon_video_widget_set_fullscreen (totem->bvw,
					   totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN);

	action = g_action_map_lookup_action (G_ACTION_MAP (totem), "fullscreen");
	g_simple_action_set_state (G_SIMPLE_ACTION (action),
				   g_variant_new_boolean (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN));

	g_object_notify (G_OBJECT (totem), "fullscreen");

	return FALSE;
}

static void
totem_object_action_fullscreen_toggle (TotemObject *totem)
{
	if (totem_object_is_fullscreen (totem) != FALSE)
		gtk_window_unfullscreen (GTK_WINDOW (totem->win));
	else
		gtk_window_fullscreen (GTK_WINDOW (totem->win));
}

/**
 * totem_object_set_fullscreen:
 * @totem: a #TotemObject
 * @state: %TRUE if Totem should be fullscreened
 *
 * Sets Totem's fullscreen state according to @state.
 **/
void
totem_object_set_fullscreen (TotemObject *totem, gboolean state)
{
	if (totem_object_is_fullscreen (totem) == state)
		return;

	if (state)
		gtk_window_fullscreen (GTK_WINDOW (totem->win));
	else
		gtk_window_unfullscreen (GTK_WINDOW (totem->win));
}

void
totem_object_open (TotemObject *totem)
{
	totem_object_open_dialog (totem, NULL);
}

static void
totem_open_location_response_cb (GtkDialog *dialog, gint response, TotemObject *totem)
{
	char *uri;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (totem->open_location));
		return;
	}

	gtk_widget_hide (GTK_WIDGET (dialog));

	/* Open the specified URI */
	uri = totem_open_location_get_uri (totem->open_location);

	if (uri != NULL) {
		totem_grilo_add_item_to_recent (TOTEM_GRILO (totem->grilo), uri, NULL, TRUE);
		g_free (uri);
	}

	gtk_widget_destroy (GTK_WIDGET (totem->open_location));
}

void
totem_object_open_location (TotemObject *totem)
{
	if (totem->open_location != NULL) {
		gtk_window_present (GTK_WINDOW (totem->open_location));
		return;
	}

	totem->open_location = TOTEM_OPEN_LOCATION (totem_open_location_new ());

	g_signal_connect (G_OBJECT (totem->open_location), "delete-event",
			G_CALLBACK (gtk_widget_destroy), NULL);
	g_signal_connect (G_OBJECT (totem->open_location), "response",
			G_CALLBACK (totem_open_location_response_cb), totem);
	g_object_add_weak_pointer (G_OBJECT (totem->open_location), (gpointer *)&(totem->open_location));

	gtk_window_set_transient_for (GTK_WINDOW (totem->open_location),
			GTK_WINDOW (totem->win));
	gtk_widget_show (GTK_WIDGET (totem->open_location));
}

static char *
totem_get_nice_name_for_stream (TotemObject *totem)
{
	GValue title_value = { 0, };
	GValue album_value = { 0, };
	GValue artist_value = { 0, };
	GValue value = { 0, };
	char *retval;
	int tracknum;

	bacon_video_widget_get_metadata (totem->bvw, BVW_INFO_TITLE, &title_value);
	bacon_video_widget_get_metadata (totem->bvw, BVW_INFO_ARTIST, &artist_value);
	bacon_video_widget_get_metadata (totem->bvw, BVW_INFO_ALBUM, &album_value);
	bacon_video_widget_get_metadata (totem->bvw,
					 BVW_INFO_TRACK_NUMBER,
					 &value);

	tracknum = g_value_get_int (&value);
	g_value_unset (&value);

	emit_metadata_updated (totem,
	                       g_value_get_string (&artist_value),
	                       g_value_get_string (&title_value),
	                       g_value_get_string (&album_value),
	                       tracknum);

	if (g_value_get_string (&title_value) == NULL) {
		retval = NULL;
		goto bail;
	}
	if (g_value_get_string (&artist_value) == NULL) {
		retval = g_value_dup_string (&title_value);
		goto bail;
	}

	if (tracknum != 0) {
		retval = g_strdup_printf ("%02d. %s - %s",
					  tracknum,
					  g_value_get_string (&artist_value),
					  g_value_get_string (&title_value));
	} else {
		retval = g_strdup_printf ("%s - %s",
					  g_value_get_string (&artist_value),
					  g_value_get_string (&title_value));
	}

bail:
	g_value_unset (&album_value);
	g_value_unset (&artist_value);
	g_value_unset (&title_value);

	return retval;
}

static void
update_mrl_label (TotemObject *totem, const char *name)
{
	if (name != NULL) {
		/* Update the mrl label */
		g_clear_pointer (&totem->player_title, g_free);
		totem->player_title = g_strdup (name);
	} else {
		bacon_time_label_set_time (totem->time_label,
					   0, 0);
		bacon_time_label_set_time (totem->time_rem_label,
					   0, 0);

		g_object_notify (G_OBJECT (totem), "stream-length");

		/* Update the mrl label */
		g_clear_pointer (&totem->player_title, g_free);
	}

	if (g_strcmp0 (totem_object_get_main_page (totem), "player") == 0)
		g_object_set (totem->header, "title", totem->player_title, NULL);
}

static void
totem_object_set_next_subtitle (TotemObject *totem,
				const char  *subtitle)
{
	g_clear_pointer (&totem->next_subtitle, g_free);
	totem->next_subtitle = g_strdup (subtitle);
}

/**
 * totem_object_set_mrl:
 * @totem: a #TotemObject
 * @mrl: the MRL to play
 * @subtitle: a subtitle file to load, or %NULL
 *
 * Loads the specified @mrl and optionally the specified subtitle
 * file. If @subtitle is %NULL Totem will attempt to auto-locate
 * any subtitle files for @mrl.
 *
 * If a stream is already playing, it will be stopped and closed.
 *
 * Errors will be reported asynchronously.
 **/
void
totem_object_set_mrl (TotemObject *totem,
		      const char *mrl,
		      const char *subtitle)
{
	if (totem->mrl != NULL) {
		totem->pause_start = FALSE;

		g_clear_pointer (&totem->mrl, g_free);
		bacon_video_widget_close (totem->bvw);
		emit_file_closed (totem);
		totem->has_played_emitted = FALSE;
		play_pause_set_label (totem, STATE_STOPPED);
		update_fill (totem, -1.0);
	}

	if (mrl == NULL) {
		play_pause_set_label (totem, STATE_STOPPED);

		/* Play/Pause */
		totem_object_set_sensitivity2 ("play", FALSE);

		/* Volume */
		totem_controls_set_sensitivity ("volume_button", FALSE);
		totem->volume_sensitive = FALSE;

		/* Control popup */
		totem_object_set_sensitivity2 ("next-chapter", FALSE);
		totem_object_set_sensitivity2 ("previous-chapter", FALSE);

		/* Subtitle selection */
		totem_object_set_sensitivity2 ("select-subtitle", FALSE);

		/* Set the logo */
		bacon_video_widget_set_logo_mode (totem->bvw, TRUE);
		update_mrl_label (totem, NULL);

		g_object_notify (G_OBJECT (totem), "playing");
	} else {
		gboolean caps;
		char *user_agent;
		char *autoload_sub;

		bacon_video_widget_set_logo_mode (totem->bvw, FALSE);

		autoload_sub = NULL;
		if (subtitle == NULL)
			g_signal_emit (G_OBJECT (totem), totem_table_signals[GET_TEXT_SUBTITLE], 0, mrl, &autoload_sub);

		user_agent = NULL;
		g_signal_emit (G_OBJECT (totem), totem_table_signals[GET_USER_AGENT], 0, mrl, &user_agent);
		bacon_video_widget_set_user_agent (totem->bvw, user_agent);
		g_free (user_agent);

		g_application_mark_busy (G_APPLICATION (totem));
		bacon_video_widget_open (totem->bvw, mrl);
		if (subtitle) {
			bacon_video_widget_set_text_subtitle (totem->bvw, subtitle);
		} else if (autoload_sub) {
			bacon_video_widget_set_text_subtitle (totem->bvw, autoload_sub);
			g_free (autoload_sub);
		} else {
			totem_playlist_set_current_subtitle (totem->playlist, totem->next_subtitle);
			totem_object_set_next_subtitle (totem, NULL);
		}
		g_application_unmark_busy (G_APPLICATION (totem));
		totem->mrl = g_strdup (mrl);

		/* Play/Pause */
		totem_object_set_sensitivity2 ("play", TRUE);

		/* Volume */
		caps = bacon_video_widget_can_set_volume (totem->bvw);
		totem_controls_set_sensitivity ("volume_button", caps);
		totem->volume_sensitive = caps;

		/* Subtitle selection */
		totem_object_set_sensitivity2 ("select-subtitle", !totem_is_special_mrl (mrl));

		/* Set the playlist */
		play_pause_set_label (totem, STATE_PAUSED);

		emit_file_opened (totem, totem->mrl);

		totem_object_set_main_page (totem, "player");
	}

	g_object_notify (G_OBJECT (totem), "current-mrl");

	update_buttons (totem);
	update_media_menu_items (totem);
}

static gboolean
totem_time_within_seconds (TotemObject *totem)
{
	gint64 _time;

	_time = bacon_video_widget_get_current_time (totem->bvw);

	return (_time < REWIND_OR_PREVIOUS);
}

#define totem_has_direction_track(totem, dir) (dir == TOTEM_PLAYLIST_DIRECTION_NEXT ? bacon_video_widget_has_next_track (totem->bvw) : bacon_video_widget_has_previous_track (totem->bvw))

static void
totem_object_direction (TotemObject *totem, TotemPlaylistDirection dir)
{
	if (totem_has_direction_track (totem, dir) == FALSE &&
	    totem_playlist_has_direction (totem->playlist, dir) == FALSE &&
	    totem_playlist_get_repeat (totem->playlist) == FALSE)
		return;

	if (totem_has_direction_track (totem, dir) != FALSE) {
		BvwDVDEvent event;
		event = (dir == TOTEM_PLAYLIST_DIRECTION_NEXT ? BVW_DVD_NEXT_CHAPTER : BVW_DVD_PREV_CHAPTER);
		bacon_video_widget_dvd_event (totem->bvw, event);
		return;
	}

	if (dir == TOTEM_PLAYLIST_DIRECTION_NEXT ||
	    bacon_video_widget_is_seekable (totem->bvw) == FALSE ||
	    totem_time_within_seconds (totem) != FALSE) {
		char *mrl, *subtitle;

		totem_playlist_set_direction (totem->playlist, dir);
		mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);
		totem_object_set_mrl_and_play (totem, mrl, subtitle);

		g_free (subtitle);
		g_free (mrl);
	} else {
		totem_object_seek (totem, 0);
	}
}

/**
 * totem_object_can_seek_previous:
 * @totem: a #TotemObject
 *
 * Returns true if totem_object_seek_previous() would have an effect.
 */
gboolean
totem_object_can_seek_previous (TotemObject *totem)
{
	return bacon_video_widget_has_previous_track (totem->bvw) ||
		totem_playlist_has_previous_mrl (totem->playlist) ||
		totem_playlist_get_repeat (totem->playlist);
}

/**
 * totem_object_seek_previous:
 * @totem: a #TotemObject
 *
 * If a DVD is being played, goes to the previous chapter. If a normal stream
 * is being played, goes to the start of the stream if possible. If seeking is
 * not possible, plays the previous entry in the playlist.
 **/
void
totem_object_seek_previous (TotemObject *totem)
{
	totem_object_direction (totem, TOTEM_PLAYLIST_DIRECTION_PREVIOUS);
}

/**
 * totem_object_can_seek_next:
 * @totem: a #TotemObject
 *
 * Returns true if totem_object_seek_next() would have an effect.
 */
gboolean
totem_object_can_seek_next (TotemObject *totem)
{
	return bacon_video_widget_has_next_track (totem->bvw) ||
		totem_playlist_has_next_mrl (totem->playlist) ||
		totem_playlist_get_repeat (totem->playlist);
}

/**
 * totem_object_seek_next:
 * @totem: a #TotemObject
 *
 * If a DVD is being played, goes to the next chapter. If a normal stream
 * is being played, plays the next entry in the playlist.
 **/
void
totem_object_seek_next (TotemObject *totem)
{
	totem_object_direction (totem, TOTEM_PLAYLIST_DIRECTION_NEXT);
}

static void
totem_seek_time_rel (TotemObject *totem, gint64 _time, gboolean relative, gboolean accurate)
{
	GError *err = NULL;
	gint64 sec;

	if (totem->mrl == NULL)
		return;
	if (bacon_video_widget_is_seekable (totem->bvw) == FALSE)
		return;

	if (relative != FALSE) {
		gint64 oldmsec;
		oldmsec = bacon_video_widget_get_current_time (totem->bvw);
		sec = MAX (0, oldmsec + _time);
	} else {
		sec = _time;
	}

	bacon_video_widget_seek_time (totem->bvw, sec, accurate, &err);

	if (err != NULL)
	{
		char *msg, *disp;

		disp = totem_uri_escape_for_display (totem->mrl);
		msg = g_strdup_printf(_("Totem could not play “%s”."), disp);
		g_free (disp);

		bacon_video_widget_stop (totem->bvw);
		play_pause_set_label (totem, STATE_STOPPED);
		totem_object_show_error (totem, msg, err->message);
		g_free (msg);
		g_error_free (err);
	}
}

/**
 * totem_object_seek_relative:
 * @totem: a #TotemObject
 * @offset: the time offset to seek to
 * @accurate: whether to use accurate seek, an accurate seek might be slower for some formats (see GStreamer docs)
 *
 * Seeks to an @offset from the current position in the stream,
 * or displays an error dialog if that's not possible.
 **/
void
totem_object_seek_relative (TotemObject *totem, gint64 offset, gboolean accurate)
{
	totem_seek_time_rel (totem, offset, TRUE, accurate);
}

/**
 * totem_object_seek_time:
 * @totem: a #TotemObject
 * @msec: the time to seek to
 * @accurate: whether to use accurate seek, an accurate seek might be slower for some formats (see GStreamer docs)
 *
 * Seeks to an absolute time in the stream, or displays an
 * error dialog if that's not possible.
 **/
void
totem_object_seek_time (TotemObject *totem, gint64 msec, gboolean accurate)
{
	totem_seek_time_rel (totem, msec, FALSE, accurate);
}

void
totem_object_set_zoom (TotemObject *totem,
		       gboolean     zoom)
{
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (totem), "zoom");
	g_action_change_state (action, g_variant_new_boolean (zoom));
}

/**
 * totem_object_get_volume:
 * @totem: a #TotemObject
 *
 * Gets the current volume level, as a value between <code class="literal">0.0</code> and <code class="literal">1.0</code>.
 *
 * Return value: the volume level
 **/
double
totem_object_get_volume (TotemObject *totem)
{
	return bacon_video_widget_get_volume (totem->bvw);
}

/**
 * totem_object_set_volume:
 * @totem: a #TotemObject
 * @volume: the new absolute volume value
 *
 * Sets the volume, with <code class="literal">1.0</code> being the maximum, and <code class="literal">0.0</code> being the minimum level.
 **/
void
totem_object_set_volume (TotemObject *totem, double volume)
{
	if (bacon_video_widget_can_set_volume (totem->bvw) == FALSE)
		return;

	bacon_video_widget_set_volume (totem->bvw, volume);
}

/**
 * totem_object_get_rate:
 * @totem: a #TotemObject
 *
 * Gets the current playback rate, with `1.0` being the normal playback rate.
 *
 * Return value: the volume level
 **/
float
totem_object_get_rate (TotemObject *totem)
{
	return bacon_video_widget_get_rate (totem->bvw);
}

/**
 * totem_object_set_rate:
 * @totem: a #TotemObject
 * @rate: the new absolute playback rate
 *
 * Sets the playback rate, with `1.0` being the normal playback rate.
 *
 * Return value: %TRUE on success, %FALSE on failure.
 **/
gboolean
totem_object_set_rate (TotemObject *totem, float rate)
{
	return bacon_video_widget_set_rate (totem->bvw, rate);
}

/**
 * totem_object_set_volume_relative:
 * @totem: a #TotemObject
 * @off_pct: the value by which to increase or decrease the volume
 *
 * Sets the volume relative to its current level, with <code class="literal">1.0</code> being the
 * maximum, and <code class="literal">0.0</code> being the minimum level.
 **/
void
totem_object_set_volume_relative (TotemObject *totem, double off_pct)
{
	double vol;

	if (bacon_video_widget_can_set_volume (totem->bvw) == FALSE)
		return;
	if (totem->muted != FALSE)
		totem_object_volume_toggle_mute (totem);

	vol = bacon_video_widget_get_volume (totem->bvw);
	bacon_video_widget_set_volume (totem->bvw, vol + off_pct);
}

/**
 * totem_object_volume_toggle_mute:
 * @totem: a #TotemObject
 *
 * Toggles the mute status.
 **/
void
totem_object_volume_toggle_mute (TotemObject *totem)
{
	if (totem->muted == FALSE) {
		totem->muted = TRUE;
		totem->prev_volume = bacon_video_widget_get_volume (totem->bvw);
		bacon_video_widget_set_volume (totem->bvw, 0.0);
	} else {
		totem->muted = FALSE;
		bacon_video_widget_set_volume (totem->bvw, totem->prev_volume);
	}
}

static void
totem_object_toggle_aspect_ratio (TotemObject *totem)
{
	GAction *action;
	int tmp;

	tmp = bacon_video_widget_get_aspect_ratio (totem->bvw);
	tmp++;
	if (tmp > BVW_RATIO_DVB)
		tmp = BVW_RATIO_AUTO;

	action = g_action_map_lookup_action (G_ACTION_MAP (totem), "aspect-ratio");
	g_action_change_state (action, g_variant_new ("i", tmp));
}

void
totem_object_show_help (TotemObject *totem)
{
	GError *error = NULL;

	if (gtk_show_uri (gtk_widget_get_screen (totem->win), "help:totem", gtk_get_current_event_time (), &error) == FALSE) {
		totem_object_show_error (totem, _("Totem could not display the help contents."), error->message);
		g_error_free (error);
	}
}

void
totem_object_show_keyboard_shortcuts (TotemObject *totem)
{
	GtkBuilder *builder;

	if (totem->shortcuts_win) {
		gtk_window_present (totem->shortcuts_win);
		return;
	}

	builder = totem_interface_load ("shortcuts.ui", FALSE, NULL, NULL);
	totem->shortcuts_win = GTK_WINDOW (gtk_builder_get_object (builder, "shortcuts-totem"));
	gtk_window_set_transient_for (totem->shortcuts_win, GTK_WINDOW (totem->win));

	g_signal_connect (totem->shortcuts_win, "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &totem->shortcuts_win);

	gtk_widget_show (GTK_WIDGET (totem->shortcuts_win));
	g_object_unref (builder);
}

/* This is called in the main thread */
static void
totem_object_drop_files_finished (TotemPlaylist *playlist, GAsyncResult *result, TotemObject *totem)
{
	char *mrl, *subtitle;

	/* Reconnect the playlist's changed signal (which was disconnected below in totem_object_drop_files(). */
	g_signal_connect (G_OBJECT (playlist), "changed", G_CALLBACK (playlist_changed_cb), totem);
	mrl = totem_playlist_get_current_mrl (playlist, &subtitle);
	totem_object_set_mrl_and_play (totem, mrl, subtitle);
	g_free (mrl);
	g_free (subtitle);

	g_object_unref (totem);
}

static gboolean
totem_object_drop_files (TotemObject      *totem,
			 GtkSelectionData *data,
			 int               drop_type)
{
	char **list;
	guint i, len;
	GList *p, *file_list, *mrl_list = NULL;

	list = g_uri_list_extract_uris ((const char *) gtk_selection_data_get_data (data));
	file_list = NULL;

	for (i = 0; list[i] != NULL; i++) {
		char *filename;

		if (list[i] == NULL)
			continue;

		filename = totem_create_full_path (list[i]);
		file_list = g_list_prepend (file_list,
					    filename ? filename : g_strdup (list[i]));
	}
	g_strfreev (list);

	if (file_list == NULL)
		return FALSE;

	if (drop_type != 1)
		file_list = g_list_sort (file_list, (GCompareFunc) strcmp);
	else
		file_list = g_list_reverse (file_list);

	/* How many files? Check whether those could be subtitles */
	len = g_list_length (file_list);
	if (len == 1 || (len == 2 && drop_type == 1)) {
		if (totem_uri_is_subtitle (file_list->data) != FALSE) {
			totem_playlist_set_current_subtitle (totem->playlist, file_list->data);
			goto bail;
		}
	}

	/* The function that calls us knows better if we should be doing something with the changed playlist... */
	g_signal_handlers_disconnect_by_func (G_OBJECT (totem->playlist), playlist_changed_cb, totem);
	totem_playlist_clear (totem->playlist);

	/* Add each MRL to the playlist asynchronously */
	for (p = file_list; p != NULL; p = p->next) {
		const char *filename, *title;

		filename = p->data;
		title = NULL;

		/* Super _NETSCAPE_URL trick */
		if (drop_type == 1) {
			p = p->next;
			if (p != NULL) {
				if (g_str_has_prefix (p->data, "File:") != FALSE)
					title = (char *)p->data + 5;
				else
					title = p->data;
			}
		}

		/* Add the MRL data to the list of MRLs to add to the playlist */
		mrl_list = g_list_prepend (mrl_list, totem_playlist_mrl_data_new (filename, title));
	}

	/* Add the MRLs to the playlist asynchronously and in order. We need to reconnect playlist's "changed" signal once all of the add-MRL
	 * operations have completed. */
	if (mrl_list != NULL) {
		totem_playlist_add_mrls (totem->playlist, g_list_reverse (mrl_list), TRUE, NULL,
		                         (GAsyncReadyCallback) totem_object_drop_files_finished, g_object_ref (totem));
	}

bail:
	g_list_free_full (file_list, g_free);

	return TRUE;
}

static void
drop_video_cb (GtkWidget          *widget,
	       GdkDragContext     *context,
	       gint                x,
	       gint                y,
	       GtkSelectionData   *data,
	       guint               info,
	       guint               _time,
	       Totem              *totem)
{
	GtkWidget *source_widget;
	GdkDragAction action = gdk_drag_context_get_selected_action (context);

	source_widget = gtk_drag_get_source_widget (context);

	/* Drop of video on itself */
	if (source_widget && widget == source_widget && action == GDK_ACTION_MOVE) {
		gtk_drag_finish (context, FALSE, FALSE, _time);
		return;
	}

	totem_object_drop_files (totem, data, info);
	gtk_drag_finish (context, TRUE, FALSE, _time);
	return;
}

static void
back_button_clicked_cb (GtkButton   *button,
			TotemObject *totem)
{
	if (g_strcmp0 (totem_object_get_main_page (totem), "player") == 0) {
		totem_playlist_clear (totem->playlist);
		gtk_window_unfullscreen (GTK_WINDOW (totem->win));
		totem_object_set_main_page (totem, "grilo");
	} else {
		totem_grilo_back_button_clicked (TOTEM_GRILO (totem->grilo));
	}
}

static void
on_got_redirect (BaconVideoWidget *bvw, const char *mrl, TotemObject *totem)
{
	char *new_mrl;

	if (strstr (mrl, "://") != NULL) {
		new_mrl = NULL;
	} else {
		GFile *old_file, *parent, *new_file;
		char *old_mrl;

		/* Get the parent for the current MRL, that's our base */
		old_mrl = totem_playlist_get_current_mrl (TOTEM_PLAYLIST (totem->playlist), NULL);
		old_file = g_file_new_for_uri (old_mrl);
		g_free (old_mrl);
		parent = g_file_get_parent (old_file);
		g_object_unref (old_file);

		/* Resolve the URL */
		new_file = g_file_get_child (parent, mrl);
		g_object_unref (parent);

		new_mrl = g_file_get_uri (new_file);
		g_object_unref (new_file);
	}

	bacon_video_widget_close (totem->bvw);
	emit_file_closed (totem);
	totem->has_played_emitted = FALSE;
	g_application_mark_busy (G_APPLICATION (totem));
	bacon_video_widget_open (totem->bvw, new_mrl ? new_mrl : mrl);
	emit_file_opened (totem, new_mrl ? new_mrl : mrl);
	g_application_unmark_busy (G_APPLICATION (totem));
	if (bacon_video_widget_play (bvw, NULL) != FALSE) {
		totem_file_has_played (totem, totem->mrl);
		totem->has_played_emitted = TRUE;
	}
	g_free (new_mrl);
}

static void
on_channels_change_event (BaconVideoWidget *bvw, TotemObject *totem)
{
	gchar *name;

	totem_sublang_update (totem);
	update_media_menu_items (totem);

	/* updated stream info (new song) */
	name = totem_get_nice_name_for_stream (totem);

	if (name != NULL) {
		update_mrl_label (totem, name);
		totem_playlist_set_title
			(TOTEM_PLAYLIST (totem->playlist), name);
		g_free (name);
	}
}

static void
on_playlist_change_name (TotemPlaylist *playlist, TotemObject *totem)
{
	char *name;

	name = totem_playlist_get_current_title (playlist);
	if (name != NULL) {
		update_mrl_label (totem, name);
		g_free (name);
	}
}

static void
on_got_metadata_event (BaconVideoWidget *bvw, TotemObject *totem)
{
        char *name;

	name = totem_get_nice_name_for_stream (totem);

	if (name != NULL) {
		totem_playlist_set_title
			(TOTEM_PLAYLIST (totem->playlist), name);
		g_free (name);
	}

	totem_sublang_update (totem);
	update_buttons (totem);
	on_playlist_change_name (TOTEM_PLAYLIST (totem->playlist), totem);
}

static void
on_error_event (BaconVideoWidget *bvw, char *message,
                gboolean playback_stopped, TotemObject *totem)
{
	/* Clear the seek if it's there, we only want to try and seek
	 * the first file, even if it's not there */
	totem_playlist_steal_current_starttime (totem->playlist);
	totem->pause_start = FALSE;

	if (playback_stopped)
		play_pause_set_label (totem, STATE_STOPPED);

	totem_object_show_error (totem, _("An error occurred"), message);
}

static void
on_buffering_event (BaconVideoWidget *bvw, gdouble percentage, TotemObject *totem)
{
	//FIXME show that somehow
}

static void
on_download_buffering_event (BaconVideoWidget *bvw, gdouble level, TotemObject *totem)
{
	update_fill (totem, level);
}

static void
update_fill (TotemObject *totem, gdouble level)
{
	if (level < 0.0) {
		gtk_range_set_show_fill_level (GTK_RANGE (totem->seek), FALSE);
	} else {
		gtk_range_set_fill_level (GTK_RANGE (totem->seek), level * 65535.0f);
		gtk_range_set_show_fill_level (GTK_RANGE (totem->seek), TRUE);
	}
}

static void
update_seekable (TotemObject *totem)
{
	gboolean seekable;
	gboolean notify;

	seekable = bacon_video_widget_is_seekable (totem->bvw);
	notify = (totem->seekable == seekable);
	totem->seekable = seekable;

	/* Check if the stream is seekable */
	gtk_widget_set_sensitive (totem->seek, seekable);

	if (seekable != FALSE) {
		gint64 starttime;

		starttime = totem_playlist_steal_current_starttime (totem->playlist);
		if (starttime != 0) {
			bacon_video_widget_seek_time (totem->bvw,
						      starttime * 1000, FALSE, NULL);
			if (totem->pause_start) {
				totem_object_pause (totem);
				totem->pause_start = FALSE;
			}
		}
	}

	if (notify)
		g_object_notify (G_OBJECT (totem), "seekable");
}

static void
update_slider_visibility (TotemObject *totem,
			  gint64 stream_length)
{
	if (totem->stream_length == stream_length)
		return;
	if (totem->stream_length > 0 && stream_length > 0)
		return;
	if (stream_length != 0)
		gtk_range_set_range (GTK_RANGE (totem->seek), 0., 65535.);
	else
		gtk_range_set_range (GTK_RANGE (totem->seek), 0., 0.);
}

static void
update_current_time (BaconVideoWidget *bvw,
		     gint64            current_time,
		     gint64            stream_length,
		     double            current_position,
		     gboolean          seekable,
		     TotemObject      *totem)
{
	update_slider_visibility (totem, stream_length);

	if (totem->seek_lock == FALSE) {
		gtk_adjustment_set_value (totem->seekadj,
					  current_position * 65535);

		if (stream_length == 0 && totem->mrl != NULL) {
			bacon_time_label_set_time (totem->time_label,
						   current_time, -1);
			bacon_time_label_set_time (totem->time_rem_label,
						   current_time, -1);
		} else {
			bacon_time_label_set_time (totem->time_label,
						   current_time,
						   stream_length);
			bacon_time_label_set_time (totem->time_rem_label,
						   current_time,
						   stream_length);
		}
	}

	if (totem->stream_length != stream_length) {
		g_object_notify (G_OBJECT (totem), "stream-length");
		totem->stream_length = stream_length;
	}
}

static void
volume_button_value_changed_cb (GtkScaleButton *button, gdouble value, TotemObject *totem)
{
	totem->muted = FALSE;
	bacon_video_widget_set_volume (totem->bvw, value);
}

static void
update_volume_sliders (TotemObject *totem)
{
	double volume;

	volume = bacon_video_widget_get_volume (totem->bvw);

	g_signal_handlers_block_by_func (totem->volume, volume_button_value_changed_cb, totem);
	gtk_scale_button_set_value (GTK_SCALE_BUTTON (totem->volume), volume);
	g_signal_handlers_unblock_by_func (totem->volume, volume_button_value_changed_cb, totem);
}

static void
property_notify_cb_volume (BaconVideoWidget *bvw, GParamSpec *spec, TotemObject *totem)
{
	update_volume_sliders (totem);
}

static void
property_notify_cb_seekable (BaconVideoWidget *bvw, GParamSpec *spec, TotemObject *totem)
{
	update_seekable (totem);
}

gboolean
seek_slider_pressed_cb (GtkWidget *widget, GdkEventButton *event, TotemObject *totem)
{
	/* HACK: we want the behaviour you get with the left button, so we
	 * mangle the event.  clicking with other buttons moves the slider in
	 * step increments, clicking with the left button moves the slider to
	 * the location of the click.
	 */
	event->button = GDK_BUTTON_PRIMARY;

	totem->seek_lock = TRUE;
	bacon_video_widget_mark_popup_busy (totem->bvw, "seek started");

	return FALSE;
}

void
seek_slider_changed_cb (GtkAdjustment *adj, TotemObject *totem)
{
	double pos;
	gint _time;

	if (totem->seek_lock == FALSE)
		return;

	pos = gtk_adjustment_get_value (adj) / 65535;
	_time = bacon_video_widget_get_stream_length (totem->bvw);

	bacon_time_label_set_time (totem->time_label,
				   pos * _time, _time);
	bacon_time_label_set_time (totem->time_rem_label,
				   pos * _time, _time);

	if (bacon_video_widget_can_direct_seek (totem->bvw) != FALSE)
		totem_object_seek (totem, pos);
}

gboolean
seek_slider_released_cb (GtkWidget *widget, GdkEventButton *event, TotemObject *totem)
{
	GtkAdjustment *adj;
	gdouble val;

	/* HACK: see seek_slider_pressed_cb */
	event->button = GDK_BUTTON_PRIMARY;

	/* set to FALSE here to avoid triggering a final seek when
	 * syncing the adjustments while being in direct seek mode */
	totem->seek_lock = FALSE;
	bacon_video_widget_unmark_popup_busy (totem->bvw, "seek started");

	/* sync both adjustments */
	adj = gtk_range_get_adjustment (GTK_RANGE (widget));
	val = gtk_adjustment_get_value (adj);

	if (bacon_video_widget_can_direct_seek (totem->bvw) == FALSE)
		totem_object_seek (totem, val / 65535.0);

	return FALSE;
}

gboolean
totem_object_open_files (TotemObject *totem, char **list)
{
	GSList *slist = NULL;
	int i, retval;

	for (i = 0 ; list[i] != NULL; i++)
		slist = g_slist_prepend (slist, list[i]);

	slist = g_slist_reverse (slist);
	retval = totem_object_open_files_list (totem, slist);
	g_slist_free (slist);

	return retval;
}

static gboolean
totem_object_open_files_list (TotemObject *totem, GSList *list)
{
	GSList *l;
	GList *mrl_list = NULL;
	gboolean changed;
	gboolean cleared;

	changed = FALSE;
	cleared = FALSE;

	if (list == NULL)
		return changed;

	g_application_mark_busy (G_APPLICATION (totem));

	for (l = list ; l != NULL; l = l->next)
	{
		char *filename;
		char *data = l->data;

		if (data == NULL)
			continue;

		/* Ignore relatives paths that start with "--", tough luck */
		if (data[0] == '-' && data[1] == '-')
			continue;

		/* Get the subtitle part out for our tests */
		filename = totem_create_full_path (data);
		if (filename == NULL)
			filename = g_strdup (data);

		if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)
				|| strstr (filename, "#") != NULL
				|| strstr (filename, "://") != NULL
				|| g_str_has_prefix (filename, "dvd:") != FALSE
				|| g_str_has_prefix (filename, "vcd:") != FALSE
				|| g_str_has_prefix (filename, "dvb:") != FALSE)
		{
			if (cleared == FALSE)
			{
				/* The function that calls us knows better
				 * if we should be doing something with the 
				 * changed playlist ... */
				g_signal_handlers_disconnect_by_func
					(G_OBJECT (totem->playlist),
					 playlist_changed_cb, totem);
				changed = totem_playlist_clear (totem->playlist);
				bacon_video_widget_close (totem->bvw);
				emit_file_closed (totem);
				totem->has_played_emitted = FALSE;
				cleared = TRUE;
			}

			if (g_str_has_prefix (filename, "dvb:/") != FALSE) {
				mrl_list = g_list_prepend (mrl_list, totem_playlist_mrl_data_new (data, NULL));
				changed = TRUE;
			} else {
				mrl_list = g_list_prepend (mrl_list, totem_playlist_mrl_data_new (filename, NULL));
				changed = TRUE;
			}
		}

		g_free (filename);
	}

	/* Add the MRLs to the playlist asynchronously and in order */
	if (mrl_list != NULL)
		totem_playlist_add_mrls (totem->playlist, g_list_reverse (mrl_list), FALSE, NULL, NULL, NULL);

	g_application_unmark_busy (G_APPLICATION (totem));

	/* ... and reconnect because we're nice people */
	if (cleared != FALSE)
	{
		g_signal_connect (G_OBJECT (totem->playlist),
				"changed", G_CALLBACK (playlist_changed_cb),
				totem);
	}

	return changed;
}

void
show_controls (TotemObject *totem, gboolean was_fullscreen)
{
	GtkWidget *bvw_box;

	if (totem->bvw == NULL)
		return;

	bvw_box = GTK_WIDGET (gtk_builder_get_object (totem->xml, "tmw_bvw_box"));

	if (totem->controls_visibility == TOTEM_CONTROLS_VISIBLE) {
		totem_object_save_size (totem);
	} else {
		 /* We won't show controls in fullscreen */
		gtk_container_set_border_width (GTK_CONTAINER (bvw_box), 0);
	}
}

/**
 * totem_object_next_angle:
 * @totem: a #TotemObject
 *
 * Switches to the next angle, if watching a DVD. If not watching a DVD, this is a
 * no-op.
 **/
void
totem_object_next_angle (TotemObject *totem)
{
	bacon_video_widget_set_next_angle (totem->bvw);
}

/**
 * totem_object_remote_command:
 * @totem: a #TotemObject
 * @cmd: a #TotemRemoteCommand
 * @url: an MRL to play, or %NULL
 *
 * Executes the specified @cmd on this instance of Totem. If @cmd
 * is an operation requiring an MRL, @url is required; it can be %NULL
 * otherwise.
 *
 * If Totem's fullscreened and the operation is executed correctly,
 * the controls will appear as if the user had moved the mouse.
 **/
void
totem_object_remote_command (TotemObject *totem, TotemRemoteCommand cmd, const char *url)
{
	switch (cmd) {
	case TOTEM_REMOTE_COMMAND_PLAY:
		totem_object_play (totem);
		break;
	case TOTEM_REMOTE_COMMAND_PLAYPAUSE:
		totem_object_play_pause (totem);
		break;
	case TOTEM_REMOTE_COMMAND_PAUSE:
		totem_object_pause (totem);
		break;
	case TOTEM_REMOTE_COMMAND_STOP:
		totem_object_stop (totem);
		break;
	case TOTEM_REMOTE_COMMAND_SEEK_FORWARD: {
		double offset = 0;

		if (url != NULL)
			offset = g_ascii_strtod (url, NULL);
		if (offset == 0) {
			totem_object_seek_relative (totem, SEEK_FORWARD_OFFSET * 1000, FALSE);
		} else {
			totem_object_seek_relative (totem, offset * 1000, FALSE);
		}
		break;
	}
	case TOTEM_REMOTE_COMMAND_SEEK_BACKWARD: {
		double offset = 0;

		if (url != NULL)
			offset = g_ascii_strtod (url, NULL);
		if (offset == 0)
			totem_object_seek_relative (totem, SEEK_BACKWARD_OFFSET * 1000, FALSE);
		else
			totem_object_seek_relative (totem,  - (offset * 1000), FALSE);
		break;
	}
	case TOTEM_REMOTE_COMMAND_VOLUME_UP:
		totem_object_set_volume_relative (totem, VOLUME_UP_OFFSET);
		break;
	case TOTEM_REMOTE_COMMAND_VOLUME_DOWN:
		totem_object_set_volume_relative (totem, VOLUME_DOWN_OFFSET);
		break;
	case TOTEM_REMOTE_COMMAND_NEXT:
		totem_object_seek_next (totem);
		break;
	case TOTEM_REMOTE_COMMAND_PREVIOUS:
		totem_object_seek_previous (totem);
		break;
	case TOTEM_REMOTE_COMMAND_FULLSCREEN:
		if (g_strcmp0 (totem_object_get_main_page (totem), "player") == 0)
			totem_object_action_fullscreen_toggle (totem);
		break;
	case TOTEM_REMOTE_COMMAND_QUIT:
		totem_object_exit (totem);
		break;
	case TOTEM_REMOTE_COMMAND_ENQUEUE:
		g_assert (url != NULL);
		if (!totem_uri_is_subtitle (url))
			totem_playlist_add_mrl (totem->playlist, url, NULL, TRUE, NULL, NULL, NULL);
		else
			totem_object_set_next_subtitle (totem, url);
		break;
	case TOTEM_REMOTE_COMMAND_REPLACE:
		if (url == NULL ||
		    !totem_uri_is_subtitle (url)) {
			totem_playlist_clear (totem->playlist);
			if (url == NULL) {
				bacon_video_widget_close (totem->bvw);
				emit_file_closed (totem);
				totem->has_played_emitted = FALSE;
				totem_object_set_mrl (totem, NULL, NULL);
				break;
			}
			totem_playlist_add_mrl (totem->playlist, url, NULL, TRUE, NULL, NULL, NULL);
		} else if (totem->mrl != NULL) {
			totem_playlist_set_current_subtitle (totem->playlist, url);
		} else {
			totem_object_set_next_subtitle (totem, url);
		}
		break;
	case TOTEM_REMOTE_COMMAND_SHOW:
		gtk_window_present_with_time (GTK_WINDOW (totem->win), GDK_CURRENT_TIME);
		break;
	case TOTEM_REMOTE_COMMAND_UP:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_UP);
		break;
	case TOTEM_REMOTE_COMMAND_DOWN:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_DOWN);
		break;
	case TOTEM_REMOTE_COMMAND_LEFT:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_LEFT);
		break;
	case TOTEM_REMOTE_COMMAND_RIGHT:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_RIGHT);
		break;
	case TOTEM_REMOTE_COMMAND_SELECT:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU_SELECT);
		break;
	case TOTEM_REMOTE_COMMAND_DVD_MENU:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_ROOT_MENU);
		break;
	case TOTEM_REMOTE_COMMAND_ZOOM_UP:
		totem_object_set_zoom (totem, TRUE);
		break;
	case TOTEM_REMOTE_COMMAND_ZOOM_DOWN:
		totem_object_set_zoom (totem, FALSE);
		break;
	case TOTEM_REMOTE_COMMAND_EJECT:
		totem_object_eject (totem);
		break;
	case TOTEM_REMOTE_COMMAND_PLAY_DVD:
		if (g_strcmp0 (totem_object_get_main_page (totem), "player") == 0)
			back_button_clicked_cb (NULL, totem);
		totem_grilo_set_current_page (TOTEM_GRILO (totem->grilo), TOTEM_GRILO_PAGE_RECENT);
		break;
	case TOTEM_REMOTE_COMMAND_MUTE:
		totem_object_volume_toggle_mute (totem);
		break;
	case TOTEM_REMOTE_COMMAND_TOGGLE_ASPECT:
		totem_object_toggle_aspect_ratio (totem);
		break;
	case TOTEM_REMOTE_COMMAND_UNKNOWN:
	default:
		break;
	}
}

/**
 * totem_object_remote_set_setting:
 * @totem: a #TotemObject
 * @setting: a #TotemRemoteSetting
 * @value: the new value for the setting
 *
 * Sets @setting to @value on this instance of Totem.
 **/
void totem_object_remote_set_setting (TotemObject *totem,
				      TotemRemoteSetting setting,
				      gboolean value)
{
	GAction *action;

	switch (setting) {
	case TOTEM_REMOTE_SETTING_REPEAT:
		action = g_action_map_lookup_action (G_ACTION_MAP (totem), "repeat");
		break;
	default:
		g_assert_not_reached ();
	}

	g_simple_action_set_state (G_SIMPLE_ACTION (action),
				   g_variant_new_boolean (value));
}

/**
 * totem_object_remote_get_setting:
 * @totem: a #TotemObject
 * @setting: a #TotemRemoteSetting
 *
 * Returns the value of @setting for this instance of Totem.
 *
 * Return value: %TRUE if the setting is enabled, %FALSE otherwise
 **/
gboolean
totem_object_remote_get_setting (TotemObject        *totem,
				 TotemRemoteSetting  setting)
{
	GAction *action;
	GVariant *v;
	gboolean ret;

	action = NULL;

	switch (setting) {
	case TOTEM_REMOTE_SETTING_REPEAT:
		action = g_action_map_lookup_action (G_ACTION_MAP (totem), "repeat");
		break;
	default:
		g_assert_not_reached ();
	}

	v = g_action_get_state (action);
	ret = g_variant_get_boolean (v);
	g_variant_unref (v);

	return ret;
}

static void
playlist_changed_cb (GtkWidget *playlist, TotemObject *totem)
{
	char *mrl, *subtitle;

	update_buttons (totem);
	mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);

	if (mrl == NULL)
		return;

	if (totem_playlist_get_playing (totem->playlist) == TOTEM_PLAYLIST_STATUS_NONE) {
		if (totem->pause_start)
			totem_object_set_mrl (totem, mrl, subtitle);
		else
			totem_object_set_mrl_and_play (totem, mrl, subtitle);
	}

	totem->pause_start = FALSE;

	g_free (mrl);
	g_free (subtitle);
}

static void
item_activated_cb (GtkWidget *playlist, TotemObject *totem)
{
	totem_object_seek (totem, 0);
}

static void
current_removed_cb (GtkWidget *playlist, TotemObject *totem)
{
	char *mrl, *subtitle;

	/* Set play button status */
	play_pause_set_label (totem, STATE_STOPPED);
	mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);

	if (mrl == NULL) {
		g_free (subtitle);
		subtitle = NULL;
		totem_playlist_set_at_start (totem->playlist);
		update_buttons (totem);
		mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);
	} else {
		update_buttons (totem);
	}

	totem_object_set_mrl_and_play (totem, mrl, subtitle);
	g_free (mrl);
	g_free (subtitle);
}

static void
subtitle_changed_cb (GtkWidget *playlist, TotemObject *totem)
{
	char *mrl, *subtitle;

	mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);
	bacon_video_widget_set_text_subtitle (totem->bvw, subtitle);

	g_free (mrl);
	g_free (subtitle);
}

static void
playlist_repeat_toggle_cb (TotemPlaylist *playlist, GParamSpec *pspec, TotemObject *totem)
{
	GAction *action;
	gboolean repeat;

	repeat = totem_playlist_get_repeat (playlist);
	action = g_action_map_lookup_action (G_ACTION_MAP (totem), "repeat");
	g_simple_action_set_state (G_SIMPLE_ACTION (action),
				   g_variant_new_boolean (repeat));
}

/**
 * totem_object_is_fullscreen:
 * @totem: a #TotemObject
 *
 * Returns %TRUE if Totem is fullscreened.
 *
 * Return value: %TRUE if Totem is fullscreened
 **/
gboolean
totem_object_is_fullscreen (TotemObject *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), FALSE);

	return (totem->controls_visibility == TOTEM_CONTROLS_FULLSCREEN);
}

/**
 * totem_object_is_playing:
 * @totem: a #TotemObject
 *
 * Returns %TRUE if Totem is playing a stream.
 *
 * Return value: %TRUE if Totem is playing a stream
 **/
gboolean
totem_object_is_playing (TotemObject *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), FALSE);

	if (totem->bvw == NULL)
		return FALSE;

	return bacon_video_widget_is_playing (totem->bvw) != FALSE;
}

/**
 * totem_object_is_paused:
 * @totem: a #TotemObject
 *
 * Returns %TRUE if playback is paused.
 *
 * Return value: %TRUE if playback is paused, %FALSE otherwise
 **/
gboolean
totem_object_is_paused (TotemObject *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), FALSE);

	return totem->state == STATE_PAUSED;
}

/**
 * totem_object_is_seekable:
 * @totem: a #TotemObject
 *
 * Returns %TRUE if the current stream is seekable.
 *
 * Return value: %TRUE if the current stream is seekable
 **/
gboolean
totem_object_is_seekable (TotemObject *totem)
{
	g_return_val_if_fail (TOTEM_IS_OBJECT (totem), FALSE);

	if (totem->bvw == NULL)
		return FALSE;

	return bacon_video_widget_is_seekable (totem->bvw) != FALSE;
}

static gboolean
event_is_touch (GdkEventButton *event)
{
	GdkDevice *device;

	device = gdk_event_get_device ((GdkEvent *) event);
	return (gdk_device_get_source (device) == GDK_SOURCE_TOUCHSCREEN);
}

static gboolean
on_video_button_press_event (BaconVideoWidget *bvw, GdkEventButton *event,
		TotemObject *totem)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
		gtk_widget_grab_focus (GTK_WIDGET (bvw));
		return TRUE;
	} else if (event->type == GDK_2BUTTON_PRESS &&
		   event->button == 1 &&
		   event_is_touch (event) == FALSE) {
		totem_object_action_fullscreen_toggle (totem);
		return TRUE;
	} else if (event->type == GDK_BUTTON_PRESS && event->button == 2) {
		totem_object_play_pause (totem);
		return TRUE;
	}

	return FALSE;
}

static gboolean
on_eos_event (GtkWidget *widget, TotemObject *totem)
{
	reset_seek_status (totem);

	if (bacon_video_widget_get_logo_mode (totem->bvw) != FALSE)
		return FALSE;

	if (totem_playlist_has_next_mrl (totem->playlist) == FALSE &&
	    totem_playlist_get_repeat (totem->playlist) == FALSE &&
	    (totem_playlist_get_last (totem->playlist) != 0 ||
	     totem_object_is_seekable (totem) == FALSE)) {
		char *mrl, *subtitle;

		/* Set play button status */
		totem_playlist_set_at_start (totem->playlist);
		update_buttons (totem);
		bacon_video_widget_stop (totem->bvw);
		play_pause_set_label (totem, STATE_STOPPED);
		mrl = totem_playlist_get_current_mrl (totem->playlist, &subtitle);
		totem_object_set_mrl (totem, mrl, subtitle);
		bacon_video_widget_pause (totem->bvw);
		g_free (mrl);
		g_free (subtitle);
	} else {
		if (totem_playlist_get_last (totem->playlist) == 0 &&
		    totem_object_is_seekable (totem)) {
			if (totem_playlist_get_repeat (totem->playlist) != FALSE) {
				totem_object_seek_time (totem, 0, FALSE);
				totem_object_play (totem);
			} else {
				totem_object_pause (totem);
				totem_object_seek_time (totem, 0, FALSE);
			}
		} else {
			totem_object_seek_next (totem);
		}
	}

	return FALSE;
}

static void
totem_object_handle_seek (TotemObject *totem, GdkEventKey *event, gboolean is_forward)
{
	if (is_forward != FALSE) {
		if (event->state & GDK_SHIFT_MASK)
			totem_object_seek_relative (totem, SEEK_FORWARD_SHORT_OFFSET * 1000, FALSE);
		else if (event->state & GDK_CONTROL_MASK)
			totem_object_seek_relative (totem, SEEK_FORWARD_LONG_OFFSET * 1000, FALSE);
		else
			totem_object_seek_relative (totem, SEEK_FORWARD_OFFSET * 1000, FALSE);
	} else {
		if (event->state & GDK_SHIFT_MASK)
			totem_object_seek_relative (totem, SEEK_BACKWARD_SHORT_OFFSET * 1000, FALSE);
		else if (event->state & GDK_CONTROL_MASK)
			totem_object_seek_relative (totem, SEEK_BACKWARD_LONG_OFFSET * 1000, FALSE);
		else
			totem_object_seek_relative (totem, SEEK_BACKWARD_OFFSET * 1000, FALSE);
	}
}

static gboolean
totem_object_handle_key_press (TotemObject *totem, GdkEventKey *event)
{
	GdkModifierType mask;
	gboolean retval;
	gboolean switch_rtl = FALSE;

	retval = TRUE;

	mask = event->state & gtk_accelerator_get_default_mod_mask ();

	switch (event->keyval) {
	case GDK_KEY_A:
	case GDK_KEY_a:
		totem_object_toggle_aspect_ratio (totem);
		break;
	case GDK_KEY_AudioCycleTrack:
		bacon_video_widget_set_next_language (totem->bvw);
		break;
	case GDK_KEY_AudioPrev:
	case GDK_KEY_Back:
	case GDK_KEY_B:
	case GDK_KEY_b:
		totem_object_seek_previous (totem);
		bacon_video_widget_show_popup (totem->bvw);
		break;
	case GDK_KEY_C:
	case GDK_KEY_c:
		bacon_video_widget_dvd_event (totem->bvw,
				BVW_DVD_CHAPTER_MENU);
		break;
	case GDK_KEY_F5:
		/* Start presentation button */
		totem_object_set_fullscreen (totem, TRUE);
		totem_object_play_pause (totem);
		break;
	case GDK_KEY_F11:
	case GDK_KEY_f:
	case GDK_KEY_F:
		totem_object_action_fullscreen_toggle (totem);
		break;
	case GDK_KEY_CycleAngle:
	case GDK_KEY_g:
	case GDK_KEY_G:
		totem_object_next_angle (totem);
		break;
	case GDK_KEY_H:
	case GDK_KEY_h:
		totem_object_show_keyboard_shortcuts (totem);
		break;
	case GDK_KEY_question:
		totem_object_show_keyboard_shortcuts (totem);
		break;
	case GDK_KEY_M:
	case GDK_KEY_m:
		bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU);
		break;
	case GDK_KEY_AudioNext:
	case GDK_KEY_Forward:
	case GDK_KEY_N:
	case GDK_KEY_n:
	case GDK_KEY_End:
		totem_object_seek_next (totem);
		bacon_video_widget_show_popup (totem->bvw);
		break;
	case GDK_KEY_OpenURL:
		totem_object_set_fullscreen (totem, FALSE);
		totem_object_open_location (totem);
		break;
	case GDK_KEY_O:
	case GDK_KEY_o:
	case GDK_KEY_Open:
		totem_object_set_fullscreen (totem, FALSE);
		totem_object_open (totem);
		break;
	case GDK_KEY_AudioPlay:
	case GDK_KEY_p:
	case GDK_KEY_P:
		totem_object_play_pause (totem);
		break;
	case GDK_KEY_comma:
	case GDK_KEY_FrameBack:
		totem_object_pause (totem);
		bacon_video_widget_step (totem->bvw, FALSE, NULL);
		break;
	case GDK_KEY_period:
	case GDK_KEY_FrameForward:
		totem_object_pause (totem);
		bacon_video_widget_step (totem->bvw, TRUE, NULL);
		break;
	case GDK_KEY_AudioPause:
	case GDK_KEY_Pause:
	case GDK_KEY_AudioStop:
		totem_object_pause (totem);
		break;
	case GDK_KEY_q:
	case GDK_KEY_Q:
		totem_object_exit (totem);
		break;
	case GDK_KEY_r:
	case GDK_KEY_R:
	case GDK_KEY_ZoomIn:
		totem_object_set_zoom (totem, TRUE);
		break;
	case GDK_KEY_Subtitle:
		bacon_video_widget_set_next_subtitle (totem->bvw);
		break;
	case GDK_KEY_t:
	case GDK_KEY_T:
	case GDK_KEY_ZoomOut:
		totem_object_set_zoom (totem, FALSE);
		break;
	case GDK_KEY_Eject:
		totem_object_eject (totem);
		break;
	case GDK_KEY_Escape:
		if (mask == GDK_SUPER_MASK)
			bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU);
		else
			totem_object_set_fullscreen (totem, FALSE);
		break;
	case GDK_KEY_space:
	case GDK_KEY_Return:
		if (mask != GDK_CONTROL_MASK) {
			GtkWidget *focus = gtk_window_get_focus (GTK_WINDOW (totem->win));
			if (totem_object_is_fullscreen (totem) != FALSE || focus == NULL ||
			    focus == GTK_WIDGET (totem->bvw) || focus == totem->seek) {
				if (event->keyval == GDK_KEY_space) {
					totem_object_play_pause (totem);
				} else if (bacon_video_widget_has_menus (totem->bvw) != FALSE) {
					bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU_SELECT);
				}
			} else
				retval = FALSE;
		}
		break;
	case GDK_KEY_Left:
	case GDK_KEY_Right:
		switch_rtl = TRUE;
		/* fall through */
	case GDK_KEY_Page_Up:
	case GDK_KEY_Page_Down:
		if (bacon_video_widget_has_menus (totem->bvw) == FALSE) {
			gboolean is_forward;

			is_forward = (event->keyval == GDK_KEY_Right || event->keyval == GDK_KEY_Page_Up);
			/* Switch direction in RTL environment */
			if (switch_rtl && gtk_widget_get_direction (totem->win) == GTK_TEXT_DIR_RTL)
				is_forward = !is_forward;

			if (totem_object_is_seekable (totem)) {
				totem_object_handle_seek (totem, event, is_forward);
				bacon_video_widget_show_popup (totem->bvw);
			}
		} else {
			if (event->keyval == GDK_KEY_Left || event->keyval == GDK_KEY_Page_Down)
				bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU_LEFT);
			else
				bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU_RIGHT);
		}
		break;
	case GDK_KEY_Home:
		totem_object_seek (totem, 0);
		bacon_video_widget_show_popup (totem->bvw);
		break;
	case GDK_KEY_Up:
		if (bacon_video_widget_has_menus (totem->bvw) != FALSE)
			bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU_UP);
		else if (mask == GDK_SHIFT_MASK)
			totem_object_set_volume_relative (totem, VOLUME_UP_SHORT_OFFSET);
		else
			totem_object_set_volume_relative (totem, VOLUME_UP_OFFSET);
		break;
	case GDK_KEY_Down:
		if (bacon_video_widget_has_menus (totem->bvw) != FALSE)
			bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU_DOWN);
		else if (mask == GDK_SHIFT_MASK)
			totem_object_set_volume_relative (totem, VOLUME_DOWN_SHORT_OFFSET);
		else
			totem_object_set_volume_relative (totem, VOLUME_DOWN_OFFSET);
		break;
	case GDK_KEY_Select:
		if (bacon_video_widget_has_menus (totem->bvw) != FALSE)
			bacon_video_widget_dvd_event (totem->bvw, BVW_DVD_ROOT_MENU_SELECT);
		break;
	case GDK_KEY_0:
		if (mask == GDK_CONTROL_MASK)
			totem_object_set_zoom (totem, FALSE);
		break;
	case GDK_KEY_Menu:
	case GDK_KEY_F10:
		bacon_video_widget_show_popup (totem->bvw);
		if (totem->controls_visibility != TOTEM_CONTROLS_FULLSCREEN) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (totem->gear_button),
						      !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (totem->gear_button)));
		} else {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (totem->fullscreen_gear_button),
						      !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (totem->fullscreen_gear_button)));
		}
		break;
	case GDK_KEY_Time:
		bacon_video_widget_show_popup (totem->bvw);
		break;
	case GDK_KEY_equal:
		if (mask == GDK_CONTROL_MASK)
			totem_object_set_zoom (totem, TRUE);
		break;
	case GDK_KEY_hyphen:
		if (mask == GDK_CONTROL_MASK)
			totem_object_set_zoom (totem, FALSE);
		break;
	case GDK_KEY_plus:
	case GDK_KEY_KP_Add:
		if (mask != GDK_CONTROL_MASK) {
			totem_object_seek_next (totem);
			bacon_video_widget_show_popup (totem->bvw);
		} else {
			totem_object_set_zoom (totem, TRUE);
		}
		break;
	case GDK_KEY_minus:
	case GDK_KEY_KP_Subtract:
		if (mask != GDK_CONTROL_MASK) {
			totem_object_seek_previous (totem);
			bacon_video_widget_show_popup (totem->bvw);
		} else {
			totem_object_set_zoom (totem, FALSE);
		}
		break;
	case GDK_KEY_KP_Up:
	case GDK_KEY_KP_8:
		bacon_video_widget_dvd_event (totem->bvw, 
					      BVW_DVD_ROOT_MENU_UP);
		break;
	case GDK_KEY_KP_Down:
	case GDK_KEY_KP_2:
		bacon_video_widget_dvd_event (totem->bvw, 
					      BVW_DVD_ROOT_MENU_DOWN);
		break;
	case GDK_KEY_KP_Right:
	case GDK_KEY_KP_6:
		bacon_video_widget_dvd_event (totem->bvw, 
					      BVW_DVD_ROOT_MENU_RIGHT);
		break;
	case GDK_KEY_KP_Left:
	case GDK_KEY_KP_4:
		bacon_video_widget_dvd_event (totem->bvw, 
					      BVW_DVD_ROOT_MENU_LEFT);
		break;
	case GDK_KEY_KP_Begin:
	case GDK_KEY_KP_5:
		bacon_video_widget_dvd_event (totem->bvw,
					      BVW_DVD_ROOT_MENU_SELECT);
	default:
		retval = FALSE;
	}

	return retval;
}

static void
on_seek_requested_event (BaconVideoWidget *bvw,
			 gboolean          forward,
			 TotemObject      *totem)
{
	gint64 offset;

	offset = forward ? SEEK_FORWARD_OFFSET * 1000 : SEEK_BACKWARD_OFFSET * 1000;
	totem_object_seek_relative (totem, offset, FALSE);
}

static void
on_track_skip_requested_event (BaconVideoWidget *bvw,
			       gboolean          is_forward,
			       TotemObject      *totem)
{
	totem_object_direction (totem, is_forward ? TOTEM_PLAYLIST_DIRECTION_NEXT : TOTEM_PLAYLIST_DIRECTION_PREVIOUS);
}

static void
on_volume_change_requested_event (BaconVideoWidget *bvw,
				  gboolean          increase,
				  TotemObject      *totem)
{
	totem_object_set_volume_relative (totem, increase ? VOLUME_UP_OFFSET : VOLUME_DOWN_OFFSET);
}

gboolean
window_key_press_event_cb (GtkWidget *win, GdkEventKey *event, TotemObject *totem)
{
	/* Shortcuts disabled? */
	if (totem->disable_kbd_shortcuts != FALSE)
		return FALSE;

	/* Handle Quit */
	if (event->state & GDK_CONTROL_MASK &&
	    event->type == GDK_KEY_PRESS &&
	    (event->keyval == GDK_KEY_Q ||
	     event->keyval == GDK_KEY_q)) {
		return totem_object_handle_key_press (totem, event);
	}

	/* Handle back/quit */
	if (event->state & GDK_CONTROL_MASK &&
	    event->type == GDK_KEY_PRESS &&
	    (event->keyval == GDK_KEY_W ||
	     event->keyval == GDK_KEY_w)) {
		if (totem_grilo_get_show_back_button (TOTEM_GRILO (totem->grilo)) ||
		    g_str_equal (totem_object_get_main_page (totem), "player"))
			back_button_clicked_cb (NULL, totem);
		else
			totem_object_exit (totem);
		return FALSE;
	}

	/* Check whether we're in the player panel */
	if (!g_str_equal (totem_object_get_main_page (totem), "player"))
		return FALSE;

	/* Special case Eject, Open, Open URI,
	 * seeking and zoom keyboard shortcuts */
	if (event->state != 0
			&& (event->state & GDK_CONTROL_MASK))
	{
		switch (event->keyval) {
		case GDK_KEY_E:
		case GDK_KEY_e:
		case GDK_KEY_O:
		case GDK_KEY_o:
		case GDK_KEY_L:
		case GDK_KEY_l:
		case GDK_KEY_q:
		case GDK_KEY_Q:
		case GDK_KEY_Right:
		case GDK_KEY_Left:
		case GDK_KEY_plus:
		case GDK_KEY_KP_Add:
		case GDK_KEY_minus:
		case GDK_KEY_KP_Subtract:
		case GDK_KEY_0:
		case GDK_KEY_equal:
		case GDK_KEY_hyphen:
			if (event->type == GDK_KEY_PRESS)
				return totem_object_handle_key_press (totem, event);
		default:
			break;
		}
	}

	if (event->state != 0 && (event->state & GDK_SUPER_MASK)) {
		switch (event->keyval) {
		case GDK_KEY_Escape:
			if (event->type == GDK_KEY_PRESS)
				return totem_object_handle_key_press (totem, event);
		default:
			break;
		}
	}


	/* If we have modifiers, and either Ctrl, Mod1 (Alt), or any
	 * of Mod3 to Mod5 (Mod2 is num-lock...) are pressed, we
	 * let Gtk+ handle the key */
	if (event->state != 0
			&& ((event->state & GDK_CONTROL_MASK)
			|| (event->state & GDK_MOD1_MASK)
			|| (event->state & GDK_MOD3_MASK)
			|| (event->state & GDK_MOD4_MASK)))
		return FALSE;

	if (event->type == GDK_KEY_PRESS)
		return totem_object_handle_key_press (totem, event);

	return FALSE;
}

static void
update_media_menu_items (TotemObject *totem)
{
	GMount *mount;
	gboolean playing;

	playing = totem_playing_dvd (totem->mrl);

	totem_object_set_sensitivity2 ("dvd-root-menu", playing);
	totem_object_set_sensitivity2 ("dvd-title-menu", playing);
	totem_object_set_sensitivity2 ("dvd-audio-menu", playing);
	totem_object_set_sensitivity2 ("dvd-angle-menu", playing);
	totem_object_set_sensitivity2 ("dvd-chapter-menu", playing);

	totem_object_set_sensitivity2 ("next-angle",
				       bacon_video_widget_has_angles (totem->bvw));

	mount = totem_get_mount_for_media (totem->mrl);
	totem_object_set_sensitivity2 ("eject", mount != NULL);
	if (mount != NULL)
		g_object_unref (mount);
}

static void
update_buttons (TotemObject *totem)
{
	totem_object_set_sensitivity2 ("previous-chapter",
				       totem_object_can_seek_previous (totem));
	totem_object_set_sensitivity2 ("next-chapter",
				       totem_object_can_seek_next (totem));
}

void
totem_setup_window (TotemObject *totem)
{
	GKeyFile *keyfile;
	int w, h;
	char *filename;
	GError *err = NULL;
	GtkWidget *vbox;
	GdkRGBA black;

	filename = g_build_filename (totem_dot_dir (), "state.ini", NULL);
	keyfile = g_key_file_new ();
	if (g_key_file_load_from_file (keyfile, filename,
			G_KEY_FILE_NONE, NULL) == FALSE) {
		w = DEFAULT_WINDOW_W;
		h = DEFAULT_WINDOW_H;
		totem->maximised = TRUE;
		g_free (filename);
	} else {
		g_free (filename);

		w = g_key_file_get_integer (keyfile, "State", "window_w", &err);
		if (err != NULL) {
			w = 0;
			g_error_free (err);
			err = NULL;
		}

		h = g_key_file_get_integer (keyfile, "State", "window_h", &err);
		if (err != NULL) {
			h = 0;
			g_error_free (err);
			err = NULL;
		}

		totem->maximised = g_key_file_get_boolean (keyfile, "State",
				"maximised", &err);
		if (err != NULL) {
			g_error_free (err);
			err = NULL;
		}
	}

	if (w > 0 && h > 0 && totem->maximised == FALSE) {
		gtk_window_set_default_size (GTK_WINDOW (totem->win),
				w, h);
		totem->window_w = w;
		totem->window_h = h;
	} else if (totem->maximised != FALSE) {
		gtk_window_maximize (GTK_WINDOW (totem->win));
	}

	/* Set the vbox to be completely black */
	vbox = GTK_WIDGET (gtk_builder_get_object (totem->xml, "tmw_bvw_box"));
	gdk_rgba_parse (&black, "Black");
	gtk_widget_override_background_color (vbox, (GTK_STATE_FLAG_FOCUSED << 1), &black);

	/* Headerbar */
	totem->header = g_object_new (TOTEM_TYPE_MAIN_TOOLBAR,
				      "show-search-button", TRUE,
				      "show-select-button", TRUE,
				      "show-close-button", TRUE,
				      "title", _("Videos"),
				      NULL);
	g_signal_connect (G_OBJECT (totem->header), "back-clicked",
			  G_CALLBACK (back_button_clicked_cb), totem);
	gtk_window_set_titlebar (GTK_WINDOW (totem->win), totem->header);

	return;
}

static void
popup_menu_shown_cb (GtkToggleButton *button,
		     TotemObject     *totem)
{
	if (gtk_toggle_button_get_active (button))
		bacon_video_widget_mark_popup_busy (totem->bvw, "toolbar/go menu visible");
	else
		bacon_video_widget_unmark_popup_busy (totem->bvw, "toolbar/go menu visible");
}

static void
volume_button_menu_shown_cb (GObject     *popover,
			     GParamSpec  *pspec,
			     TotemObject *totem)
{
	if (gtk_widget_is_visible (GTK_WIDGET (popover)))
		bacon_video_widget_mark_popup_busy (totem->bvw, "volume menu visible");
	else
		bacon_video_widget_unmark_popup_busy (totem->bvw, "volume menu visible");
}

static void
update_add_button_visibility (GObject     *gobject,
			      GParamSpec  *pspec,
			      TotemObject *totem)
{
	TotemMainToolbar *bar = TOTEM_MAIN_TOOLBAR (gobject);

	if (totem_main_toolbar_get_search_mode (bar) ||
	    totem_main_toolbar_get_select_mode (bar)) {
		gtk_widget_hide (totem->add_button);
	} else {
		gtk_widget_set_visible (totem->add_button,
					totem_grilo_get_current_page (TOTEM_GRILO (totem->grilo))  == TOTEM_GRILO_PAGE_RECENT);
	}
}

static GtkWidget *
create_control_button (TotemObject *totem,
		       const gchar *action_name,
		       const gchar *icon_name,
		       const gchar *tooltip_text)
{
	GtkWidget *button, *image;

	button = gtk_button_new ();
	gtk_actionable_set_action_name (GTK_ACTIONABLE (button), action_name);
	image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
	gtk_button_set_image (GTK_BUTTON (button), image);
	gtk_widget_set_valign (GTK_WIDGET (button), GTK_ALIGN_CENTER);
	gtk_style_context_add_class (gtk_widget_get_style_context (button), "image-button");
	if (g_str_equal (action_name, "app.play")) {
		g_object_set (G_OBJECT (image),
			      "margin-start", 16,
			      "margin-end", 16,
			      NULL);
		totem->play_button = button;
	}

	gtk_button_set_label (GTK_BUTTON (button), NULL);
	gtk_widget_set_tooltip_text (button, tooltip_text);
	atk_object_set_name (gtk_widget_get_accessible (button), tooltip_text);

	gtk_widget_show_all (button);

	return button;
}

void
totem_callback_connect (TotemObject *totem)
{
	GtkWidget *item;
	GtkBox *box;
	GAction *gaction;
	GMenuModel *menu;
	GtkPopover *popover;

	/* Menu items */
	gaction = g_action_map_lookup_action (G_ACTION_MAP (totem), "repeat");
	g_simple_action_set_state (G_SIMPLE_ACTION (gaction),
				   g_variant_new_boolean (totem_playlist_get_repeat (totem->playlist)));

	/* Controls */
	box = g_object_get_data (totem->controls, "controls_box");
	gtk_widget_insert_action_group (GTK_WIDGET (box), "app", G_ACTION_GROUP (totem));

	/* Previous */
	item = create_control_button (totem, "app.previous-chapter",
				      "media-skip-backward-symbolic",
				      _("Previous Chapter/Movie"));
	gtk_box_pack_start (box, item, FALSE, FALSE, 0);

	/* Play/Pause */
	item = create_control_button (totem, "app.play",
				      "media-playback-start-symbolic",
				      _("Play / Pause"));
	gtk_box_pack_start (box, item, FALSE, FALSE, 0);

	/* Next */
	item = create_control_button (totem, "app.next-chapter",
				      "media-skip-forward-symbolic",
				      _("Next Chapter/Movie"));
	gtk_box_pack_start (box, item, FALSE, FALSE, 0);

	/* Seekbar */
	g_signal_connect (totem->seek, "button-press-event",
			  G_CALLBACK (seek_slider_pressed_cb), totem);
	g_signal_connect (totem->seek, "button-release-event",
			  G_CALLBACK (seek_slider_released_cb), totem);
	g_signal_connect (totem->seekadj, "value-changed",
			  G_CALLBACK (seek_slider_changed_cb), totem);

	/* Volume */
	g_signal_connect (totem->volume, "value-changed",
			  G_CALLBACK (volume_button_value_changed_cb), totem);
	item = gtk_scale_button_get_popup (GTK_SCALE_BUTTON (totem->volume));
	g_signal_connect (G_OBJECT (item), "notify::visible",
			  G_CALLBACK (volume_button_menu_shown_cb), totem);

	/* Go button */
	item = g_object_get_data (totem->controls, "go_button");
	menu = (GMenuModel *) gtk_builder_get_object (totem->xml, "gomenu");
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (item), menu);
	popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (item));
	gtk_popover_set_transitions_enabled (GTK_POPOVER (popover), FALSE);
	gtk_widget_set_size_request (GTK_WIDGET (popover), 175, -1);
	g_signal_connect (G_OBJECT (item), "toggled",
			  G_CALLBACK (popup_menu_shown_cb), totem);

	/* Main menu */
	item = totem->main_menu_button = totem_interface_create_header_button (totem->header,
									       gtk_menu_button_new (),
									       "open-menu-symbolic",
									       GTK_PACK_END);
	gtk_container_child_set (GTK_CONTAINER (totem->header), totem->main_menu_button,
				 "position", 0,
				 NULL);
	menu = (GMenuModel *) gtk_builder_get_object (totem->xml, "appmenu");
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (item), menu);
	gtk_widget_show (item);

	/* Player menu */
	item = totem->gear_button = totem_interface_create_header_button (totem->header,
									  gtk_menu_button_new (),
									  "view-more-symbolic",
									  GTK_PACK_END);
	menu = (GMenuModel *) gtk_builder_get_object (totem->xml, "playermenu");
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (item), menu);
	popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (item));
	gtk_popover_set_transitions_enabled (GTK_POPOVER (popover), FALSE);
	g_signal_connect (G_OBJECT (item), "toggled",
			  G_CALLBACK (popup_menu_shown_cb), totem);

	/* Add button */
	item = totem->add_button = totem_interface_create_header_button (totem->header,
									 gtk_menu_button_new (),
									 "list-add-symbolic",
									 GTK_PACK_START);
	menu = (GMenuModel *) gtk_builder_get_object (totem->xml, "addmenu");
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (item), menu);
	gtk_widget_show (item);

	g_signal_connect (G_OBJECT (totem->header), "notify::search-mode",
			  G_CALLBACK (update_add_button_visibility), totem);
	g_signal_connect (G_OBJECT (totem->header), "notify::select-mode",
			  G_CALLBACK (update_add_button_visibility), totem);

	/* Fullscreen button */
	item = totem->fullscreen_button = totem_interface_create_header_button (totem->header,
										gtk_button_new (),
										"view-fullscreen-symbolic",
										GTK_PACK_END);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (item), "app.fullscreen");

	/* Connect the keys */
	gtk_widget_add_events (totem->win, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	/* Set sensitivity of the toolbar buttons */
	totem_object_set_sensitivity2 ("play", FALSE);
	totem_object_set_sensitivity2 ("next-chapter", FALSE);
	totem_object_set_sensitivity2 ("previous-chapter", FALSE);

	/* Volume */
	g_signal_connect (G_OBJECT (totem->bvw), "notify::volume",
			G_CALLBACK (property_notify_cb_volume), totem);
	g_signal_connect (G_OBJECT (totem->bvw), "notify::seekable",
			G_CALLBACK (property_notify_cb_seekable), totem);
	update_volume_sliders (totem);
}

void
playlist_widget_setup (TotemObject *totem)
{
	totem->playlist = TOTEM_PLAYLIST (totem_playlist_new ());

	if (totem->playlist == NULL)
		totem_object_exit (totem);

#if 0
	{
		GtkWidget *window;

		window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_default_size (GTK_WINDOW (window), 500, 400);
		gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (totem->playlist));
		gtk_widget_show_all (window);
	}
#endif
	g_signal_connect (G_OBJECT (totem->playlist), "active-name-changed",
			  G_CALLBACK (on_playlist_change_name), totem);
	g_signal_connect (G_OBJECT (totem->playlist), "item-activated",
			  G_CALLBACK (item_activated_cb), totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			  "changed", G_CALLBACK (playlist_changed_cb),
			  totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			  "current-removed", G_CALLBACK (current_removed_cb),
			  totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			  "notify::repeat",
			  G_CALLBACK (playlist_repeat_toggle_cb),
			  totem);
	g_signal_connect (G_OBJECT (totem->playlist),
			  "subtitle-changed",
			  G_CALLBACK (subtitle_changed_cb),
			  totem);
}

static void
grilo_show_back_button_changed (TotemGrilo  *grilo,
				GParamSpec  *spec,
				TotemObject *totem)
{
	if (g_strcmp0 (totem_object_get_main_page (totem), "grilo") == 0) {
		g_object_set (totem->header,
			      "show-back-button", totem_grilo_get_show_back_button (TOTEM_GRILO (totem->grilo)),
			      NULL);
	}
}

static void
grilo_current_page_changed (TotemGrilo  *grilo,
			    GParamSpec  *spec,
			    TotemObject *totem)
{
	if (g_strcmp0 (totem_object_get_main_page (totem), "grilo") == 0) {
		TotemGriloPage page;

		page = totem_grilo_get_current_page (TOTEM_GRILO (totem->grilo));
		gtk_widget_set_visible (totem->add_button,
					page == TOTEM_GRILO_PAGE_RECENT);
	}
}

void
grilo_widget_setup (TotemObject *totem)
{
	totem->grilo = totem_grilo_new (totem, totem->header);
	g_signal_connect (G_OBJECT (totem->grilo), "notify::show-back-button",
			  G_CALLBACK (grilo_show_back_button_changed), totem);
	g_signal_connect (G_OBJECT (totem->grilo), "notify::current-page",
			  G_CALLBACK (grilo_current_page_changed), totem);
	gtk_stack_add_named (GTK_STACK (totem->stack),
			     totem->grilo,
			     "grilo");
	gtk_stack_set_visible_child_name (GTK_STACK (totem->stack), "grilo");
}

static void
add_fullscreen_toolbar (TotemObject *totem)
{
	GtkWidget *container, *item;
	GMenuModel *menu;

	container = GTK_WIDGET (bacon_video_widget_get_header_controls_object (totem->bvw));

	totem->fullscreen_header = g_object_new (TOTEM_TYPE_MAIN_TOOLBAR,
						 "show-search-button", FALSE,
						 "show-select-button", FALSE,
						 "show-back-button", TRUE,
						 NULL);
	g_object_bind_property (totem->header, "title",
				totem->fullscreen_header, "title", 0);
	g_object_bind_property (totem->header, "subtitle",
				totem->fullscreen_header, "subtitle", 0);
	g_signal_connect (G_OBJECT (totem->fullscreen_header), "back-clicked",
			  G_CALLBACK (back_button_clicked_cb), totem);

	item = totem_interface_create_header_button (totem->fullscreen_header,
						     gtk_button_new (),
						     "view-restore-symbolic",
						     GTK_PACK_END);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (item), "app.fullscreen");

	item = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
	gtk_header_bar_pack_end (GTK_HEADER_BAR (totem->fullscreen_header), item);
	gtk_style_context_add_class (gtk_widget_get_style_context (item), "header-bar-separator");

	item = totem_interface_create_header_button (totem->fullscreen_header,
						     gtk_menu_button_new (),
						     "view-more-symbolic",
						     GTK_PACK_END);
	menu = (GMenuModel *) gtk_builder_get_object (totem->xml, "playermenu");
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (item), menu);
	g_signal_connect (G_OBJECT (item), "toggled",
			  G_CALLBACK (popup_menu_shown_cb), totem);
	totem->fullscreen_gear_button = item;

	gtk_container_add (GTK_CONTAINER (container), totem->fullscreen_header);
	gtk_widget_show_all (totem->fullscreen_header);
}

void
video_widget_create (TotemObject *totem)
{
	GError *err = NULL;
	GtkContainer *container;
	BaconVideoWidget **bvw;
	GdkWindow *window;
	gboolean fullscreen;

	totem->bvw = BACON_VIDEO_WIDGET (bacon_video_widget_new (&err));

	if (totem->bvw == NULL) {
		totem_object_show_error_and_exit (_("Totem could not startup."), err != NULL ? err->message : _("No reason."), totem);
		if (err != NULL)
			g_error_free (err);
	}

	window = gtk_widget_get_window (totem->win);

	fullscreen = window && ((gdk_window_get_state (window) & GDK_WINDOW_STATE_FULLSCREEN) != 0);
	bacon_video_widget_set_fullscreen (totem->bvw, fullscreen);
	totem->controls = bacon_video_widget_get_controls_object (totem->bvw);

	g_signal_connect_after (G_OBJECT (totem->bvw),
			"button-press-event",
			G_CALLBACK (on_video_button_press_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"eos",
			G_CALLBACK (on_eos_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"got-redirect",
			G_CALLBACK (on_got_redirect),
			totem);
	g_signal_connect (G_OBJECT(totem->bvw),
			"channels-change",
			G_CALLBACK (on_channels_change_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"tick",
			G_CALLBACK (update_current_time),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"got-metadata",
			G_CALLBACK (on_got_metadata_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"buffering",
			G_CALLBACK (on_buffering_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"download-buffering",
			G_CALLBACK (on_download_buffering_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			"error",
			G_CALLBACK (on_error_event),
			totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			  "seek-requested",
			  G_CALLBACK (on_seek_requested_event),
			  totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			  "track-skip-requested",
			  G_CALLBACK (on_track_skip_requested_event),
			  totem);
	g_signal_connect (G_OBJECT (totem->bvw),
			  "volume-change-requested",
			  G_CALLBACK (on_volume_change_requested_event),
			  totem);

	container = GTK_CONTAINER (gtk_builder_get_object (totem->xml, "tmw_bvw_box"));
	gtk_container_add (container,
			GTK_WIDGET (totem->bvw));

	add_fullscreen_toolbar (totem);

	/* Events for the widget video window as well */
	gtk_widget_add_events (GTK_WIDGET (totem->bvw),
			GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
	g_signal_connect (G_OBJECT(totem->bvw), "key_press_event",
			G_CALLBACK (window_key_press_event_cb), totem);
	g_signal_connect (G_OBJECT(totem->bvw), "key_release_event",
			G_CALLBACK (window_key_press_event_cb), totem);

	g_signal_connect (G_OBJECT (totem->bvw), "drag_data_received",
			G_CALLBACK (drop_video_cb), totem);
	gtk_drag_dest_set (GTK_WIDGET (totem->bvw), GTK_DEST_DEFAULT_ALL,
			   target_table, G_N_ELEMENTS (target_table),
			   GDK_ACTION_MOVE);

	bvw = &(totem->bvw);
	g_object_add_weak_pointer (G_OBJECT (totem->bvw),
				   (gpointer *) bvw);

	gtk_widget_show (GTK_WIDGET (totem->bvw));

	/* FIXME: Otherwise we get a visible but
	 * not realized widget ?!?! */
	gtk_widget_realize (GTK_WIDGET (totem->bvw));
}

/**
 * totem_object_get_supported_content_types:
 *
 * Get the full list of file content types which Totem supports playing.
 *
 * Return value: (array zero-terminated=1) (transfer none): a %NULL-terminated array of the content types Totem supports
 * Since: 3.1.5
 */
const gchar * const *
totem_object_get_supported_content_types (void)
{
	return mime_types;
}

/**
 * totem_object_get_supported_uri_schemes:
 *
 * Get the full list of URI schemes which Totem supports accessing.
 *
 * Return value: (array zero-terminated=1) (transfer none): a %NULL-terminated array of the URI schemes Totem supports
 * Since: 3.1.5
 */
const gchar * const *
totem_object_get_supported_uri_schemes (void)
{
	return uri_schemes;
}
