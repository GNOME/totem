/*
 * Copyright Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 */

#include "config.h"

#include <locale.h>
#include <glib.h>

#include "totem-time-helpers.h"

static void
test_time (void)
{
	g_assert_cmpstr (totem_time_to_string (0, FALSE, FALSE), ==, "0:00");
	g_assert_cmpstr (totem_time_to_string (500, FALSE, FALSE), ==, "0:00");
	g_assert_cmpstr (totem_time_to_string (500, TRUE, FALSE), ==, "-0:01");
	g_assert_cmpstr (totem_time_to_string (1250, FALSE, FALSE), ==, "0:01");
	g_assert_cmpstr (totem_time_to_string (1250, TRUE, FALSE), ==, "-0:02");
}

static void
log_handler (const char *log_domain, GLogLevelFlags log_level, const char *message, gpointer user_data)
{
	g_test_message ("%s", message);
}

int
main (int argc, char *argv[])
{
	setlocale (LC_ALL, "en_US.UTF-8");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	/* We need to handle log messages produced by g_message so they're interpreted correctly by the GTester framework */
	g_log_set_handler (NULL, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG, log_handler, NULL);

	g_test_add_func ("/time", test_time);

	return g_test_run ();
}
