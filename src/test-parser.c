
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>

#include "totem-pl-parser.c"

static GMainLoop *loop = NULL;

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
	g_print ("added URI '%s' with title '%s'\n", uri,
			title ? title : "empty");
}

static void
test_parsing_real (TotemPlParser *pl, const char *url)
{
	totem_pl_parser_parse (pl, url, FALSE);
}

static gboolean
push_parser (gpointer data)
{
	TotemPlParser *pl = (TotemPlParser *)data;
	test_parsing_real (pl, "/mnt/cdrom");
	g_main_loop_quit (loop);
	return FALSE;
}

static void
test_parsing (void)
{
	TotemPlParser *pl = totem_pl_parser_new ();
	g_signal_connect (G_OBJECT (pl), "entry", G_CALLBACK (entry_added), NULL);

	header ("parsing");
	g_timeout_add (1000, push_parser, pl);
	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
}

int main (int argc, char **argv)
{
	gnome_vfs_init();

	test_relative ();

	test_parsing ();

	return 0;
}

