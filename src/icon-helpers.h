/*
 * Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include <gtk/gtk.h>
#include <grilo.h>

void             totem_grilo_setup_icons          (void);
void             totem_grilo_clear_icons          (void);
GdkPixbuf       *totem_grilo_get_icon             (GrlMedia *media,
						   gboolean *thumbnailing);
const GdkPixbuf *totem_grilo_get_video_icon       (void);
const GdkPixbuf *totem_grilo_get_box_icon         (void);
const GdkPixbuf *totem_grilo_get_channel_icon     (void);
const GdkPixbuf *totem_grilo_get_optical_icon     (void);

void             totem_grilo_pause_icon_thumbnailing  (void);
void             totem_grilo_resume_icon_thumbnailing (void);

void             totem_grilo_get_thumbnail        (GObject             *object,
						   GCancellable        *cancellable,
						   GAsyncReadyCallback  callback,
						   gpointer             user_data);
GdkPixbuf       *totem_grilo_get_thumbnail_finish (GObject             *object,
						   GAsyncResult        *res,
						   GError             **error);
