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

#define GNOME_DESKTOP_USE_UNSTABLE_API 1
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#define DEFAULT_MAX_THREADS   1
#define THUMB_SEARCH_SIZE     256
#define THUMB_SEARCH_HEIGHT   (THUMB_SEARCH_SIZE / 4 * 3)

typedef enum {
	ICON_BOX = 0,
	ICON_VIDEO,
	ICON_VIDEO_THUMBNAILING,
	ICON_OPTICAL,
	NUM_ICONS
} IconType;

static GnomeDesktopThumbnailFactory *factory;
static GThreadPool *thumbnail_pool;
static GdkPixbuf *icons[NUM_ICONS];
static GHashTable *cache_thumbnails; /* key=url, value=GdkPixbuf */

static gboolean
media_is_local (GrlMedia *media)
{
	const char *id;

	id = grl_media_get_source (media);
	if (g_strcmp0 (id, "grl-tracker-source") == 0 ||
	    g_strcmp0 (id, "grl-filesystem") == 0 ||
	    g_strcmp0 (id, "grl-bookmarks") == 0)
		return TRUE;
	return FALSE;
}

GdkPixbuf *
totem_grilo_get_thumbnail_finish (GObject       *source_object,
				  GAsyncResult  *res,
				  GError       **error)
{
	g_return_val_if_fail (g_task_is_valid (res, source_object), NULL);

	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
load_thumbnail_cb (GObject *source_object,
		   GAsyncResult *res,
		   gpointer user_data)
{
	GTask *task = user_data;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	const GFile *file;

	pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
	if (!pixbuf) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	/* Cache it */
	file = g_task_get_task_data (task);
	if (file) {
		g_hash_table_insert (cache_thumbnails,
				     g_file_get_uri (G_FILE (file)),
				     g_object_ref (pixbuf));
	}

	g_task_return_pointer (task, pixbuf, g_object_unref);
	g_object_unref (task);
}

static void
get_stream_thumbnail_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GTask *task = user_data;
	GFileInputStream *stream;
	GError *error = NULL;

	stream = g_file_read_finish (G_FILE (source_object), res, &error);
	if (!stream) {
		g_task_return_error (task, error);
		g_object_unref (task);
		return;
	}

	gdk_pixbuf_new_from_stream_at_scale_async (G_INPUT_STREAM (stream),
						   THUMB_SEARCH_SIZE,
						   THUMB_SEARCH_HEIGHT,
						   TRUE,
						   g_task_get_cancellable (task),
						   load_thumbnail_cb,
						   task);
	g_object_unref (G_OBJECT (stream));
}

static void
save_bookmark_thumbnail (GrlMedia *media,
			 const char *uri)
{
	char *thumb_path, *thumb_url;
	GrlRegistry *registry;
	GrlSource *source;

	if (g_strcmp0 (grl_media_get_source (media), "grl-bookmarks") != 0)
		return;

	thumb_path = gnome_desktop_thumbnail_path_for_uri (uri, GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
	thumb_url = g_filename_to_uri (thumb_path, NULL, NULL);
	g_free (thumb_path);
	grl_media_set_thumbnail (media, thumb_url);
	g_free (thumb_url);

	registry = grl_registry_get_default ();
	source = grl_registry_lookup_source (registry, "grl-bookmarks");
	grl_source_store_sync (source,
			       NULL,
			       media,
			       GRL_WRITE_NORMAL,
			       NULL);
}

static void
thumbnail_media_async_thread (GTask    *task,
			      gpointer  user_data)
{
	GrlMedia *media;
	GdkPixbuf *pixbuf;
	const char *uri;
	GDateTime *mtime;

	if (g_task_return_error_if_cancelled (task)) {
		g_object_unref (task);
		return;
	}

	media = GRL_MEDIA (g_task_get_source_object (task));
	uri = grl_media_get_url (media);

	mtime = grl_media_get_modification_date (media);
	if (!mtime) {
		GrlRegistry *registry;
		GrlKeyID key_id;

		registry = grl_registry_get_default ();
		key_id = grl_registry_lookup_metadata_key (registry, "bookmark-date");
		mtime = grl_data_get_boxed (GRL_DATA (media), key_id);
	}

	if (!uri || !mtime) {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "URI or mtime missing");
		g_object_unref (task);
		return;
	}

	pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (factory, uri, "video/x-totem-stream");

	if (!pixbuf) {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Thumbnailing failed");
		g_object_unref (task);
		return;
	}

	gnome_desktop_thumbnail_factory_save_thumbnail (factory, pixbuf, uri, g_date_time_to_unix (mtime));

	/* Save the thumbnail URL for the bookmarks source */
	save_bookmark_thumbnail (media, uri);

	g_task_return_pointer (task, pixbuf, g_object_unref);
	g_object_unref (task);
}

static void
totem_grilo_thumbnail_media (GrlMedia            *media,
			     GCancellable        *cancellable,
			     GAsyncReadyCallback  callback,
			     gpointer             user_data)
{
	GTask *task;

	task = g_task_new (media, cancellable, callback, user_data);
	g_task_set_priority (task, G_PRIORITY_LOW);
	g_thread_pool_push (thumbnail_pool, task, NULL);
}

static GdkPixbuf *
totem_grilo_thumbnail_media_finish (GrlMedia      *media,
				    GAsyncResult  *res,
				    GError       **error)
{
	g_return_val_if_fail (g_task_is_valid (res, media), NULL);

	return g_task_propagate_pointer (G_TASK (res), error);
}

static void
thumbnail_media_cb (GObject      *source_object,
		    GAsyncResult *res,
		    gpointer      user_data)
{
	GTask *task = user_data;
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	pixbuf = totem_grilo_thumbnail_media_finish (GRL_MEDIA (source_object), res, &error);
	if (!pixbuf)
		g_task_return_error (task, error);
	else
		g_task_return_pointer (task, pixbuf, g_object_unref);
	g_object_unref (task);
}

void
totem_grilo_get_thumbnail (GObject             *object,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data)
{
	GTask *task;
	const char *url_thumb = NULL;
	const GdkPixbuf *thumbnail = NULL;
	GFile *file;

	task = g_task_new (G_OBJECT (object),
			   cancellable,
			   callback,
			   user_data);

	if (GRL_IS_MEDIA (object)) {
		url_thumb = grl_media_get_thumbnail (GRL_MEDIA (object));
		if (!url_thumb && media_is_local (GRL_MEDIA (object))) {
			totem_grilo_thumbnail_media (GRL_MEDIA (object),
						     cancellable,
						     thumbnail_media_cb,
						     task);
			return;
		}
	} else if (GRL_IS_SOURCE (object)) {
		GIcon *icon;

		icon = grl_source_get_icon (GRL_SOURCE (object));
		if (icon) {
			GFile *file;

			file = g_file_icon_get_file (G_FILE_ICON (icon));
			url_thumb = g_file_get_uri (file);
			g_object_unref (file);
		}
	}
	if (url_thumb == NULL) {
		g_task_return_pointer (task, NULL, NULL);
		g_object_unref (task);
		return;
	}

	/* Check cache */
	thumbnail = g_hash_table_lookup (cache_thumbnails, url_thumb);
	if (thumbnail) {
		g_task_return_pointer (task,
				       g_object_ref (G_OBJECT (thumbnail)),
				       g_object_unref);
		g_object_unref (task);
		return;
	}

	file = g_file_new_for_uri (url_thumb);
	g_task_set_task_data (task, file, g_object_unref);
	g_file_read_async (file, G_PRIORITY_DEFAULT, cancellable,
			   get_stream_thumbnail_cb, task);
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
	GApplication *app;
	GdkScreen *screen;
	GIcon *icon;
	GList *windows;
	GtkIconInfo *info;
	GtkIconTheme *theme;
	GtkStyleContext *context;
	GdkPixbuf *pixbuf, *ret;
	guchar *pixels;
	int rowstride;
	int x, y;

	app = g_application_get_default ();
	windows = gtk_application_get_windows (GTK_APPLICATION (app));
	if (windows == NULL)
		return NULL;

	icon = g_themed_icon_new (name);
	screen = gdk_screen_get_default ();
	theme = gtk_icon_theme_get_for_screen (screen);
	info = gtk_icon_theme_lookup_by_gicon (theme, icon, size, GTK_ICON_LOOKUP_FORCE_SYMBOLIC);
	context = gtk_widget_get_style_context (GTK_WIDGET (windows->data));
	pixbuf = gtk_icon_info_load_symbolic_for_context (info, context, NULL, NULL);

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
	gdk_pixbuf_copy_area (pixbuf, 0, 0,
			      gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf),
			      ret,
			      (THUMB_SEARCH_SIZE - gdk_pixbuf_get_width (pixbuf)) / 2,
			      (THUMB_SEARCH_HEIGHT - gdk_pixbuf_get_height (pixbuf)) / 2);

	g_object_unref (pixbuf);
	g_object_unref (info);
	g_object_unref (icon);

	return ret;
}

GdkPixbuf *
totem_grilo_get_icon (GrlMedia *media,
		      gboolean *thumbnailing)
{
	*thumbnailing = FALSE;

	if (GRL_IS_MEDIA_BOX (media)) {
		return g_object_ref (icons[ICON_BOX]);
	} else {
		if (grl_media_get_thumbnail (media) ||
		    media_is_local (media)) {
			*thumbnailing = TRUE;
			return g_object_ref (icons[ICON_VIDEO_THUMBNAILING]);
		} else {
			if (g_str_equal (grl_media_get_source (media), "grl-optical-media"))
				return g_object_ref (icons[ICON_OPTICAL]);
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
	g_clear_object (&factory);
	g_thread_pool_free (thumbnail_pool, TRUE, FALSE);
	thumbnail_pool = NULL;
}

void
totem_grilo_setup_icons (Totem *totem)
{
	icons[ICON_BOX] = load_icon (totem, "folder-symbolic", THUMB_SEARCH_HEIGHT, FALSE);
	icons[ICON_VIDEO] = load_icon (totem, "folder-videos-symbolic", 16, TRUE);
	icons[ICON_VIDEO_THUMBNAILING] = load_icon (totem, "content-loading-symbolic", 16, TRUE);
	icons[ICON_OPTICAL] = load_icon (totem, "media-optical-dvd-symbolic", THUMB_SEARCH_HEIGHT, FALSE);

	cache_thumbnails = g_hash_table_new_full (g_str_hash,
						  g_str_equal,
						  g_free,
						  g_object_unref);

	factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
	thumbnail_pool = g_thread_pool_new ((GFunc) thumbnail_media_async_thread, NULL, DEFAULT_MAX_THREADS, TRUE, NULL);
}

void
totem_grilo_pause_icon_thumbnailing (void)
{
	g_return_if_fail (thumbnail_pool != NULL);
	g_thread_pool_set_max_threads (thumbnail_pool, 0, NULL);
}

void
totem_grilo_resume_icon_thumbnailing (void)
{
	g_return_if_fail (thumbnail_pool != NULL);
	g_thread_pool_set_max_threads (thumbnail_pool, DEFAULT_MAX_THREADS, NULL);
}
