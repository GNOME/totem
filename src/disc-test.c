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
  MediaType type;
  GError *error = NULL;
  const char *type_s = NULL;
  char *url = NULL;
  gboolean is_dir = FALSE;

  if (argc != 2) {
    g_print ("Usage: %s <device>\n", argv[0]);
    return -1;
  }

  g_type_init ();
  gnome_vfs_init ();

  if (g_file_test (argv[1], G_FILE_TEST_IS_DIR) != FALSE) {
    type = cd_detect_type_from_dir (argv[1], &url, &error);
    is_dir = TRUE;
  } else {
    type = cd_detect_type (argv[1], &error);
  }

  switch (type) {
    case MEDIA_TYPE_ERROR:
      g_print ("Error: %s\n", error ? error->message : "unknown reason");
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
