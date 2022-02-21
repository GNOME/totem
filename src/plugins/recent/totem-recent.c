/*
 *  Copyright (C) 2012 Bastien Nocera <hadess@hadess.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
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


#include "config.h"

#include <glib-object.h>
#include <string.h>

#include "totem-plugin.h"
#include "totem.h"

#define TOTEM_TYPE_RECENT_PLUGIN	(totem_recent_plugin_get_type ())
#define TOTEM_RECENT_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_RECENT_PLUGIN, TotemRecentPlugin))

typedef struct {
	PeasExtensionBase parent;

	guint signal_id;
	TotemObject *totem;
	GtkRecentManager *recent_manager;
} TotemRecentPlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_RECENT_PLUGIN, TotemRecentPlugin, totem_recent_plugin)

static void
recent_info_cb (GFile *file,
		GAsyncResult *res,
		TotemRecentPlugin *pi)
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
		if (!data.mime_type)
			data.mime_type = g_strdup ("video/x-totem-stream");
		data.display_name = g_strdup (g_file_info_get_display_name (file_info));
		g_object_unref (file_info);
		groups[0] = (gchar*) "Totem";
	}

	data.app_name = g_strdup (g_get_application_name ());
	data.app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
	data.groups = groups;
	if (gtk_recent_manager_add_full (pi->recent_manager,
					 uri, &data) == FALSE) {
		g_warning ("Couldn't add recent file for '%s'", uri);
	}

	g_free (data.display_name);
	g_free (data.mime_type);
	g_free (data.app_name);
	g_free (data.app_exec);

	g_object_unref (file);
}

static void
add_recent (TotemRecentPlugin *pi,
	    const char *uri,
	    const char *display_name,
	    const char *content_type)
{
	GFile *file;

	/* FIXME implement
	if (totem_is_special_mrl (uri) != FALSE)
		return; */

	/* If we already have a content-type, the display_name is
	 * probably decent as well */
	if (content_type != NULL) {
		GtkRecentData data;
		char *groups[] = { NULL, NULL };

		memset (&data, 0, sizeof (data));

		data.mime_type = (char *) content_type;
		data.display_name = (char *) display_name;
		groups[0] = (char*) "Totem";
		data.app_name = (char *) g_get_application_name ();
		data.app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
		data.groups = groups;

		if (gtk_recent_manager_add_full (pi->recent_manager,
						 uri, &data) == FALSE) {
			g_warning ("Couldn't add recent file for '%s'", uri);
		}
		g_free (data.app_exec);

		return;
	}

	file = g_file_new_for_uri (uri);
	g_object_set_data_full (G_OBJECT (file), "uri", g_strdup (uri), g_free);
	g_object_set_data_full (G_OBJECT (file), "display_name", g_strdup (display_name), g_free);
	g_file_query_info_async (file,
				 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
				 G_FILE_QUERY_INFO_NONE, 0, NULL, (GAsyncReadyCallback) recent_info_cb, pi);
}

static void
file_has_played_cb (TotemObject       *totem,
		    const char        *mrl,
		    TotemRecentPlugin *pi)
{
	char *content_type;
	char *display_name;

	g_object_get (G_OBJECT (totem),
		      "current-display-name", &display_name,
		      "current-content-type", &content_type,
		      NULL);

	add_recent (pi, mrl, display_name, content_type);

	g_free (display_name);
	g_free (content_type);
}

static void
impl_activate (PeasActivatable *plugin)
{
	TotemRecentPlugin *pi = TOTEM_RECENT_PLUGIN (plugin);

	pi->totem = g_object_ref (g_object_get_data (G_OBJECT (plugin), "object"));
	pi->recent_manager = gtk_recent_manager_get_default ();
	pi->signal_id = g_signal_connect (G_OBJECT (pi->totem), "file-has-played",
						G_CALLBACK (file_has_played_cb), pi);
}

static void
impl_deactivate (PeasActivatable *plugin)
{
	TotemRecentPlugin *pi = TOTEM_RECENT_PLUGIN (plugin);

	if (pi->signal_id) {
		g_signal_handler_disconnect (pi->totem, pi->signal_id);
		pi->signal_id = 0;
	}

	if (pi->totem) {
		g_object_unref (pi->totem);
		pi->totem = NULL;
	}
}
