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

char *totem_screenshot_plugin_filename_for_current_video (TotemObject *totem, const char *format);
void totem_screenshot_plugin_set_file_chooser_folder (GtkFileChooser *chooser);
void totem_screenshot_plugin_update_file_chooser (const char *filename);
