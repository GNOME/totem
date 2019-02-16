#include "config.h"

#include <locale.h>
#define GST_USE_UNSTABLE_API 1
#include <gst/tag/tag.h>

#include "backend/bacon-video-widget.h"
#include "totem-menu.h"

static BvwLangInfo *
bvw_lang_info_new (const char *language,
		   const char *codec)
{
	BvwLangInfo *info;

	info = g_new0 (BvwLangInfo, 1);
	info->language = g_strdup (language);
	info->codec = g_strdup (codec);
	return info;
}

static void
test_menus_lang_info (void)
{
	GList *l, *ret;

	/* Same language, same codecs */
	l = NULL;
	l = g_list_append (l, bvw_lang_info_new ("eng", "Dolby Pro Racing"));
	l = g_list_append (l, bvw_lang_info_new ("eng", "Dolby Pro Racing"));
	l = g_list_append (l, bvw_lang_info_new ("fre", "Dolby Amateur 5.1"));

	ret = bvw_lang_info_to_menu_labels (l);
	g_list_free_full (l, (GDestroyNotify) bacon_video_widget_lang_info_free);

	g_assert_cmpstr (g_list_nth_data (ret, 0), ==, "English #1");
	g_assert_cmpstr (g_list_nth_data (ret, 1), ==, "English #2");
	g_assert_cmpstr (g_list_nth_data (ret, 2), ==, "French");
	g_list_free_full (ret, g_free);

	/* Same language, different codecs */
	l = NULL;
	l = g_list_append (l, bvw_lang_info_new ("eng", "Dolby Pro Racing"));
	l = g_list_append (l, bvw_lang_info_new ("eng", "Dolby Amateur 5.1"));
	l = g_list_append (l, bvw_lang_info_new ("fre", "Dolby Amateur 5.1"));

	ret = bvw_lang_info_to_menu_labels (l);
	g_list_free_full (l, (GDestroyNotify) bacon_video_widget_lang_info_free);

	g_assert_cmpstr (g_list_nth_data (ret, 0), ==, "English — Dolby Pro Racing");
	g_assert_cmpstr (g_list_nth_data (ret, 1), ==, "English — Dolby Amateur 5.1");
	g_assert_cmpstr (g_list_nth_data (ret, 2), ==, "French");
	g_list_free_full (ret, g_free);

	/* Different languages */
	l = NULL;
	l = g_list_append (l, bvw_lang_info_new ("eng", "Dolby Amateur 5.1"));
	l = g_list_append (l, bvw_lang_info_new ("spa", "Dolby Amateur 5.1"));
	l = g_list_append (l, bvw_lang_info_new ("fre", "Dolby Amateur 5.1"));

	ret = bvw_lang_info_to_menu_labels (l);
	g_list_free_full (l, (GDestroyNotify) bacon_video_widget_lang_info_free);

	g_assert_cmpstr (g_list_nth_data (ret, 0), ==, "English");
	g_assert_cmpstr (g_list_nth_data (ret, 1), ==, "Spanish; Castilian");
	g_assert_cmpstr (g_list_nth_data (ret, 2), ==, "French");
	g_list_free_full (ret, g_free);
}

int main (int argc, char **argv)
{
	setlocale (LC_ALL, "en_GB.UTF-8");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	g_test_add_func ("/menus/lang_info", test_menus_lang_info);

	return g_test_run ();
}
