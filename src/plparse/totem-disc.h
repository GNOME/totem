/* Totem Disc Content Detection
 * (c) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * totem-disc.h: media content detection
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

#ifndef TOTEM_DISC_H
#define TOTEM_DISC_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  MEDIA_TYPE_ERROR = -1, /* error */
  MEDIA_TYPE_DATA = 1,
  MEDIA_TYPE_CDDA,
  MEDIA_TYPE_VCD,
  MEDIA_TYPE_DVD
} MediaType;

MediaType	totem_cd_detect_type	(const char *device,
					 GError     **error);
MediaType	totem_cd_detect_type_with_url (const char  *device,
					       char       **url,
					       GError     **error);
MediaType	totem_cd_detect_type_from_dir (const char *dir,
					 char      **url, 
					 GError    **error);
const char *	totem_cd_get_human_readable_name (MediaType type);
char *		totem_cd_mrl_from_type (const char *scheme, const char *dir);

G_END_DECLS

#endif /* TOTEM_DISC_H */
