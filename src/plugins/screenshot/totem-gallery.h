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
#include <handy.h>

#include "totem.h"

#define TOTEM_TYPE_GALLERY		(totem_gallery_get_type ())
G_DECLARE_FINAL_TYPE(TotemGallery, totem_gallery, TOTEM, GALLERY, HdyWindow)

GType totem_gallery_get_type (void);
TotemGallery *totem_gallery_new (Totem *totem);
