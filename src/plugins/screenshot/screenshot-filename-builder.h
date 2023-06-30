/* screenshot-filename-builder.c - Builds a filename suitable for a screenshot
 *
 * Copyright (C) 2008, 2011 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <gio/gio.h>

gchar *get_fallback_screenshot_dir (void);
gchar *get_default_screenshot_dir (void);
void screenshot_build_filename_async (const char *save_dir,
                                      const char *screenshot_origin,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data);
gchar *screenshot_build_filename_finish (GAsyncResult *result,
                                         GError **error);
