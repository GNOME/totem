/* totem-uri.h

   Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>

   SPDX-License-Identifier: GPL-3-or-later

   Author: Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include "totem.h"
#include <gtk/gtk.h>
#include <gio/gio.h>

const char *	totem_dot_dir			(void);
const char *	totem_data_dot_dir		(void);
char *		totem_create_full_path		(const char *path);
GMount *	totem_get_mount_for_media	(const char *uri);
gboolean	totem_playing_dvd		(const char *uri);
gboolean	totem_uri_is_subtitle		(const char *uri);
gboolean	totem_is_special_mrl		(const char *uri);
gboolean	totem_is_block_device		(const char *uri);
void		totem_setup_file_monitoring	(Totem *totem);
void		totem_setup_file_filters	(void);
void		totem_destroy_file_filters	(void);
char *		totem_uri_escape_for_display	(const char *uri);
GObject *	totem_add_files			(GtkWindow *parent,
						 const char *path);
GObject *	totem_add_subtitle		(GtkWindow *parent,
						 const char *uri);
