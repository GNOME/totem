/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Philip Withnall <philip@tecnocode.co.uk>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 * Author: Bastien Nocera <hadess@hadess.net>, Philip Withnall <philip@tecnocode.co.uk>
 */

#pragma once

#include <gtk/gtk.h>

#define TOTEM_TYPE_OPEN_LOCATION		(totem_open_location_get_type ())
G_DECLARE_FINAL_TYPE(TotemOpenLocation, totem_open_location, TOTEM, OPEN_LOCATION, GtkDialog)

GType totem_open_location_get_type		(void);
GtkWidget *totem_open_location_new		(void);
char *totem_open_location_get_uri		(TotemOpenLocation *open_location);
