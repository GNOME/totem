/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Philip Withnall <philip@tecnocode.co.uk>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <glib.h>

#include "totem.h"
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

gchar *totem_screenshot_plugin_setup_file_chooser (const char *filename_format, const char *movie_name) G_GNUC_WARN_UNUSED_RESULT G_GNUC_FORMAT (1);
void totem_screenshot_plugin_update_file_chooser (const char *filename);
