/*
 * Copyright (C) 2001,2002,2003 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <handy.h>

#define TOTEM_TYPE_PREFERENCES_DIALOG (totem_preferences_dialog_get_type())
G_DECLARE_FINAL_TYPE (TotemPreferencesDialog, totem_preferences_dialog, TOTEM, PREFERENCES_DIALOG, HdyPreferencesWindow)

GtkWidget *totem_preferences_dialog_new (Totem *totem);
