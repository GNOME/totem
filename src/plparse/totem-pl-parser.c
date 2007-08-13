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
#include <gobject/gvaluecollector.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-wm.h"
#include "totem-pl-parser-qt.h"
#include "totem-pl-parser-pls.h"
#include "totem-pl-parser-xspf.h"
#include "totem-pl-parser-media.h"
#include "totem-pl-parser-smil.h"
#include "totem-pl-parser-pla.h"
#include "totem-pl-parser-lines.h"
#include "totem-pl-parser-misc.h"
#include "totem-pl-parser-private.h"

#define READ_CHUNK_SIZE 8192
#define RECURSE_LEVEL_MAX 4
#define DIR_MIME_TYPE "x-directory/normal"
#define BLOCK_DEVICE_TYPE "x-special/device-block"
#define EMPTY_FILE_TYPE "application/x-zerosize"
#define TEXT_URI_TYPE "text/uri-list"
#define AUDIO_MPEG_TYPE "audio/mpeg"

typedef gboolean (*PlaylistIdenCallback) (const char *data, gsize len);

#ifndef TOTEM_PL_PARSER_MINI
typedef TotemPlParserResult (*PlaylistCallback) (TotemPlParser *parser, const char *url, const char *base, gpointer data);
#endif

typedef struct {
	char *mimetype;
#ifndef TOTEM_PL_PARSER_MINI
	PlaylistCallback func;
#endif
	PlaylistIdenCallback iden;
#ifndef TOTEM_PL_PARSER_MINI
	guint unsafe : 1;
#endif
} PlaylistTypes;

#ifndef TOTEM_PL_PARSER_MINI

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
	PROP_DEBUG,
	PROP_FORCE,
	PROP_DISABLE_UNSAFE
};

/* Signals */
enum {
	ENTRY_PARSED,
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

	g_object_class_install_property (object_class,
					 PROP_FORCE,
					 g_param_spec_boolean ("force",
							       "force",
							       "Whether or not to force parsing the file if the playlist looks unsupported", 
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_DISABLE_UNSAFE,
					 g_param_spec_boolean ("disable-unsafe",
							       "disable-unsafe",
							       "Whether or not to disable parsing of unsafe locations", 
							       FALSE,
							       G_PARAM_READWRITE));

	/* Signals */
	totem_pl_parser_table_signals[ENTRY_PARSED] =
		g_signal_new ("entry-parsed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemPlParserClass, entry_parsed),
			      NULL, NULL,
			      totemplparser_marshal_VOID__STRING_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_POINTER);
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
	case PROP_FORCE:
		parser->priv->force = g_value_get_boolean (value) != FALSE;
		break;
	case PROP_DISABLE_UNSAFE:
		parser->priv->disable_unsafe = g_value_get_boolean (value) != FALSE;
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
	case PROP_FORCE:
		g_value_set_boolean (value, parser->priv->force);
		break;
	case PROP_DISABLE_UNSAFE:
		g_value_set_boolean (value, parser->priv->disable_unsafe);
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

void
totem_pl_parser_playlist_start (TotemPlParser *parser, const char *playlist_title)
{
	g_signal_emit (G_OBJECT (parser),
		       totem_pl_parser_table_signals[PLAYLIST_START],
		       0, playlist_title);
}

void
totem_pl_parser_playlist_end (TotemPlParser *parser, const char *playlist_title)
{
	g_signal_emit (G_OBJECT (parser),
		       totem_pl_parser_table_signals[PLAYLIST_END],
		       0, playlist_title);
}

static char *
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
				return g_strdup (BLOCK_DEVICE_TYPE);
		}
	}

	/* Open the file. */
	result = gnome_vfs_open (&handle, uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		if (result == GNOME_VFS_ERROR_IS_DIRECTORY)
			return g_strdup (DIR_MIME_TYPE);
		DEBUG(g_print ("URL '%s' couldn't be opened in _get_mime_type_with_data: '%s'\n", uri, gnome_vfs_result_to_string (result)));
		return NULL;
	}
	DEBUG(g_print ("URL '%s' was opened successfully in _get_mime_type_with_data:\n", uri));

	/* Read the whole thing, up to MIME_READ_CHUNK_SIZE */
	buffer = NULL;
	total_bytes_read = 0;
	bytes_read = 0;
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

	/* Empty file */
	if (total_bytes_read == 0) {
		DEBUG(g_print ("URL '%s' is empty in _get_mime_type_with_data\n", uri));
		return g_strdup (EMPTY_FILE_TYPE);
	}

	/* Return the file null-terminated. */
	buffer = g_realloc (buffer, total_bytes_read + 1);
	buffer[total_bytes_read] = '\0';
	*data = buffer;

	mimetype = gnome_vfs_get_mime_type_for_data (*data, total_bytes_read);

	if (mimetype != NULL && strcmp (mimetype, "text/plain") == 0) {
		if (totem_pl_parser_is_uri_list (*data, total_bytes_read) != FALSE)
			return g_strdup (TEXT_URI_TYPE);
	}

	return g_strdup (mimetype);
}

xmlDocPtr
totem_pl_parser_parse_xml_file (const char *url)
{
	xmlDocPtr doc;
	char *contents;
	int size;

	if (gnome_vfs_read_entire_file (url, &size, &contents) != GNOME_VFS_OK)
		return NULL;

	/* Try to remove HTML style comments */
	{
		char *needle;

		while ((needle = strstr (contents, "<!--")) != NULL) {
			while (strncmp (needle, "-->", 3) != 0) {
				*needle = ' ';
				needle++;
				if (*needle == '\0')
					break;
			}
		}
	}

	doc = xmlParseMemory (contents, size);
	if (doc == NULL)
		doc = xmlRecoverMemory (contents, size);
	g_free (contents);

	return doc;
}

char *
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

gboolean
totem_pl_parser_line_is_empty (const char *line)
{
	guint i;

	if (line == NULL)
		return TRUE;

	for (i = 0; line[i] != '\0'; i++) {
		if (line[i] != '\t' && line[i] != ' ')
			return FALSE;
	}
	return TRUE;
}

gboolean
totem_pl_parser_write_string (GnomeVFSHandle *handle, const char *buf, GError **error)
{
	guint len;

	len = strlen (buf);
	return totem_pl_parser_write_buffer (handle, buf, len, error);
}

gboolean
totem_pl_parser_write_buffer (GnomeVFSHandle *handle, const char *buf, guint len, GError **error)
{
	GnomeVFSResult res;
	GnomeVFSFileSize written;

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

int
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

char *
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
	case TOTEM_PL_PARSER_IRIVER_PLA:
		return totem_pl_parser_write_pla (parser, model, func,
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

int
totem_pl_parser_read_ini_line_int (char **lines, const char *key)
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

char*
totem_pl_parser_read_ini_line_string_with_sep (char **lines, const char *key,
		gboolean dos_mode, const char *sep)
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

			bits = g_strsplit (line, sep, 2);
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

char*
totem_pl_parser_read_ini_line_string (char **lines, const char *key, gboolean dos_mode)
{
	return totem_pl_parser_read_ini_line_string_with_sep (lines, key, dos_mode, "=");
}

static void
totem_pl_parser_init (TotemPlParser *parser)
{
	GParamSpec *pspec;
	parser->priv = g_new0 (TotemPlParserPrivate, 1);

	parser->priv->pspec_pool = g_param_spec_pool_new (FALSE);
	pspec = g_param_spec_string ("url", "url",
				     "URL to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("title", "title",
				     "Title of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("author", "author",
				     "Author of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("genre", "genre",
				     "Genre of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("base", "base",
				     "Base URL of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("volume", "volume",
				     "Default playback volume (in percents)", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("autoplay", "autoplay",
				     "Whether or not to autoplay the stream", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("duration", "duration",
				     "String representing the duration of the entry, used for still images", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("starttime", "starttime",
				     "String representing the start time of the stream (initial seek)", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("copyright", "copyright",
				     "Copyright of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("abstract", "abstract",
				     "Abstract of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("screensize", "screensize",
				     "String representing the default movie size (double, full or original)", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("ui-mode", "ui-mode",
				     "String representing the default UI mode (only compact is supported)", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (parser->priv->pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
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

static void
totem_pl_parser_add_url_valist (TotemPlParser *parser,
				const gchar *first_property_name,
				va_list      var_args)
{
	const char *name;
	char *title, *url, *genre, *base;
	GHashTable *metadata;

	title = url = genre = base = NULL;

	g_object_ref (G_OBJECT (parser));
	metadata = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	name = first_property_name;

	while (name) {
		GValue value = { 0, };
		GParamSpec *pspec;
		char *error = NULL;
		const char *string;

		pspec = g_param_spec_pool_lookup (parser->priv->pspec_pool,
						  name,
						  G_OBJECT_TYPE (parser),
						  FALSE);

		if (!pspec) {
			g_warning ("Unknown property '%s'", name);
			name = va_arg (var_args, char*);
			continue;
		}

		g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
		G_VALUE_COLLECT (&value, var_args, 0, &error);
		if (error != NULL) {
			g_warning ("Error getting the value for property '%s'", name);
			break;
		}

		if (strcmp (name, "url") == 0)
			url = g_value_dup_string (&value);

		/* Ignore empty values */
		string = g_value_get_string (&value);
		if (string != NULL && string[0] != '\0') {
			char *fixed = NULL;

			if (g_utf8_validate (string, -1, NULL) == FALSE) {
				fixed = g_convert (string, -1, "UTF-8", "ISO8859-1", NULL, NULL, NULL);
				if (fixed == NULL) {
					g_value_unset (&value);
					name = va_arg (var_args, char*);
					continue;
				}
			}

			/* Add other values to the metadata hashtable */
			g_hash_table_insert (metadata,
					     g_strdup (name),
					     g_strdup (fixed ? fixed : string));
		}

		g_value_unset (&value);
		name = va_arg (var_args, char*);
	}

	if (parser->priv->disable_unsafe != FALSE) {
		//FIXME fix this! 396710
	}

	if (g_hash_table_size (metadata) > 0 || url != NULL) {
		g_signal_emit (G_OBJECT (parser),
			       totem_pl_parser_table_signals[ENTRY_PARSED],
			       0, url, metadata);
	}

	g_hash_table_destroy (metadata);

	g_free (url);
	g_object_unref (G_OBJECT (parser));
}

void
totem_pl_parser_add_url (TotemPlParser *parser,
			 const char *first_property_name,
			 ...)
{
	va_list var_args;
	va_start (var_args, first_property_name);
	totem_pl_parser_add_url_valist (parser, first_property_name, var_args);
	va_end (var_args);
}

void
totem_pl_parser_add_one_url (TotemPlParser *parser, const char *url, const char *title)
{
	totem_pl_parser_add_url (parser,
				 TOTEM_PL_PARSER_FIELD_URL, url,
				 TOTEM_PL_PARSER_FIELD_TITLE, title,
				 NULL);
}

static char *
totem_pl_parser_remove_filename (const char *url)
{
	char *no_frag, *no_file, *no_qmark, *qmark;

	no_frag = gnome_vfs_make_uri_canonical_strip_fragment (url);
	qmark = strrchr (no_frag, '?');
	if (qmark == NULL)
		return no_frag;
	no_qmark = g_strndup (no_frag, qmark - no_frag);
	no_file = totem_pl_parser_base_url (no_qmark);
	g_free (no_qmark);
	g_free (no_frag);

	return no_file;
}

char *
totem_pl_resolve_url (const char *base, const char *url)
{
	GnomeVFSURI *base_uri, *new;
	char *resolved, *base_no_frag;

	g_return_val_if_fail (url != NULL, NULL);
	g_return_val_if_fail (base != NULL, g_strdup (url));

	/* If the URI isn't relative, just leave */
	if (strstr (url, "://") != NULL)
		return g_strdup (url);

	/* Strip fragment and filename */
	base_no_frag = totem_pl_parser_remove_filename (base);

	/* gnome_vfs_uri_append_path is trying to be clever and
	 * merges paths that look like they're the same */
	if (url[0] != '/') {
		char *newbase = g_strdup_printf ("%s/", base_no_frag);
		base_uri = gnome_vfs_uri_new (newbase);
		g_free (newbase);
	} else {
		base_uri = gnome_vfs_uri_new (base_no_frag);
	}
	g_free (base_no_frag);

	g_return_val_if_fail (base_uri != NULL, g_strdup (url));

	if (url[0] == '/')
		new = gnome_vfs_uri_resolve_symbolic_link (base_uri, url);
	else
		new = gnome_vfs_uri_append_path (base_uri, url);

	g_return_val_if_fail (new != NULL, g_strdup (url));
	gnome_vfs_uri_unref (base_uri);
	resolved = gnome_vfs_uri_to_string (new, GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (new);

	return resolved;
}

#endif /* !TOTEM_PL_PARSER_MINI */

#ifndef TOTEM_PL_PARSER_MINI
#define PLAYLIST_TYPE(mime,cb,identcb,unsafe) { mime, cb, identcb, unsafe }
#define PLAYLIST_TYPE2(mime,cb,identcb) { mime, cb, identcb }
#define PLAYLIST_TYPE3(mime) { mime, NULL, NULL, FALSE }
#else
#define PLAYLIST_TYPE(mime,cb,identcb,unsafe) { mime }
#define PLAYLIST_TYPE2(mime,cb,identcb) { mime, identcb }
#define PLAYLIST_TYPE3(mime) { mime }
#endif

/* These ones need a special treatment, mostly parser formats */
static PlaylistTypes special_types[] = {
	PLAYLIST_TYPE ("audio/x-mpegurl", totem_pl_parser_add_m3u, NULL, FALSE),
	PLAYLIST_TYPE ("audio/playlist", totem_pl_parser_add_m3u, NULL, FALSE),
	PLAYLIST_TYPE ("audio/x-scpls", totem_pl_parser_add_pls, NULL, FALSE),
	PLAYLIST_TYPE ("application/x-smil", totem_pl_parser_add_smil, NULL, FALSE),
	PLAYLIST_TYPE ("application/smil", totem_pl_parser_add_smil, NULL, FALSE),
	PLAYLIST_TYPE ("video/x-ms-wvx", totem_pl_parser_add_asx, NULL, FALSE),
	PLAYLIST_TYPE ("audio/x-ms-wax", totem_pl_parser_add_asx, NULL, FALSE),
	PLAYLIST_TYPE ("application/xspf+xml", totem_pl_parser_add_xspf, NULL, FALSE),
	PLAYLIST_TYPE ("text/uri-list", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list, FALSE),
	PLAYLIST_TYPE ("text/x-google-video-pointer", totem_pl_parser_add_gvp, NULL, FALSE),
	PLAYLIST_TYPE ("text/google-video-pointer", totem_pl_parser_add_gvp, NULL, FALSE),
	PLAYLIST_TYPE ("audio/x-iriver-pla", totem_pl_parser_add_pla, NULL, FALSE),
#ifndef TOTEM_PL_PARSER_MINI
	PLAYLIST_TYPE ("application/x-desktop", totem_pl_parser_add_desktop, NULL, TRUE),
	PLAYLIST_TYPE ("application/x-gnome-app-info", totem_pl_parser_add_desktop, NULL, TRUE),
	PLAYLIST_TYPE ("application/x-cd-image", totem_pl_parser_add_iso, NULL, TRUE),
	PLAYLIST_TYPE ("application/x-extension-img", totem_pl_parser_add_iso, NULL, TRUE),
	PLAYLIST_TYPE ("application/x-cue", totem_pl_parser_add_cue, NULL, TRUE),
	PLAYLIST_TYPE (DIR_MIME_TYPE, totem_pl_parser_add_directory, NULL, TRUE),
	PLAYLIST_TYPE (BLOCK_DEVICE_TYPE, totem_pl_parser_add_block, NULL, TRUE),
#endif
};

/* These ones are "dual" types, might be a video, might be a parser */
static PlaylistTypes dual_types[] = {
	PLAYLIST_TYPE2 ("audio/x-real-audio", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("audio/x-pn-realaudio", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("application/ram", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("application/vnd.rn-realmedia", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("audio/x-pn-realaudio-plugin", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("audio/vnd.rn-realaudio", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("audio/x-realaudio", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("text/plain", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("audio/x-ms-asx", totem_pl_parser_add_asx, totem_pl_parser_is_asx),
	PLAYLIST_TYPE2 ("video/x-ms-asf", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
	PLAYLIST_TYPE2 ("video/x-ms-wmv", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
	PLAYLIST_TYPE2 ("video/quicktime", totem_pl_parser_add_quicktime, totem_pl_parser_is_quicktime),
	PLAYLIST_TYPE2 ("application/x-quicktime-media-link", totem_pl_parser_add_quicktime, totem_pl_parser_is_quicktime),
	PLAYLIST_TYPE2 ("application/x-quicktimeplayer", totem_pl_parser_add_quicktime, totem_pl_parser_is_quicktime),
};

#ifndef TOTEM_PL_PARSER_MINI

static PlaylistTypes ignore_types[] = {
	PLAYLIST_TYPE3 ("image/*"),
	PLAYLIST_TYPE3 ("text/plain"),
	PLAYLIST_TYPE3 ("application/x-rar"),
	PLAYLIST_TYPE3 ("application/zip"),
	PLAYLIST_TYPE3 ("application/x-trash"),
};

gboolean
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

//FIXME remove ?
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

gboolean
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

TotemPlParserResult
totem_pl_parser_parse_internal (TotemPlParser *parser, const char *url,
				const char *base)
{
	char *mimetype;
	guint i;
	gpointer data = NULL;
	TotemPlParserResult ret = TOTEM_PL_PARSER_RESULT_ERROR;
	gboolean found = FALSE;

	if (parser->priv->recurse_level > RECURSE_LEVEL_MAX)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	/* Shouldn't gnome-vfs have a list of schemes it supports? */
	if (g_str_has_prefix (url, "mms") != FALSE
			|| g_str_has_prefix (url, "rtsp") != FALSE
			|| g_str_has_prefix (url, "icy") != FALSE) {
		DEBUG(g_print ("URL '%s' is MMS, RTSP or ICY, ignoring\n", url));
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	if (!parser->priv->recurse && parser->priv->recurse_level > 0) {
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	/* In force mode we want to get the data */
	if (parser->priv->force != FALSE) {
		mimetype = my_gnome_vfs_get_mime_type_with_data (url, &data, parser);
	} else {
		mimetype = g_strdup (gnome_vfs_get_mime_type_for_name (url));
	}

	DEBUG(g_print ("_get_mime_type_for_name for '%s' returned '%s'\n", url, mimetype));
	if (mimetype == NULL || strcmp (GNOME_VFS_MIME_TYPE_UNKNOWN, mimetype) == 0) {
		mimetype = my_gnome_vfs_get_mime_type_with_data (url, &data, parser);
		DEBUG(g_print ("_get_mime_type_with_data for '%s' returned '%s'\n", url, mimetype ? mimetype : "NULL"));
	}

	if (mimetype == NULL) {
		g_free (data);
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	if (strcmp (mimetype, EMPTY_FILE_TYPE) == 0) {
		g_free (data);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	/* If we're at the top-level of the parsing, try to get more
	 * data from the playlist parser */
	if (strcmp (mimetype, AUDIO_MPEG_TYPE) == 0 && parser->priv->recurse_level == 0 && data == NULL) {
		char *tmp;
		tmp = my_gnome_vfs_get_mime_type_with_data (url, &data, parser);
		if (tmp != NULL) {
			g_free (mimetype);
			mimetype = tmp;
		}
		DEBUG(g_print ("_get_mime_type_with_data for '%s' returned '%s' (was %s)\n", url, mimetype, AUDIO_MPEG_TYPE));
	}

	if (totem_pl_parser_mimetype_is_ignored (parser, mimetype) != FALSE) {
		g_free (mimetype);
		g_free (data);
		return TOTEM_PL_PARSER_RESULT_IGNORED;
	}

	if (parser->priv->recurse || parser->priv->recurse_level == 0) {
		parser->priv->recurse_level++;

		for (i = 0; i < G_N_ELEMENTS(special_types); i++) {
			if (strcmp (special_types[i].mimetype, mimetype) == 0) {
				DEBUG(g_print ("URL '%s' is special type '%s'\n", url, mimetype));
				if (parser->priv->disable_unsafe != FALSE && special_types[i].unsafe != FALSE) {
					g_free (mimetype);
					g_free (data);
					return TOTEM_PL_PARSER_RESULT_IGNORED;
				}
				ret = (* special_types[i].func) (parser, url, base, data);
				found = TRUE;
				break;
			}
		}

		for (i = 0; i < G_N_ELEMENTS(dual_types) && found == FALSE; i++) {
			if (strcmp (dual_types[i].mimetype, mimetype) == 0) {
				DEBUG(g_print ("URL '%s' is dual type '%s'\n", url, mimetype));
				if (data == NULL) {
					g_free (mimetype);
					mimetype = my_gnome_vfs_get_mime_type_with_data (url, &data, parser);
				}
				ret = (* dual_types[i].func) (parser, url, base, data);
				found = TRUE;
				break;
			}
		}

		g_free (data);

		parser->priv->recurse_level--;
	}

	if (ret == TOTEM_PL_PARSER_RESULT_SUCCESS) {
		g_free (mimetype);
		return ret;
	}

	if (totem_pl_parser_ignore_from_mimetype (parser, mimetype)) {
		g_free (mimetype);
		return TOTEM_PL_PARSER_RESULT_IGNORED;
	}
	g_free (mimetype);

	if (ret != TOTEM_PL_PARSER_RESULT_SUCCESS && parser->priv->fallback) {
		totem_pl_parser_add_one_url (parser, url, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return ret;
}

TotemPlParserResult
totem_pl_parser_parse_with_base (TotemPlParser *parser, const char *url,
				 const char *base, gboolean fallback)
{
	g_return_val_if_fail (TOTEM_IS_PL_PARSER (parser), TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_return_val_if_fail (url != NULL, TOTEM_PL_PARSER_RESULT_UNHANDLED);

	if (totem_pl_parser_scheme_is_ignored (parser, url) != FALSE)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;

	g_return_val_if_fail (strstr (url, "://") != NULL,
			TOTEM_PL_PARSER_RESULT_IGNORED);

	parser->priv->recurse_level = 0;
	parser->priv->fallback = fallback != FALSE;
	return totem_pl_parser_parse_internal (parser, url, base);
}

TotemPlParserResult
totem_pl_parser_parse (TotemPlParser *parser, const char *url,
		       gboolean fallback)
{
	return totem_pl_parser_parse_with_base (parser, url, NULL, fallback);
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

#define D(x) if (debug) x

gboolean
totem_pl_parser_can_parse_from_data (const char *data,
				     gsize len,
				     gboolean debug)
{
	const char *mimetype;
	guint i;

	g_return_val_if_fail (data != NULL, FALSE);

	/* Bad cast! */
	mimetype = gnome_vfs_get_mime_type_for_data ((gpointer) data, (int) len);

	if (mimetype == NULL || strcmp (GNOME_VFS_MIME_TYPE_UNKNOWN, mimetype) == 0) {
		D(g_message ("totem_pl_parser_can_parse_from_data couldn't get mimetype"));
		return FALSE;
	}

	for (i = 0; i < G_N_ELEMENTS(special_types); i++) {
		if (strcmp (special_types[i].mimetype, mimetype) == 0) {
			D(g_message ("Is special type '%s'", mimetype));
			return TRUE;
		}
	}

	for (i = 0; i < G_N_ELEMENTS(dual_types); i++) {
		if (strcmp (dual_types[i].mimetype, mimetype) == 0) {
			D(g_message ("Should be dual type '%s', making sure now", mimetype));
			if (dual_types[i].iden != NULL) {
				gboolean retval = (* dual_types[i].iden) (data, len);
				D(g_message ("%s dual type '%s'",
							retval ? "Is" : "Is not", mimetype));
				return retval;
			}
			return FALSE;
		}
	}

	D(g_message ("Is unsupported mime-type '%s'", mimetype));

	return FALSE;
}

gboolean
totem_pl_parser_can_parse_from_filename (const char *filename, gboolean debug)
{
	GMappedFile *map;
	GError *err = NULL;
	gboolean retval;

	g_return_val_if_fail (filename != NULL, FALSE);

	map = g_mapped_file_new (filename, FALSE, &err);
	if (map == NULL) {
		D(g_message ("couldn't mmap %s: %s", filename, err->message));
		g_error_free (err);
		return FALSE;
	}

	retval = totem_pl_parser_can_parse_from_data
		(g_mapped_file_get_contents (map),
		 g_mapped_file_get_length (map), debug);

	g_mapped_file_free (map);

	return retval;
}

