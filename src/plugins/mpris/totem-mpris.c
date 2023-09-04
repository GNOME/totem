/*
 * Copyright (C) 2010-2014, 2016, 2020-2021  Jonathan Matthew  <jonathan@d14n.org>
 * Copyright (C) 2022  Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <libpeas.h>
#include "totem-plugin-activatable.h"
#include <string.h>

#include "totem-plugin.h"
#include "totem.h"
#include "mpris-spec.h"

#define TOTEM_TYPE_MPRIS_PLUGIN		(totem_mpris_plugin_get_type ())
#define TOTEM_MPRIS_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), TOTEM_TYPE_MPRIS_PLUGIN, TotemMprisPlugin))

typedef struct {
	PeasExtensionBase parent;

	GDBusConnection *connection;
	GDBusNodeInfo *node_info;
	guint name_own_id;
	guint root_id;
	guint player_id;

	TotemObject *totem;

	GHashTable *player_property_changes;
	gboolean emit_seeked;
	guint property_emit_id;

	char *current_mrl;
	gint64 last_position;

	GHashTable *metadata; /* key: str, value: str */
	guint32 track_number;
} TotemMprisPlugin;

TOTEM_PLUGIN_REGISTER(TOTEM_TYPE_MPRIS_PLUGIN, TotemMprisPlugin, totem_mpris_plugin);

static void
emit_property_changes (TotemMprisPlugin *pi, GHashTable *changes, const char *interface)
{
	GError *error = NULL;
	GVariantBuilder *properties;
	GVariantBuilder *invalidated;
	GVariant *parameters;
	gpointer propname, propvalue;
	GHashTableIter iter;

	properties = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	invalidated = g_variant_builder_new (G_VARIANT_TYPE ("as"));
	g_hash_table_iter_init (&iter, changes);
	while (g_hash_table_iter_next (&iter, &propname, &propvalue)) {
		if (propvalue != NULL) {
			g_variant_builder_add (properties,
					       "{sv}",
					       propname,
					       propvalue);
		} else {
			g_variant_builder_add (invalidated, "s", propname);
		}

	}

	parameters = g_variant_new ("(sa{sv}as)",
				    interface,
				    properties,
				    invalidated);
	g_variant_builder_unref (properties);
	g_variant_builder_unref (invalidated);
	g_dbus_connection_emit_signal (pi->connection,
				       NULL,
				       MPRIS_OBJECT_NAME,
				       "org.freedesktop.DBus.Properties",
				       "PropertiesChanged",
				       parameters,
				       &error);
	if (error != NULL) {
		g_warning ("Unable to send MPRIS property changes for %s: %s",
			   interface, error->message);
		g_clear_error (&error);
	}

}

static gboolean
emit_properties_idle (TotemMprisPlugin *pi)
{
	if (pi->player_property_changes != NULL) {
		emit_property_changes (pi, pi->player_property_changes, MPRIS_PLAYER_INTERFACE);
		g_hash_table_destroy (pi->player_property_changes);
		pi->player_property_changes = NULL;
	}

	if (pi->emit_seeked) {
		GError *error = NULL;
		g_debug ("emitting Seeked; new time %" G_GINT64_FORMAT, pi->last_position/1000);
		g_dbus_connection_emit_signal (pi->connection,
					       NULL,
					       MPRIS_OBJECT_NAME,
					       MPRIS_PLAYER_INTERFACE,
					       "Seeked",
					       g_variant_new ("(x)", pi->last_position / 1000),
					       &error);
		if (error != NULL) {
			g_warning ("Unable to set MPRIS Seeked signal: %s", error->message);
			g_clear_error (&error);
		}
		pi->emit_seeked = 0;
	}
	pi->property_emit_id = 0;
	return FALSE;
}

static void
add_player_property_change (TotemMprisPlugin *pi,
			    const char *property,
			    GVariant *value)
{
	if (pi->player_property_changes == NULL) {
		pi->player_property_changes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
	}
	g_hash_table_insert (pi->player_property_changes, g_strdup (property), g_variant_ref_sink (value));

	if (pi->property_emit_id == 0) {
		pi->property_emit_id = g_idle_add ((GSourceFunc)emit_properties_idle, pi);
	}
}

/* MPRIS root interface */

static void
handle_root_method_call (GDBusConnection *connection,
			 const char *sender,
			 const char *object_path,
			 const char *interface_name,
			 const char *method_name,
			 GVariant *parameters,
			 GDBusMethodInvocation *invocation,
			 TotemMprisPlugin *pi)
{
	if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
	    g_strcmp0 (interface_name, MPRIS_ROOT_INTERFACE) != 0) {
		g_dbus_method_invocation_return_error (invocation,
						       G_DBUS_ERROR,
						       G_DBUS_ERROR_NOT_SUPPORTED,
						       "Method %s.%s not supported",
						       interface_name,
						       method_name);
		return;
	}

	if (g_strcmp0 (method_name, "Raise") == 0) {
		GtkWindow *window = totem_object_get_main_window (pi->totem);
		gtk_window_present (window);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "Quit") == 0) {
		totem_object_exit (pi->totem);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		g_dbus_method_invocation_return_error (invocation,
						       G_DBUS_ERROR,
						       G_DBUS_ERROR_NOT_SUPPORTED,
						       "Method %s.%s not supported",
						       interface_name,
						       method_name);
	}
}

static GVariant *
get_root_property (GDBusConnection *connection,
		   const char *sender,
		   const char *object_path,
		   const char *interface_name,
		   const char *property_name,
		   GError **error,
		   TotemMprisPlugin *pi)
{
	if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
	    g_strcmp0 (interface_name, MPRIS_ROOT_INTERFACE) != 0) {
		g_set_error (error,
			     G_DBUS_ERROR,
			     G_DBUS_ERROR_NOT_SUPPORTED,
			     "Property %s.%s not supported",
			     interface_name,
			     property_name);
		return NULL;
	}

	if (g_strcmp0 (property_name, "CanQuit") == 0) {
		return g_variant_new_boolean (TRUE);
	} else if (g_strcmp0 (property_name, "CanRaise") == 0) {
		return g_variant_new_boolean (TRUE);
	} else if (g_strcmp0 (property_name, "HasTrackList") == 0) {
		return g_variant_new_boolean (FALSE);
	} else if (g_strcmp0 (property_name, "Identity") == 0) {
		return g_variant_new_string ("Videos");
	} else if (g_strcmp0 (property_name, "DesktopEntry") == 0) {
		return g_variant_new_string ("org.gnome.Totem");
	} else if (g_strcmp0 (property_name, "SupportedUriSchemes") == 0) {
		return g_variant_new_strv (totem_object_get_supported_uri_schemes (), -1);
	} else if (g_strcmp0 (property_name, "SupportedMimeTypes") == 0) {
		return g_variant_new_strv (totem_object_get_supported_content_types (), -1);
	}

	g_set_error (error,
		     G_DBUS_ERROR,
		     G_DBUS_ERROR_NOT_SUPPORTED,
		     "Property %s.%s not supported",
		     interface_name,
		     property_name);
	return NULL;
}

static const GDBusInterfaceVTable root_vtable =
{
	(GDBusInterfaceMethodCallFunc) handle_root_method_call,
	(GDBusInterfaceGetPropertyFunc) get_root_property,
	NULL
};

/* MPRIS player interface */

struct {
	const char *property;
	gboolean is_strv;
} str_metadata[] = {
	{ "xesam:title", FALSE },
	{ "xesam:artist", TRUE },
	{ "xesam:album", FALSE }
};

static void
calculate_metadata (TotemMprisPlugin *pi,
		    GVariantBuilder *builder)
{
	guint i;
	gint64 stream_length;

	g_object_get (G_OBJECT (pi->totem), "stream-length", &stream_length, NULL);

	g_variant_builder_add (builder,
			       "{sv}",
			       "mpris:length",
			       g_variant_new_int64 (stream_length * 1000));
	g_variant_builder_add (builder,
			       "{sv}",
			       "xesam:trackNumber",
			       g_variant_new_uint32 (pi->track_number));
	/* See https://gitlab.freedesktop.org/mpris/mpris-spec/-/issues/19 */
	g_variant_builder_add (builder,
			       "{sv}",
			       "mpris:trackid",
			       g_variant_new_object_path ("/org/mpris/MediaPlayer2/TrackList/NoTrack"));
	for (i = 0; i < G_N_ELEMENTS (str_metadata); i++) {
		const char *str;

		str = g_hash_table_lookup (pi->metadata, str_metadata[i].property);
		if (!str)
			continue;
		if (!str_metadata[i].is_strv) {
			g_variant_builder_add (builder,
					       "{sv}",
					       str_metadata[i].property,
					       g_variant_new_string (str));
		} else {
			const char *strv[] = { NULL };

			strv[0] = str;
			g_variant_builder_add (builder,
					       "{sv}",
					       str_metadata[i].property,
					       g_variant_new_strv (strv, 1));
		}
	}
}

static void
handle_player_method_call (GDBusConnection *connection,
			   const char *sender,
			   const char *object_path,
			   const char *interface_name,
			   const char *method_name,
			   GVariant *parameters,
			   GDBusMethodInvocation *invocation,
			   TotemMprisPlugin *pi)

{
	if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
	    g_strcmp0 (interface_name, MPRIS_PLAYER_INTERFACE) != 0) {
		g_dbus_method_invocation_return_error (invocation,
						       G_DBUS_ERROR,
						       G_DBUS_ERROR_NOT_SUPPORTED,
						       "Method %s.%s not supported",
						       interface_name,
						       method_name);
		return;
	}

	if (g_strcmp0 (method_name, "Next") == 0) {
		totem_object_seek_next (pi->totem);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "Previous") == 0) {
		totem_object_seek_previous (pi->totem);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "Pause") == 0) {
		totem_object_pause (pi->totem);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "PlayPause") == 0) {
		totem_object_play_pause (pi->totem);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "Stop") == 0) {
		totem_object_stop (pi->totem);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "Play") == 0) {
		totem_object_play (pi->totem);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "Seek") == 0) {
		gint64 offset;
		g_variant_get (parameters, "(x)", &offset);
		totem_object_seek_relative (pi->totem, offset / 1000, FALSE);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetPosition") == 0) {
		gint64 position, stream_length;
		const char *client_entry_path;

		g_variant_get (parameters, "(&ox)", &client_entry_path, &position);
		position /= 1000;
		g_object_get (G_OBJECT (pi->totem), "stream-length", &stream_length, NULL);

		if (position < 0 || position > stream_length) {
			g_dbus_method_invocation_return_value (invocation, NULL);
			return;
		}

		totem_object_seek_time (pi->totem, position, FALSE);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "OpenUri") == 0) {
		const char *uri;

		g_variant_get (parameters, "(&s)", &uri);
		totem_object_add_to_playlist (pi->totem, uri, NULL, TRUE);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		g_dbus_method_invocation_return_error (invocation,
						       G_DBUS_ERROR,
						       G_DBUS_ERROR_NOT_SUPPORTED,
						       "Method %s.%s not supported",
						       interface_name,
						       method_name);
	}
}

static GVariant *
calculate_playback_status (TotemMprisPlugin *pi)
{
	if (totem_object_is_playing (pi->totem))
		return g_variant_new_string ("Playing");
	else if (totem_object_is_paused (pi->totem))
		return g_variant_new_string ("Paused");
	return g_variant_new_string ("Stopped");
}

static GVariant *
calculate_loop_status (TotemMprisPlugin *pi)
{
	if (totem_object_remote_get_setting (pi->totem, TOTEM_REMOTE_SETTING_REPEAT))
		return g_variant_new_string ("Playlist");
	return g_variant_new_string ("None");
}

static GVariant *
calculate_can_seek (TotemMprisPlugin *pi)
{
	return g_variant_new_boolean (pi->current_mrl != NULL &&
				      totem_object_is_seekable (pi->totem));
}

static GVariant *
get_player_property (GDBusConnection *connection,
		     const char *sender,
		     const char *object_path,
		     const char *interface_name,
		     const char *property_name,
		     GError **error,
		     TotemMprisPlugin *pi)
{
	if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
	    g_strcmp0 (interface_name, MPRIS_PLAYER_INTERFACE) != 0) {
		g_set_error (error,
			     G_DBUS_ERROR,
			     G_DBUS_ERROR_NOT_SUPPORTED,
			     "Property %s.%s not supported",
			     interface_name,
			     property_name);
		return NULL;
	}

	if (g_strcmp0 (property_name, "PlaybackStatus") == 0) {
		return calculate_playback_status (pi);
	} else if (g_strcmp0 (property_name, "LoopStatus") == 0) {
		return calculate_loop_status (pi);
	} else if (g_strcmp0 (property_name, "Rate") == 0) {
		return g_variant_new_double (totem_object_get_rate (pi->totem));
	} else if (g_strcmp0 (property_name, "Metadata") == 0) {
		GVariantBuilder *builder;
		GVariant *v;

		builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
		calculate_metadata (pi, builder);
		v = g_variant_builder_end (builder);
		g_variant_builder_unref (builder);
		return v;
	} else if (g_strcmp0 (property_name, "Volume") == 0) {
		return g_variant_new_double (totem_object_get_volume (pi->totem));
	} else if (g_strcmp0 (property_name, "Position") == 0) {
		return g_variant_new_int64 (totem_object_get_current_time (pi->totem) * 1000);
	} else if (g_strcmp0 (property_name, "MinimumRate") == 0) {
		return g_variant_new_double (0.75);
	} else if (g_strcmp0 (property_name, "MaximumRate") == 0) {
		return g_variant_new_double (1.75);
	} else if (g_strcmp0 (property_name, "CanGoNext") == 0) {
		return g_variant_new_boolean (totem_object_can_seek_next (pi->totem));
	} else if (g_strcmp0 (property_name, "CanGoPrevious") == 0) {
		return g_variant_new_boolean (totem_object_can_seek_previous (pi->totem));
	} else if (g_strcmp0 (property_name, "CanPlay") == 0) {
		return g_variant_new_boolean (pi->current_mrl != NULL);
	} else if (g_strcmp0 (property_name, "CanPause") == 0) {
		return g_variant_new_boolean (pi->current_mrl != NULL);
	} else if (g_strcmp0 (property_name, "CanSeek") == 0) {
		return calculate_can_seek (pi);
	} else if (g_strcmp0 (property_name, "CanControl") == 0) {
		return g_variant_new_boolean (TRUE);
	}

	g_set_error (error,
		     G_DBUS_ERROR,
		     G_DBUS_ERROR_NOT_SUPPORTED,
		     "Property %s.%s not supported",
		     interface_name,
		     property_name);
	return NULL;
}

static gboolean
set_player_property (GDBusConnection *connection,
		     const char *sender,
		     const char *object_path,
		     const char *interface_name,
		     const char *property_name,
		     GVariant *value,
		     GError **error,
		     TotemMprisPlugin *pi)
{
	if (g_strcmp0 (object_path, MPRIS_OBJECT_NAME) != 0 ||
	    g_strcmp0 (interface_name, MPRIS_PLAYER_INTERFACE) != 0) {
		g_set_error (error,
			     G_DBUS_ERROR,
			     G_DBUS_ERROR_NOT_SUPPORTED,
			     "%s:%s not supported",
			     object_path,
			     interface_name);
		return FALSE;
	}

	if (g_strcmp0 (property_name, "LoopStatus") == 0) {
		const char *status;

		status = g_variant_get_string (value, NULL);
		totem_object_remote_set_setting (pi->totem, TOTEM_REMOTE_SETTING_REPEAT,
						 g_strcmp0 (status, "Playlist") == 0);
		return TRUE;
	} else if (g_strcmp0 (property_name, "Rate") == 0) {
		totem_object_set_rate (pi->totem, g_variant_get_double (value));
		return TRUE;
	} else if (g_strcmp0 (property_name, "Volume") == 0) {
		totem_object_set_volume (pi->totem, g_variant_get_double (value));
		return TRUE;
	}

	g_set_error (error,
		     G_DBUS_ERROR,
		     G_DBUS_ERROR_NOT_SUPPORTED,
		     "Property %s.%s not supported",
		     interface_name,
		     property_name);
	return FALSE;
}

static const GDBusInterfaceVTable player_vtable =
{
	(GDBusInterfaceMethodCallFunc) handle_player_method_call,
	(GDBusInterfaceGetPropertyFunc) get_player_property,
	(GDBusInterfaceSetPropertyFunc) set_player_property,
};

static void
playing_changed_cb (TotemObject *totem, GParamSpec *pspec, TotemMprisPlugin *pi)
{
	g_debug ("emitting PlaybackStatus change");
	add_player_property_change (pi, "PlaybackStatus", calculate_playback_status (pi));
}

static void
seekable_changed_cb (TotemObject *totem, GParamSpec *pspec, TotemMprisPlugin *pi)
{
	g_debug ("emitting CanSeek change");
	add_player_property_change (pi, "CanSeek", calculate_can_seek (pi));
}

static void
metadata_updated_cb (TotemObject *totem,
		     const char *artist,
		     const char *title,
		     const char *album,
		     guint32 track_number,
		     TotemMprisPlugin *pi)
{
	GVariantBuilder *builder;

	g_hash_table_insert (pi->metadata, "xesam:artist", g_strdup (artist));
	g_hash_table_insert (pi->metadata, "xesam:title", g_strdup (title));
	g_hash_table_insert (pi->metadata, "xesam:album", g_strdup (album));
	pi->track_number = track_number;

	builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));
	calculate_metadata (pi, builder);
	add_player_property_change (pi, "Metadata", g_variant_builder_end (builder));
	g_variant_builder_unref (builder);
}

static void
time_changed_cb (TotemObject *totem, GParamSpec *pspec, TotemMprisPlugin *pi)
{
	gint64 position;

	position = totem_object_get_current_time (pi->totem);
	/* Only notify of seeks if we've skipped more than 3 seconds */
	if (ABS (position - pi->last_position) < 3) {
		pi->last_position = position;
		return;
	}

	if (pi->property_emit_id == 0) {
		pi->property_emit_id = g_idle_add ((GSourceFunc)emit_properties_idle, pi);
	}
	pi->emit_seeked = TRUE;
	pi->last_position = position;
}

static void
mrl_changed_cb (TotemObject *totem, GParamSpec *pspec, TotemMprisPlugin *pi)
{
	g_clear_pointer (&pi->current_mrl, g_free);
	pi->current_mrl = totem_object_get_current_mrl (totem);

	add_player_property_change (pi, "CanPlay",
				    g_variant_new_boolean (pi->current_mrl != NULL));
	add_player_property_change (pi, "CanPause",
				    g_variant_new_boolean (pi->current_mrl != NULL));
	add_player_property_change (pi, "CanSeek", calculate_can_seek (pi));
	add_player_property_change (pi, "CanGoNext",
				    g_variant_new_boolean (totem_object_can_seek_next (pi->totem)));
	add_player_property_change (pi, "CanGoPrevious",
				    g_variant_new_boolean (totem_object_can_seek_previous (pi->totem)));
}

static void
name_acquired_cb (GDBusConnection *connection, const char *name, TotemMprisPlugin *pi)
{
	g_debug ("successfully acquired dbus name %s", name);
}

static void
name_lost_cb (GDBusConnection *connection, const char *name, TotemMprisPlugin *pi)
{
	g_debug ("lost dbus name %s", name);
}

static void
impl_activate (TotemPluginActivatable *plugin)
{
	TotemMprisPlugin *pi = TOTEM_MPRIS_PLUGIN (plugin);
	GDBusInterfaceInfo *ifaceinfo;
	g_autoptr(GError) error = NULL;

	pi->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (!pi->connection) {
		g_warning ("Unable to connect to D-Bus session bus: %s", error->message);
		return;
	}

	pi->node_info = g_dbus_node_info_new_for_xml (mpris_introspection_xml, &error);
	if (error != NULL) {
		g_warning ("Unable to read MPRIS interface specificiation: %s", error->message);
		return;
	}

	/* register root interface */
	ifaceinfo = g_dbus_node_info_lookup_interface (pi->node_info, MPRIS_ROOT_INTERFACE);
	pi->root_id = g_dbus_connection_register_object (pi->connection,
							 MPRIS_OBJECT_NAME,
							 ifaceinfo,
							 &root_vtable,
							 plugin,
							 NULL,
							 &error);
	if (error != NULL) {
		g_warning ("unable to register MPRIS root interface: %s", error->message);
		g_clear_error (&error);
	}

	/* register player interface */
	ifaceinfo = g_dbus_node_info_lookup_interface (pi->node_info, MPRIS_PLAYER_INTERFACE);
	pi->player_id = g_dbus_connection_register_object (pi->connection,
							   MPRIS_OBJECT_NAME,
							   ifaceinfo,
							   &player_vtable,
							   plugin,
							   NULL,
							   &error);
	if (error != NULL) {
		g_warning ("Unable to register MPRIS player interface: %s", error->message);
		g_clear_error (&error);
	}

	pi->totem = g_object_get_data (G_OBJECT (plugin), "object");

	/* connect signal handlers for stuff */
	g_signal_connect_object (pi->totem,
				 "metadata-updated",
				 G_CALLBACK (metadata_updated_cb),
				 plugin, 0);
	g_signal_connect_object (pi->totem,
				 "notify::playing",
				 G_CALLBACK (playing_changed_cb),
				 plugin, 0);
	g_signal_connect_object (pi->totem,
				 "notify::seekable",
				 G_CALLBACK (seekable_changed_cb),
				 plugin, 0);
	g_signal_connect_object (pi->totem,
				 "notify::current-mrl",
				 G_CALLBACK (mrl_changed_cb),
				 plugin, 0);
	g_signal_connect_object (pi->totem,
				 "notify::current-time",
				 G_CALLBACK (time_changed_cb),
				 plugin, 0);

	pi->name_own_id = g_bus_own_name (G_BUS_TYPE_SESSION,
					  MPRIS_BUS_NAME_PREFIX ".totem",
					  G_BUS_NAME_OWNER_FLAGS_NONE,
					  NULL,
					  (GBusNameAcquiredCallback) name_acquired_cb,
					  (GBusNameLostCallback) name_lost_cb,
					  g_object_ref (pi),
					  g_object_unref);

	pi->metadata = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
	pi->current_mrl = totem_object_get_current_mrl (pi->totem);
}

static void
impl_deactivate (TotemPluginActivatable *plugin)
{
	TotemMprisPlugin *pi = TOTEM_MPRIS_PLUGIN (plugin);
	TotemObject *totem;

	if (pi->root_id != 0) {
		g_dbus_connection_unregister_object (pi->connection, pi->root_id);
		pi->root_id = 0;
	}
	if (pi->player_id != 0) {
		g_dbus_connection_unregister_object (pi->connection, pi->player_id);
		pi->player_id = 0;
	}

	g_clear_handle_id (&pi->property_emit_id, g_source_remove);
	g_clear_pointer (&pi->player_property_changes, g_hash_table_destroy);
	g_clear_pointer (&pi->current_mrl, g_free);
	g_clear_pointer (&pi->metadata, g_hash_table_destroy);

	totem = g_object_get_data (G_OBJECT (plugin), "object");
	if (totem != NULL) {
		g_signal_handlers_disconnect_by_func (totem,
						      G_CALLBACK (metadata_updated_cb),
						      plugin);
		g_signal_handlers_disconnect_by_func (totem,
						      G_CALLBACK (playing_changed_cb),
						      plugin);
		g_signal_handlers_disconnect_by_func (totem,
						      G_CALLBACK (seekable_changed_cb),
						      plugin);
		g_signal_handlers_disconnect_by_func (totem,
						      G_CALLBACK (mrl_changed_cb),
						      plugin);
		g_signal_handlers_disconnect_by_func (totem,
						      G_CALLBACK (time_changed_cb),
						      plugin);
	}
	g_clear_handle_id (&pi->name_own_id, g_bus_unown_name);
	g_clear_pointer (&pi->node_info, g_dbus_node_info_unref);
	g_clear_object (&pi->connection);
}
