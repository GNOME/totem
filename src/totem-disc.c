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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <mntent.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/cdrom.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>

#include "totem-disc.h"

typedef struct _CdCache {
  /* device node and mountpoint */
  char *device, *mountpoint;
  GnomeVFSDrive *drive;

  /* file descriptor to the device */
  int fd;

  /* capabilities of the device */
  int cap;

  /* if we're checking a media, or a dir */
  gboolean is_media;

  /* indicates if we mounted this mountpoint ourselves or if it
   * was already mounted. */
  gboolean self_mounted;
  gboolean mounted;
} CdCache;

/* 
 * Resolve relative paths
 */

static char *
totem_disc_resolve_link (const char *dev, const char *buf)
{
  char *parent, *new;

  /* is it an absolute path? */
  if (g_path_is_absolute (buf) != FALSE)
    return g_strdup (buf);

  parent = g_path_get_dirname (dev);
  new = g_build_filename (parent, buf, NULL);
  g_free (parent);

  return new;
}

/*
 * So, devices can be symlinks and that screws up.
 */

static char *
get_device (const char *device,
	    GError     **error)
{
  char *buf;
  char *dev = g_strdup (device);
  struct stat st;

  while (1) {
    char *new;

    if (lstat (dev, &st) != 0) {
      g_set_error (error, 0, 0,
          _("Failed to find real device node for %s: %s"),
          dev, g_strerror (errno));
      g_free (dev);
      return NULL;
    }

    if (!S_ISLNK (st.st_mode))
      break;

    if (!(buf = g_file_read_link (dev, NULL))) {
      g_set_error (error, 0, 0,
          _("Failed to read symbolic link %s: %s"),
          dev, g_strerror (errno));
      g_free (dev);
      return NULL;
    }
    new = totem_disc_resolve_link (dev, buf);
    g_free (dev);
    g_free (buf);
    dev = new;
  }

  return dev;
}

static CdCache *
cd_cache_new (const char *dev,
	      GError     **error)
{
  CdCache *cache;
  char *mountpoint = NULL, *device;
  GnomeVFSVolumeMonitor *mon;
  GnomeVFSDrive *drive = NULL;
  GList *list, *or;
  gboolean is_dir;

  is_dir = g_file_test (dev, G_FILE_TEST_IS_DIR);
  if (is_dir) {
    cache = g_new0 (CdCache, 1);
    cache->mountpoint = g_strdup (dev);
    cache->fd = -1;
    cache->is_media = FALSE;

    return cache;
  }

  /* retrieve mountpoint (/etc/fstab). We could also use HAL for this,
   * I think (gnome-volume-manager does that). */
  if (!(device = get_device (dev, error)))
    return NULL;
  mon = gnome_vfs_get_volume_monitor ();
  for (or = list = gnome_vfs_volume_monitor_get_connected_drives (mon);
       list != NULL; list = list->next) {
    char *pdev, *pdev2;

    drive = list->data;
    if (!(pdev = gnome_vfs_drive_get_device_path (drive)))
      continue;
    if (!(pdev2 = get_device (pdev, NULL))) {
      g_free (pdev);
      continue;
    }
    g_free (pdev);

    if (strcmp (pdev2, device) == 0) {
      char *mnt;

      mnt = gnome_vfs_drive_get_activation_uri (drive);
      g_free (pdev2);
      if (!strncmp (mnt, "file://", 7)) {
        mountpoint = g_strdup (mnt + 7);
        g_free (mnt);
        gnome_vfs_drive_ref (drive);
        break;
      }
      g_free (mnt);
    }
    g_free (pdev2);
  }
  g_list_foreach (or, (GFunc) gnome_vfs_drive_unref, NULL);
  g_list_free (or);

  if (!mountpoint) {
    g_set_error (error, 0, 0,
        _("Failed to find mountpoint for device %s in /etc/fstab"),
        device);
    return NULL;
  }

  /* create struture */
  cache = g_new0 (CdCache, 1);
  cache->device = device;
  cache->mountpoint = mountpoint;
  cache->fd = -1;
  cache->self_mounted = FALSE;
  cache->drive = drive;
  cache->is_media = TRUE;

  return cache;
}

static gboolean
cd_cache_open_device (CdCache *cache,
		      GError **error)
{
  int drive, err;

  /* not a medium? */
  if (cache->is_media == FALSE) {
    cache->cap = CDC_DVD;
    return TRUE;
  }

  /* already open? */
  if (cache->fd > 0)
    return TRUE;

  /* try to open the CD before creating anything */
  if ((cache->fd = open (cache->device, O_RDONLY)) < 0) {
    err = errno;
    if (err == ENOMEDIUM) {
      g_set_error (error, 0, 0,
          _("Please check that a disc is present in the drive."));
    } else {
      g_set_error (error, 0, 0,
          _("Failed to open device %s for reading: %s"),
        cache->device, g_strerror (err));
    }
    return FALSE;
  }

  /* get capabilities */
  if ((cache->cap = ioctl (cache->fd, CDROM_GET_CAPABILITY, NULL)) < 0) {
    close (cache->fd);
    cache->fd = -1;
    g_set_error (error, 0, 0,
        _("Failed to retrieve capabilities of device %s: %s"),
        cache->device, g_strerror (errno));
    return FALSE;
  }

  /* is there a disc in the tray? */
  if ((drive = ioctl (cache->fd, CDROM_DRIVE_STATUS, NULL)) != CDS_DISC_OK) {
    const char *drive_s;

    close (cache->fd);
    cache->fd = -1;

    switch (drive) {
      case CDS_NO_INFO:
        drive_s = "Not implemented";
        break;
      case CDS_NO_DISC:
        drive_s = "No disc in tray";
        break;
      case CDS_TRAY_OPEN:
        drive_s = "Tray open";
        break;
      case CDS_DRIVE_NOT_READY:
        drive_s = "Drive not ready";
        break;
      case CDS_DISC_OK:
        drive_s = "OK";
        break;
      default:
        drive_s = "Unknown";
        break;
    }
    g_set_error (error, 0, 0,
        _("Drive status 0x%x (%s) - check disc"),
        drive, drive_s);
    return FALSE;
  }

  return TRUE;
}

static gboolean
cd_cache_open_mountpoint (CdCache *cache,
			  GError **error)
{
  /* already opened? */
  if (cache->mounted || cache->is_media == FALSE)
    return TRUE;

  /* check for mounting - assume we'll mount ourselves */
  cache->self_mounted = !gnome_vfs_drive_is_mounted (cache->drive);

  /* mount if we have to */
  if (cache->self_mounted) {
    char *command;
    int status;

    command = g_strdup_printf ("mount %s", cache->mountpoint);
    if (!g_spawn_command_line_sync (command,
             NULL, NULL, &status, error)) {
      g_free (command);
      return FALSE;
    }
    g_free (command);
    if (status != 0) {
      g_set_error (error, 0, 0,
          _("Unexpected error status %d while mounting %s"),
          status, cache->mountpoint);
      return FALSE;
    }
  }

  cache->mounted = TRUE;
  return TRUE;
}

static void
cd_cache_free (CdCache *cache)
{
  /* umount if we mounted */
  if (cache->self_mounted && cache->mounted) {
    char *command;

    command = g_strdup_printf ("umount %s", cache->mountpoint);
    g_spawn_command_line_sync (command, NULL, NULL, NULL, NULL);
    g_free (command);
  }

  /* close file descriptor to device */
  if (cache->fd > 0) {
    close (cache->fd);
  }

  /* free mem */
  gnome_vfs_drive_unref (cache->drive);
  g_free (cache->mountpoint);
  g_free (cache->device);
  g_free (cache);
}

static MediaType
cd_cache_disc_is_cdda (CdCache *cache,
		       GError **error)
{
  MediaType type = MEDIA_TYPE_DATA;
  int disc;
  const char *disc_s;

  /* We can't have audio CDs on disc, yet */
  if (cache->is_media == FALSE)
    return type;

  /* open disc and open mount */
  if (!cd_cache_open_device (cache, error))
    return MEDIA_TYPE_ERROR;

  if ((disc = ioctl (cache->fd, CDROM_DISC_STATUS, NULL)) < 0) {
    g_set_error (error, 0, 0,
        _("Error getting %s disc status: %s"),
        cache->device, g_strerror (errno));
    return MEDIA_TYPE_ERROR;
  }

  switch (disc) {
    case CDS_NO_INFO:
      /* The drive doesn't implement CDROM_DISC_STATUS */
      break;
    case CDS_NO_DISC:
      disc_s = "No disc in tray";
      type = MEDIA_TYPE_ERROR;
      break;
    case CDS_AUDIO:
    case CDS_MIXED:
      type = MEDIA_TYPE_CDDA;
      break;
    case CDS_DATA_1:
    case CDS_DATA_2:
    case CDS_XA_2_1:
    case CDS_XA_2_2:
      break;
    default:
      disc_s = "Unknown";
      type = MEDIA_TYPE_ERROR;
      break;
  }
  if (type == MEDIA_TYPE_ERROR) {
    g_set_error (error, 0, 0,
        _("Unexpected/unknown cd type 0x%x (%s)"),
        disc, disc_s);
    return MEDIA_TYPE_ERROR;
  }

  return type;
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
  if (!(cache->cap & CDC_DVD))
    return MEDIA_TYPE_DATA;
  if (!cd_cache_open_mountpoint (cache, error))
    return MEDIA_TYPE_ERROR;
  if (cd_cache_file_exists (cache, "VIDEO_TS", "VIDEO_TS.IFO"))
    return MEDIA_TYPE_DVD;

  return MEDIA_TYPE_DATA;
}

MediaType
cd_detect_type_from_dir (const char *dir, char **url, GError **error)
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
    if (g_str_has_prefix (dir, "file://") != FALSE) {
      char *local;
      local = g_filename_from_uri (dir, NULL, NULL);
      *url = g_strdup_printf ("dvd://%s", local);
      g_free (local);
    } else {
      *url = g_strdup_printf ("dvd://%s", dir);
    }
  } else if (type == MEDIA_TYPE_VCD) {
    if (g_str_has_prefix (dir, "file://") != FALSE) {
      char *local;
      local = g_filename_from_uri (dir, NULL, NULL);
      *url = g_strdup_printf ("vcd://%s", local);
      g_free (local);
    } else {
      *url = g_strdup_printf ("vcd://%s", dir);
    }
  }

  return type;
}

MediaType
cd_detect_type (const char *device,
		GError     **error)
{
  CdCache *cache;
  MediaType type;

  if (!(cache = cd_cache_new (device, error)))
    return MEDIA_TYPE_ERROR;
  if ((type = cd_cache_disc_is_cdda (cache, error)) == MEDIA_TYPE_DATA &&
      (type = cd_cache_disc_is_vcd (cache, error)) == MEDIA_TYPE_DATA &&
      (type = cd_cache_disc_is_dvd (cache, error)) == MEDIA_TYPE_DATA) {
    /* crap, nothing found */
  }
  cd_cache_free (cache);

  return type;
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
