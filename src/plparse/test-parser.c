
#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>

#include "totem-pl-parser.c"

static void
test (const char *url, const char *output)
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

int main (int argc, char **argv)
{
	gnome_vfs_init();

	test ("/home/hadess/test/test file.avi",
			"/home/hadess/foobar.m3u");
	test ("file:///home/hadess/test/test%20file.avi",
			"/home/hadess/whatever.m3u");
	test ("smb://server/share/file.mp3",
			"/home/hadess/whatever again.m3u");
	test ("smb://server/share/file.mp3",
			"smb://server/share/file.m3u");
	test ("/home/hadess/test.avi",
			"/home/hadess/test/file.m3u");

	return 0;
}

