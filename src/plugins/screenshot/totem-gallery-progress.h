/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Philip Withnall <philip@tecnocode.co.uk>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#define TOTEM_TYPE_GALLERY_PROGRESS		(totem_gallery_progress_get_type ())
G_DECLARE_FINAL_TYPE(TotemGalleryProgress, totem_gallery_progress, TOTEM, GALLERY_PROGRESS, GObject)

GType totem_gallery_progress_get_type (void);
TotemGalleryProgress *totem_gallery_progress_new (GPid child_pid, const gchar *output_filename);
void totem_gallery_progress_run (TotemGalleryProgress *self, gint stdout_fd);
