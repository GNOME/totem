/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2008 Philip Withnall <philip@tecnocode.co.uk>
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
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <libpeas/peas-activatable.h>

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#include "totem-plugin.h"
#include "totem-screenshot-plugin.h"
#include "totem-screenshot.h"
#include "totem-gallery.h"
#include "totem-uri.h"
#include "backend/bacon-video-widget.h"

struct TotemScreenshotPluginPrivate {
	Totem *totem;
	BaconVideoWidget *bvw;

	gulong got_metadata_signal;
	gulong notify_logo_mode_signal;
	gulong key_press_event_signal;

	GSettings *settings;
	gboolean save_to_disk;

	guint ui_merge_id;
	GtkActionGroup *action_group;
};

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_SCREENSHOT_PLUGIN,
		      TotemScreenshotPlugin,
		      totem_screenshot_plugin)

static void
totem_screenshot_plugin_class_init (TotemScreenshotPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = set_property;
	object_class->get_property = get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");
	g_type_class_add_private (klass, sizeof (TotemScreenshotPluginPrivate));
}

static void
totem_screenshot_plugin_init (TotemScreenshotPlugin *plugin)
{
	plugin->priv = G_TYPE_INSTANCE_GET_PRIVATE (plugin,
						    TOTEM_TYPE_SCREENSHOT_PLUGIN,
						    TotemScreenshotPluginPrivate);
}

static void
take_screenshot_action_cb (GtkAction *action, TotemScreenshotPlugin *self)
{
	TotemScreenshotPluginPrivate *priv = self->priv;
	GdkPixbuf *pixbuf;
	GtkWidget *dialog;
	GError *err = NULL;

	if (bacon_video_widget_get_logo_mode (priv->bvw) != FALSE)
		return;

	if (bacon_video_widget_can_get_frames (priv->bvw, &err) == FALSE) {
		if (err == NULL)
			return;

		totem_action_error (priv->totem, _("Totem could not get a screenshot of the video."), err->message);
		g_error_free (err);
		return;
	}

	pixbuf = bacon_video_widget_get_current_frame (priv->bvw);
	if (pixbuf == NULL) {
		totem_action_error (priv->totem, _("Totem could not get a screenshot of the video."), _("This is not supposed to happen; please file a bug report."));
		return;
	}

	dialog = totem_screenshot_new (priv->totem, self, pixbuf);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_object_unref (pixbuf);
}

static void
take_gallery_response_cb (GtkDialog *dialog,
			  int response_id,
			  TotemScreenshotPlugin *self)
{
	if (response_id != GTK_RESPONSE_OK)
		gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
take_gallery_action_cb (GtkAction *action, TotemScreenshotPlugin *self)
{
	Totem *totem = self->priv->totem;
	GtkDialog *dialog;

	if (bacon_video_widget_get_logo_mode (self->priv->bvw) != FALSE)
		return;

	dialog = GTK_DIALOG (totem_gallery_new (totem, self));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (take_gallery_response_cb), self);
	gtk_widget_show (GTK_WIDGET (dialog));
}

static gboolean
window_key_press_event_cb (GtkWidget *window, GdkEventKey *event, TotemScreenshotPlugin *self)
{
	switch (event->keyval) {
#ifdef HAVE_XFREE
	case XF86XK_Save:
		take_screenshot_action_cb (NULL, self);
		break;
#endif /* HAVE_XFREE */
	case GDK_s:
	case GDK_S:
		if (event->state & GDK_CONTROL_MASK)
			take_screenshot_action_cb (NULL, self);
		else
			return FALSE;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

static void
update_state (TotemScreenshotPlugin *self)
{
	TotemScreenshotPluginPrivate *priv = self->priv;
	gboolean sensitive;
	GtkAction *action;

	sensitive = bacon_video_widget_can_get_frames (priv->bvw, NULL) &&
		    (bacon_video_widget_get_logo_mode (priv->bvw) == FALSE) &&
		    priv->save_to_disk;

	action = gtk_action_group_get_action (priv->action_group, "take-screenshot");
	gtk_action_set_sensitive (action, sensitive);
	action = gtk_action_group_get_action (priv->action_group, "take-gallery");
	gtk_action_set_sensitive (action, sensitive);
}

static void
got_metadata_cb (BaconVideoWidget *bvw, TotemScreenshotPlugin *self)
{
	update_state (self);
}

static void
notify_logo_mode_cb (GObject *object, GParamSpec *pspec, TotemScreenshotPlugin *self)
{
	update_state (self);
}

static void
disable_save_to_disk_changed_cb (GSettings *settings, const gchar *key, TotemScreenshotPlugin *self)
{
	self->priv->save_to_disk = !g_settings_get_boolean (settings, "disable-save-to-disk");
}

static void
impl_activate (PeasActivatable *plugin)
{
	GtkWindow *window;
	GtkUIManager *manager;
	TotemScreenshotPlugin *self = TOTEM_SCREENSHOT_PLUGIN (plugin);
	TotemScreenshotPluginPrivate *priv = self->priv;
	const GtkActionEntry menu_entries[] = {
		{ "take-screenshot", "camera-photo", N_("Take _Screenshot..."), "<Ctrl>S", N_("Take a screenshot"), G_CALLBACK (take_screenshot_action_cb) },
		{ "take-gallery", NULL, N_("Create Screenshot _Gallery..."), NULL, N_("Create a gallery of screenshots"), G_CALLBACK (take_gallery_action_cb) }
	};

	priv->totem = g_object_get_data (G_OBJECT (plugin), "object");
	priv->bvw = BACON_VIDEO_WIDGET (totem_get_video_widget (priv->totem));
	priv->got_metadata_signal = g_signal_connect (G_OBJECT (priv->bvw),
						      "got-metadata",
						      G_CALLBACK (got_metadata_cb),
						      self);
	priv->notify_logo_mode_signal = g_signal_connect (G_OBJECT (priv->bvw),
							  "notify::logo-mode",
							  G_CALLBACK (notify_logo_mode_cb),
							  self);

	/* Key press handler */
	window = totem_get_main_window (priv->totem);
	priv->key_press_event_signal = g_signal_connect (G_OBJECT (window),
							 "key-press-event", 
							 G_CALLBACK (window_key_press_event_cb),
							 self);
	g_object_unref (window);

	/* Install the menu */
	priv->action_group = gtk_action_group_new ("screenshot_group");
	gtk_action_group_set_translation_domain (priv->action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (priv->action_group, menu_entries,
				      G_N_ELEMENTS (menu_entries), self);

	manager = totem_get_ui_manager (priv->totem);

	gtk_ui_manager_insert_action_group (manager, priv->action_group, -1);
	g_object_unref (priv->action_group);

	priv->ui_merge_id = gtk_ui_manager_new_merge_id (manager);
	gtk_ui_manager_add_ui (manager, priv->ui_merge_id,
			       "/ui/tmw-menubar/edit/repeat-mode", "take-screenshot",
			       "take-screenshot", GTK_UI_MANAGER_AUTO, TRUE);
	gtk_ui_manager_add_ui (manager, priv->ui_merge_id,
			       "/ui/tmw-menubar/edit/repeat-mode", "take-gallery",
			       "take-gallery", GTK_UI_MANAGER_AUTO, TRUE);
	gtk_ui_manager_add_ui (manager, priv->ui_merge_id,
			       "/ui/tmw-menubar/edit/repeat-mode", NULL,
			       NULL, GTK_UI_MANAGER_SEPARATOR, TRUE);

	/* Set up a GSettings watch for lockdown keys */
	priv->settings = g_settings_new ("org.gnome.desktop.lockdown");
	g_signal_connect (priv->settings, "changed::disable-save-to-disk", (GCallback) disable_save_to_disk_changed_cb, self);
	disable_save_to_disk_changed_cb (priv->settings, "disable-save-to-disk", self);

	/* Update the menu entries' states */
	update_state (self);
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemScreenshotPluginPrivate *priv = TOTEM_SCREENSHOT_PLUGIN (plugin)->priv;
	GtkWindow *window;
	GtkUIManager *manager;

	/* Disconnect signal handlers */
	g_signal_handler_disconnect (G_OBJECT (priv->bvw), priv->got_metadata_signal);
	g_signal_handler_disconnect (G_OBJECT (priv->bvw), priv->notify_logo_mode_signal);

	window = totem_get_main_window (priv->totem);
	g_signal_handler_disconnect (G_OBJECT (window), priv->key_press_event_signal);
	g_object_unref (window);

	/* Disconnect from GSettings */
	g_object_unref (priv->settings);

	/* Remove the menu */
	manager = totem_get_ui_manager (priv->totem);
	gtk_ui_manager_remove_ui (manager, priv->ui_merge_id);
	gtk_ui_manager_remove_action_group (manager, priv->action_group);

	g_object_unref (priv->bvw);
}

static char *
make_filename_for_dir (const char *directory, const char *format, const char *movie_title)
{
	char *fullpath, *filename;
	guint i = 1;

	filename = g_strdup_printf (_(format), movie_title, i);
	fullpath = g_build_filename (directory, filename, NULL);

	while (g_file_test (fullpath, G_FILE_TEST_EXISTS) != FALSE && i < G_MAXINT) {
		i++;
		g_free (filename);
		g_free (fullpath);

		filename = g_strdup_printf (_(format), movie_title, i);
		fullpath = g_build_filename (directory, filename, NULL);
	}

	g_free (fullpath);

	return filename;
}

gchar *
totem_screenshot_plugin_setup_file_chooser (const char *filename_format, const char *movie_title)
{
	GSettings *settings;
	char *path, *filename, *full, *uri;
	GFile *file;

	/* Set the default path */
	settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);
	path = g_settings_get_string (settings, "screenshot-save-path");
	g_object_unref (settings);

	/* Default to the Pictures directory */
	if (path == NULL || path[0] == '\0') {
		g_free (path);
		path = totem_pictures_dir ();
		/* No pictures dir, then it's the home dir */
		if (path == NULL)
			path = g_strdup (g_get_home_dir ());
	}

	filename = make_filename_for_dir (path, filename_format, movie_title);

	/* Build the URI */
	full = g_build_filename (path, filename, NULL);
	g_free (path);
	g_free (filename);

	file = g_file_new_for_path (full);
	uri = g_file_get_uri (file);
	g_free (full);
	g_object_unref (file);

	return uri;
}

void
totem_screenshot_plugin_update_file_chooser (const char *uri)
{
	GSettings *settings;
	char *dir;
	GFile *file, *parent;

	file = g_file_new_for_uri (uri);
	parent = g_file_get_parent (file);
	g_object_unref (file);

	dir = g_file_get_path (parent);
	g_object_unref (parent);

	settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);
	g_settings_set_string (settings, "screenshot-save-path", dir);
	g_object_unref (settings);
	g_free (dir);
}

