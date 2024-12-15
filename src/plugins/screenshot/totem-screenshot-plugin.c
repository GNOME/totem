/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Philip Withnall <philip@tecnocode.co.uk>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include "totem-plugin-activatable.h"

#include "totem-plugin.h"
#include "totem-screenshot-plugin.h"
#include "totem-gallery.h"
#include "totem-uri.h"
#include "backend/bacon-video-widget.h"

#define TOTEM_TYPE_SCREENSHOT_PLUGIN		(totem_screenshot_plugin_get_type ())
#define TOTEM_SCREENSHOT_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_SCREENSHOT_PLUGIN, TotemScreenshotPlugin))

typedef struct {
	PeasExtensionBase parent;

	Totem *totem;
	BaconVideoWidget *bvw;

	gulong got_metadata_signal;

	GSettings *settings;
	gboolean save_to_disk;

	GSimpleAction *screenshot_action;
	GSimpleAction *gallery_action;
} TotemScreenshotPlugin;

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
save_file_replace_ready_cb (GObject *source,
			    GAsyncResult *res,
			    gpointer user_data)
{
	GFileOutputStream *stream;
	GError *error = NULL;
	ScreenshotSaveJob *job = (ScreenshotSaveJob *) user_data;

	stream = g_file_replace_finish (G_FILE (source), res, &error);
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
					 "tEXt::Software", "org.gnome.Totem",
					 NULL);

	g_object_unref (stream);
}

static void
filechooser_response_callback (GtkNativeDialog *file_chooser,
			       gint response_id,
			       ScreenshotSaveJob *job)
{
	g_autoptr(GFile) filename = NULL;

	if (response_id == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (file_chooser));

		totem_screenshot_plugin_update_file_chooser (gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (file_chooser)));

		g_file_replace_async (filename,
				      NULL,
				      FALSE,
				      G_FILE_CREATE_REPLACE_DESTINATION,
				      G_PRIORITY_DEFAULT,
				      NULL,
				      save_file_replace_ready_cb, job);
	}

	gtk_native_dialog_destroy (file_chooser);
}

static char *
escape_video_name (const char *orig)
{
	g_auto(GStrv) elems = NULL;

	/* '/' can't be in a filename */
	elems = g_strsplit (orig, "/", -1);
	return g_strjoinv ("–", elems);
}

char *
totem_screenshot_plugin_filename_for_current_video (TotemObject *totem,
						    const char *format)
{
	g_autofree char *video_name = NULL;
	g_autofree char *escaped_video_name = NULL;

	video_name = totem_object_get_short_title (totem);
	escaped_video_name = escape_video_name (video_name);
	return g_strdup_printf (_(format), escaped_video_name);
}

static void
take_screenshot_action_cb (GSimpleAction         *action,
			   GVariant              *parameter,
			   TotemScreenshotPlugin *pi)
{
	GtkFileChooserNative *file_chooser;
	GdkPixbuf *pixbuf;
	GError *err = NULL;
	ScreenshotSaveJob *job;
	g_autofree char *video_filename = NULL;

	if (bacon_video_widget_can_get_frames (pi->bvw, &err) == FALSE) {
		totem_object_show_error (pi->totem, _("Videos could not get a screenshot of the video."), err->message ?: _("No reason."));
		g_error_free (err);
		return;
	}

	pixbuf = bacon_video_widget_get_current_frame (pi->bvw);
	if (pixbuf == NULL) {
		totem_object_show_error (pi->totem, _("Videos could not get a screenshot of the video."), _("This is not supposed to happen; please file a bug report."));
		return;
	}

	video_filename = totem_screenshot_plugin_filename_for_current_video (pi->totem, N_("Screenshot from %s.png"));

	file_chooser = gtk_file_chooser_native_new (_("Save Gallery"),
						    GTK_WINDOW (totem_object_get_main_window(pi->totem)),
						    GTK_FILE_CHOOSER_ACTION_SAVE,
						    _("_Save"), _("_Cancel"));
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (file_chooser), video_filename);
	totem_screenshot_plugin_set_file_chooser_folder (GTK_FILE_CHOOSER (file_chooser));
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (file_chooser), TRUE);

	job = g_slice_new (ScreenshotSaveJob);
	job->plugin = pi;
	job->pixbuf = pixbuf;

	g_signal_connect (file_chooser, "response",
			  G_CALLBACK (filechooser_response_callback), job);

	gtk_native_dialog_show (GTK_NATIVE_DIALOG (file_chooser));
}

static void
take_gallery_action_cb (GAction               *action,
			GVariant              *parameter,
			TotemScreenshotPlugin *pi)
{
	TotemGallery *gallery_window;

	gallery_window = totem_gallery_new (pi->totem);

	gtk_window_present (GTK_WINDOW (gallery_window));
}

static void
update_state (TotemScreenshotPlugin *pi)
{
	gboolean sensitive;

	sensitive = bacon_video_widget_can_get_frames (pi->bvw, NULL) &&
		    pi->save_to_disk;

	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->screenshot_action), sensitive);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->gallery_action), sensitive);
}

static void
got_metadata_cb (BaconVideoWidget *bvw, TotemScreenshotPlugin *pi)
{
	update_state (pi);
}

static void
disable_save_to_disk_changed_cb (GSettings *settings, const gchar *key, TotemScreenshotPlugin *pi)
{
	pi->save_to_disk = !g_settings_get_boolean (settings, "disable-save-to-disk");
}

static void
impl_activate (TotemPluginActivatable *plugin)
{
	TotemScreenshotPlugin *pi = TOTEM_SCREENSHOT_PLUGIN (plugin);
	GMenu *menu;
	GMenuItem *item;
	const char * const accels[]= { "<Primary><Alt>s", NULL };

	pi->totem = g_object_get_data (G_OBJECT (plugin), "object");
	pi->bvw = BACON_VIDEO_WIDGET (totem_object_get_video_widget (pi->totem));
	pi->got_metadata_signal = g_signal_connect (G_OBJECT (pi->bvw),
						      "got-metadata",
						      G_CALLBACK (got_metadata_cb),
						      pi);
	pi->screenshot_action = g_simple_action_new ("take-screenshot", NULL);
	g_signal_connect (G_OBJECT (pi->screenshot_action), "activate",
			  G_CALLBACK (take_screenshot_action_cb), plugin);
	g_action_map_add_action (G_ACTION_MAP (pi->totem), G_ACTION (pi->screenshot_action));
	gtk_application_set_accels_for_action (GTK_APPLICATION (pi->totem),
					       "app.take-screenshot",
					       accels);

	pi->gallery_action = g_simple_action_new ("take-gallery", NULL);
	g_signal_connect (G_OBJECT (pi->gallery_action), "activate",
			  G_CALLBACK (take_gallery_action_cb), plugin);
	g_action_map_add_action (G_ACTION_MAP (pi->totem), G_ACTION (pi->gallery_action));

	/* Install the menu */
	menu = totem_object_get_menu_section (pi->totem, "screenshot-placeholder");
	item = g_menu_item_new (_("Take _Screenshot"), "app.take-screenshot");
	g_menu_item_set_attribute (item, "accel", "s", "<Primary><Alt>s");
	g_menu_item_set_attribute_value (item, "hidden-when",
					 g_variant_new_string ("action-disabled"));
	g_menu_append_item (G_MENU (menu), item);
	g_object_unref (item);
	item = g_menu_item_new (_("Create Screenshot _Gallery…"), "app.take-gallery");
	g_menu_item_set_attribute_value (item, "hidden-when",
					 g_variant_new_string ("action-disabled"));
	g_menu_append_item (G_MENU (menu), item);
	g_object_unref (item);

	/* Set up a GSettings watch for lockdown keys */
	pi->settings = g_settings_new ("org.gnome.desktop.lockdown");
	g_signal_connect (pi->settings, "changed::disable-save-to-disk", (GCallback) disable_save_to_disk_changed_cb, pi);
	disable_save_to_disk_changed_cb (pi->settings, "disable-save-to-disk", pi);

	/* Update the menu entries' states */
	update_state (pi);
}

static void
impl_deactivate (TotemPluginActivatable *plugin)
{
	TotemScreenshotPlugin *pi = TOTEM_SCREENSHOT_PLUGIN (plugin);
	const char * const accels[] = { NULL };

	/* Disconnect signal handlers */
	g_signal_handler_disconnect (G_OBJECT (pi->bvw), pi->got_metadata_signal);

	gtk_application_set_accels_for_action (GTK_APPLICATION (pi->totem),
					       "app.take-screenshot",
					       accels);

	/* Disconnect from GSettings */
	g_object_unref (pi->settings);

	/* Remove the menu */
	totem_object_empty_menu_section (pi->totem, "screenshot-placeholder");

	g_object_unref (pi->bvw);
}

void
totem_screenshot_plugin_set_file_chooser_folder (GtkFileChooser *chooser)
{
	g_autoptr(GSettings) settings = NULL;
	g_autofree char *path = NULL;

	/* Set the default path */
	settings = g_settings_new (TOTEM_GSETTINGS_SCHEMA);
	path = g_settings_get_string (settings, "screenshot-save-uri");

	/* Default to the Screenshots directory */
	if (*path != '\0')
		gtk_file_chooser_set_current_folder (chooser, path);
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

