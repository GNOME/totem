/*
 * Copyright © 2002-2012 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#pragma once

#include <glib.h>

typedef enum {
	TOTEM_TIME_FLAG_NONE            = 0,
	TOTEM_TIME_FLAG_REMAINING       = 1 << 0,
	TOTEM_TIME_FLAG_FORCE_HOUR      = 1 << 2,
	TOTEM_TIME_FLAG_MSECS           = 1 << 3,
} TotemTimeFlag;

char *totem_time_to_string (gint64        msecs,
			    TotemTimeFlag flags);
