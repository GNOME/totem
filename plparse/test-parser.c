#include "config.h"

#include <locale.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgnomevfs/gnome-vfs.h>

#include "totem-pl-parser.h"
#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-private.h"

#define USE_DATA

static GMainLoop *loop = NULL;
static gboolean option_no_recurse = FALSE;
static gboolean option_debug = FALSE;
static gboolean option_data = FALSE;
static gboolean option_force = FALSE;
static gboolean option_disable_unsafe = FALSE;
static gboolean option_duration = FALSE;
static gboolean g_fatal_warnings = FALSE;
static char *option_base_uri = NULL;
static char **files = NULL;

static void
header (const char *message)
{
	g_print ("\n");
	g_print ("###################### %s ################\n", message);
	g_print ("\n");
}

#define error(x...) { g_warning (x); exit(1); }

#if 0
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
#endif

static void
test_resolve_real (const char *base, const char *url, const char *expected)
{
	char *result;

	result = totem_pl_parser_resolve_url (base, url);
	if (result == NULL)
		error ("NULL output resolving '%s' with base '%s'", url, base);
	if (strcmp (result, expected) != 0)
		error ("Resolving '%s' with base '%s', different results than expected:\n'%s' instead of '%s'",
		       url, base, result, expected);
	g_print ("Resolved: '%s' with base '%s' to '%s'\n", url, base, result);
	g_free (result);
}

static void
test_resolve (void)
{
	header ("Resolve URL");

	test_resolve_real ("http://localhost:12345/foobar", "/leopard.mov", "http://localhost:12345/leopard.mov");
	test_resolve_real ("file:///home/hadess/Movies", "Movies/mymovie.mov", "file:///home/hadess/Movies/Movies/mymovie.mov");
	test_resolve_real ("http://localhost/video.dir/video.mpg?param1=foo&param2=bar", "dir/image.jpg", "http://localhost/video.dir/dir/image.jpg");
	test_resolve_real ("http://movies.apple.com/movies/us/apple/ipoditunes/2007/touch/features/apple_ipodtouch_safari_r640-9cie.mov", "/movies/us/apple/ipoditunes/2007/touch/features/apple_ipodtouch_safari_i320x180.m4v", "http://movies.apple.com/movies/us/apple/ipoditunes/2007/touch/features/apple_ipodtouch_safari_i320x180.m4v");
}

static void
test_duration_real (const char *duration, gint64 expected)
{
	gint64 res;

	res = totem_pl_parser_parse_duration (duration, option_debug);
	if (res != expected)
		error ("Error parsing '%s' to %"G_GINT64_FORMAT" secs, got %"G_GINT64_FORMAT" secs",
		       duration ? duration : "(null)", expected, res);
	g_print ("Parsed '%s' to %"G_GINT64_FORMAT" secs\n", duration ? duration : "(null)", res);
}

static void
test_duration (void)
{
	header ("Duration string parsing");

	test_duration_real ("500", 500);
	test_duration_real ("01:01", 61);
	test_duration_real ("00:00:00.01", 1);
	test_duration_real ("01:00:01.01", 3601);
	test_duration_real ("01:00.01", 60);
}

#define MAX_DESCRIPTION_LEN 128

static void
entry_metadata_foreach (const char *key,
			const char *value,
			gpointer data)
{
	if (g_ascii_strcasecmp (key, TOTEM_PL_PARSER_FIELD_URL) == 0)
		return;
	if (g_ascii_strcasecmp (key, TOTEM_PL_PARSER_FIELD_DESCRIPTION) == 0
	    && strlen (value) > MAX_DESCRIPTION_LEN) {
		char *tmp = g_strndup (value, MAX_DESCRIPTION_LEN), *s;
		for (s = tmp; s - tmp < MAX_DESCRIPTION_LEN; s++)
			if (*s == '\n' || *s == '\r') {
				*s = '\0';
				break;
			}
		g_print ("\t%s = '%s' (truncated)\n", key, tmp);
		return;
	}
	g_print ("\t%s = '%s'\n", key, value);
}

static void
entry_parsed (TotemPlParser *parser, const char *uri,
	      GHashTable *metadata, gpointer data)
{
	g_print ("added URI '%s'\n", uri);
	g_hash_table_foreach (metadata, (GHFunc) entry_metadata_foreach, NULL);
}

static void
test_parsing_real (TotemPlParser *pl, const char *url)
{
	TotemPlParserResult res;

	res = totem_pl_parser_parse_with_base (pl, url, option_base_uri, FALSE);
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
playlist_started (TotemPlParser *parser, const char *uri, GHashTable *metadata)
{
	g_print ("Started playlist '%s'\n", uri);
	g_hash_table_foreach (metadata, (GHFunc) entry_metadata_foreach, NULL);
	g_print ("\n");
}

static void
playlist_ended (TotemPlParser *parser, const char *uri)
{
	g_print ("Playlist '%s' ended\n", uri);
	g_print ("\n");
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
	g_signal_connect (G_OBJECT (pl), "entry-parsed", G_CALLBACK (entry_parsed), NULL);
	g_signal_connect (G_OBJECT (pl), "playlist-started", G_CALLBACK (playlist_started), NULL);
	g_signal_connect (G_OBJECT (pl), "playlist-ended", G_CALLBACK (playlist_ended), NULL);

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
		{ "base-uri", 'b', 0, G_OPTION_ARG_STRING, &option_base_uri, "Base URI to resolve relative items from", NULL },
		{ "duration", 0, 0, G_OPTION_ARG_NONE, &option_duration, "Run duration test", NULL },
		{ "g-fatal-warnings", 0, 0, G_OPTION_ARG_NONE, &g_fatal_warnings, "Make all warnings fatal", NULL },
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &files, NULL, "[URI...]" },
		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
	gboolean retval;

	setlocale (LC_ALL, "");

	g_thread_init (NULL);

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

	if (g_fatal_warnings) {
		GLogLevelFlags fatal_mask;

		fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
		fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
		g_log_set_always_fatal (fatal_mask);
	}

	if (option_data != FALSE && files == NULL) {
		g_message ("Please pass specific files to check by data");
		return 1;
	}

	if (option_duration != FALSE) {
		if (files == NULL) {
			test_duration ();
			return 0;
		} else {
			guint i;

			for (i = 0; files[i] != NULL; i++) {
				g_print ("Parsed '%s' to %"G_GINT64_FORMAT" secs\n",
					 files[i],
					 totem_pl_parser_parse_duration (files[i], option_debug));
			}

			return 0;
		}
	}

	if (files == NULL) {
		test_duration ();
		test_resolve ();
#if 0
		test_relative ();
#endif
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
