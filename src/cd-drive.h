/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   cd-drive.h: easy to use cd burner software

   Copyright (C) 2002 Red Hat, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

  Authors: Alexander Larsson <alexl@redhat.com>
  Bastien Nocera <hadess@hadess.net>
*/

#ifndef CD_DRIVE_H
#define CD_DRIVE_H

#include <glib.h>

typedef enum {
	DRIVE_TYPE_FILE,
	DRIVE_TYPE_CD_RECORDER,
	DRIVE_TYPE_DVD_RECORDER,
	DRIVE_TYPE_CD_DRIVE,
	DRIVE_TYPE_DVD_DRIVE,
} CDDriveType;

typedef struct {
	CDDriveType type;
	char *name;
	int max_speed;
	char *id;
	char *device;
} CDDrive;

/* Returns a list of CDDrive structs */
GList *scan_for_cdroms (gboolean recorder_only, gboolean add_image);

#endif
