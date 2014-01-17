/*
 * Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>
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

#include <icon-helpers.h>

#define THUMB_SEARCH_SIZE     128
#define THUMB_SEARCH_HEIGHT   (THUMB_SEARCH_SIZE / 4 * 3)

typedef enum {
	ICON_BOX = 0,
	ICON_VIDEO,
	ICON_VIDEO_THUMBNAILING,
	ICON_OPTICAL,
	NUM_ICONS
} IconType;

static GdkPixbuf *icons[NUM_ICONS];
static GHashTable *cache_thumbnails; /* key=url, value=GdkPixbuf */

GdkPixbuf *
totem_grilo_get_thumbnail_finish (GAsyncResult  *res,
				  GError       **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);

	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == totem_grilo_get_thumbnail);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_simple_async_result_get_op_res_gpointer (simple);
}

static void
load_thumbnail_cb (GObject *source_object,
		   GAsyncResult *res,
		   gpointer user_data)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	const GFile *file;

	pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
	if (!pixbuf) {
		g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (res);
		return;
	}

	/* Cache it */
	file = g_object_get_data (source_object, "file");
	if (file) {
		g_hash_table_insert (cache_thumbnails,
				     g_file_get_uri (G_FILE (file)),
				     g_object_ref (pixbuf));
	}

	g_simple_async_result_set_op_res_gpointer (simple, pixbuf, NULL);
	g_simple_async_result_complete_in_idle (simple);
	g_object_unref (simple);
}

static void
get_stream_thumbnail_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	GFileInputStream *stream;
	GCancellable *cancellable;
	GError *error = NULL;

	stream = g_file_read_finish (G_FILE (source_object), res, &error);
	if (!stream) {
		g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (res);
		return;
	}

	cancellable = g_object_get_data (G_OBJECT (simple), "cancellable");
	gdk_pixbuf_new_from_stream_at_scale_async (G_INPUT_STREAM (stream),
						   THUMB_SEARCH_SIZE,
						   THUMB_SEARCH_HEIGHT,
						   TRUE,
						   cancellable,
						   load_thumbnail_cb,
						   simple);
	g_object_unref (G_OBJECT (stream));
}

void
totem_grilo_get_thumbnail (GObject             *object,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data)
{
	GSimpleAsyncResult *simple;
	const char *url_thumb = NULL;
	const GdkPixbuf *thumbnail;
	GFile *file;

	if (GRL_IS_MEDIA (object))
		url_thumb = grl_media_get_thumbnail (GRL_MEDIA (object));
	else if (GRL_IS_SOURCE (object)) {
		GIcon *icon;

		icon = grl_source_get_icon (GRL_SOURCE (object));
		if (icon) {
			GFile *file;

			file = g_file_icon_get_file (G_FILE_ICON (icon));
			url_thumb = g_file_get_uri (file);
			g_object_unref (file);
		} else {
			//FIXME
			return;
		}
	}
	g_return_if_fail (url_thumb != NULL);

	simple = g_simple_async_result_new (G_OBJECT (object),
					    callback,
					    user_data,
					    totem_grilo_get_thumbnail);

	/* Check cache */
	thumbnail = g_hash_table_lookup (cache_thumbnails, url_thumb);
	if (thumbnail) {
		g_simple_async_result_set_op_res_gpointer (simple, g_object_ref (G_OBJECT (thumbnail)), NULL);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		return;
	}

	file = g_file_new_for_uri (url_thumb);
	g_object_set_data_full (G_OBJECT (simple), "file", file, g_object_unref);
	g_object_set_data (G_OBJECT (simple), "cancellable", cancellable);
	g_file_read_async (file, G_PRIORITY_DEFAULT, NULL,
			   get_stream_thumbnail_cb, simple);
}

static void
put_pixel (guchar *p)
{
	p[0] = 46;
	p[1] = 52;
	p[2] = 54;
	p[3] = 0xff;
}

static GdkPixbuf *
load_icon (Totem      *totem,
	   const char *name,
	   int         size,
	   gboolean    with_border)
{
	GdkScreen *screen;
	GtkIconTheme *theme;
	GdkPixbuf *icon, *ret;
	guchar *pixels;
	int rowstride;
	int x, y;

	screen = gtk_window_get_screen (totem_object_get_main_window (totem));
	theme = gtk_icon_theme_get_for_screen (screen);
	icon = gtk_icon_theme_load_icon (theme, name, size, 0, NULL);

	ret = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
			      TRUE,
			      8, THUMB_SEARCH_SIZE, THUMB_SEARCH_HEIGHT);
	pixels = gdk_pixbuf_get_pixels (ret);
	rowstride = gdk_pixbuf_get_rowstride (ret);

	/* Clean up */
	gdk_pixbuf_fill (ret, 0x00000000);

	/* Draw a border */
	if (with_border) {
		/* top */
		for (x = 0; x < THUMB_SEARCH_SIZE; x++)
			put_pixel (pixels + x * 4);
		/* bottom */
		for (x = 0; x < THUMB_SEARCH_SIZE; x++)
			put_pixel (pixels + (THUMB_SEARCH_HEIGHT -1) * rowstride + x * 4);
		/* left */
		for (y = 1; y < THUMB_SEARCH_HEIGHT - 1; y++)
			put_pixel (pixels + y * rowstride);
		/* right */
		for (y = 1; y < THUMB_SEARCH_HEIGHT - 1; y++)
			put_pixel (pixels + y * rowstride + (THUMB_SEARCH_SIZE - 1) * 4);
	}

	/* Put the icon in the middle */
	gdk_pixbuf_copy_area (icon, 0, 0,
			      gdk_pixbuf_get_width (icon), gdk_pixbuf_get_height (icon),
			      ret,
			      (THUMB_SEARCH_SIZE - gdk_pixbuf_get_width (icon)) / 2,
			      (THUMB_SEARCH_HEIGHT - gdk_pixbuf_get_height (icon)) / 2);

	g_object_unref (icon);

	return ret;
}

GdkPixbuf *
totem_grilo_get_icon (GrlMedia *media,
		      gboolean *thumbnailing)
{
	*thumbnailing = FALSE;

	if (GRL_IS_MEDIA_BOX (media))
		return g_object_ref (icons[ICON_BOX]);
	else if (GRL_IS_MEDIA_VIDEO (media)) {
		if (g_str_equal (grl_media_get_source (media), "grl-optical-media"))
			return g_object_ref (icons[ICON_OPTICAL]);
		if (grl_media_get_thumbnail (media)) {
			*thumbnailing = TRUE;
			return g_object_ref (icons[ICON_VIDEO_THUMBNAILING]);
		} else {
			return g_object_ref (icons[ICON_VIDEO]);
		}
	}
	return NULL;
}

const GdkPixbuf *
totem_grilo_get_video_icon (void)
{
	return icons[ICON_VIDEO];
}

const GdkPixbuf *
totem_grilo_get_box_icon (void)
{
	return icons[ICON_BOX];
}

const GdkPixbuf *
totem_grilo_get_optical_icon (void)
{
	return icons[ICON_OPTICAL];
}

void
totem_grilo_clear_icons (void)
{
	guint i;

	for (i = 0; i < NUM_ICONS; i++)
		g_clear_object (&icons[i]);

	g_clear_pointer (&cache_thumbnails, g_hash_table_destroy);
}

void
totem_grilo_setup_icons (Totem *totem)
{
	icons[ICON_BOX] = load_icon (totem, "folder-symbolic", THUMB_SEARCH_HEIGHT, FALSE);
	icons[ICON_VIDEO] = load_icon (totem, "folder-videos-symbolic", THUMB_SEARCH_HEIGHT, FALSE);
	icons[ICON_VIDEO_THUMBNAILING] = load_icon (totem, "folder-videos-symbolic", 24, TRUE);
	icons[ICON_OPTICAL] = load_icon (totem, "media-optical-dvd-symbolic", THUMB_SEARCH_HEIGHT, FALSE);

	cache_thumbnails = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  g_object_unref);
}
