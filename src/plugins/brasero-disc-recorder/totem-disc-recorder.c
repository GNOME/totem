/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include "config.h"

#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>
#include <gdk/gdkx.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>
#include <libpeas/peas-activatable.h>

#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/xmlsave.h>

#include "totem-plugin.h"
#include "totem-interface.h"

#define TOTEM_TYPE_DISC_RECORDER_PLUGIN		(totem_disc_recorder_plugin_get_type ())
#define TOTEM_DISC_RECORDER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_DISC_RECORDER_PLUGIN, TotemDiscRecorderPlugin))
#define TOTEM_DISC_RECORDER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), TOTEM_TYPE_DISC_RECORDER_PLUGIN, TotemDiscRecorderPluginClass))
#define TOTEM_IS_DISC_RECORDER_PLUGIN(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), TOTEM_TYPE_DISC_RECORDER_PLUGIN))
#define TOTEM_IS_DISC_RECORDER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), TOTEM_TYPE_DISC_RECORDER_PLUGIN))
#define TOTEM_DISC_RECORDER_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TOTEM_TYPE_DISC_RECORDER_PLUGIN, TotemDiscRecorderPluginClass))

typedef struct {
	TotemObject *totem;

	GSimpleAction *dvd_action;
	GSimpleAction *copy_action;
	GSimpleAction *copy_vcd_action;
} TotemDiscRecorderPluginPrivate;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_DISC_RECORDER_PLUGIN, TotemDiscRecorderPlugin, totem_disc_recorder_plugin)

static gboolean
totem_disc_recorder_plugin_start_burning (TotemDiscRecorderPlugin *pi,
					  const char *path,
					  gboolean copy)
{
	GtkWindow *main_window;
	GdkScreen *screen;
	GdkDisplay *display;
	gchar *command_line;
	GList *uris;
	GAppInfo *info;
	GdkAppLaunchContext *context;
	GError *error = NULL;
	char *xid_arg;

	main_window = totem_object_get_main_window (pi->priv->totem);
	screen = gtk_widget_get_screen (GTK_WIDGET (main_window));
	display = gdk_display_get_default ();

	/* Build a command line to use */
	xid_arg = NULL;
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY (display))
		xid_arg = g_strdup_printf ("-x %d", (int) gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (main_window))));
#endif /* GDK_WINDOWING_X11 */
	g_object_unref (main_window);

	if (copy != FALSE)
		command_line = g_strdup_printf ("brasero %s -c", xid_arg ? xid_arg : "");
	else
		command_line = g_strdup_printf ("brasero %s -r", xid_arg ? xid_arg : "");

	/* Build the app info */
	info = g_app_info_create_from_commandline (command_line, NULL,
	                                           G_APP_INFO_CREATE_SUPPORTS_URIS | G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION, &error);
	g_free (command_line);

	if (error != NULL)
		goto error;

	/* Create a launch context and launch it */
	context = gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (main_window)));
	gdk_app_launch_context_set_screen (context, screen);

	uris = g_list_prepend (NULL, (gpointer) path);
	g_app_info_launch_uris (info, uris, G_APP_LAUNCH_CONTEXT (context), &error);
	g_list_free (uris);

	g_object_unref (info);
	g_object_unref (context);

	if (error != NULL)
		goto error;

	return TRUE;

error:
	main_window = totem_object_get_main_window (pi->priv->totem);

	if (copy != FALSE)
		totem_interface_error (_("The video disc could not be duplicated."), error->message, main_window);
	else
		totem_interface_error (_("The movie could not be recorded."), error->message, main_window);

	g_error_free (error);
	g_object_unref (main_window);

	return FALSE;
}

static char*
totem_disc_recorder_plugin_write_video_project (TotemDiscRecorderPlugin *pi,
						char **error)
{
	xmlTextWriter *project;
	xmlDocPtr doc = NULL;
	xmlSaveCtxt *save;
	xmlChar *escaped;
	gint success;
	char *title, *path, *uri;
	int fd;

	/* get a temporary path */
	path = g_build_filename (g_get_tmp_dir (), "brasero-tmp-project-XXXXXX",  NULL);
	fd = g_mkstemp (path);
	if (!fd) {
		g_free (path);

		*error = g_strdup (_("Unable to write a project."));
		return NULL;
	}

	project = xmlNewTextWriterDoc (&doc, 0);
	if (!project) {
		g_remove (path);
		g_free (path);
		close (fd);

		*error = g_strdup (_("Unable to write a project."));
		return NULL;
	}

	xmlTextWriterSetIndent (project, 1);
	xmlTextWriterSetIndentString (project, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (project,
					      NULL,
					      "UTF8",
					      NULL);
	if (success < 0)
		goto error;

	success = xmlTextWriterStartElement (project, (xmlChar *) "braseroproject");
	if (success < 0)
		goto error;

	/* write the name of the version */
	success = xmlTextWriterWriteElement (project,
					     (xmlChar *) "version",
					     (xmlChar *) "0.2");
	if (success < 0)
		goto error;

	title = totem_object_get_short_title (pi->priv->totem);
	if (title) {
		success = xmlTextWriterWriteElement (project,
						     (xmlChar *) "label",
						     (xmlChar *) title);
		g_free (title);

		if (success < 0)
			goto error;
	}

	success = xmlTextWriterStartElement (project, (xmlChar *) "track");
	if (success < 0)
		goto error;

	success = xmlTextWriterStartElement (project, (xmlChar *) "video");
	if (success < 0)
		goto error;

	uri = totem_object_get_current_mrl (pi->priv->totem);
	escaped = (unsigned char *) g_uri_escape_string (uri, NULL, FALSE);
	g_free (uri);

	success = xmlTextWriterWriteElement (project,
					     (xmlChar *) "uri",
					     escaped);
	g_free (escaped);
	if (success == -1)
		goto error;

	/* start of the song always 0 */
	success = xmlTextWriterWriteElement (project,
					     (xmlChar *) "start",
					     (xmlChar *) "0");
	if (success == -1)
		goto error;

	success = xmlTextWriterEndElement (project); /* video */
	if (success < 0)
		goto error;

	success = xmlTextWriterEndElement (project); /* track */
	if (success < 0)
		goto error;

	success = xmlTextWriterEndElement (project); /* braseroproject */
	if (success < 0)
		goto error;

	xmlTextWriterEndDocument (project);
	xmlFreeTextWriter (project);

	save = xmlSaveToFd (fd, "UTF8", XML_SAVE_FORMAT);
	xmlSaveDoc (save, doc);
	xmlSaveClose (save);

	xmlFreeDoc (doc);
	close (fd);

	return path;

error:

	/* cleanup */
	xmlTextWriterEndDocument (project);
	xmlFreeTextWriter (project);

	g_remove (path);
	g_free (path);
	close (fd);

	*error = g_strdup (_("Unable to write a project."));
	return NULL;
}

static void
totem_disc_recorder_plugin_burn (GAction                 *action,
				 GVariant                *variant,
				 TotemDiscRecorderPlugin *pi)
{
	char *path;
	char *error = NULL;

	path = totem_disc_recorder_plugin_write_video_project (pi, &error);
	if (!path) {
		totem_interface_error (_("The movie could not be recorded."),
				       error,
				       totem_object_get_main_window (pi->priv->totem));
		g_free (error);
		return;
	}

	if (!totem_disc_recorder_plugin_start_burning (pi, path, FALSE))
		g_remove (path);

	g_free (path);
}

static void
totem_disc_recorder_plugin_copy (GAction                 *action,
				 GVariant                *variant,
				 TotemDiscRecorderPlugin *pi)
{
	char *mrl;

	mrl = totem_object_get_current_mrl (pi->priv->totem);
	if (!g_str_has_prefix (mrl, "dvd:") && !g_str_has_prefix (mrl, "vcd:")) {
		g_free (mrl);
		g_assert_not_reached ();
		return;
	}

	totem_disc_recorder_plugin_start_burning (pi, mrl + 6, TRUE);
}

static void
set_menu_items_state (TotemDiscRecorderPlugin *pi,
		      gboolean                 dvd,
		      gboolean                 dvd_copy,
		      gboolean                 vcd_copy)
{
	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->priv->dvd_action), dvd);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->priv->copy_action), dvd_copy);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (pi->priv->copy_vcd_action), vcd_copy);
}

static void
totem_disc_recorder_file_closed (TotemObject *totem,
				 TotemDiscRecorderPlugin *pi)
{
	/* FIXME hide the menu items if necessary:
	 * https://bugzilla.gnome.org/show_bug.cgi?id=688421 */
	set_menu_items_state (pi, FALSE, FALSE, FALSE);
}

static void
totem_disc_recorder_file_opened (TotemObject *totem,
				 const char *mrl,
				 TotemDiscRecorderPlugin *pi)
{
	/* Check if that stream is supported by brasero */
	if (g_str_has_prefix (mrl, "file:")) {
		/* If the file is supported we can always burn, even if there
		 * aren't any burner since we can still create an image. */
		set_menu_items_state (pi, TRUE, FALSE, FALSE);
	} else if (g_str_has_prefix (mrl, "dvd:")) {
		set_menu_items_state (pi, FALSE, TRUE, FALSE);
	} else if (g_str_has_prefix (mrl, "vcd:")) {
		set_menu_items_state (pi, FALSE, FALSE, TRUE);
	} else {
		set_menu_items_state (pi, FALSE, FALSE, FALSE);
	}
}

static void
menu_append_hidden (GMenu      *menu,
		    const char *label,
		    const char *detailed_action)
{
	GMenuItem *item;

	item = g_menu_item_new (label, detailed_action);
	g_menu_item_set_attribute_value (item, "hidden-when",
					 g_variant_new_string ("action-disabled"));
	g_menu_append_item (menu, item);
	g_object_unref (item);
}

static void
impl_activate (PeasActivatable *plugin)
{
	TotemDiscRecorderPlugin *pi = TOTEM_DISC_RECORDER_PLUGIN (plugin);
	TotemDiscRecorderPluginPrivate *priv = pi->priv;
	GMenu *menu;
	char *path;

	/* make sure brasero is in the path */
	path = g_find_program_in_path ("brasero");
	if (!path)
		return;
	g_free (path);

	priv->totem = g_object_get_data (G_OBJECT (plugin), "object");

	g_signal_connect (priv->totem,
			  "file-opened",
			  G_CALLBACK (totem_disc_recorder_file_opened),
			  plugin);
	g_signal_connect (priv->totem,
			  "file-closed",
			  G_CALLBACK (totem_disc_recorder_file_closed),
			  plugin);

	/* Create the actions */
	priv->dvd_action = g_simple_action_new ("media-optical-video-new", NULL);
	g_signal_connect (G_OBJECT (priv->dvd_action), "activate",
			  G_CALLBACK (totem_disc_recorder_plugin_burn), plugin);
	g_action_map_add_action (G_ACTION_MAP (priv->totem), G_ACTION (priv->dvd_action));

	priv->copy_action = g_simple_action_new ("media-optical-copy", NULL);
	g_signal_connect (G_OBJECT (priv->copy_action), "activate",
			  G_CALLBACK (totem_disc_recorder_plugin_copy), plugin);
	g_action_map_add_action (G_ACTION_MAP (priv->totem), G_ACTION (priv->copy_action));

	priv->copy_vcd_action = g_simple_action_new ("media-optical-copy-vcd", NULL);
	g_signal_connect (G_OBJECT (priv->copy_vcd_action), "activate",
			  G_CALLBACK (totem_disc_recorder_plugin_copy), plugin);
	g_action_map_add_action (G_ACTION_MAP (priv->totem), G_ACTION (priv->copy_vcd_action));

	/* Install the menu */
	menu = totem_object_get_menu_section (priv->totem, "burn-placeholder");
	menu_append_hidden (G_MENU (menu), _("_Create Video Disc..."), "app.media-optical-video-new");
	menu_append_hidden (G_MENU (menu), _("Copy Vide_o DVD..."), "app.media-optical-copy");
	menu_append_hidden (G_MENU (menu), _("Copy (S)VCD..."), "app.media-optical-copy-vcd");

	if (!totem_object_is_paused (priv->totem) && !totem_object_is_playing (priv->totem)) {
		set_menu_items_state (pi, FALSE, FALSE, FALSE);
	} else {
		char *mrl;

		mrl = totem_object_get_current_mrl (priv->totem);
		totem_disc_recorder_file_opened (priv->totem, mrl, pi);
		g_free (mrl);
	}
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemDiscRecorderPlugin *pi = TOTEM_DISC_RECORDER_PLUGIN (plugin);
	TotemDiscRecorderPluginPrivate *priv = pi->priv;

	g_signal_handlers_disconnect_by_func (priv->totem, totem_disc_recorder_file_opened, plugin);
	g_signal_handlers_disconnect_by_func (priv->totem, totem_disc_recorder_file_closed, plugin);

	totem_object_empty_menu_section (priv->totem, "burn-placeholder");

	priv->totem = NULL;
}
