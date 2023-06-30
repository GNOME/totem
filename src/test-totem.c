/*
 * SPDX-License-Identifier: GPL-3-or-later
 */

#include "config.h"

#include <locale.h>
#define GST_USE_UNSTABLE_API 1
#include <gst/tag/tag.h>

#include "gst/totem-time-helpers.h"
#include "backend/bacon-video-widget.h"
#include "backend/bacon-time-label.h"
#include "totem-menu.h"

static BvwLangInfo *
bvw_lang_info_new (const char *title,
		   const char *language,
		   const char *codec)
{
	BvwLangInfo *info;

	info = g_new0 (BvwLangInfo, 1);
	info->title = g_strdup (title);
	info->language = g_strdup (language);
	info->codec = g_strdup (codec);
	return info;
}

static const char *
nth_label (GList *list, guint index)
{
	MenuItem *item = g_list_nth_data (list, index);
	if (!item)
		return NULL;
	return item->label;
}

static void
test_menus_lang_info (void)
{
	GList *l, *ret;

	/* No language, no codec */
	l = NULL;
	l = g_list_append (l, bvw_lang_info_new (NULL, "und", NULL));
	l = g_list_append (l, bvw_lang_info_new (NULL, "und", NULL));

	ret = bvw_lang_info_to_menu_labels (l, BVW_TRACK_TYPE_AUDIO);
	g_list_free_full (l, (GDestroyNotify) bacon_video_widget_lang_info_free);

	g_assert_cmpstr (nth_label (ret, 0), ==, "Audio Track #1");
	g_assert_cmpstr (nth_label (ret, 1), ==, "Audio Track #2");
	g_list_free_full (ret, g_free);

	/* Same language, same codecs */
	l = NULL;
	l = g_list_append (l, bvw_lang_info_new (NULL, "eng", "Dolby Pro Racing"));
	l = g_list_append (l, bvw_lang_info_new (NULL, "eng", "Dolby Pro Racing"));
	l = g_list_append (l, bvw_lang_info_new (NULL, "fre", "Dolby Amateur 5.1"));

	ret = bvw_lang_info_to_menu_labels (l, BVW_TRACK_TYPE_AUDIO);
	g_list_free_full (l, (GDestroyNotify) bacon_video_widget_lang_info_free);

	g_assert_cmpstr (nth_label (ret, 0), ==, "English #1");
	g_assert_cmpstr (nth_label (ret, 1), ==, "English #2");
	g_assert_cmpstr (nth_label (ret, 2), ==, "French");
	g_list_free_full (ret, g_free);

	/* Same language, different codecs */
	l = NULL;
	l = g_list_append (l, bvw_lang_info_new (NULL, "eng", "Dolby Pro Racing"));
	l = g_list_append (l, bvw_lang_info_new (NULL, "eng", "Dolby Amateur 5.1"));
	l = g_list_append (l, bvw_lang_info_new (NULL, "fre", "Dolby Amateur 5.1"));

	ret = bvw_lang_info_to_menu_labels (l, BVW_TRACK_TYPE_AUDIO);
	g_list_free_full (l, (GDestroyNotify) bacon_video_widget_lang_info_free);

	g_assert_cmpstr (nth_label (ret, 0), ==, "English — Dolby Pro Racing");
	g_assert_cmpstr (nth_label (ret, 1), ==, "English — Dolby Amateur 5.1");
	g_assert_cmpstr (nth_label (ret, 2), ==, "French");
	g_list_free_full (ret, g_free);

	/* Different languages */
	l = NULL;
	l = g_list_append (l, bvw_lang_info_new (NULL, "eng", "Dolby Amateur 5.1"));
	l = g_list_append (l, bvw_lang_info_new (NULL, "spa", "Dolby Amateur 5.1"));
	l = g_list_append (l, bvw_lang_info_new (NULL, "fre", "Dolby Amateur 5.1"));

	ret = bvw_lang_info_to_menu_labels (l, BVW_TRACK_TYPE_AUDIO);
	g_list_free_full (l, (GDestroyNotify) bacon_video_widget_lang_info_free);

	g_assert_cmpstr (nth_label (ret, 0), ==, "English");
	g_assert_cmpstr (nth_label (ret, 1), ==, "Spanish; Castilian");
	g_assert_cmpstr (nth_label (ret, 2), ==, "French");
	g_list_free_full (ret, g_free);
}

static void
set_labels (GtkWidget  *label,
	    GtkWidget  *label_remaining,
	    gint64      _time,
	    gint64      length,
	    const char *expected,
	    const char *expected_remaining)
{
	const char *str;

	bacon_time_label_set_time (BACON_TIME_LABEL (label), _time, length);
	bacon_time_label_set_time (BACON_TIME_LABEL (label_remaining), _time, length);

	str = bacon_time_label_get_label (BACON_TIME_LABEL (label));
	g_assert_cmpstr (str, ==, expected);

	str = bacon_time_label_get_label (BACON_TIME_LABEL (label_remaining));
	g_assert_cmpstr (str, ==, expected_remaining);
}

static void
test_time_label (void)
{
	GtkWidget *label, *label_remaining;
	char *str;

	label = bacon_time_label_new ();
	label_remaining = bacon_time_label_new ();
	bacon_time_label_set_remaining (BACON_TIME_LABEL (label_remaining), TRUE);

	set_labels (label, label_remaining,
		    0, 1000,
		    "0:00", "-0:01");

	set_labels (label, label_remaining,
		    500, 1000,
		    "0:00", "-0:01");

	set_labels (label, label_remaining,
		    700, 1400,
		    "0:00", "-0:01");

	set_labels (label, label_remaining,
		    1000, 1400,
		    "0:01", "-0:01");

	set_labels (label, label_remaining,
		    0, 45 * 60 * 1000,
		    "0:00", "-45:00");

	set_labels (label, label_remaining,
		    50 * 60 * 1000, 45 * 60 * 1000,
		    "50:00", "--:--");

	bacon_time_label_set_show_msecs (BACON_TIME_LABEL (label), TRUE);
	bacon_time_label_set_show_msecs (BACON_TIME_LABEL (label_remaining), TRUE);

	set_labels (label, label_remaining,
		    0, 1000,
		    "0:00.000", "-0:01.000");

	set_labels (label, label_remaining,
		    500, 1000,
		    "0:00.500", "-0:00.500");

	set_labels (label, label_remaining,
		    700, 1400,
		    "0:00.700", "-0:00.700");

	set_labels (label, label_remaining,
		    1000, 1400,
		    "0:01.000", "-0:00.400");

	set_labels (label, label_remaining,
		    0, 45 * 60 * 1000,
		    "0:00.000", "-45:00.000");

	set_labels (label, label_remaining,
		    50 * 60 * 1000, 45 * 60 * 1000,
		    "50:00.000", "--:--");

	str = totem_time_to_string (0, TOTEM_TIME_FLAG_NONE);
	g_assert_cmpstr (str, ==, "0:00");
	g_free (str);

	str = totem_time_to_string (500, TOTEM_TIME_FLAG_NONE);
	g_assert_cmpstr (str, ==, "0:01");
	g_free (str);

	str = totem_time_to_string (500, TOTEM_TIME_FLAG_REMAINING);
	g_assert_cmpstr (str, ==, "-0:01");
	g_free (str);

	str = totem_time_to_string (1250, TOTEM_TIME_FLAG_NONE);
	g_assert_cmpstr (str, ==, "0:01");
	g_free (str);

	str = totem_time_to_string (1250, TOTEM_TIME_FLAG_REMAINING);
	g_assert_cmpstr (str, ==, "-0:02");
	g_free (str);

	str = totem_time_to_string (1250, TOTEM_TIME_FLAG_MSECS);
	g_assert_cmpstr (str, ==, "0:01.250");
	g_free (str);
}

int main (int argc, char **argv)
{
	setlocale (LC_ALL, "en_GB.UTF-8");

	g_test_init (&argc, &argv, NULL);
	gtk_init (&argc, &argv);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	g_test_add_func ("/menus/lang_info", test_menus_lang_info);
	g_test_add_func ("/osd/time_label", test_time_label);

	return g_test_run ();
}
