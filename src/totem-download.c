/*
 * Copyright (C) 2003 Bastien Nocera <hadess@hadess.net>
 *
 * This code downloads, extracts and installs Microsoft Win32 codecs
 * given a FourCC identifier
 *
 * Explanations from:
 * http://www.mplayerhq.hu/pipermail/mplayer-dev-eng/2002-March/006058.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>

#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <glib.h>
#include <curl/curl.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-program.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgnomevfs/gnome-vfs-xfer.h>
#include <libgnomevfs/gnome-vfs-init.h>

/* #define DEBUG */

#ifndef DEBUG
#define USER_AGENT "Windows-Media-Player/8.00.00.4477"
#define CODECS_URL "http://codecs.microsoft.com/isapi/ocget.dll"
#define ACTIVEX_URL "http://activex.microsoft.com/objects/ocget.dll"
#else
#define USER_AGENT "Totem-Tester/1.0.0.0.0.0"
#define CODECS_URL "http://localhost/tmp/cinepak.cab"
#define ACTIVEX_URL "http://localhost/tmp/cinepak.cab"
#endif


typedef enum {
	CONNECTING,
	DOWNLOADING,
	INSTALLING,
	DONE
} Labels;


typedef struct {
	GladeXML *xml;
	GtkWidget *dialog;
	guint32 video_fcc, audio_fcc;
	FILE *f;
	gboolean cancelled;
	gboolean completed;
	char *dirpath;
} TotemDownload;

static void
set_label (TotemDownload *td, Labels label, gboolean done)
{
	GtkWidget *image;
	char *name;

	label++;

	name = g_strdup_printf ("image%d", label);
	image = glade_xml_get_widget (td->xml, name);
	g_free (name);

	if (done == TRUE)
	{
		gtk_image_set_from_stock (GTK_IMAGE (image),
				GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON);
	} else {
		gtk_image_set_from_stock (GTK_IMAGE (image),
				GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_BUTTON);
	}
}

static char *
msid_from_fourcc (guint32 f)
{
	char fcc[5];

	memset(&fcc, 0, sizeof(fcc));

	/* Should we take care about endianess ? */
	fcc[0] = f     | 0xFFFFFF00;
	fcc[1] = f>>8  | 0xFFFFFF00;
	fcc[2] = f>>16 | 0xFFFFFF00;
	fcc[3] = f>>24 | 0xFFFFFF00;
	fcc[4] = 0;

	return g_strdup_printf ("%x%x%x%x-0000-0010-8000-00AA00389B71",
			fcc[3], fcc[2], fcc[1], fcc[0]);
}

static size_t
write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	TotemDownload *td = (TotemDownload *) userp;
	return fwrite (buffer, size, nmemb, td->f);
}

int
progress_cb (void *clientp, double dltotal, double dlnow,
		double ultotal, double ulnow)
{
	TotemDownload *td = (TotemDownload *) clientp;

	while (gtk_events_pending ())
		gtk_main_iteration ();

	return td->cancelled;
}

static int
cab_extract (TotemDownload *td, const char *path)
{
	const char *name;
	char *cmdline, *destdir;
	int err;
	GDir *dir;

	set_label (td, INSTALLING, FALSE);

	/* The -d doesn't work very well... */
	chdir (td->dirpath);
	cmdline = g_strdup_printf ("cabextract -q -L %s", path);

	g_spawn_command_line_sync (cmdline, NULL, NULL, &err, NULL);
	g_free (cmdline);

	unlink (path);
	dir = g_dir_open (td->dirpath, 0, NULL);
	if (dir == NULL)
		return -1;

	destdir = g_build_filename (G_DIR_SEPARATOR_S,
			g_get_home_dir (),
			".gnome2", "totem-addons", NULL);

	name = g_dir_read_name (dir);
	while (name != NULL)
	{
		char *filename;

		filename = g_build_filename (G_DIR_SEPARATOR_S,
				td->dirpath, name, NULL);

		/* This is not solid, but MS only knows DOS-like 8.3
		 * filenames anyway */
		if (strstr (name, ".inf") != NULL)
		{
			unlink (filename);
		} else {
			GnomeVFSResult res;
			char *destfile;
			GnomeVFSURI *origuri, *desturi;

			destfile = g_build_filename (G_DIR_SEPARATOR_S,
					destdir, name, NULL);

			origuri = gnome_vfs_uri_new (filename);
			desturi = gnome_vfs_uri_new (destfile);
			res = gnome_vfs_xfer_uri
				(origuri, desturi,
				 GNOME_VFS_XFER_REMOVESOURCE,
				 GNOME_VFS_XFER_ERROR_MODE_ABORT,
				 GNOME_VFS_XFER_OVERWRITE_MODE_ABORT,
				 NULL, NULL);
			if (res != GNOME_VFS_OK)
				g_message ("res: %s", gnome_vfs_result_to_string (res));

			gnome_vfs_uri_unref (origuri);
			gnome_vfs_uri_unref (desturi);
			g_free (destfile);
		}

		g_free (filename);

		name = g_dir_read_name (dir);
	}

	g_dir_close (dir);
	g_free (destdir);

	set_label (td, INSTALLING, TRUE);

	return 0;
}

static void
totem_download_free (TotemDownload *td)
{
	rmdir (td->dirpath);

	g_object_unref (G_OBJECT (td->xml));
	gtk_widget_destroy (td->dialog);
	g_free (td->dirpath);
	g_free (td);
}

static int
cab_url_from_fourcc_with_url (TotemDownload *td, const char *path,
		const char *url, guint32 fcc)
{
	CURL *c;
	char *fourcc, *msid;
	int ret;
	FILE *file;

	set_label (td, CONNECTING, FALSE);

	c = curl_easy_init ();
	if (c == NULL)
		return -1;

	fourcc = msid_from_fourcc (fcc);
	msid = g_strdup_printf ("CLSID={%s}", fourcc);
	g_free (fourcc);

	//FIXME set the proxies
#ifndef DEBUG
	curl_easy_setopt (c, CURLOPT_POSTFIELDS, msid);
#endif
	curl_easy_setopt (c, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
	curl_easy_setopt (c, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt (c, CURLOPT_MAXREDIRS, 1);
	curl_easy_setopt (c, CURLOPT_URL, url);

	file = fopen (path, "w+");

	if (file == NULL)
	{
		curl_easy_cleanup (c);
		g_free (msid);
		return -1;
	}

	td->f = file;
	curl_easy_setopt (c, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt (c, CURLOPT_WRITEDATA, td);
	curl_easy_setopt (c, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt (c, CURLOPT_PROGRESSFUNCTION, progress_cb);
	curl_easy_setopt (c, CURLOPT_PROGRESSDATA, td);

	set_label (td, CONNECTING, TRUE);
	set_label (td, DOWNLOADING, FALSE);
	ret = curl_easy_perform (c);
	if (ret == 0)
		set_label (td, DOWNLOADING, TRUE);

	curl_easy_cleanup (c);

	fclose (td->f);
	g_free (msid);

	/* If the file is empty, remove it and return -1 */
	if (ret == 0)
	{
		struct stat st;

		stat (path, &st);
		if (st.st_size == 0)
		{
			unlink (path);
			ret = -1;
		}
	}

	return ret;
}

static int
cab_download_from_td_try (TotemDownload *td, const char *path, guint32 fcc)
{
	int i, retval;

	for (i = 0; i < 3; i++)
	{
		set_label (td, CONNECTING, FALSE);

		retval = cab_url_from_fourcc_with_url (td, path,
				ACTIVEX_URL, fcc);
		if (retval == 0)
		{
			retval = cab_extract (td, path);
			return retval;
		}
	}

	for (i = 0; i < 3; i++)
	{
		set_label (td, CONNECTING, FALSE);

		retval = cab_url_from_fourcc_with_url (td, path,
				CODECS_URL, fcc);
		if (retval == 0)
		{
			retval = cab_extract (td, path);
			return retval;
		}
	}

	return -1;
}

static int
cab_download (TotemDownload *td)
{
	int retval;
	char *filepath;
	char *template;

	retval = 0;

	template = g_build_filename (G_DIR_SEPARATOR_S, g_get_tmp_dir (),
			"totemXXXXXX", NULL);
	td->dirpath = mktemp (template);
	if (mkdir (td->dirpath, 0700) == -1)
		return -1;

	if (td->video_fcc != 0)
	{
		filepath = g_build_filename (G_DIR_SEPARATOR_S, td->dirpath,
				"totem_v.cab", NULL);
		if (cab_download_from_td_try (td, filepath, td->video_fcc) < 0)
			retval = -1;
		g_free (filepath);
	}

	if (td->audio_fcc != 0)
	{
		filepath = g_build_filename (G_DIR_SEPARATOR_S, td->dirpath,
				"totem_a.cab", NULL);
		if (cab_download_from_td_try (td, filepath, td->audio_fcc) < 0)
		{
			if (retval == 0)
				retval = -1;
		}
		g_free (filepath);
	}

	set_label (td, DONE, TRUE);

	return retval;
}

int
totem_download_from_fourcc (GtkWindow *parent,
		guint32 video_fcc, guint32 audio_fcc)
{
	GladeXML *xml;
	TotemDownload *td;
	char *filename;
	int retval;

	filename = gnome_program_locate_file (NULL,
			GNOME_FILE_DOMAIN_APP_DATADIR,
			"totem/totem-download.glade", FALSE, NULL);
	xml = glade_xml_new (filename, NULL, NULL);
	g_free (filename);

	if (xml == NULL)
	{
		//FIXME
		return FALSE;
	}

	td = g_new0 (TotemDownload, 1);
	td->video_fcc = video_fcc;
	td->audio_fcc = audio_fcc;
	td->xml = xml;

	td->dialog = glade_xml_get_widget (xml, "td-dialog");
	gtk_window_set_transient_for (GTK_WINDOW (td->dialog), parent);
	gtk_window_set_modal (GTK_WINDOW (td->dialog), TRUE);

	set_label (td, CONNECTING, FALSE);

	gtk_widget_show_all (td->dialog);

	retval = cab_download (td);
	totem_download_free (td);

	return retval;
}

