
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>

#include "totem-pl-parser.c"

#define USE_DATA

static GMainLoop *loop = NULL;
static gboolean option_no_recurse = FALSE;
static gboolean option_debug = FALSE;
static gboolean option_data = FALSE;
static gboolean option_force = FALSE;
static gboolean option_disable_unsafe = FALSE;
static char **files = NULL;

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
			title ? title : "empty", genre ? genre : "empty");
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

	if (files != NULL) {
		guint i;

		for (i = 0; files[i] != NULL; ++i) {
			test_parsing_real (pl, files[i]);
		}
	} else {
		//test_parsing_real (pl, "file:///mnt/cdrom");
		test_parsing_real (pl, "file:///home/hadess/Movies");
		/* Bugzilla 158052, 404 */
		test_parsing_real (pl, "http://live.hujjat.org:7860/main");
		/* Bugzilla 330120 */
		test_parsing_real (pl, "file:///tmp/file_doesnt_exist.wmv");
		/* Bugzilla 323683 */
		test_parsing_real (pl, "http://www.comedycentral.com/sitewide/media_player/videoswitcher.jhtml?showid=934&category=/shows/the_daily_show/videos/headlines&sec=videoId%3D36032%3BvideoFeatureId%3D%3BpoppedFrom%3D_shows_the_daily_show_index.jhtml%3BisIE%3Dfalse%3BisPC%3Dtrue%3Bpagename%3Dmedia_player%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bgateway%3Dshows%3Bsection_1%3Dthe_daily_show%3Bsection_2%3Dvideos%3Bsection_3%3Dheadlines%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bera%3D%27%2Bif_nt_era%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bfla%3D%27%2Bif_nt_Flash%2B%27&itemid=36032&clip=com/dailyshow/headlines/10156_headline.wmv&mswmext=.asx");
	}
	g_main_loop_quit (loop);
	return FALSE;
}

#ifdef USE_DATA

#define READ_CHUNK_SIZE 8192
#define MIME_READ_CHUNK_SIZE 1024

static char *
test_data_get_data (const char *uri, guint *len)
{
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	char *buffer;
	GnomeVFSFileSize total_bytes_read;
	GnomeVFSFileSize bytes_read;

	*len = 0;

	/* Open the file. */
	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK)
		return NULL;

	/* Read the whole thing, up to MIME_READ_CHUNK_SIZE */
	buffer = NULL;
	total_bytes_read = 0;
	do {
		buffer = g_realloc (buffer, total_bytes_read
				+ MIME_READ_CHUNK_SIZE);
		result = gnome_vfs_read (handle,
				buffer + total_bytes_read,
				MIME_READ_CHUNK_SIZE,
				&bytes_read);
		if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
			g_free (buffer);
			gnome_vfs_close (handle);
			return NULL;
		}

		/* Check for overflow. */
		if (total_bytes_read + bytes_read < total_bytes_read) {
			g_free (buffer);
			gnome_vfs_close (handle);
			return NULL;
		}

		total_bytes_read += bytes_read;
	} while (result == GNOME_VFS_OK
			&& total_bytes_read < MIME_READ_CHUNK_SIZE);

	/* Close the file but don't overwrite the possible error */
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF)
		gnome_vfs_close (handle);
	else
		result = gnome_vfs_close (handle);

	if (result != GNOME_VFS_OK) {
		g_message ("URL '%s' couldn't be read or closed in _get_mime_type_with_data: '%s'\n", uri, gnome_vfs_result_to_string (result));
		g_free (buffer);
		return NULL;
	}

	/* Return the file null-terminated. */
	buffer = g_realloc (buffer, total_bytes_read + 1);
	buffer[total_bytes_read] = '\0';
	*len = total_bytes_read;

	return buffer;
}
#endif /* USE_DATA */

static void
test_data (void)
{
	guint i;

	for (i = 0; files[i] != NULL; ++i) {
		char *filename = files[i];
		gboolean retval;
#ifdef USE_DATA
		char *data;
		guint len;

		data = test_data_get_data (filename, &len);
		if (data == NULL) {
			g_message ("Couldn't get data for %s", filename);
			continue;
		}
		retval = totem_pl_parser_can_parse_from_data (data, len, TRUE);
		g_free (data);
#else
		retval = totem_pl_parser_can_parse_from_filename (filename, TRUE);
#endif /* USE_DATA */

		if (retval != FALSE) {
			g_message ("IS a playlist: %s", filename);
		} else {
			g_message ("ISNOT playlist: %s", filename);
		}
	}
}

static void
playlist_started (TotemPlParser *parser, const char *title)
{
	g_message ("Playlist with name '%s' started", title);
}

static void
playlist_ended (TotemPlParser *parser, const char *title)
{
	g_message ("Playlist with name '%s' ended", title);
}

static void
test_parsing (void)
{
	TotemPlParser *pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", !option_no_recurse,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);
	g_signal_connect (G_OBJECT (pl), "entry", G_CALLBACK (entry_added), NULL);
	g_signal_connect (G_OBJECT (pl), "playlist-start", G_CALLBACK (playlist_started), NULL);
	g_signal_connect (G_OBJECT (pl), "playlist-end", G_CALLBACK (playlist_ended), NULL);

	header ("parsing");
	g_idle_add (push_parser, pl);
	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
}

int main (int argc, char **argv)
{
	GOptionEntry option_entries [] =
	{
		{ "no-recurse", 'n', 0, G_OPTION_ARG_NONE, &option_no_recurse, "Disable recursion", NULL },
		{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug, "Enable debug", NULL },
		{ "data", 't', 0, G_OPTION_ARG_NONE, &option_data, "Use data instead of filename", NULL },
		{ "force", 'f', 0, G_OPTION_ARG_NONE, &option_force, "Force parsing", NULL },
		{ "disable-unsafe", 'u', 0, G_OPTION_ARG_NONE, &option_disable_unsafe, "Disabling unsafe playlist-types", NULL },
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, NULL, "[FILE...]" },
		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
	gboolean retval;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, option_entries, NULL);

	retval = g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);

	if (!retval) {
		g_print ("Error parsing arguments: %s\n", error->message);
		g_error_free (error);

		g_print ("Usage: %s <-n | --no-recurse> <-d | --debug> <-h | --help> <-t | --data > <-u | --disable-unsafe> <url>\n", argv[0]);
		exit (1);
	}

	gnome_vfs_init();

	if (option_data != FALSE && files == NULL) {
		g_message ("Please pass specific files to check by data");
		return 1;
	}

	if (files == NULL) {
		test_relative ();
		test_parsing ();
	} else {
		if (option_data) {
			test_data ();
		} else {
			test_parsing ();
		}
	}

	return 0;
}
