/* Small test app for disc concent detection
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

#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>

#include "totem-disc.h"

gint
main (gint   argc,
      gchar *argv[])
{
  TotemDiscMediaType type;
  GError *error = NULL;
  const char *type_s = NULL;
  char *url = NULL;
  gboolean is_dir = FALSE;
  GList *or, *list;
  GnomeVFSVolumeMonitor *mon;

  if (argc != 2) {
    g_print ("Usage: %s <device>\n", argv[0]);
    return -1;
  }

  g_type_init ();
  gnome_vfs_init ();
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING);

  if (g_file_test (argv[1], G_FILE_TEST_IS_DIR) != FALSE) {
    type = totem_cd_detect_type_from_dir (argv[1], &url, &error);
    is_dir = TRUE;
  } else {
    type = totem_cd_detect_type (argv[1], &error);
  }

  switch (type) {
    case MEDIA_TYPE_ERROR:
      mon = gnome_vfs_get_volume_monitor ();
      g_print ("Error: %s\n", error ? error->message : "unknown reason");
      g_print ("\n");
      g_print ("List of connected drives:\n");
      for (or = list = gnome_vfs_volume_monitor_get_connected_drives (mon);
		      list != NULL; list = list->next) {
        char *device;
	device = gnome_vfs_drive_get_device_path ((GnomeVFSDrive *) list->data);
        g_print ("%s\n", device);
	g_free (device);
      }
      if (or == NULL)
        g_print ("No connected drives!\n");

      g_print ("List of mounted volumes:\n");
      for (or = list = gnome_vfs_volume_monitor_get_mounted_volumes (mon);
		      list != NULL; list = list->next) {
        char *device;

	device = gnome_vfs_volume_get_device_path ((GnomeVFSVolume *) list->data);
	g_print ("%s\n", device);
	g_free (device);
      }
      if (or == NULL)
        g_print ("No mounted volumes!\n");

      return -1;
    case MEDIA_TYPE_DATA:
      type_s = "Data CD";
      break;
    case MEDIA_TYPE_CDDA:
      type_s = "Audio CD";
      break;
    case MEDIA_TYPE_VCD:
      type_s = "Video CD";
      break;
    case MEDIA_TYPE_DVD:
      type_s = "DVD";
      break;
    default:
      g_assert_not_reached ();
  }

  g_print ("%s contains a %s\n", argv[1], type_s);

  if (is_dir != FALSE && url != NULL) {
    g_print ("URL for directory is %s\n", url);
  }

  g_free (url);

  return 0;
}
