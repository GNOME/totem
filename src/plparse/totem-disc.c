/* Totem Disc Content Detection
 * (c) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission are above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add excemption clause.
 * See license_change file for details.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>

#ifdef HAVE_HAL
#include <libhal.h>
#include <dbus/dbus.h>
#endif

#include "totem-disc.h"

typedef struct _CdCache {
  /* device node and mountpoint */
  char *device, *mountpoint;
  GnomeVFSDrive *drive;

#ifdef HAVE_HAL
  LibHalContext *ctx;
  /* If the disc is a media, have the UDI available here */
  char *disc_udi;
#endif

  /* Whether we have a medium */
  guint has_medium : 1;
  /* if we're checking a media, or a dir */
  guint is_media : 1;

  /* indicates if we mounted this mountpoint ourselves or if it
   * was already mounted. */
  guint self_mounted : 1;
  guint mounted : 1;
} CdCache;

/* 
 * Resolve relative paths
 */

/* Copied from gtk+/gtk/gtkfilesystemunix.c */

/* If this was a publically exported function, it should return
 * a dup'ed result, but we make it modify-in-place for efficiency
 * here, and because it works for us.
 */
static void
canonicalize_filename (gchar *filename)
{
  gchar *p, *q;
  gboolean last_was_slash = FALSE;

  p = filename;
  q = filename;

  while (*p)
    {
      if (*p == G_DIR_SEPARATOR)
	{
	  if (!last_was_slash)
	    *q++ = G_DIR_SEPARATOR;

	  last_was_slash = TRUE;
	}
      else
	{
	  if (last_was_slash && *p == '.')
	    {
	      if (*(p + 1) == G_DIR_SEPARATOR ||
		  *(p + 1) == '\0')
		{
		  if (*(p + 1) == '\0')
		    break;

		  p += 1;
		}
	      else if (*(p + 1) == '.' &&
		       (*(p + 2) == G_DIR_SEPARATOR ||
			*(p + 2) == '\0'))
		{
		  if (q > filename + 1)
		    {
		      q--;
		      while (q > filename + 1 &&
			     *(q - 1) != G_DIR_SEPARATOR)
			q--;
		    }

		  if (*(p + 2) == '\0')
		    break;

		  p += 2;
		}
	      else
		{
		  *q++ = *p;
		  last_was_slash = FALSE;
		}
	    }
	  else
	    {
	      *q++ = *p;
	      last_was_slash = FALSE;
	    }
	}

      p++;
    }

  if (q > filename + 1 && *(q - 1) == G_DIR_SEPARATOR)
    q--;

  *q = '\0';
}

static char *
totem_resolve_symlink (const char *device, GError **error)
{
  char *dir, *link;
  char *f;
  char *f1;

  f = g_strdup (device);
  while (g_file_test (f, G_FILE_TEST_IS_SYMLINK)) {
    link = g_file_read_link (f, error);
    if(link == NULL) {
      g_free (f);
      return NULL;
    }

    dir = g_path_get_dirname (f);
    f1 = g_build_filename (dir, link, NULL);
    g_free (dir);
    g_free (f);
    f = f1;
  }

  if (f != NULL)
    canonicalize_filename (f);
  return f;
}

static gboolean
cd_cache_get_dev_from_volumes (GnomeVFSVolumeMonitor *mon, const char *device,
			      char **mountpoint)
{
  gboolean found;
  GnomeVFSVolume *volume = NULL;
  GList *list, *or;

  found = FALSE;

  for (or = list = gnome_vfs_volume_monitor_get_mounted_volumes (mon);
       list != NULL; list = list->next) {
    char *pdev, *pdev2;

    volume = list->data;
    if (!(pdev = gnome_vfs_volume_get_device_path (volume)))
      continue;
    pdev2 = totem_resolve_symlink (pdev, NULL);
    if (!pdev2) {
      g_free (pdev);
      continue;
    }
    g_free (pdev);

    if (strcmp (pdev2, device) == 0) {
      char *mnt;

      mnt = gnome_vfs_volume_get_activation_uri (volume);
      if (mnt && strncmp (mnt, "file://", 7) == 0) {
	g_free (pdev2);
        *mountpoint = g_strdup (mnt + 7);
        g_free (mnt);
	found = TRUE;
        break;
      } else if (mnt && strncmp (mnt, "cdda://", 7) == 0) {
	g_free (pdev2);
	*mountpoint = NULL;
	g_free (mnt);
	found = TRUE;
	break;
      }
      g_free (mnt);
    }
    g_free (pdev2);
  }
  g_list_foreach (or, (GFunc) gnome_vfs_volume_unref, NULL);
  g_list_free (or);

  return found;
}

static gboolean
cd_cache_get_dev_from_drives (GnomeVFSVolumeMonitor *mon, const char *device,
			      char **mountpoint, GnomeVFSDrive **d)
{
  gboolean found;
  GnomeVFSDrive *drive = NULL;
  GList *list, *or;

  found = FALSE;

  for (or = list = gnome_vfs_volume_monitor_get_connected_drives (mon);
       list != NULL; list = list->next) {
    char *pdev, *pdev2;

    drive = list->data;
    if (!(pdev = gnome_vfs_drive_get_device_path (drive)))
      continue;
    pdev2 = totem_resolve_symlink (pdev, NULL);
    if (!pdev2) {
      g_free (pdev);
      continue;
    }
    g_free (pdev);

    if (strcmp (pdev2, device) == 0) {
      char *mnt;

      mnt = gnome_vfs_drive_get_activation_uri (drive);
      if (mnt && strncmp (mnt, "file://", 7) == 0) {
        *mountpoint = g_strdup (mnt + 7);
      } else {
	*mountpoint = NULL;
      }
      found = TRUE;
      g_free (pdev2);
      g_free (mnt);
      gnome_vfs_drive_ref (drive);
      break;
    }
    g_free (pdev2);
  }
  g_list_foreach (or, (GFunc) gnome_vfs_drive_unref, NULL);
  g_list_free (or);

  *d = drive;

  return found;
}

#ifdef HAVE_HAL
static LibHalContext *
cd_cache_new_hal_ctx (void)
{
  LibHalContext *ctx;
  DBusConnection *conn;
  DBusError error;

  ctx = libhal_ctx_new ();
  if (ctx == NULL)
    return NULL;

  dbus_error_init (&error);
  conn = dbus_bus_get_private (DBUS_BUS_SYSTEM, &error);

  if (conn != NULL && !dbus_error_is_set (&error)) {
    if (!libhal_ctx_set_dbus_connection (ctx, conn)) {
      libhal_ctx_free (ctx);
      return NULL;
    }
    if (libhal_ctx_init (ctx, &error))
      return ctx;
  }

  if (dbus_error_is_set (&error)) {
    g_warning ("Couldn't get the system D-Bus: %s", error.message);
    dbus_error_free (&error);
  }

  libhal_ctx_free (ctx);
  if (conn != NULL) {
    dbus_connection_close (conn);
    dbus_connection_unref (conn);
  }

  return NULL;
}
#endif

static CdCache *
cd_cache_new (const char *dev,
	      GError     **error)
{
  CdCache *cache;
  char *mountpoint = NULL, *device, *local;
  GnomeVFSVolumeMonitor *mon;
  GnomeVFSDrive *drive = NULL;
#ifdef HAVE_HAL
  LibHalContext *ctx = NULL;
#endif
  gboolean found;

  if (g_str_has_prefix (dev, "file://") != FALSE)
    local = g_filename_from_uri (dev, NULL, NULL);
  else
    local = g_strdup (dev);

  g_assert (local != NULL);

  if (g_file_test (local, G_FILE_TEST_IS_DIR) != FALSE) {
    cache = g_new0 (CdCache, 1);
    cache->mountpoint = local;
    cache->is_media = FALSE;

    return cache;
  }

  /* retrieve mountpoint from gnome-vfs volumes and drives */
  device = totem_resolve_symlink (local, error);
  g_free (local);
  if (!device)
    return NULL;
  mon = gnome_vfs_get_volume_monitor ();
  found = cd_cache_get_dev_from_drives (mon, device, &mountpoint, &drive);
  if (!found) {
    drive = NULL;
    found = cd_cache_get_dev_from_volumes (mon, device, &mountpoint);
  }

  if (!found) {
    g_set_error (error, 0, 0,
	_("Failed to find mountpoint for device %s"),
	device);
    return NULL;
  }

#ifdef HAVE_HAL
  ctx = cd_cache_new_hal_ctx ();
  if (!ctx) {
    g_set_error (error, 0, 0,
	_("Could not connect to the HAL daemon"));
    return NULL;
  }
#endif

  /* create struture */
  cache = g_new0 (CdCache, 1);
  cache->device = device;
  cache->mountpoint = mountpoint;
  cache->self_mounted = FALSE;
  cache->drive = drive;
  cache->is_media = TRUE;
#ifdef HAVE_HAL
  cache->ctx = ctx;
#endif

  return cache;
}

#ifndef HAVE_HAL
static gboolean
cd_cache_has_medium (CdCache *cache)
{
  return TRUE;
}
#endif

#ifdef HAVE_HAL
static gboolean
cd_cache_has_medium (CdCache *cache)
{
  char **devices;
  int num_devices;
  char *udi;
  gboolean retval = FALSE;
  DBusError error;

  if (cache->drive == NULL)
    return FALSE;

  udi = gnome_vfs_drive_get_hal_udi (cache->drive);
  if (udi == NULL)
    return FALSE;

  dbus_error_init (&error);
  devices = libhal_manager_find_device_string_match (cache->ctx,
      "info.parent", udi, &num_devices, &error);
  if (devices != NULL && num_devices >= 1)
    retval = TRUE;

  if (dbus_error_is_set (&error)) {
    g_warning ("Error getting the children: %s", error.message);
    dbus_error_free (&error);
    g_free (udi);
    return FALSE;
  }

  if (retval == FALSE) {
    dbus_bool_t volume;

    if (libhal_device_property_exists (cache->ctx,
	  udi, "volume.is_disc", NULL) == FALSE) {
      g_free (udi);
      return FALSE;
    }

    volume = libhal_device_get_property_bool (cache->ctx,
	udi, "volume.is_disc", &error);
    if (dbus_error_is_set (&error)) {
      g_warning ("Error checking whether the volume is a disc: %s",
	  error.message);
      dbus_error_free (&error);
      g_free (udi);
      return FALSE;
    }
    retval = TRUE;
    cache->disc_udi = udi;
  } else {
    g_free (udi);
  }

  if (devices != NULL)
    libhal_free_string_array (devices);

  return retval;
}
#endif

static gboolean
cd_cache_open_device (CdCache *cache,
		      GError **error)
{
  /* not a medium? */
  if (cache->is_media == FALSE || cache->has_medium != FALSE) {
    return TRUE;
  }

  if (cd_cache_has_medium (cache) == FALSE) {
    g_set_error (error, 0, 0,
	_("Please check that a disc is present in the drive."));
    return FALSE;
  }
  cache->has_medium = TRUE;

  return TRUE;
}

typedef struct _CdCacheCallbackData {
  CdCache *cache;
  gboolean called;
} CdCacheCallbackData;

static void
cb_mount_done (gboolean success, char * error,
               char * detail, CdCacheCallbackData * data)
{
  data->called = TRUE;
  data->cache->mounted = success != FALSE;
}

static gboolean
cd_cache_open_mountpoint (CdCache *cache,
			  GError **error)
{
  CdCacheCallbackData data;

  /* already opened? */
  if (cache->mounted || cache->is_media == FALSE)
    return TRUE;

  /* check for mounting - assume we'll mount ourselves */
  if (cache->drive == NULL)
    return TRUE;
  cache->self_mounted = !gnome_vfs_drive_is_mounted (cache->drive);

  /* mount if we have to */
  if (cache->self_mounted) {
    /* mount - wait for callback */
    data.called = FALSE;
    data.cache = cache;
    gnome_vfs_drive_mount (cache->drive,
	(GnomeVFSVolumeOpCallback) cb_mount_done, &data);
    while (!data.called) g_main_context_iteration (NULL, TRUE);

    if (!cache->mounted) {
      g_set_error (error, 0, 0,
	  _("Failed to mount %s"), cache->device);
      return FALSE;
    }
  }

  if (!cache->mountpoint) {
    GList *vol, *item;

    for (vol = item = gnome_vfs_drive_get_mounted_volumes (cache->drive);
	item != NULL; item = item->next) {
      char *mnt = gnome_vfs_volume_get_activation_uri (item->data);

      if (mnt && strncmp (mnt, "file://", 7) == 0) {
	cache->mountpoint = g_strdup (mnt + 7);
	g_free (mnt);
	break;
      }
      g_free (mnt);
    }
    g_list_foreach (vol, (GFunc) gnome_vfs_volume_unref, NULL);
    g_list_free (vol);

    if (!cache->mountpoint) {
      g_set_error (error, 0, 0,
	  _("Failed to find mountpoint for %s"), cache->device);
      return FALSE;
    }
  }

  return TRUE;
}

static void
cd_cache_free (CdCache *cache)
{
#ifdef HAVE_HAL
  if (cache->ctx != NULL) {
    DBusConnection *conn;

    conn = libhal_ctx_get_dbus_connection (cache->ctx);
    libhal_ctx_shutdown (cache->ctx, NULL);
    libhal_ctx_free(cache->ctx);
    /* Close the connection before doing the last unref */
    dbus_connection_close (conn);
    dbus_connection_unref (conn);

    g_free (cache->disc_udi);
  }
#endif /* HAVE_HAL */

  /* free mem */
  if (cache->drive)
    gnome_vfs_drive_unref (cache->drive);
  g_free (cache->mountpoint);
  g_free (cache->device);
  g_free (cache);
}

static MediaType
cd_cache_disc_is_cdda (CdCache *cache,
		       GError **error)
{
  MediaType type;

  /* We can't have audio CDs on disc, yet */
  if (cache->is_media == FALSE)
    return MEDIA_TYPE_DATA;
  if (!cd_cache_open_device (cache, error))
    return MEDIA_TYPE_ERROR;

#ifdef HAVE_HAL
  {
    DBusError error;
    dbus_bool_t is_cdda;

    dbus_error_init (&error);

    is_cdda = libhal_device_get_property_bool (cache->ctx,
	cache->disc_udi, "volume.disc.has_audio", &error);
    type = is_cdda ? MEDIA_TYPE_CDDA : MEDIA_TYPE_DATA;

    if (dbus_error_is_set (&error)) {
      g_warning ("Error checking whether the volume is an audio CD: %s",
	  error.message);
      dbus_error_free (&error);
      return MEDIA_TYPE_ERROR;
    }
    return type;
  }
#else
  {
    GList *vol, *item;

    type = MEDIA_TYPE_DATA;

    for (vol = item = gnome_vfs_drive_get_mounted_volumes (cache->drive);
	item != NULL; item = item->next) {
      char *mnt = gnome_vfs_volume_get_activation_uri (item->data);
      if (mnt && strncmp (mnt, "cdda://", 7) == 0) {
	g_free (mnt);
	type = MEDIA_TYPE_CDDA;
	break;
      }
      g_free (mnt);
    }
    g_list_foreach (vol, (GFunc) gnome_vfs_volume_unref, NULL);
    g_list_free (vol);
  }

  return type;
#endif
}

static gboolean
cd_cache_file_exists (CdCache *cache, const char *subdir, const char *filename)
{
  char *path, *dir;
  gboolean ret;

  dir = NULL;

  /* Check whether the directory exists, for a start */
  path = g_build_filename (cache->mountpoint, subdir, NULL);
  ret = g_file_test (path, G_FILE_TEST_IS_DIR);
  if (ret == FALSE) {
    char *subdir_low;

    g_free (path);
    subdir_low = g_ascii_strdown (subdir, -1);
    path = g_build_filename (cache->mountpoint, subdir_low, NULL);
    ret = g_file_test (path, G_FILE_TEST_IS_DIR);
    g_free (path);
    if (ret) {
      dir = subdir_low;
    } else {
      g_free (subdir_low);
      return FALSE;
    }
  } else {
    g_free (path);
    dir = g_strdup (subdir);
  }

  /* And now the file */
  path = g_build_filename (cache->mountpoint, dir, filename, NULL);
  ret = g_file_test (path, G_FILE_TEST_IS_REGULAR);
  if (ret == FALSE) {
    char *fname_low;

    g_free (path);
    fname_low = g_ascii_strdown (filename, -1);
    path = g_build_filename (cache->mountpoint, dir, fname_low, NULL);
    ret = g_file_test (path, G_FILE_TEST_IS_REGULAR);
    g_free (fname_low);
  }

  g_free (dir);
  g_free (path);

  return ret;
}

static MediaType
cd_cache_disc_is_vcd (CdCache *cache,
                      GError **error)
{
  /* open disc and open mount */
  if (!cd_cache_open_device (cache, error))
    return MEDIA_TYPE_ERROR;
  if (!cd_cache_open_mountpoint (cache, error))
    return MEDIA_TYPE_ERROR;
  if (!cache->mountpoint)
    return MEDIA_TYPE_ERROR;
#ifdef HAVE_HAL
  if (cache->is_media != FALSE) {
    DBusError error;
    dbus_bool_t is_vcd;

    dbus_error_init (&error);

    is_vcd = libhal_device_get_property_bool (cache->ctx,
	cache->disc_udi, "volume.disc.is_vcd", &error);

    if (dbus_error_is_set (&error)) {
      g_warning ("Error checking whether the volume is a VCD: %s",
	  error.message);
      dbus_error_free (&error);
      return MEDIA_TYPE_ERROR;
    }
    if (is_vcd != FALSE)
      return MEDIA_TYPE_VCD;
    is_vcd = libhal_device_get_property_bool (cache->ctx,
	cache->disc_udi, "volume.disc.is_svcd", &error);

    if (dbus_error_is_set (&error)) {
      g_warning ("Error checking whether the volume is an SVCD: %s",
	  error.message);
      dbus_error_free (&error);
      return MEDIA_TYPE_ERROR;
    }
    return is_vcd ? MEDIA_TYPE_VCD : MEDIA_TYPE_DATA;
  }
#endif
  /* first is VCD, second is SVCD */
  if (cd_cache_file_exists (cache, "MPEGAV", "AVSEQ01.DAT") ||
      cd_cache_file_exists (cache, "MPEG2", "AVSEQ01.MPG"))
    return MEDIA_TYPE_VCD;

  return MEDIA_TYPE_DATA;
}

static MediaType
cd_cache_disc_is_dvd (CdCache *cache,
		      GError **error)
{
  /* open disc, check capabilities and open mount */
  if (!cd_cache_open_device (cache, error))
    return MEDIA_TYPE_ERROR;
  if (!cd_cache_open_mountpoint (cache, error))
    return MEDIA_TYPE_ERROR;
  if (!cache->mountpoint)
    return MEDIA_TYPE_ERROR;
#ifdef HAVE_HAL
  if (cache->is_media != FALSE) {
    DBusError error;
    dbus_bool_t is_dvd;

    dbus_error_init (&error);

    is_dvd = libhal_device_get_property_bool (cache->ctx,
	cache->disc_udi, "volume.disc.is_videodvd", &error);

    if (dbus_error_is_set (&error)) {
      g_warning ("Error checking whether the volume is a DVD: %s",
	  error.message);
      dbus_error_free (&error);
      return MEDIA_TYPE_ERROR;
    }
    return is_dvd ? MEDIA_TYPE_DVD : MEDIA_TYPE_DATA;
  }
#endif
  if (cd_cache_file_exists (cache, "VIDEO_TS", "VIDEO_TS.IFO"))
    return MEDIA_TYPE_DVD;

  return MEDIA_TYPE_DATA;
}

char *
totem_cd_mrl_from_type (const char *scheme, const char *dir)
{
  char *retval;

  if (g_str_has_prefix (dir, "file://") != FALSE) {
    char *local;
    local = g_filename_from_uri (dir, NULL, NULL);
    retval = g_strdup_printf ("%s://%s", scheme, local);
    g_free (local);
  } else {
    retval = g_strdup_printf ("%s://%s", scheme, dir);
  }
  return retval;
}

MediaType
totem_cd_detect_type_from_dir (const char *dir, char **url, GError **error)
{
  CdCache *cache;
  MediaType type;

  g_return_val_if_fail (dir != NULL, MEDIA_TYPE_ERROR);

  if (dir[0] != '/' && g_str_has_prefix (dir, "file://") == FALSE)
    return MEDIA_TYPE_ERROR;

  if (!(cache = cd_cache_new (dir, error)))
    return MEDIA_TYPE_ERROR;
  if ((type = cd_cache_disc_is_vcd (cache, error)) == MEDIA_TYPE_DATA &&
      (type = cd_cache_disc_is_dvd (cache, error)) == MEDIA_TYPE_DATA) {
    /* crap, nothing found */
    cd_cache_free (cache);
    return type;
  }
  cd_cache_free (cache);

  if (url == NULL) {
    return type;
  }

  if (type == MEDIA_TYPE_DVD) {
    *url = totem_cd_mrl_from_type ("dvd", dir);
  } else if (type == MEDIA_TYPE_VCD) {
    *url = totem_cd_mrl_from_type ("vcd", dir);
  }

  return type;
}

MediaType
totem_cd_detect_type_with_url (const char *device,
    			       char      **url,
			       GError     **error)
{
  CdCache *cache;
  MediaType type;

  if (url != NULL)
    *url = NULL;

  if (!(cache = cd_cache_new (device, error)))
    return MEDIA_TYPE_ERROR;

  type = cd_cache_disc_is_cdda (cache, error);
  if (type == MEDIA_TYPE_ERROR && *error != NULL) {
    cd_cache_free (cache);
    return type;
  }

  if ((type == MEDIA_TYPE_DATA || type == MEDIA_TYPE_ERROR) &&
      (type = cd_cache_disc_is_vcd (cache, error)) == MEDIA_TYPE_DATA &&
      (type = cd_cache_disc_is_dvd (cache, error)) == MEDIA_TYPE_DATA) {
    /* crap, nothing found */
  }

  if (url == NULL) {
    cd_cache_free (cache);
    return type;
  }

  switch (type) {
  case MEDIA_TYPE_DVD:
    *url = totem_cd_mrl_from_type ("dvd", device);
    break;
  case MEDIA_TYPE_VCD:
    *url = totem_cd_mrl_from_type ("vcd", device);
    break;
  case MEDIA_TYPE_CDDA:
    *url = totem_cd_mrl_from_type ("cdda", device);
    break;
  case MEDIA_TYPE_DATA:
    *url = g_strdup (cache->mountpoint);
    break;
  default:
    break;
  }

  cd_cache_free (cache);

  return type;
}

MediaType
totem_cd_detect_type (const char  *device,
		      GError     **error)
{
  return totem_cd_detect_type_with_url (device, NULL, error);
}

gboolean
totem_cd_has_medium (const char  *device)
{
  CdCache *cache;
  gboolean retval = TRUE;

  if (!(cache = cd_cache_new (device, NULL)))
    return TRUE;

  retval = cd_cache_has_medium (cache);
  cd_cache_free (cache);

  return retval;
}

const char *
totem_cd_get_human_readable_name (MediaType type)
{
  switch (type)
  {
  case MEDIA_TYPE_CDDA:
    return N_("Audio CD");
  case MEDIA_TYPE_VCD:
    return N_("Video CD");
  case MEDIA_TYPE_DVD:
    return N_("DVD");
  default:
    g_assert_not_reached ();
  }

  return NULL;
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
