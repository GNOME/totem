
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>

#include "totem-pl-parser.c"

static GMainLoop *loop = NULL;
static GList *list = NULL;

static void
header (const char *message)
{
	g_print ("\n");
	g_print ("###################### %s ################\n", message);
	g_print ("\n");
}

static void
test_relative_real (const char *url, const char *output)
{
	char *base, *dos;

	g_print ("url: %s\n", url);
	g_print ("output: %s\n", output);
	base = totem_pl_parser_relative (url, output);
	if (base) {
		g_print ("relative path: %s\n", base);
	} else {
		g_print ("no relative path\n");
	}
	dos = totem_pl_parser_url_to_dos (url, output);
	g_print ("DOS path: %s\n", dos);
	g_print ("\n");

	g_free (base);
	g_free (dos);
}

static void
test_relative (void)
{
	header ("relative");

	test_relative_real ("/home/hadess/test/test file.avi",
			"/home/hadess/foobar.m3u");
	test_relative_real ("file:///home/hadess/test/test%20file.avi",
			"/home/hadess/whatever.m3u");
	test_relative_real ("smb://server/share/file.mp3",
			"/home/hadess/whatever again.m3u");
	test_relative_real ("smb://server/share/file.mp3",
			"smb://server/share/file.m3u");
	test_relative_real ("/home/hadess/test.avi",
			"/home/hadess/test/file.m3u");
	test_relative_real ("http://foobar.com/test.avi",
			"/home/hadess/test/file.m3u");
}

static void
entry_added (TotemPlParser *parser, const char *uri, const char *title,
		const char *genre, gpointer data)
{
	g_print ("added URI '%s' with title '%s' genre '%s'\n", uri,
			title ? title : "empty", genre);
}

static void
test_parsing_real (TotemPlParser *pl, const char *url)
{
	TotemPlParserResult res;

	res = totem_pl_parser_parse (pl, url, FALSE);
	if (res != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		switch (res) {
		case TOTEM_PL_PARSER_RESULT_UNHANDLED:
			g_print ("url '%s' unhandled\n", url);
			break;
		case TOTEM_PL_PARSER_RESULT_ERROR:
			g_print ("error handling url '%s'\n", url);
			break;
		case TOTEM_PL_PARSER_RESULT_IGNORED:
			g_print ("ignored url '%s'\n", url);
			break;
		default:
			g_assert_not_reached ();
			;;
		}
	}
}

static gboolean
push_parser (gpointer data)
{
	TotemPlParser *pl = (TotemPlParser *)data;

	if (list != NULL) {
		GList *l;

		for (l = list; l != NULL; l = l->next) {
			test_parsing_real (pl, l->data);
		}
		g_list_free (list);
	} else {
		//test_parsing_real (pl, "file:///mnt/cdrom");
		test_parsing_real (pl, "file:///home/hadess/Movies");
		/* Bugzilla 158052, 404 */
		test_parsing_real (pl, "http://live.hujjat.org:7860/main");
		/* Bugzilla 323683 */
		test_parsing_real (pl, "http://www.comedycentral.com/sitewide/media_player/videoswitcher.jhtml?showid=934&category=/shows/the_daily_show/videos/headlines&sec=videoId%3D36032%3BvideoFeatureId%3D%3BpoppedFrom%3D_shows_the_daily_show_index.jhtml%3BisIE%3Dfalse%3BisPC%3Dtrue%3Bpagename%3Dmedia_player%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bgateway%3Dshows%3Bsection_1%3Dthe_daily_show%3Bsection_2%3Dvideos%3Bsection_3%3Dheadlines%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bera%3D%27%2Bif_nt_era%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bfla%3D%27%2Bif_nt_Flash%2B%27&itemid=36032&clip=com/dailyshow/headlines/10156_headline.wmv&mswmext=.asx");
	}
	g_main_loop_quit (loop);
	return FALSE;
}

static void
test_parsing (void)
{
	TotemPlParser *pl = totem_pl_parser_new ();
	g_signal_connect (G_OBJECT (pl), "entry", G_CALLBACK (entry_added), NULL);

	header ("parsing");
	g_idle_add (push_parser, pl);
	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
}

int main (int argc, char **argv)
{
	gnome_vfs_init();

	if (argc == 0) {
		test_relative ();
		test_parsing ();
	} else {
		int i;
		for (i = 1; i < argc; i++) {
			list = g_list_prepend (list, argv[i]);
		}
		list = g_list_reverse (list);
		test_parsing ();
	}

	return 0;
}

