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

#include "totem-plugin.h"
#include "totem-screenshot-plugin.h"
#include "screenshot-filename-builder.h"
#include "totem-gallery.h"
#include "totem-uri.h"
#include "backend/bacon-video-widget.h"

#define TOTEM_TYPE_SCREENSHOT_PLUGIN		(totem_screenshot_plugin_get_type ())
#define TOTEM_SCREENSHOT_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_SCREENSHOT_PLUGIN, TotemScreenshotPlugin))
#define TOTEM_SCREENSHOT_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_SCREENSHOT_PLUGIN, TotemScreenshotPluginClass))
#define TOTEM_IS_SCREENSHOT_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_SCREENSHOT_PLUGIN))
#define TOTEM_IS_SCREENSHOT_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_SCREENSHOT_PLUGIN))
#define TOTEM_SCREENSHOT_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_SCREENSHOT_PLUGIN, TotemScreenshotPluginClass))

typedef struct {
	Totem *totem;
	BaconVideoWidget *bvw;

	gulong got_metadata_signal;
	gulong notify_logo_mode_signal;

	GSettings *settings;
	gboolean save_to_disk;

	GSimpleAction *screenshot_action;
	GSimpleAction *gallery_action;
} TotemScreenshotPluginPrivate;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_SCREENSHOT_PLUGIN,
		      TotemScreenshotPlugin,
		      totem_screenshot_plugin)

typedef struct {
	TotemScreenshotPlugin *plugin;
	GdkPixbuf *pixbuf;
} ScreenshotSaveJob;

static void
screenshot_save_job_free (ScreenshotSaveJob *job)
{
	g_object_unref (job->pixbuf);
	g_slice_free (ScreenshotSaveJob, job);
}

static void
save_pixbuf_ready_cb (GObject *source,
		      GAsyncResult *res,
		      gpointer user_data)
{
	GError *error = NULL;
	ScreenshotSaveJob *job = (ScreenshotSaveJob *) user_data;

	if (gdk_pixbuf_save_to_stream_finish (res, &error) == FALSE) {
		g_warning ("Couldn't save screenshot: %s", error->message);
		g_error_free (error);
	}

	screenshot_save_job_free (job);
}

static void
save_file_create_ready_cb (GObject *source,
			   GAsyncResult *res,
			   gpointer user_data)
{
	GFileOutputStream *stream;
	GError *error = NULL;
	ScreenshotSaveJob *job = (ScreenshotSaveJob *) user_data;

	stream = g_file_create_finish (G_FILE (source), res, &error);
	if (stream == NULL) {
		char *path;

		path = g_file_get_path (G_FILE (source));
		g_warning ("Couldn't create a new file at '%s': %s", path, error->message);
		g_free (path);

		g_error_free (error);
		screenshot_save_job_free (job);
		return;
	}

	gdk_pixbuf_save_to_stream_async (job->pixbuf,
					 G_OUTPUT_STREAM (stream),
					 "png", NULL,
					 save_pixbuf_ready_cb, job,
					 "tEXt::Software", "totem",
					 NULL);

	g_object_unref (stream);
}

static void
screenshot_name_ready_cb (GObject *source,
			  GAsyncResult *res,
			  gpointer user_data)
{
	GFile *save_file;
	char *save_path;
	GError *error = NULL;
	ScreenshotSaveJob *job = (ScreenshotSaveJob *) user_data;

	save_path = screenshot_build_filename_finish (res, &error);
	if (save_path == NULL) {
		g_warning ("Could not find a valid location to save the screenshot: %s", error->message);
		g_error_free (error);
		screenshot_save_job_free (job);
		return;
	}

	save_file = g_file_new_for_path (save_path);
	g_free (save_path);

	g_file_create_async (save_file,
			     G_FILE_CREATE_NONE,
			     G_PRIORITY_DEFAULT,
			     NULL,
			     save_file_create_ready_cb, job);

	g_object_unref (save_file);
}

static void
flash_area_done_cb (GObject *source_object,
		    GAsyncResult *res,
		    gpointer user_data)
{
	GVariant *variant;

	variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, NULL);
	if (variant != NULL)
		g_variant_unref (variant);
}

static void
flash_area (GtkWidget *widget)
{
	GDBusProxy *proxy;
	GdkWindow *window;
	int x, y, w, h;

	window = gtk_widget_get_window (widget);
	gdk_window_get_origin (window, &x, &y);
	w = gdk_window_get_width (window);
	h = gdk_window_get_height (window);

	proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
					       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
					       G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
					       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
					       NULL,
					       "org.gnome.Shell",
					       "/org/gnome/Shell/Screenshot",
					       "org.gnome.Shell.Screenshot",
					       NULL, NULL);
	if (proxy == NULL)
		g_warning ("no proxy");

	g_dbus_proxy_call (proxy, "org.gnome.Shell.Screenshot.FlashArea",
			   g_variant_new ("(iiii)", x, y, w, h),
			   G_DBUS_CALL_FLAGS_NO_AUTO_START,
			   -1,
			   NULL,
			   flash_area_done_cb,
			   NULL);
}

static void
take_screenshot_action_cb (GSimpleAction         *action,
			   GVariant              *parameter,
			   TotemScreenshotPlugin *self)
{
	TotemScreenshotPluginPrivate *priv = self->priv;
	GdkPixbuf *pixbuf;
	GError *err = NULL;
	ScreenshotSaveJob *job;
	char *video_name;

	if (bacon_video_widget_get_logo_mode (priv->bvw) != FALSE)
		return;

	if (bacon_video_widget_can_get_frames (priv->bvw, &err) == FALSE) {
		if (err == NULL)
			return;

		totem_object_action_error (priv->totem, _("Totem could not get a screenshot of the video."), err->message);
		g_error_free (err);
		return;
	}

	flash_area (GTK_WIDGET (priv->bvw));

	pixbuf = bacon_video_widget_get_current_frame (priv->bvw);
	if (pixbuf == NULL) {
		totem_object_action_error (priv->totem, _("Totem could not get a screenshot of the video."), _("This is not supposed to happen; please file a bug report."));
		return;
	}

	video_name = totem_object_get_short_title (self->priv->totem);

	job = g_slice_new (ScreenshotSaveJob);
	job->plugin = self;
	job->pixbuf = pixbuf;

	screenshot_build_filename_async (NULL, video_name, screenshot_name_ready_cb, job);

	g_free (video_name);
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
take_gallery_action_cb (GAction               *action,
			GVariant              *parameter,
			TotemScreenshotPlugin *self)
{
	Totem *totem = self->priv->totem;
	GtkDialog *dialog;

	if (bacon_video_widget_get_logo_mode (self->priv->bvw) != FALSE)
		return;

	dialog = GTK_DIALOG (totem_gallery_new (totem));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (take_gallery_response_cb), self);
	gtk_widget_show (GTK_WIDGET (dialog));
}

static void
update_state (TotemScreenshotPlugin *self)
{
	TotemScreenshotPluginPrivate *priv = self->priv;
	gboolean sensitive;

	sensitive = bacon_video_widget_can_get_frames (priv->bvw, NULL) &&
		    (bacon_video_widget_get_logo_mode (priv->bvw) == FALSE) &&
		    priv->save_to_disk;

	g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->screenshot_action), sensitive);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (priv->gallery_action), sensitive);
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
	TotemScreenshotPlugin *self = TOTEM_SCREENSHOT_PLUGIN (plugin);
	TotemScreenshotPluginPrivate *priv = self->priv;
	GMenu *menu;
	GMenuItem *item;

	priv->totem = g_object_get_data (G_OBJECT (plugin), "object");
	priv->bvw = BACON_VIDEO_WIDGET (totem_object_get_video_widget (priv->totem));
	priv->got_metadata_signal = g_signal_connect (G_OBJECT (priv->bvw),
						      "got-metadata",
						      G_CALLBACK (got_metadata_cb),
						      self);
	priv->notify_logo_mode_signal = g_signal_connect (G_OBJECT (priv->bvw),
							  "notify::logo-mode",
							  G_CALLBACK (notify_logo_mode_cb),
							  self);

	priv->screenshot_action = g_simple_action_new ("take-screenshot", NULL);
	g_signal_connect (G_OBJECT (priv->screenshot_action), "activate",
			  G_CALLBACK (take_screenshot_action_cb), plugin);
	g_action_map_add_action (G_ACTION_MAP (priv->totem), G_ACTION (priv->screenshot_action));
	gtk_application_add_accelerator (GTK_APPLICATION (priv->totem),
					 "<Primary><Alt>s",
					 "app.take-screenshot",
					 NULL);

	priv->gallery_action = g_simple_action_new ("take-gallery", NULL);
	g_signal_connect (G_OBJECT (priv->gallery_action), "activate",
			  G_CALLBACK (take_gallery_action_cb), plugin);
	g_action_map_add_action (G_ACTION_MAP (priv->totem), G_ACTION (priv->gallery_action));

	/* Install the menu */
	menu = totem_object_get_menu_section (priv->totem, "screenshot-placeholder");
	item = g_menu_item_new (_("Take _Screenshot"), "app.take-screenshot");
	g_menu_item_set_attribute (item, "accel", "s", "<Primary><Alt>s");
	g_menu_append_item (G_MENU (menu), item);
	g_menu_append (G_MENU (menu), _("Create Screenshot _Gallery..."), "app.take-gallery");

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

	/* Disconnect signal handlers */
	g_signal_handler_disconnect (G_OBJECT (priv->bvw), priv->got_metadata_signal);
	g_signal_handler_disconnect (G_OBJECT (priv->bvw), priv->notify_logo_mode_signal);

	gtk_application_remove_accelerator (GTK_APPLICATION (priv->totem),
					    "app.take-screenshot",
					    NULL);

	/* Disconnect from GSettings */
	g_object_unref (priv->settings);

	/* Remove the menu */
	totem_object_empty_menu_section (priv->totem, "screenshot-placeholder");

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
	path = g_settings_get_string (settings, "screenshot-save-uri");
	g_object_unref (settings);

	/* Default to the Pictures directory */
	if (*path == '\0') {
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
	g_settings_set_string (settings, "screenshot-save-uri", dir);
	g_object_unref (settings);
	g_free (dir);
}

