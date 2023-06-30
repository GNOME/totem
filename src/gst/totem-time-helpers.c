/*
 * Copyright © 2002-2012 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

#include <math.h>
#include <glib/gi18n.h>
#include <libintl.h>

#include "totem-time-helpers.h"

/* FIXME: Remove
 * See https://gitlab.freedesktop.org/gstreamer/gstreamer/issues/26 */
char *
totem_time_to_string (gint64        msecs,
		      TotemTimeFlag flags)
{
	gint64 _time;
	int msec, sec, min, hour;

	if (msecs < 0) {
		/* translators: Unknown time */
		return g_strdup (_("--:--"));
	}

	/* When calculating the remaining time,
	 * we want to make sure that:
	 * current time + time remaining = total run time */
	msec = msecs % 1000;
	if (flags & TOTEM_TIME_FLAG_MSECS) {
		_time = msecs - msec;
		_time = _time / 1000;
	} else {
		double time_f;

		time_f = (double) msecs / 1000;
		if (flags & TOTEM_TIME_FLAG_REMAINING)
			time_f = ceil (time_f);
		else
			time_f = round (time_f);
		_time = (gint64) time_f;
	}

	sec = _time % 60;
	_time = _time - sec;
	min = (_time % (60*60)) / 60;
	_time = _time - (min * 60);
	hour = _time / (60*60);

	if (hour > 0 || flags & TOTEM_TIME_FLAG_FORCE_HOUR) {
		if (!(flags & TOTEM_TIME_FLAG_REMAINING)) {
			if (!(flags & TOTEM_TIME_FLAG_MSECS)) {
				/* hour:minutes:seconds */
				/* Translators: This is a time format, like "9:05:02" for 9
				 * hours, 5 minutes, and 2 seconds. You may change ":" to
				 * the separator that your locale uses or use "%Id" instead
				 * of "%d" if your locale uses localized digits.
				 */
				return g_strdup_printf (C_("long time format", "%d:%02d:%02d"), hour, min, sec);
			} else {
				/* hour:minutes:seconds.msecs */
				/* Translators: This is a time format, like "9:05:02.050" for 9
				 * hours, 5 minutes, 2 seconds and 50 milliseconds. You may change ":" to
				 * the separator that your locale uses or use "%Id" instead
				 * of "%d" if your locale uses localized digits.
				 */
				return g_strdup_printf (C_("long time format", "%d:%02d:%02d.%03d"), hour, min, sec, msec);
			}
		} else {
			if (!(flags & TOTEM_TIME_FLAG_MSECS)) {
				/* -hour:minutes:seconds */
				/* Translators: This is a time format, like "-9:05:02" for 9
				 * hours, 5 minutes, and 2 seconds playback remaining. You may
				 * change ":" to the separator that your locale uses or use
				 * "%Id" instead of "%d" if your locale uses localized digits.
				 */
				return g_strdup_printf (C_("long time format", "-%d:%02d:%02d"), hour, min, sec);
			} else {
				/* -hour:minutes:seconds.msecs */
				/* Translators: This is a time format, like "-9:05:02.050" for 9
				 * hours, 5 minutes, 2 seconds and 50 milliseconds playback remaining. You may
				 * change ":" to the separator that your locale uses or use
				 * "%Id" instead of "%d" if your locale uses localized digits.
				 */
				return g_strdup_printf (C_("long time format", "-%d:%02d:%02d.%03d"), hour, min, sec, msec);
			}
		}
	}

	if (flags & TOTEM_TIME_FLAG_REMAINING) {
		if (!(flags & TOTEM_TIME_FLAG_MSECS)) {
			/* -minutes:seconds */
			/* Translators: This is a time format, like "-5:02" for 5
			 * minutes and 2 seconds playback remaining. You may change
			 * ":" to the separator that your locale uses or use "%Id"
			 * instead of "%d" if your locale uses localized digits.
			 */
			return g_strdup_printf (C_("short time format", "-%d:%02d"), min, sec);
		} else {
			/* -minutes:seconds.msec */
			/* Translators: This is a time format, like "-5:02.050" for 5
			 * minutes 2 seconds and 50 milliseconds playback remaining. You may change
			 * ":" to the separator that your locale uses or use "%Id"
			 * instead of "%d" if your locale uses localized digits.
			 */
			return g_strdup_printf (C_("short time format", "-%d:%02d.%03d"), min, sec, msec);
		}
	}

	if (flags & TOTEM_TIME_FLAG_MSECS) {
		/* minutes:seconds.msec */
		/* Translators: This is a time format, like "5:02" for 5
		 * minutes 2 seconds and 50 milliseconds. You may change ":" to the
		 * separator that your locale uses or use "%Id" instead of
		 * "%d" if your locale uses localized digits.
		 */
		return g_strdup_printf (C_("short time format", "%d:%02d.%03d"), min, sec, msec);
	}

	/* minutes:seconds */
	/* Translators: This is a time format, like "5:02" for 5
	 * minutes and 2 seconds. You may change ":" to the
	 * separator that your locale uses or use "%Id" instead of
	 * "%d" if your locale uses localized digits.
	 */
	return g_strdup_printf (C_("short time format", "%d:%02d"), min, sec);
}
