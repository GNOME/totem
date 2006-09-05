/* 
   Copyright (C) 2002, 2003, 2004, 2005, 2006 Bastien Nocera
   Copyright (C) 2003, 2004 Colin Walters <walters@rhythmbox.org>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#ifndef TOTEM_PL_PARSER_MINI
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"

#define READ_CHUNK_SIZE 8192
#define MIME_READ_CHUNK_SIZE 1024
#define RECURSE_LEVEL_MAX 4
#define DIR_MIME_TYPE "x-directory/normal"
#define BLOCK_DEVICE_TYPE "x-special/device-block"
#define EXTINF "#EXTINF:"
#define DEBUG(x) { if (parser->priv->debug) x; }

typedef gboolean (*PlaylistIdenCallback) (const char *data, gsize len);

#ifndef TOTEM_PL_PARSER_MINI
typedef TotemPlParserResult (*PlaylistCallback) (TotemPlParser *parser, const char *url, gpointer data);
#endif

typedef struct {
	char *mimetype;
#ifndef TOTEM_PL_PARSER_MINI
	PlaylistCallback func;
#endif
	PlaylistIdenCallback iden;
} PlaylistTypes;

static gboolean totem_pl_parser_is_ra (const char *data, gsize len);
static gboolean totem_pl_parser_is_asf (const char *data, gsize len);
static gboolean totem_pl_parser_is_quicktime (const char *data, gsize len);

#ifndef TOTEM_PL_PARSER_MINI

static gboolean totem_pl_parser_scheme_is_ignored (TotemPlParser *parser, const char *url);
static gboolean totem_pl_parser_ignore (TotemPlParser *parser, const char *url);
static TotemPlParserResult totem_pl_parser_parse_internal (TotemPlParser *parser, const char *url);
static TotemPlParserResult totem_pl_parser_add_asx (TotemPlParser *parser, const char *url, gpointer data);

struct TotemPlParserPrivate
{
	GList *ignore_schemes;
	GList *ignore_mimetypes;
	guint recurse_level;
	guint fallback : 1;
	guint recurse : 1;
	guint debug : 1;
};

static void totem_pl_parser_set_property (GObject *object,
					  guint prop_id,
					  const GValue *value,
					  GParamSpec *pspec);
static void totem_pl_parser_get_property (GObject *object,
					  guint prop_id,
					  GValue *value,
					  GParamSpec *pspec);

enum {
	PROP_NONE,
	PROP_RECURSE,
	PROP_DEBUG
};

/* Signals */
enum {
	ENTRY,
	PLAYLIST_START,
	PLAYLIST_END,
	LAST_SIGNAL
};

static int totem_pl_parser_table_signals[LAST_SIGNAL];
static gboolean i18n_done = FALSE;

static void totem_pl_parser_class_init (TotemPlParserClass *class);
static void totem_pl_parser_init       (TotemPlParser      *parser);
static void totem_pl_parser_finalize   (GObject *object);

G_DEFINE_TYPE(TotemPlParser, totem_pl_parser, G_TYPE_OBJECT)

static void
totem_pl_parser_class_init (TotemPlParserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = totem_pl_parser_finalize;
	object_class->set_property = totem_pl_parser_set_property;
	object_class->get_property = totem_pl_parser_get_property;

	/* properties */
	g_object_class_install_property (object_class,
					 PROP_RECURSE,
					 g_param_spec_boolean ("recurse",
							       "recurse",
							       "Whether or not to process URLs further", 
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
					 PROP_DEBUG,
					 g_param_spec_boolean ("debug",
							       "debug",
							       "Whether or not to enable debugging output", 
							       FALSE,
							       G_PARAM_READWRITE));

	/* Signals */
	totem_pl_parser_table_signals[ENTRY] =
		g_signal_new ("entry",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemPlParserClass, entry),
			      NULL, NULL,
			      totemplparser_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	totem_pl_parser_table_signals[PLAYLIST_START] =
		g_signal_new ("playlist-start",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemPlParserClass, playlist_start),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	totem_pl_parser_table_signals[PLAYLIST_END] =
		g_signal_new ("playlist-end",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemPlParserClass, playlist_end),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
totem_pl_parser_set_property (GObject *object,
			      guint prop_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	TotemPlParser *parser = TOTEM_PL_PARSER (object);

	switch (prop_id)
	{
	case PROP_RECURSE:
		parser->priv->recurse = g_value_get_boolean (value) != FALSE;
		break;
	case PROP_DEBUG:
		parser->priv->debug = g_value_get_boolean (value) != FALSE;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
totem_pl_parser_get_property (GObject *object,
			      guint prop_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	TotemPlParser *parser = TOTEM_PL_PARSER (object);

	switch (prop_id)
	{
	case PROP_RECURSE:
		g_value_set_boolean (value, parser->priv->recurse);
		break;
	case PROP_DEBUG:
		g_value_set_boolean (value, parser->priv->debug);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

GQuark
totem_pl_parser_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("totem_pl_parser_error");

	return quark;
}

static void
totem_pl_parser_init_i18n (void)
{
	if (i18n_done == FALSE) {
		/* set up translation catalog */
		bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
		i18n_done = TRUE;
	}
}

TotemPlParser *
totem_pl_parser_new (void)
{
	totem_pl_parser_init_i18n ();
	return TOTEM_PL_PARSER (g_object_new (TOTEM_TYPE_PL_PARSER, NULL));
}

static const char *
my_gnome_vfs_get_mime_type_with_data (const char *uri, gpointer *data, TotemPlParser *parser)
{
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	char *buffer;
	const char *mimetype;
	GnomeVFSFileSize total_bytes_read;
	GnomeVFSFileSize bytes_read;

	*data = NULL;

	/* Stat for a block device, we're screwed as far as speed
	 * is concerned now */
	if (g_str_has_prefix (uri, "file://") != FALSE) {
		struct stat buf;
		if (stat (uri + strlen ("file://"), &buf) == 0) {
			if (S_ISBLK (buf.st_mode))
				return BLOCK_DEVICE_TYPE;
		}
	}

	/* Open the file. */
	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		if (result == GNOME_VFS_ERROR_IS_DIRECTORY)
			return DIR_MIME_TYPE;
		DEBUG(g_print ("URL '%s' couldn't be opened in _get_mime_type_with_data: '%s'\n", uri, gnome_vfs_result_to_string (result)));
		return NULL;
	}
	DEBUG(g_print ("URL '%s' was opened successfully in _get_mime_type_with_data:\n", uri));

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
		DEBUG(g_print ("URL '%s' couldn't be read or closed in _get_mime_type_with_data: '%s'\n", uri, gnome_vfs_result_to_string (result)));
		g_free (buffer);
		return NULL;
	}

	/* Return the file null-terminated. */
	buffer = g_realloc (buffer, total_bytes_read + 1);
	buffer[total_bytes_read] = '\0';
	*data = buffer;

	mimetype = gnome_vfs_get_mime_type_for_data (*data, total_bytes_read);

	return mimetype;
}

static char *
totem_pl_parser_read_entire_file (const char *url, int *size)
{
	char *contents;

	if (gnome_vfs_read_entire_file (url, size, &contents) != GNOME_VFS_OK)
		return NULL;

	contents = g_realloc ((gpointer) contents, *size + 1);
	contents[*size] = '\0';

	return contents;
}

static char*
totem_pl_parser_base_url (const char *url)
{
	/* Yay, let's reconstruct the base by hand */
	GnomeVFSURI *uri, *parent;
	char *base;

	uri = gnome_vfs_uri_new (url);
	if (uri == NULL)
		return NULL;

	parent = gnome_vfs_uri_get_parent (uri);
	if (!parent) {
		parent = uri;
	}
	base = gnome_vfs_uri_to_string (parent, 0);

	gnome_vfs_uri_unref (uri);
	if (parent != uri) {
		gnome_vfs_uri_unref (parent);
	}

	return base;
}

static gboolean
write_string (GnomeVFSHandle *handle, const char *buf, GError **error)
{
	GnomeVFSResult res;
	GnomeVFSFileSize written;
	guint len;

	len = strlen (buf);
	res = gnome_vfs_write (handle, buf, len, &written);
	if (res != GNOME_VFS_OK || written < len) {
		g_set_error (error,
			     TOTEM_PL_PARSER_ERROR,
			     TOTEM_PL_PARSER_ERROR_VFS_WRITE,
			     _("Couldn't write parser: %s"),
			     gnome_vfs_result_to_string (res));
		gnome_vfs_close (handle);
		return FALSE;
	}

	return TRUE;
}

static int
totem_pl_parser_num_entries (TotemPlParser *parser, GtkTreeModel *model,
			     TotemPlParserIterFunc func, gpointer user_data)
{
	int num_entries, i, ignored;

	num_entries = gtk_tree_model_iter_n_children (model, NULL);
	ignored = 0;

	for (i = 1; i <= num_entries; i++)
	{
		GtkTreeIter iter;
		char *url, *title;
		gboolean custom_title;

		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i -1) == FALSE)
			return i - ignored;

		func (model, &iter, &url, &title, &custom_title, user_data);
		if (totem_pl_parser_scheme_is_ignored (parser, url) != FALSE)
			ignored++;

		g_free (url);
		g_free (title);
	}

	return num_entries - ignored;
}

static char *
totem_pl_parser_relative (const char *url, const char *output)
{
	char *url_base, *output_base;
	char *base, *needle;

	base = NULL;
	url_base = totem_pl_parser_base_url (url);
	if (url_base == NULL)
		return NULL;

	output_base = totem_pl_parser_base_url (output);

	needle = strstr (url_base, output_base);
	if (needle != NULL)
	{
		GnomeVFSURI *uri;
		char *newurl;

		uri = gnome_vfs_uri_new (url);
		newurl = gnome_vfs_uri_to_string (uri, 0);
		if (newurl[strlen (output_base)] == '/') {
			base = g_strdup (newurl + strlen (output_base) + 1);
		} else {
			base = g_strdup (newurl + strlen (output_base));
		}
		gnome_vfs_uri_unref (uri);
		g_free (newurl);

		/* And finally unescape the string */
		newurl = gnome_vfs_unescape_string (base, NULL);
		g_free (base);
		base = newurl;
	}

	g_free (url_base);
	g_free (output_base);

	return base;
}

#ifndef TOTEM_PL_PARSER_MINI
static gboolean
totem_pl_parser_write_pls (TotemPlParser *parser, GtkTreeModel *model,
			   TotemPlParserIterFunc func, 
			   const char *output, const char *title,
			   gpointer user_data, GError **error)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult res;
	int num_entries_total, num_entries, i;
	char *buf;
	gboolean success;

	num_entries = totem_pl_parser_num_entries (parser, model, func, user_data);
	num_entries_total = gtk_tree_model_iter_n_children (model, NULL);

	res = gnome_vfs_open (&handle, output, GNOME_VFS_OPEN_WRITE);
	if (res == GNOME_VFS_ERROR_NOT_FOUND) {
		res = gnome_vfs_create (&handle, output,
				GNOME_VFS_OPEN_WRITE, FALSE,
				GNOME_VFS_PERM_USER_WRITE
				| GNOME_VFS_PERM_USER_READ
				| GNOME_VFS_PERM_GROUP_READ);
	}

	if (res != GNOME_VFS_OK) {
		g_set_error(error,
			    TOTEM_PL_PARSER_ERROR,
			    TOTEM_PL_PARSER_ERROR_VFS_OPEN,
			    _("Couldn't open file '%s': %s"),
			    output, gnome_vfs_result_to_string (res));
		return FALSE;
	}

	buf = g_strdup ("[playlist]\n");
	success = write_string (handle, buf, error);
	g_free (buf);
	if (success == FALSE)
		return FALSE;

	if (title != NULL) {
		buf = g_strdup_printf ("X-GNOME-Title=%s\n", title);
		success = write_string (handle, buf, error);
		g_free (buf);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			return FALSE;
		}
	}

	buf = g_strdup_printf ("NumberOfEntries=%d\n", num_entries);
	success = write_string (handle, buf, error);
	g_free (buf);
	if (success == FALSE)
	{
		gnome_vfs_close (handle);
		return FALSE;
	}

	for (i = 1; i <= num_entries_total; i++) {
		GtkTreeIter iter;
		char *url, *title, *relative;
		gboolean custom_title;

		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i - 1) == FALSE)
			continue;

		func (model, &iter, &url, &title, &custom_title, user_data);

		if (totem_pl_parser_scheme_is_ignored (parser, url) != FALSE)
		{
			g_free (url);
			g_free (title);
			continue;
		}

		relative = totem_pl_parser_relative (url, output);
		buf = g_strdup_printf ("File%d=%s\n", i,
				relative ? relative : url);
		g_free (relative);
		g_free (url);
		success = write_string (handle, buf, error);
		g_free (buf);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			g_free (title);
			return FALSE;
		}

		if (custom_title == FALSE) {
			g_free (title);
			continue;
		}

		buf = g_strdup_printf ("Title%d=%s\n", i, title);
		success = write_string (handle, buf, error);
		g_free (buf);
		g_free (title);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			return FALSE;
		}
	}

	gnome_vfs_close (handle);
	return TRUE;
}

static char *
totem_pl_parser_url_to_dos (const char *url, const char *output)
{
	char *retval, *i;

	retval = totem_pl_parser_relative (url, output);

	if (retval == NULL)
		retval = g_strdup (url);

	/* Don't change URIs, but change smb:// */
	if (g_str_has_prefix (retval, "smb://") != FALSE)
	{
		char *tmp;
		tmp = g_strdup (retval + strlen ("smb:"));
		g_free (retval);
		retval = tmp;
	}

	if (strstr (retval, "://") != NULL)
		return retval;

	i = retval;
	while (*i != '\0')
	{
		if (*i == '/')
			*i = '\\';
		i++;
	}

	return retval;
}

static gboolean
totem_pl_parser_write_m3u (TotemPlParser *parser, GtkTreeModel *model,
		TotemPlParserIterFunc func, const char *output,
		gboolean dos_compatible, gpointer user_data, GError **error)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult res;
	int num_entries_total, i;
	gboolean success;
	char *buf;
	char *cr;

	res = gnome_vfs_open (&handle, output, GNOME_VFS_OPEN_WRITE);
	if (res == GNOME_VFS_ERROR_NOT_FOUND) {
		res = gnome_vfs_create (&handle, output,
				GNOME_VFS_OPEN_WRITE, FALSE,
				GNOME_VFS_PERM_USER_WRITE
				| GNOME_VFS_PERM_USER_READ
				| GNOME_VFS_PERM_GROUP_READ);
	}

	if (res != GNOME_VFS_OK) {
		g_set_error(error,
			    TOTEM_PL_PARSER_ERROR,
			    TOTEM_PL_PARSER_ERROR_VFS_OPEN,
			    _("Couldn't open file '%s': %s"),
			    output, gnome_vfs_result_to_string (res));
		return FALSE;
	}

	cr = dos_compatible ? "\r\n" : "\n";
	num_entries_total = gtk_tree_model_iter_n_children (model, NULL);
	if (num_entries_total == 0)
		return TRUE;

	for (i = 1; i <= num_entries_total; i++) {
		GtkTreeIter iter;
		char *url, *title, *path2;
		gboolean custom_title;

		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i - 1) == FALSE)
			continue;

		func (model, &iter, &url, &title, &custom_title, user_data);

		if (totem_pl_parser_scheme_is_ignored (parser, url) != FALSE)
		{
			g_free (url);
			g_free (title);
			continue;
		}

		if (custom_title != FALSE) {
			buf = g_strdup_printf (EXTINF",%s%s", title, cr);
			success = write_string (handle, buf, error);
			g_free (buf);
			if (success == FALSE) {
				g_free (title);
				g_free (url);
				gnome_vfs_close (handle);
				return FALSE;
			}
		}
		g_free (title);

		if (dos_compatible == FALSE) {
			char *tmp;
			tmp = totem_pl_parser_relative (url, output);
			if (tmp == NULL && g_str_has_prefix (url, "file:")) {
				path2 = g_filename_from_uri (url, NULL, NULL);
			} else {
				path2 = tmp;
			}
		} else {
			path2 = totem_pl_parser_url_to_dos (url, output);
		}

		buf = g_strdup_printf ("%s%s", path2 ? path2 : url, cr);
		g_free (path2);
		g_free (url);

		success = write_string (handle, buf, error);
		g_free (buf);

		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			return FALSE;
		}
	}

	gnome_vfs_close (handle);

	return TRUE;
}

static gboolean
totem_pl_parser_write_xspf (TotemPlParser *parser, GtkTreeModel *model,
			   TotemPlParserIterFunc func, 
			   const char *output, const char *title,
			   gpointer user_data, GError **error)
{
	GnomeVFSHandle *handle;
	GnomeVFSResult res;
	int num_entries_total, num_entries, i;
	char *buf;
	gboolean success;

	num_entries = totem_pl_parser_num_entries (parser, model, func, user_data);
	num_entries_total = gtk_tree_model_iter_n_children (model, NULL);

	res = gnome_vfs_open (&handle, output, GNOME_VFS_OPEN_WRITE);
	if (res == GNOME_VFS_ERROR_NOT_FOUND) {
		res = gnome_vfs_create (&handle, output,
				GNOME_VFS_OPEN_WRITE, FALSE,
				GNOME_VFS_PERM_USER_WRITE
				| GNOME_VFS_PERM_USER_READ
				| GNOME_VFS_PERM_GROUP_READ);
	}

	if (res != GNOME_VFS_OK) {
		g_set_error(error,
			    TOTEM_PL_PARSER_ERROR,
			    TOTEM_PL_PARSER_ERROR_VFS_OPEN,
			    _("Couldn't open file '%s': %s"),
			    output, gnome_vfs_result_to_string (res));
		return FALSE;
	}

	buf = g_strdup_printf ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\n"
				" <trackList>\n");
	success = write_string (handle, buf, error);
	g_free (buf);
	if (success == FALSE)
	{
		gnome_vfs_close (handle);
		return FALSE;
	}

	for (i = 1; i <= num_entries_total; i++) {
		GtkTreeIter iter;
		char *url, *url_escaped, *relative, *title;
		gboolean custom_title;

		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i - 1) == FALSE)
			continue;

		func (model, &iter, &url, &title, &custom_title, user_data);

		if (totem_pl_parser_scheme_is_ignored (parser, url) != FALSE)
		{
			g_free (url);
			g_free (title);
			continue;
		}

		relative = totem_pl_parser_relative (url, output);
		url_escaped = g_markup_escape_text (relative ? relative : url, -1);
		buf = g_strdup_printf ("  <track>\n"
					"   <location>%s</location>\n", url_escaped);
		success = write_string (handle, buf, error);
		g_free (url);
		g_free (url_escaped);
		g_free (relative);
		g_free (buf);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			g_free (title);
			return FALSE;
		}

		if (custom_title == TRUE)
			buf = g_strdup_printf ("   <title>%s</title>\n"
						"  </track>\n", title);
		else
			buf = g_strdup_printf ("  </track>\n");
		
		success = write_string (handle, buf, error);
		g_free (buf);
		g_free (title);
		if (success == FALSE)
		{
			gnome_vfs_close (handle);
			return FALSE;
		}
	}

	buf = g_strdup_printf (" </trackList>\n"
				"</playlist>");
	success = write_string (handle, buf, error);
	g_free (buf);
	gnome_vfs_close (handle);

	return success;
}

gboolean
totem_pl_parser_write_with_title (TotemPlParser *parser, GtkTreeModel *model,
				  TotemPlParserIterFunc func,
				  const char *output, const char *title,
				  TotemPlParserType type,
				  gpointer user_data, GError **error)
{
	switch (type)
	{
	case TOTEM_PL_PARSER_PLS:
		return totem_pl_parser_write_pls (parser, model, func,
				output, title, user_data, error);
	case TOTEM_PL_PARSER_M3U:
	case TOTEM_PL_PARSER_M3U_DOS:
		return totem_pl_parser_write_m3u (parser, model, func,
				output, (type == TOTEM_PL_PARSER_M3U_DOS),
                                user_data, error);
	case TOTEM_PL_PARSER_XSPF:
		return totem_pl_parser_write_xspf (parser, model, func,
				output, title, user_data, error);
	default:
		g_assert_not_reached ();
	}

	return FALSE;
}

gboolean
totem_pl_parser_write (TotemPlParser *parser, GtkTreeModel *model,
		       TotemPlParserIterFunc func,
		       const char *output, TotemPlParserType type,
		       gpointer user_data,
		       GError **error)
{
	return totem_pl_parser_write_with_title (parser, model, func, output,
			NULL, type, user_data, error);
}

#endif /* TOTEM_PL_PARSER_MINI */

static int
read_ini_line_int (char **lines, const char *key)
{
	int retval = -1;
	int i;

	if (lines == NULL || key == NULL)
		return -1;

	for (i = 0; (lines[i] != NULL && retval == -1); i++) {
		char *line = lines[i];

		while (*line == '\t' || *line == ' ')
			line++;

		if (g_ascii_strncasecmp (line, key, strlen (key)) == 0) {
			char **bits;

			bits = g_strsplit (line, "=", 2);
			if (bits[0] == NULL || bits [1] == NULL) {
				g_strfreev (bits);
				return -1;
			}

			retval = (gint) g_strtod (bits[1], NULL);
			g_strfreev (bits);
		}
	}

	return retval;
}

static char*
read_ini_line_string (char **lines, const char *key, gboolean dos_mode)
{
	char *retval = NULL;
	int i;

	if (lines == NULL || key == NULL)
		return NULL;

	for (i = 0; (lines[i] != NULL && retval == NULL); i++) {
		char *line = lines[i];

		while (*line == '\t' || *line == ' ')
			line++;

		if (g_ascii_strncasecmp (line, key, strlen (key)) == 0) {
			char **bits;
			ssize_t len;

			bits = g_strsplit (line, "=", 2);
			if (bits[0] == NULL || bits [1] == NULL) {
				g_strfreev (bits);
				return NULL;
			}

			retval = g_strdup (bits[1]);
			len = strlen (retval);
			if (dos_mode && len >= 2 && retval[len-2] == '\r') {
				retval[len-2] = '\n';
				retval[len-1] = '\0';
			}

			g_strfreev (bits);
		}
	}

	return retval;
}

static void
totem_pl_parser_init (TotemPlParser *parser)
{
	parser->priv = g_new0 (TotemPlParserPrivate, 1);
}

static void
totem_pl_parser_finalize (GObject *object)
{
	TotemPlParser *parser = TOTEM_PL_PARSER (object);

	g_return_if_fail (object != NULL);
	g_return_if_fail (parser->priv != NULL);

	g_list_foreach (parser->priv->ignore_schemes, (GFunc) g_free, NULL);
	g_list_free (parser->priv->ignore_schemes);

	g_list_foreach (parser->priv->ignore_mimetypes, (GFunc) g_free, NULL);
	g_list_free (parser->priv->ignore_mimetypes);

	g_free (parser->priv);
	parser->priv = NULL;

	G_OBJECT_CLASS (totem_pl_parser_parent_class)->finalize (object);
}

static gboolean
totem_pl_parser_check_utf8 (const char *title)
{
	return title ? g_utf8_validate (title, -1, NULL) : FALSE;
}

static void
totem_pl_parser_add_one_url (TotemPlParser *parser, const char *url, const char *title)
{
	g_signal_emit (G_OBJECT (parser), totem_pl_parser_table_signals[ENTRY],
		       0, url,
		       totem_pl_parser_check_utf8 (title) ? title : NULL,
		       NULL);
}

static void
totem_pl_parser_add_one_url_ext (TotemPlParser *parser, const char *url,
		const char *title, const char *genre)
{
	g_signal_emit (G_OBJECT (parser), totem_pl_parser_table_signals[ENTRY],
		       0, url,
		       totem_pl_parser_check_utf8 (title) ? title : NULL,
		       genre);
}

static gchar*
totem_pl_resolve_url (char *base, char *url)
{
	if (g_strrstr (url, "://") == NULL) {
		if (url[0] != '/' || g_str_has_prefix (base, "file://") == FALSE) {
			return g_strdup_printf ("%s/%s", base, url);
		}
	}
	return g_strdup (url);
}

static TotemPlParserResult
totem_pl_parser_add_ram (TotemPlParser *parser, const char *url, gpointer data)
{
	gboolean retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents, **lines;
	int size, i;
	const char *split_char;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	/* figure out whether we're a unix or dos RAM file */
	if (strstr(contents,"\x0d") == NULL)
		split_char = "\n";
	else
		split_char = "\x0d\n";

	lines = g_strsplit (contents, split_char, 0);
	g_free (contents);

	for (i = 0; lines[i] != NULL; i++) {
		if (strcmp (lines[i], "") == 0)
			continue;

		retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

		/* Either it's a URI, or it has a proper path ... */
		if (strstr(lines[i], "://") != NULL
				|| lines[i][0] == G_DIR_SEPARATOR) {
			/* .ram files can contain .smil entries */
			if (totem_pl_parser_parse_internal (parser, lines[i]) != TOTEM_PL_PARSER_RESULT_SUCCESS)
			{
				totem_pl_parser_add_one_url (parser,
						lines[i], NULL);
			}
		} else if (strcmp (lines[i], "--stop--") == 0) {
			/* For Real Media playlists, handle the stop command */
			break;
		} else {
			char *fullpath, *base;

			/* Try with a base */
			base = totem_pl_parser_base_url (url);

			fullpath = g_strdup_printf ("%s/%s", base, lines[i]);
			if (totem_pl_parser_parse_internal (parser, fullpath) != TOTEM_PL_PARSER_RESULT_SUCCESS)
			{
				totem_pl_parser_add_one_url (parser, fullpath, NULL);
			}
			g_free (fullpath);
			g_free (base);
		}
	}

	g_strfreev (lines);

	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_asf_reference_parser (TotemPlParser *parser,
		const char *url, gpointer data)
{
	char *contents, **lines, *ref, *split_char;
	int size;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (strstr(contents,"\x0d") == NULL) {
		split_char = "\n";
	} else {
		split_char = "\x0d\n";
	}

	lines = g_strsplit (contents, split_char, 0);
	g_free (contents);

	/* Try to get Ref1 first */
	ref = read_ini_line_string (lines, "Ref1", FALSE);
	if (ref == NULL) {
		g_strfreev (lines);
		return totem_pl_parser_add_asx (parser, url, data);
	}

	/* change http to mmsh, thanks Microsoft */
	if (g_str_has_prefix (ref, "http") != FALSE)
		memcpy(ref, "mmsh", 4);

	totem_pl_parser_add_one_url (parser, ref, NULL);
	g_free (ref);

	/* Don't try to get Ref2, as it's only ever
	 * supposed to be a fallback */

	g_strfreev (lines);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
totem_pl_parser_add_asf_parser (TotemPlParser *parser,
		const char *url, gpointer data)
{
	gboolean retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents, *ref;
	int size;

	if (g_str_has_prefix (data, "ASF ") == FALSE) {
		return totem_pl_parser_add_asf_reference_parser (parser, url, data);
	}

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (size <= 4) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	/* Skip 'ASF ' */
	ref = contents + 4;
	if (g_str_has_prefix (ref, "http") != FALSE) {
		memcpy(ref, "mmsh", 4);
		totem_pl_parser_add_one_url (parser, ref, NULL);
		retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	g_free (contents);
	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_pls_with_contents (TotemPlParser *parser, const char *url, const char *contents)
{
	gboolean retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char **lines;
	int i, num_entries;
	char *split_char, *playlist_title;
	gboolean dos_mode = FALSE;
	gboolean fallback;

	/* figure out whether we're a unix pls or dos pls */
	if (strstr(contents,"\x0d") == NULL) {
		split_char = "\n";
	} else {
		split_char = "\x0d\n";
		dos_mode = TRUE;
	}
	lines = g_strsplit (contents, split_char, 0);

	/* [playlist] */
	i = 0;
	playlist_title = NULL;

	/* Ignore empty lines */
	while (lines[i] != NULL && strcmp (lines[i], "") == 0)
		i++;

	if (lines[i] == NULL
			|| g_ascii_strncasecmp (lines[i], "[playlist]",
				(gsize)strlen ("[playlist]")) != 0) {
		DEBUG (g_print ("Couldn't recognise URL '%s' as a .pls playlist\n", url));
		goto bail;
	}

	playlist_title = read_ini_line_string (lines,
			"X-GNOME-Title", dos_mode);

	if (playlist_title != NULL) {
		g_signal_emit (G_OBJECT (parser),
				totem_pl_parser_table_signals[PLAYLIST_START],
				0, playlist_title);
	}

	/* numberofentries=? */
	num_entries = read_ini_line_int (lines, "numberofentries");
	if (num_entries == -1) {
		DEBUG (g_print ("Couldn't get the numberofentries for '%s'\n", url));
		goto bail;
	}

	retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	for (i = 1; i <= num_entries; i++) {
		char *file, *title, *genre;
		char *file_key, *title_key, *genre_key;

		file_key = g_strdup_printf ("file%d", i);
		title_key = g_strdup_printf ("title%d", i);
		/* Genre is our own little extension */
		genre_key = g_strdup_printf ("genre%d", i);

		file = read_ini_line_string (lines, (const char*)file_key, dos_mode);
		title = read_ini_line_string (lines, (const char*)title_key, dos_mode);
		genre = read_ini_line_string (lines, (const char*)genre_key, dos_mode);

		g_free (file_key);
		g_free (title_key);
		g_free (genre_key);

		if (file == NULL)
		{
			g_free (file);
			g_free (title);
			g_free (genre);
			continue;
		}

		fallback = parser->priv->fallback;
		if (parser->priv->recurse)
			parser->priv->fallback = FALSE;

		if (strstr (file, "://") != NULL || file[0] == G_DIR_SEPARATOR) {
			if (totem_pl_parser_parse_internal (parser, file) != TOTEM_PL_PARSER_RESULT_SUCCESS) {
				totem_pl_parser_add_one_url_ext (parser, file, title, genre);
			}
		} else {
			char *uri, *base, *escaped;

			/* Try with a base */
			base = totem_pl_parser_base_url (url);
			escaped = gnome_vfs_escape_path_string (file);

			uri = g_strdup_printf ("%s/%s", base, escaped);

			if (totem_pl_parser_parse_internal (parser, uri) != TOTEM_PL_PARSER_RESULT_SUCCESS) {
				totem_pl_parser_add_one_url_ext (parser, uri, title, genre);
			}

			g_free (escaped);
			g_free (uri);
			g_free (base);
		}

		parser->priv->fallback = fallback;
		g_free (file);
		g_free (title);
		g_free (genre);
	}

	if (playlist_title != NULL) {
		g_signal_emit (G_OBJECT (parser),
				totem_pl_parser_table_signals[PLAYLIST_END],
				0, playlist_title);
	}

bail:
	g_free (playlist_title);
	g_strfreev (lines);

	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_pls (TotemPlParser *parser, const char *url, gpointer data)
{
	gboolean retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents;
	int size;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (size == 0) {
		g_free (contents);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	retval = totem_pl_parser_add_pls_with_contents (parser, url, contents);
	g_free (contents);

	return retval;
}


static const char *
totem_pl_parser_get_extinfo_title (gboolean extinfo, char **lines, int i)
{
	const char *extinf, *comma;

	if (extinfo == FALSE || lines == NULL || i <= 0)
		return NULL;

	/* It's bound to have an EXTINF if we have extinfo */
	extinf = lines[i-1] + strlen(EXTINF);
	if (extinf[0] == '\0')
		return NULL;
	comma = strstr (extinf, ",");

	if (comma == NULL || comma[1] == '\0') {
		if (extinf[1] == '\0')
			return NULL;
		return extinf;
	}

	comma++;

	return comma;
}

static TotemPlParserResult
totem_pl_parser_add_m3u (TotemPlParser *parser, const char *url, gpointer data)
{
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	char *contents, **lines;
	int size, i;
	const char *split_char;
	gboolean extinfo;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	/* .pls files with a .m3u extension, the nasties */
	if (g_str_has_prefix (contents, "[playlist]") != FALSE
			|| g_str_has_prefix (contents, "[Playlist]") != FALSE
			|| g_str_has_prefix (contents, "[PLAYLIST]") != FALSE) {
		retval = totem_pl_parser_add_pls_with_contents (parser, url, contents);
		g_free (contents);
		return retval;
	}

	/* is TRUE if there's an EXTINF on the previous line */
	extinfo = FALSE;

	/* figure out whether we're a unix m3u or dos m3u */
	if (strstr(contents,"\x0d") == NULL)
		split_char = "\n";
	else
		split_char = "\x0d\n";

	lines = g_strsplit (contents, split_char, 0);
	g_free (contents);

	for (i = 0; lines[i] != NULL; i++) {
		if (lines[i][0] == '\0')
			continue;

		retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

		/* Ignore comments, but mark it if we have extra info */
		if (lines[i][0] == '#') {
			extinfo = g_str_has_prefix (lines[i], EXTINF);
			continue;
		}

		/* Either it's a URI, or it has a proper path ... */
		if (strstr(lines[i], "://") != NULL
				|| lines[i][0] == G_DIR_SEPARATOR) {
			totem_pl_parser_add_one_url (parser, lines[i],
					totem_pl_parser_get_extinfo_title (extinfo, lines, i));
			extinfo = FALSE;
		} else if (lines[i][0] == '\\' && lines[i][1] == '\\') {
			/* ... Or it's in the windows smb form
			 * (\\machine\share\filename), Note drive names
			 * (C:\ D:\ etc) are unhandled (unknown base for
			 * drive letters) */
		        char *tmpurl;

			lines[i] = g_strdelimit (lines[i], "\\", '/');
			tmpurl = g_strjoin (NULL, "smb:", lines[i], NULL);

			totem_pl_parser_add_one_url (parser, lines[i],
					totem_pl_parser_get_extinfo_title (extinfo, lines, i));
			extinfo = FALSE;

			g_free (tmpurl);
		} else {
			/* Try with a base */
			char *fullpath, *base, sep;

			base = totem_pl_parser_base_url (url);
			sep = (split_char[0] == '\n' ? '/' : '\\');
			if (sep == '\\')
				lines[i] = g_strdelimit (lines[i], "\\", '/');
			fullpath = g_strdup_printf ("%s/%s", base, lines[i]);
			totem_pl_parser_add_one_url (parser, fullpath,
					totem_pl_parser_get_extinfo_title (extinfo, lines, i));
			g_free (fullpath);
			g_free (base);
			extinfo = FALSE;
		}
	}

	g_strfreev (lines);

	return retval;
}

static gboolean
parse_asx_entry (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent, const char *pl_title)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	xmlChar *title, *url;
	char *fullpath = NULL;

	title = NULL;
	url = NULL;

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		/* ENTRY can only have one title node but multiple REFs */
		if (g_ascii_strcasecmp ((char *)node->name, "ref") == 0
				|| g_ascii_strcasecmp ((char *)node->name, "entryref") == 0) {
			xmlChar *tmp;

			tmp = xmlGetProp (node, (const xmlChar *)"href");
			if (tmp == NULL)
				tmp = xmlGetProp (node, (const xmlChar *)"HREF");
			if (tmp == NULL)
				continue;
			if (url == NULL || g_str_has_prefix ((char *)tmp, "mms:") != FALSE) {
				if (url)
					xmlFree (url);
				url = tmp;
			} else {
				if (tmp)
					xmlFree (tmp);
			}

			continue;
		}

		if (g_ascii_strcasecmp ((char *)node->name, "title") == 0)
			title = xmlNodeListGetString(doc, node->children, 1);
	}

	if (url == NULL) {
		if (title)
			xmlFree (title);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	fullpath = totem_pl_resolve_url (base, (char *)url);

	xmlFree (url);

	/* .asx files can contain references to other .asx files */
	retval = totem_pl_parser_parse_internal (parser, fullpath);
	if (retval != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		totem_pl_parser_add_one_url (parser, fullpath,
				(char *)title ? (char *)title : pl_title);
	}

	g_free (fullpath);
	if (title)
		xmlFree (title);

	return retval;
}

static gboolean
parse_asx_entries (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlChar *title = NULL;
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "title") == 0) {
			title = xmlNodeListGetString(doc, node->children, 1);
		}

		if (g_ascii_strcasecmp ((char *)node->name, "entry") == 0) {
			/* Whee found an entry here, find the REF and TITLE */
			if (parse_asx_entry (parser, base, doc, node, (char *)title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
		if (g_ascii_strcasecmp ((char *)node->name, "entryref") == 0) {
			/* Found an entryref, give the parent instead of the
			 * children to the parser */
			if (parse_asx_entry (parser, base, doc, parent, (char *)title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
		if (g_ascii_strcasecmp ((char *)node->name, "repeat") == 0) {
			/* Repeat at the top-level */
			if (parse_asx_entries (parser, base, doc, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
	}

	if (title)
		xmlFree (title);

	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_asx (TotemPlParser *parser, const char *url, gpointer data)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *contents = NULL, *base;
	int size;
	gboolean retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return FALSE;

	doc = xmlParseMemory (contents, size);
	if (doc == NULL)
		doc = xmlRecoverMemory (contents, size);
	g_free (contents);

	/* If the document has no root, or no name */
	if(!doc || !doc->children || !doc->children->name) {
		if (doc != NULL)
			xmlFreeDoc(doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	base = totem_pl_parser_base_url (url);

	for (node = doc->children; node != NULL; node = node->next)
		if (parse_asx_entries (parser, base, doc, node) != FALSE)
			retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	g_free (base);
	xmlFreeDoc(doc);
	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_block (TotemPlParser *parser, const char *url, gpointer data)
{
	MediaType type;
	char *media_url;
	GError *err = NULL;

	type = totem_cd_detect_type_with_url (url, &media_url, &err);
	if (err != NULL)
		DEBUG(g_print ("Couldn't get CD type for URL '%s': %s\n", url, err->message));
	if (type == MEDIA_TYPE_DATA || media_url == NULL)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	else if (type == MEDIA_TYPE_ERROR)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	totem_pl_parser_add_one_url (parser, media_url, NULL);
	g_free (media_url);
	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
totem_pl_parser_add_ra (TotemPlParser *parser, const char *url, gpointer data)
{
	if (data == NULL || totem_pl_parser_is_ra (data, strlen (data)) == FALSE) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return totem_pl_parser_add_ram (parser, url, NULL);
}

static gboolean
parse_smil_video_entry (TotemPlParser *parser, char *base,
		char *url, char *title)
{
	char *fullpath;

	fullpath = totem_pl_resolve_url (base, url);
	totem_pl_parser_add_one_url (parser, fullpath, title);

	g_free (fullpath);

	return TRUE;
}

static gboolean
parse_smil_entry (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent, xmlChar *parent_title)
{
	xmlNodePtr node;
	xmlChar *title, *url;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	title = NULL;
	url = NULL;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		/* ENTRY should only have one ref and one title nodes */
		if (g_ascii_strcasecmp ((char *)node->name, "video") == 0 || g_ascii_strcasecmp ((char *)node->name, "audio") == 0) {
			url = xmlGetProp (node, (const xmlChar *)"src");
			title = xmlGetProp (node, (const xmlChar *)"title");

			if (url != NULL) {
				if (parse_smil_video_entry (parser,
						base, (char *)url,
						title ? (char *)title
						: (char *)parent_title) != FALSE)
					retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
			}

			if (title)
				xmlFree (title);
			if (url)
				xmlFree (url);
		} else {
			if (parse_smil_entry (parser,
						base, doc, node, parent_title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		}
	}

	return retval;
}

static xmlChar *
parse_smil_head (TotemPlParser *parser, xmlDocPtr doc, xmlNodePtr parent)
{
	xmlNodePtr node;
	xmlChar *title = NULL;

	for (node = parent->children; node != NULL; node = node->next) {
		if (g_ascii_strcasecmp ((char *)node->name, "meta") == 0) {
			xmlChar *prop;
			prop = xmlGetProp (node, (const xmlChar *)"name");
			if (prop != NULL && g_ascii_strcasecmp ((char *)prop, "title") == 0) {
				title = xmlGetProp (node, (const xmlChar *)"content");
				if (title != NULL) {
					xmlFree (prop);
					break;
				}
			}
			xmlFree (prop);
		}
	}

	return title;
}

static gboolean
parse_smil_entries (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;
	xmlChar *title = NULL;

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "body") == 0) {
			if (parse_smil_entry (parser, base,
						doc, node, title) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
		} else if (title == NULL) {
			if (g_ascii_strcasecmp ((char *)node->name, "head") == 0)
				title = parse_smil_head (parser, doc, node);
		}
	}

	if (title != NULL)
		xmlFree (title);

	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_smil (TotemPlParser *parser, const char *url, gpointer data)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *contents = NULL, *base;
	int size;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	doc = xmlParseMemory (contents, size);
	if (doc == NULL)
		doc = xmlRecoverMemory (contents, size);
	g_free (contents);

	/* If the document has no root, or no name */
	if(!doc || !doc->children
			|| !doc->children->name
			|| g_ascii_strcasecmp ((char *)doc->children->name,
				"smil") != 0) {
		if (doc != NULL)
			xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	base = totem_pl_parser_base_url (url);

	for (node = doc->children; node != NULL; node = node->next)
		if (parse_smil_entries (parser, base, doc, node) != FALSE)
			retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	g_free (base);
	xmlFreeDoc (doc);

	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_asf (TotemPlParser *parser, const char *url, gpointer data)
{
	if (data == NULL) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	if (totem_pl_parser_is_asf (data, strlen (data)) == FALSE) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return totem_pl_parser_add_asf_parser (parser, url, data);
}

static TotemPlParserResult
totem_pl_parser_add_quicktime_metalink (TotemPlParser *parser, const char *url, gpointer data)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *contents = NULL;
	xmlChar *src;
	int size;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	doc = xmlParseMemory (contents, size);
	if (doc == NULL)
		doc = xmlRecoverMemory (contents, size);
	g_free (contents);

	/* If the document has no root, or no name */
	if(!doc || !doc->children
			|| !doc->children->name
			|| g_ascii_strcasecmp ((char *)doc->children->name,
				"quicktime") != 0) {
		if (doc != NULL)
			xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	if (strstr ((char *) doc->children->content, "type=\"application/x-quicktime-media-link\"") == NULL) {
		xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	node = doc->children->next;
	if (!node || !node->name
			|| g_ascii_strcasecmp ((char *) node->name,
				"embed") != 0) {
		xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	src = xmlGetProp (node, (const xmlChar *)"src");
	if (!src) {
		xmlFreeDoc (doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	totem_pl_parser_add_one_url (parser, (char *) src, NULL);

	xmlFree (src);
	xmlFreeDoc (doc);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
totem_pl_parser_add_quicktime (TotemPlParser *parser, const char *url, gpointer data)
{
	if (data == NULL || totem_pl_parser_is_quicktime (data, strlen (data)) == FALSE) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return totem_pl_parser_add_quicktime_metalink (parser, url, data);
}

static TotemPlParserResult
totem_pl_parser_add_desktop (TotemPlParser *parser, const char *url, gpointer data)
{
	char *contents, **lines;
	const char *path, *display_name, *type;
	int size;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	lines = g_strsplit (contents, "\n", 0);
	g_free (contents);

	type = read_ini_line_string (lines, "Type", FALSE);
	if (type == NULL || g_ascii_strcasecmp (type, "Link") != 0) {
		g_strfreev (lines);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	path = read_ini_line_string (lines, "URL", FALSE);
	if (path == NULL) {
		g_strfreev (lines);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	display_name = read_ini_line_string (lines, "Name", FALSE);

	if (totem_pl_parser_ignore (parser, path) == FALSE) {
		totem_pl_parser_add_one_url (parser, path, display_name);
	} else {
		if (totem_pl_parser_parse_internal (parser, path) != TOTEM_PL_PARSER_RESULT_SUCCESS)
			totem_pl_parser_add_one_url (parser, path, display_name);
	}

	g_strfreev (lines);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static int
totem_pl_parser_dir_compare (GnomeVFSFileInfo *a, GnomeVFSFileInfo *b)
{
	if (a->name == NULL) {
		if (b->name == NULL)
			return 0;
		else
			return -1;
	} else {
		if (b->name == NULL)
			return 1;
		else
			return strcmp (a->name, b->name);
	}
}

static TotemPlParserResult
totem_pl_parser_add_iso (TotemPlParser *parser, const char *url,
		gpointer data)
{
	GnomeVFSFileInfo *info;
	char *item;

	/* This is a hack, it could be a VCD or DVD */
	if (g_str_has_prefix (url, "file://") == FALSE)
		return TOTEM_PL_PARSER_RESULT_IGNORED;
	info = gnome_vfs_file_info_new ();
	if (gnome_vfs_get_file_info (url, info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS) != GNOME_VFS_OK) {
		gnome_vfs_file_info_unref (info);
		return TOTEM_PL_PARSER_RESULT_IGNORED;
	}

	/* Less than 700 megs, and it's a VCD */
	if (info->size < 700 * 1024 * 1024) {
		item = totem_cd_mrl_from_type ("vcd", url);
	} else {
		item = totem_cd_mrl_from_type ("dvd", url);
	}

	gnome_vfs_file_info_unref (info);
	totem_pl_parser_add_one_url (parser, item, NULL);
	g_free (item);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static TotemPlParserResult
totem_pl_parser_add_cue (TotemPlParser *parser, const char *url,
		gpointer data)
{
	char *vcdurl;

	vcdurl = totem_cd_mrl_from_type ("vcd", url);
	totem_pl_parser_add_one_url (parser, vcdurl, NULL);
	g_free (vcdurl);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

static gboolean
parse_xspf_track (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	xmlChar *title, *url;
	gchar *fullpath;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;
	
	title = NULL;
	url = NULL;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "location") == 0)
			url = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);

		if (g_ascii_strcasecmp ((char *)node->name, "title") == 0)
			title = xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	}

	fullpath = totem_pl_resolve_url (base, (char *)url);
	totem_pl_parser_add_one_url (parser, fullpath, (char *)title);
	retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	if (title)
		xmlFree (title);
	if (url)
		xmlFree (url);
	g_free (fullpath);

	return retval;
}

static gboolean
parse_xspf_trackList (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	for (node = parent->children; node != NULL; node = node->next)
	{
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "track") == 0)
			if (parse_xspf_track (parser, base, doc, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return retval;
}

static gboolean
parse_xspf_entries (TotemPlParser *parser, char *base, xmlDocPtr doc,
		xmlNodePtr parent)
{
	xmlNodePtr node;
	TotemPlParserResult retval = TOTEM_PL_PARSER_RESULT_ERROR;

	for (node = parent->children; node != NULL; node = node->next) {
		if (node->name == NULL)
			continue;

		if (g_ascii_strcasecmp ((char *)node->name, "trackList") == 0)
			if (parse_xspf_trackList (parser, base, doc, node) != FALSE)
				retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_xspf (TotemPlParser *parser, const char *url,
		gpointer data)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *contents = NULL, *base;
	int size;
	gboolean retval = TOTEM_PL_PARSER_RESULT_UNHANDLED;

	contents = totem_pl_parser_read_entire_file (url, &size);
	if (contents == NULL)
		return FALSE;

	doc = xmlParseMemory (contents, size);
	if (doc == NULL)
		doc = xmlRecoverMemory (contents, size);
	g_free (contents);

	/* If the document has no root, or no name */
	if(!doc || !doc->children
			|| !doc->children->name
			|| g_ascii_strcasecmp ((char *)doc->children->name,
				"playlist") != 0) {
		if (doc != NULL)
			xmlFreeDoc(doc);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	base = totem_pl_parser_base_url (url);

	for (node = doc->children; node != NULL; node = node->next)
		if (parse_xspf_entries (parser, base, doc, node) != FALSE)
			retval = TOTEM_PL_PARSER_RESULT_SUCCESS;

	g_free (base);
	xmlFreeDoc(doc);
	return retval;
}

static TotemPlParserResult
totem_pl_parser_add_directory (TotemPlParser *parser, const char *url,
			   gpointer data)
{
	MediaType type;
	GList *list, *l;
	GnomeVFSResult res;

	if (parser->priv->recurse_level == 1) {
		char *media_url;

		type = totem_cd_detect_type_from_dir (url, &media_url, NULL);
		if (type != MEDIA_TYPE_DATA && type != MEDIA_TYPE_ERROR) {
			if (media_url != NULL) {
				totem_pl_parser_add_one_url (parser, media_url, NULL);
				g_free (media_url);
				return TOTEM_PL_PARSER_RESULT_SUCCESS;
			}
		}
	}

	res = gnome_vfs_directory_list_load (&list, url,
			GNOME_VFS_FILE_INFO_DEFAULT);
	if (res != GNOME_VFS_OK)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	list = g_list_sort (list, (GCompareFunc) totem_pl_parser_dir_compare);
	l = list;

	while (l != NULL) {
		char *name, *fullpath;
		GnomeVFSFileInfo *info = l->data;
		TotemPlParserResult ret;

		if (info->name != NULL && (strcmp (info->name, ".") == 0
					|| strcmp (info->name, "..") == 0)) {
			l = l->next;
			continue;
		}

		name = gnome_vfs_escape_string (info->name);
		fullpath = g_strconcat (url, "/", name, NULL);
		g_free (name);

		ret = totem_pl_parser_parse_internal (parser, fullpath);
		if (ret != TOTEM_PL_PARSER_RESULT_SUCCESS && ret != TOTEM_PL_PARSER_RESULT_IGNORED)
			totem_pl_parser_add_one_url (parser, fullpath, NULL);

		l = l->next;
	}

	g_list_foreach (list, (GFunc) gnome_vfs_file_info_unref, NULL);
	g_list_free (list);

	return TOTEM_PL_PARSER_RESULT_SUCCESS;
}

#endif /* !TOTEM_PL_PARSER_MINI */

static gboolean
totem_pl_parser_is_ra (const char *data, gsize len)
{
	/* pnm is the smallest, 6 chars */
	if (len < 7)
		return FALSE;

	if (g_str_has_prefix (data, "http://") != FALSE
			|| g_str_has_prefix (data, "rtsp://") != FALSE
			|| g_str_has_prefix (data, "pnm://") != FALSE) {
		return TRUE;
	}
	return FALSE;
}

static gboolean
totem_pl_parser_is_asf (const char *data, gsize len)
{
	char *buffer;

	if (len == 0)
		return FALSE;

	if (g_str_has_prefix (data, "[Reference]") != FALSE
			|| g_ascii_strncasecmp (data, "<ASX", strlen ("<ASX")) == 0
			|| g_str_has_prefix (data, "ASF ") != FALSE) {
		return TRUE;
	}

	if (len > MIME_READ_CHUNK_SIZE)
		len = MIME_READ_CHUNK_SIZE;

	/* FIXME would be nicer to have an strnstr */
	buffer = g_memdup (data, len);
	if (buffer == NULL) {
		g_warning ("Couldn't dup data in totem_pl_parser_is_asf");
		return FALSE;
	}
	buffer[len - 1] = '\0';
	if (strstr (buffer, "<ASX") != NULL
			|| strstr (buffer, "<asx") != NULL) {
		g_free (buffer);
		return TRUE;
	}
	g_free (buffer);
	return FALSE;
}

static gboolean
totem_pl_parser_is_quicktime (const char *data, gsize len)
{
	char *buffer;

	if (len == 0)
		return FALSE;
	if (len > MIME_READ_CHUNK_SIZE)
		len = MIME_READ_CHUNK_SIZE;

	/* FIXME would be nicer to have an strnstr */
	buffer = g_memdup (data, len);
	if (buffer == NULL) {
		g_warning ("Couldn't dup data in totem_pl_parser_is_quicktime");
		return FALSE;
	}
	buffer[len - 1] = '\0';
	if (strstr (buffer, "<?quicktime") != NULL) {
		g_free (buffer);
		return TRUE;
	}
	g_free (buffer);
	return FALSE;
}

#ifndef TOTEM_PL_PARSER_MINI
#define PLAYLIST_TYPE(mime,cb,identcb) { mime, cb, identcb }
#define PLAYLIST_TYPE2(mime,cb,identcb) { mime, cb, identcb }
#else
#define PLAYLIST_TYPE(mime,cb,identcb) { mime }
#define PLAYLIST_TYPE2(mime,cb,identcb) { mime, identcb }
#endif

/* These ones need a special treatment, mostly parser formats */
static PlaylistTypes special_types[] = {
	PLAYLIST_TYPE ("audio/x-mpegurl", totem_pl_parser_add_m3u, NULL),
	PLAYLIST_TYPE ("audio/playlist", totem_pl_parser_add_m3u, NULL),
	PLAYLIST_TYPE ("audio/x-ms-asx", totem_pl_parser_add_asx, NULL),
	PLAYLIST_TYPE ("audio/x-scpls", totem_pl_parser_add_pls, NULL),
	PLAYLIST_TYPE ("application/x-smil", totem_pl_parser_add_smil, NULL),
	PLAYLIST_TYPE ("application/smil", totem_pl_parser_add_smil, NULL),
	PLAYLIST_TYPE ("video/x-ms-wvx", totem_pl_parser_add_asx, NULL),
	PLAYLIST_TYPE ("audio/x-ms-wax", totem_pl_parser_add_asx, NULL),
	PLAYLIST_TYPE ("application/xspf+xml", totem_pl_parser_add_xspf, NULL),
#ifndef TOTEM_PL_PARSER_MINI
	PLAYLIST_TYPE ("application/x-desktop", totem_pl_parser_add_desktop, NULL),
	PLAYLIST_TYPE ("application/x-gnome-app-info", totem_pl_parser_add_desktop, NULL),
	PLAYLIST_TYPE ("application/x-cd-image", totem_pl_parser_add_iso, NULL),
	PLAYLIST_TYPE ("application/x-cue", totem_pl_parser_add_cue, NULL),
	PLAYLIST_TYPE ("x-directory/normal", totem_pl_parser_add_directory, NULL),
	PLAYLIST_TYPE ("x-special/device-block", totem_pl_parser_add_block, NULL),
#endif
};

/* These ones are "dual" types, might be a video, might be a parser */
static PlaylistTypes dual_types[] = {
	PLAYLIST_TYPE2 ("audio/x-real-audio", totem_pl_parser_add_ra, totem_pl_parser_is_ra),
	PLAYLIST_TYPE2 ("audio/x-pn-realaudio", totem_pl_parser_add_ra, totem_pl_parser_is_ra),
	PLAYLIST_TYPE2 ("application/vnd.rn-realmedia", totem_pl_parser_add_ra, totem_pl_parser_is_ra),
	PLAYLIST_TYPE2 ("audio/x-pn-realaudio-plugin", totem_pl_parser_add_ra, totem_pl_parser_is_ra),
	PLAYLIST_TYPE2 ("audio/vnd.rn-realaudio", totem_pl_parser_add_ra, totem_pl_parser_is_ra),
	PLAYLIST_TYPE2 ("text/plain", totem_pl_parser_add_ra, totem_pl_parser_is_ra),
	PLAYLIST_TYPE2 ("video/x-ms-asf", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
	PLAYLIST_TYPE2 ("video/x-ms-wmv", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
	PLAYLIST_TYPE2 ("video/quicktime", totem_pl_parser_add_quicktime, totem_pl_parser_is_quicktime),
	PLAYLIST_TYPE2 ("application/x-quicktime-media-link", totem_pl_parser_add_quicktime, totem_pl_parser_is_quicktime),
};

#ifndef TOTEM_PL_PARSER_MINI

static PlaylistTypes ignore_types[] = {
	PLAYLIST_TYPE ("image/*", NULL, NULL),
	PLAYLIST_TYPE ("text/plain", NULL, NULL),
	PLAYLIST_TYPE ("application/x-rar", NULL, NULL),
	PLAYLIST_TYPE ("application/zip", NULL, NULL),
	PLAYLIST_TYPE ("application/x-trash", NULL, NULL),
};

static gboolean
totem_pl_parser_scheme_is_ignored (TotemPlParser *parser, const char *url)
{
	GList *l;

	if (parser->priv->ignore_schemes == NULL)
		return FALSE;

	for (l = parser->priv->ignore_schemes; l != NULL; l = l->next)
	{
		const char *scheme = l->data;
		if (g_str_has_prefix (url, scheme) != FALSE)
			return TRUE;
	}

	return FALSE;
}

static gboolean
totem_pl_parser_mimetype_is_ignored (TotemPlParser *parser,
				     const char *mimetype)
{
	GList *l;

	if (parser->priv->ignore_mimetypes == NULL)
		return FALSE;

	for (l = parser->priv->ignore_mimetypes; l != NULL; l = l->next)
	{
		const char *item = l->data;
		if (strcmp (mimetype, item) == 0)
			return TRUE;
	}

	return FALSE;

}

static gboolean
totem_pl_parser_ignore (TotemPlParser *parser, const char *url)
{
	const char *mimetype;
	guint i;

	if (totem_pl_parser_scheme_is_ignored (parser, url) != FALSE)
		return TRUE;

	mimetype = gnome_vfs_get_file_mime_type (url, NULL, TRUE);
	if (mimetype == NULL || strcmp (mimetype, GNOME_VFS_MIME_TYPE_UNKNOWN) == 0)
		return FALSE;

	for (i = 0; i < G_N_ELEMENTS (special_types); i++)
		if (strcmp (special_types[i].mimetype, mimetype) == 0)
			return FALSE;

	for (i = 0; i < G_N_ELEMENTS (dual_types); i++)
		if (strcmp (dual_types[i].mimetype, mimetype) == 0)
			return FALSE;

	/* It's a remote file that could be an m3u file */
	if (strcmp (mimetype, "audio/x-mp3") == 0)
	{
		if (strstr (url, "m3u") != NULL)
			return FALSE;
	}

	return TRUE;
}

static gboolean
totem_pl_parser_ignore_from_mimetype (TotemPlParser *parser, const char *mimetype)
{
	char *super;
	guint i;

	super = gnome_vfs_get_supertype_from_mime_type (mimetype);
	for (i = 0; i < G_N_ELEMENTS (ignore_types) && super != NULL; i++) {
		if (gnome_vfs_mime_type_is_supertype (ignore_types[i].mimetype) != FALSE) {
			if (strcmp (super, ignore_types[i].mimetype) == 0) {
				g_free (super);
				return TRUE;
			}
		} else {
			GnomeVFSMimeEquivalence eq;

			eq = gnome_vfs_mime_type_get_equivalence (mimetype, ignore_types[i].mimetype);
			if (eq == GNOME_VFS_MIME_PARENT || eq == GNOME_VFS_MIME_IDENTICAL) {
				g_free (super);
				return TRUE;
			}
		}
	}
	g_free (super);

	return FALSE;
}

static TotemPlParserResult
totem_pl_parser_parse_internal (TotemPlParser *parser, const char *url)
{
	const char *mimetype;
	guint i;
	gpointer data = NULL;
	TotemPlParserResult ret = TOTEM_PL_PARSER_RESULT_ERROR;
	gboolean found = FALSE;

	if (parser->priv->recurse_level > RECURSE_LEVEL_MAX)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	/* Shouldn't gnome-vfs have a list of schemes it supports? */
	if (g_str_has_prefix (url, "mms") != FALSE
			|| g_str_has_prefix (url, "rtsp") != FALSE) {
		DEBUG(g_print ("URL '%s' is MMS or RTSP, ignoring\n", url));
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	if (!parser->priv->recurse && parser->priv->recurse_level > 0) {
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	mimetype = gnome_vfs_mime_type_from_name (url);
	DEBUG(g_print ("_mime_type_from_name for '%s' returned '%s'\n", url, mimetype));
	if (mimetype == NULL || strcmp (GNOME_VFS_MIME_TYPE_UNKNOWN, mimetype) == 0) {
		mimetype = my_gnome_vfs_get_mime_type_with_data (url, &data, parser);
		DEBUG(g_print ("_get_mime_type_with_data for '%s' returned '%s'\n", url, mimetype ? mimetype : "NULL"));
	}

	if (mimetype == NULL)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;

	if (totem_pl_parser_mimetype_is_ignored (parser, mimetype) != FALSE) {
		g_free (data);
		return TOTEM_PL_PARSER_RESULT_IGNORED;
	}

	if (parser->priv->recurse || parser->priv->recurse_level == 0) {
		parser->priv->recurse_level++;

		for (i = 0; i < G_N_ELEMENTS(special_types); i++) {
			if (strcmp (special_types[i].mimetype, mimetype) == 0) {
				DEBUG(g_print ("URL '%s' is special type '%s'\n", url, mimetype));
				ret = (* special_types[i].func) (parser, url, data);
				found = TRUE;
				break;
			}
		}

		for (i = 0; i < G_N_ELEMENTS(dual_types) && found == FALSE; i++) {
			if (strcmp (dual_types[i].mimetype, mimetype) == 0) {
				DEBUG(g_print ("URL '%s' is dual type '%s'\n", url, mimetype));
				if (data == NULL) {
					mimetype = my_gnome_vfs_get_mime_type_with_data (url, &data, parser);
				}
				ret = (* dual_types[i].func) (parser, url, data);
				found = TRUE;
				break;
			}
		}

		g_free (data);

		parser->priv->recurse_level--;
	}

	if (ret == TOTEM_PL_PARSER_RESULT_SUCCESS)
		return ret;

	if (totem_pl_parser_ignore_from_mimetype (parser, mimetype))
		return TOTEM_PL_PARSER_RESULT_IGNORED;

	if (ret != TOTEM_PL_PARSER_RESULT_SUCCESS && parser->priv->fallback) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	if (ret == TOTEM_PL_PARSER_RESULT_SUCCESS)
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	else
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
}

TotemPlParserResult
totem_pl_parser_parse (TotemPlParser *parser, const char *url,
		       gboolean fallback)
{
	g_return_val_if_fail (TOTEM_IS_PL_PARSER (parser), TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_return_val_if_fail (url != NULL, TOTEM_PL_PARSER_RESULT_UNHANDLED);

	if (totem_pl_parser_scheme_is_ignored (parser, url) != FALSE)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;

	g_return_val_if_fail (strstr (url, "://") != NULL,
			TOTEM_PL_PARSER_RESULT_IGNORED);

	parser->priv->recurse_level = 0;
	parser->priv->fallback = fallback != FALSE;
	return totem_pl_parser_parse_internal (parser, url);
}

void
totem_pl_parser_add_ignored_scheme (TotemPlParser *parser,
		const char *scheme)
{
	g_return_if_fail (TOTEM_IS_PL_PARSER (parser));

	parser->priv->ignore_schemes = g_list_prepend
		(parser->priv->ignore_schemes, g_strdup (scheme));
}

void
totem_pl_parser_add_ignored_mimetype (TotemPlParser *parser,
		const char *mimetype)
{
	g_return_if_fail (TOTEM_IS_PL_PARSER (parser));

	parser->priv->ignore_mimetypes = g_list_prepend
		(parser->priv->ignore_mimetypes, g_strdup (mimetype));
}

#endif /* !TOTEM_PL_PARSER_MINI */

gboolean
totem_pl_parser_can_parse_from_data (const char *data,
				     gsize len,
				     gboolean debug)
{
	const char *mimetype;
	guint i;

	g_return_val_if_fail (data != NULL, FALSE);

#define D(x) if (debug) x

	/* Bad cast! */
	mimetype = gnome_vfs_get_mime_type_for_data ((gpointer) data, (int) len);

	if (mimetype == NULL || strcmp (GNOME_VFS_MIME_TYPE_UNKNOWN, mimetype) == 0) {
		D(g_message ("totem_pl_parser_can_parse_from_data couldn't get mimetype"));
		return FALSE;
	}

	for (i = 0; i < G_N_ELEMENTS(special_types); i++) {
		if (strcmp (special_types[i].mimetype, mimetype) == 0) {
			D(g_message ("Is special type '%s'\n", mimetype));
			return TRUE;
		}
	}

	for (i = 0; i < G_N_ELEMENTS(dual_types); i++) {
		if (strcmp (dual_types[i].mimetype, mimetype) == 0) {
			D(g_message ("Is dual type '%s'\n", mimetype));
			if (dual_types[i].iden != NULL)
				return (* dual_types[i].iden) (data, len);
			return FALSE;
		}
	}

	return FALSE;
}
