/* totem-missing-plugins.c

   Copyright (C) 2007 Tim-Philipp Müller <tim centricular net>

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Tim-Philipp Müller <tim centricular net>
 */

#include "config.h"

#include "totem-missing-plugins.h"

#ifdef ENABLE_MISSING_PLUGIN_INSTALLATION

#include "totem-private.h"
#include "bacon-video-widget.h"

#include <gst/pbutils/pbutils.h>
#include <gst/pbutils/install-plugins.h>

#include <gst/gst.h> /* for gst_registry_update */

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (_totem_gst_debug_cat);
#define GST_CAT_DEFAULT _totem_gst_debug_cat

/* list of blacklisted detail strings */
static GList *blacklisted_plugins = NULL;

typedef struct
{
	gboolean   playing;
	gchar    **descriptions;
	gchar    **details;
	Totem     *totem;
}
TotemCodecInstallContext;

static gboolean
totem_codec_install_plugin_is_blacklisted (const gchar * detail)
{
	GList *res;

	res = g_list_find_custom (blacklisted_plugins,
	                          detail,
	                          (GCompareFunc) strcmp);

	return (res != NULL);	
}

static void
totem_codec_install_blacklist_plugin (const gchar * detail)
{
	if (!totem_codec_install_plugin_is_blacklisted (detail))
	{
		blacklisted_plugins = g_list_prepend (blacklisted_plugins,
		                                      g_strdup (detail));
	}
}

static void
totem_codec_install_context_free (TotemCodecInstallContext *ctx)
{
	g_strfreev (ctx->descriptions);
	g_strfreev (ctx->details);
	g_free (ctx);
}

static void
on_plugin_installation_done (GstInstallPluginsReturn res, gpointer user_data)
{
	TotemCodecInstallContext *ctx = (TotemCodecInstallContext *) user_data;
	gchar **p;

	GST_INFO ("res = %d (%s)", res, gst_install_plugins_return_get_name (res));

	switch (res)
	{
		/* treat partial success the same as success; in the worst case we'll
		 * just do another round and get NOT_FOUND as result that time */
		case GST_INSTALL_PLUGINS_PARTIAL_SUCCESS:
		case GST_INSTALL_PLUGINS_SUCCESS:
			{
				/* blacklist installed plugins too, so that we don't get
				 * into endless installer loops in case of inconsistencies */
				for (p = ctx->details; p != NULL && *p != NULL; ++p)
					totem_codec_install_blacklist_plugin (*p);

				bacon_video_widget_stop (ctx->totem->bvw);
				g_message ("Missing plugins installed. Updating plugin registry ...");

				/* force GStreamer to re-read its plugin registry */
				if (gst_update_registry ())
				{
					g_message ("Plugin registry updated, trying again.");
					bacon_video_widget_play (ctx->totem->bvw, NULL);
				} else {
					g_warning ("GStreamer registry update failed");
					/* FIXME: should we show an error message here? */
				}
			}
			break;
		case GST_INSTALL_PLUGINS_NOT_FOUND:
			{
				g_message ("No installation candidate for missing plugins found.");

				/* NOT_FOUND should only be returned if not a single one of the
				 * requested plugins was found; if we managed to play something
				 * anyway, we should just continue playing what we have and
				 * blacklist the requested plugins for this session; if we
				 * could not play anything we should blacklist them as well,
				 * so the install wizard isn't called again for nothing */
				for (p = ctx->details; p != NULL && *p != NULL; ++p)
					totem_codec_install_blacklist_plugin (*p);

				if (ctx->playing)
				{
					bacon_video_widget_play (ctx->totem->bvw, NULL);
				} else {
					/* nothing we can do, user already saw error from wizard */
					bacon_video_widget_stop (ctx->totem->bvw);
				}
			}
			break;
		case GST_INSTALL_PLUGINS_USER_ABORT:
			{
				if (ctx->playing)
					bacon_video_widget_play (ctx->totem->bvw, NULL);
				else
					bacon_video_widget_stop (ctx->totem->bvw);
			}
			break;
		case GST_INSTALL_PLUGINS_ERROR:
		case GST_INSTALL_PLUGINS_CRASHED:
		default:
			{
				g_message ("Missing plugin installation failed: %s",
				           gst_install_plugins_return_get_name (res));

				if (ctx->playing)
					bacon_video_widget_play (ctx->totem->bvw, NULL);
				else
					bacon_video_widget_stop (ctx->totem->bvw);
				break;
			}
	}

	totem_codec_install_context_free (ctx);
}

static gboolean
totem_on_missing_plugins_event (BaconVideoWidget *bvw, char **details,
                                char **descriptions, gboolean playing,
                                Totem *totem)
{
	GstInstallPluginsContext *install_ctx;
	TotemCodecInstallContext *ctx;
	GstInstallPluginsReturn status;
	guint i, num;

	num = g_strv_length (details);
	g_return_val_if_fail (num > 0 && g_strv_length (descriptions) == num, FALSE);

	ctx = g_new0 (TotemCodecInstallContext, 1);
	ctx->descriptions = g_strdupv (descriptions);
	ctx->details = g_strdupv (details);
	ctx->playing = playing;
	ctx->totem = totem;

	for (i = 0; i < num; ++i)
	{
		if (totem_codec_install_plugin_is_blacklisted (ctx->details[i]))
		{
			g_message ("Missing plugin: %s (ignoring)", ctx->details[i]);
			g_free (ctx->details[i]);
			g_free (ctx->descriptions[i]);
			ctx->details[i] = ctx->details[num-1];
			ctx->descriptions[i] = ctx->descriptions[num-1];
			ctx->details[num-1] = NULL;
			ctx->descriptions[num-1] = NULL;
			--num;
			--i;
		} else {
			g_message ("Missing plugin: %s (%s)", ctx->details[i], ctx->descriptions[i]);
		}
	}

	if (num == 0)
	{
		g_message ("All missing plugins are blacklisted, doing nothing");
		totem_codec_install_context_free (ctx);
		return FALSE;
	}

	install_ctx = gst_install_plugins_context_new ();

#ifdef GDK_WINDOWING_X11
	if (totem->win != NULL && GTK_WIDGET_REALIZED (totem->win))
	{
		gulong xid = 0;

		xid = GDK_WINDOW_XWINDOW (GTK_WIDGET (totem->win)->window);
		gst_install_plugins_context_set_xid (install_ctx, xid);
	}
#endif

	status = gst_install_plugins_async (ctx->details, install_ctx,
	                                    on_plugin_installation_done,
	                                    ctx);

	gst_install_plugins_context_free (install_ctx);

	GST_INFO ("gst_install_plugins_async() result = %d", status);

	if (status != GST_INSTALL_PLUGINS_STARTED_OK)
	{
		if (status == GST_INSTALL_PLUGINS_HELPER_MISSING)
		{
			g_message ("Automatic missing codec installation not supported "
			           "(helper script missing)");
		} else {
			g_warning ("Failed to start codec installation: %s",
			           gst_install_plugins_return_get_name (status));
		}
		totem_codec_install_context_free (ctx);
		return FALSE;
	}

	/* if we managed to start playing, pause playback, since some install
	 * wizard should now take over in a second anyway and the user might not
	 * be able to use totem's controls while the wizard is running */
	if (playing)
		bacon_video_widget_pause (bvw);

	return TRUE;
}

#endif /* ENABLE_MISSING_PLUGIN_INSTALLATION */

void
totem_missing_plugins_setup (Totem *totem)
{
#ifdef ENABLE_MISSING_PLUGIN_INSTALLATION
	g_signal_connect (G_OBJECT (totem->bvw),
			"missing-plugins",
			G_CALLBACK (totem_on_missing_plugins_event),
			totem);

	gst_pb_utils_init ();

	GST_INFO ("Set up support for automatic missing plugin installation");
#endif
}
