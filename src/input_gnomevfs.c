/*
 * Copyright (C) 2000-2002 the xine project
 * 2002 Bastien Nocera <hadess@hadess.net>
 * 
 * This file is part of totem,
 * 
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libgnomevfs/gnome-vfs.h>

//#define D(x...)
#define D(x...) g_message (x)
#define LOG

#define PREVIEW_SIZE 2200
//#define PREVIEW_SIZE 16384
#define BUFSIZE 1024

#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/compat.h>
#include <xine/input_plugin.h>

#ifdef __GNUC__
#define LOG_MSG_STDERR(xine, message, args...) {                     \
    xine_log(xine, XINE_LOG_MSG, message, ##args);                 \
    fprintf(stderr, message, ##args);                                \
  }
#define LOG_MSG(xine, message, args...) {                            \
    xine_log(xine, XINE_LOG_MSG, message, ##args);                 \
    printf(message, ##args);                                         \
  }
#else
#define LOG_MSG_STDERR(xine, ...) {                                  \
    xine_log(xine, XINE_LOG_MSG, __VA_ARGS__);                     \
    fprintf(stderr, __VA_ARGS__);                                    \
  }
#define LOG_MSG(xine, ...) {                                         \
    xine_log(xine, XINE_LOG_MSG, __VA_ARGS__);                     \
    printf(__VA_ARGS__);                                             \
  }
#endif

typedef struct {
	input_plugin_t input_plugin;

	xine_t *xine;

	/* File */
	GnomeVFSHandle *fh;
	off_t curpos;
	char *mrl;
	/* Subtitle */
	GnomeVFSHandle *sub;

	/* Preview */
	char preview[PREVIEW_SIZE];
	off_t preview_size;
	off_t preview_pos;
} gnomevfs_input_plugin_t;

static off_t gnomevfs_plugin_read (input_plugin_t *this_gen, char *buf,
		off_t len);
static off_t gnomevfs_plugin_get_current_pos (input_plugin_t *this_gen);

static gboolean
scheme_can_seek (GnomeVFSHandle *handle)
{
	//FIXME to implement
	return TRUE;
}

static uint32_t
gnomevfs_plugin_get_capabilities (input_plugin_t *this_gen)
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;

	D("gnomevfs_plugin_get_capabilities: %s", this->mrl);

	if (this->fh)
	{
		if (scheme_can_seek (this->fh) == TRUE)
			return INPUT_CAP_SEEKABLE | INPUT_CAP_SPULANG;
		else
			return INPUT_CAP_SPULANG;
	}

	return INPUT_CAP_SEEKABLE | INPUT_CAP_SPULANG;
}

static int
gnomevfs_plugin_open (input_plugin_t *this_gen, char *mrl)
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;
	const char *subtitle_file;
	char *subtitle_path, *subtitle;
	GnomeVFSURI *uri;

	D("gnomevfs_plugin_open: %s", mrl);

	if (this->mrl)
	{
		g_free (this->mrl);
		this->mrl = NULL;
	}

	this->mrl = g_strdup(mrl);

	uri = gnome_vfs_uri_new (mrl);
	if (uri == NULL)
		return 0;

	/* local files should be handled by the file input */
	if (gnome_vfs_uri_is_local (uri) == TRUE)
	{
		gnome_vfs_uri_unref (uri);
		return 0;
	}

	subtitle_file = gnome_vfs_uri_get_fragment_identifier (uri);
	if (subtitle_file != NULL)
	{
		subtitle_path = gnome_vfs_uri_extract_dirname (uri);
		subtitle = g_strdup_printf ("%s%s", subtitle_path,
				subtitle_file);
		g_free (subtitle_path);

		LOG_MSG (this->xine,
			_("input_file: trying to open subtitle file '%s'\n"),
			subtitle);

		if (gnome_vfs_open (&(this->sub), subtitle, GNOME_VFS_OPEN_READ)
				!= GNOME_VFS_OK)
			LOG_MSG (this->xine,
					_("input_file: failed to open subtitle "
						"file '%s'\n"),
					subtitle);
	} else {
		this->sub = NULL;
	}

	if (gnome_vfs_open_uri (&(this->fh), uri, GNOME_VFS_OPEN_READ)
			!= GNOME_VFS_OK)
		return 0;

	D("gnomevfs_plugin_open: filling up preview");
	this->preview_size = gnomevfs_plugin_read (&this->input_plugin,
			this->preview, PREVIEW_SIZE);
	this->preview_pos  = 0;

	return 1;
}

static off_t
gnomevfs_plugin_read (input_plugin_t *this_gen, char *buf, off_t len)
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;
	off_t n, num_bytes;

	D("gnomevfs_plugin_read: %ld", (long int) len);

	num_bytes = 0;

	while (num_bytes < len)
	{
		if (this->preview_pos < this->preview_size)
		{
			n = this->preview_size - this->preview_pos;
			if (n > (len - num_bytes))
				n = len - num_bytes;
#ifdef LOG
			printf ("stdin: %lld bytes from preview "
					"(which has %lld bytes)\n",
					n, this->preview_size);
#endif

			memcpy (&buf[num_bytes],
					&this->preview[this->preview_pos], n);

			this->preview_pos += n;

			D("gnomevfs_plugin_read: read from preview");
		} else {
			GnomeVFSResult res;

			res = gnome_vfs_read (this->fh, &buf[num_bytes],
					(GnomeVFSFileSize) (len - num_bytes),
					(GnomeVFSFileSize *)&n);

			D("gnomevfs_plugin_read: read %ld from gnome-vfs",
					(long int) n);

			if (res != GNOME_VFS_OK)
			{
				D("gnomevfs_plugin_read: gnome_vfs_read returns %s",
						gnome_vfs_result_to_string (res));
				return 0;
			}
		}

		if (n <= 0)
		{
			xine_log (this->xine, XINE_LOG_MSG,
					_("input_gnomevfs: read error\n"));
		}

		num_bytes += n;
		this->curpos += n;
	}

	return num_bytes;
}

/*
 * helper function to release buffer
 * in case demux thread is cancelled
 */
static void
pool_release_buffer (void *arg)
{
	buf_element_t *buf = (buf_element_t *) arg;
	if( buf != NULL )
		buf->free_buffer(buf);
}

static buf_element_t*
gnomevfs_plugin_read_block (input_plugin_t *this_gen, fifo_buffer_t *fifo,
		off_t todo)
{
	off_t total_bytes;
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;
	buf_element_t *buf = fifo->buffer_pool_alloc (fifo);

	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE,NULL);
	pthread_cleanup_push (pool_release_buffer, buf);

	buf->content = buf->mem;
	buf->type = BUF_DEMUX_BLOCK;

	total_bytes = gnomevfs_plugin_read (this_gen, buf->content, todo);

	while (total_bytes != todo)
	{
		buf->free_buffer (buf);
		buf = NULL;
	}

	if (buf != NULL)
		buf->size = total_bytes;

	pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);
	pthread_cleanup_pop (0);

	return buf;
}

static off_t
gnomevfs_plugin_seek (input_plugin_t *this_gen, off_t offset, int origin)
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;

	if (gnome_vfs_seek (this->fh, origin, offset) == GNOME_VFS_OK)
	{
		D ("gnomevfs_plugin_seek: %d", (int) (origin + offset));
		return (off_t) (origin + offset);
	} else
		return (off_t) gnomevfs_plugin_get_current_pos (this_gen);
}

static off_t
gnomevfs_plugin_get_current_pos (input_plugin_t *this_gen)
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;
	GnomeVFSFileSize offset;

	if (this->fh == NULL)
	{
		D ("gnomevfs_plugin_get_current_pos: (this->fh == NULL)");
		return 0;
	}

	if (gnome_vfs_tell (this->fh, &offset) == GNOME_VFS_OK)
	{
		D ("gnomevfs_plugin_get_current_pos: %d", (int) offset);
		return (off_t) offset;
	} else
		return 0;
}

static off_t
gnomevfs_plugin_get_length (input_plugin_t *this_gen)
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;
	GnomeVFSFileInfo info;

	if (this->fh == NULL)
	{
		D ("gnomevfs_plugin_get_length: (this->fh == NULL)");
		return 0;
	}

	if (gnome_vfs_get_file_info_from_handle (this->fh,
				&info,
				GNOME_VFS_FILE_INFO_DEFAULT) == GNOME_VFS_OK)
	{
		D ("gnomevfs_plugin_get_length: %d", (int) info.size);
		return (off_t) info.size;
	} else
		return 0;
}

static uint32_t
gnomevfs_plugin_get_blocksize (input_plugin_t *this_gen)
{
	return 0;
}

static int
gnomevfs_plugin_eject_media (input_plugin_t *this_gen)
{
	return 1; /* doesn't make sense */
}

static char*
gnomevfs_plugin_get_mrl (input_plugin_t *this_gen)
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;

	return this->mrl;
}

static void
gnomevfs_plugin_close (input_plugin_t *this_gen)
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;

	gnome_vfs_close (this->fh);
	this->fh = NULL;

	if (this->sub)
	{
		gnome_vfs_close (this->sub);
		this->sub = NULL;
	}
}

static void
gnomevfs_plugin_stop (input_plugin_t *this_gen)
{
	gnomevfs_plugin_close(this_gen);
}

static char
*gnomevfs_plugin_get_description (input_plugin_t *this_gen)
{
	return _("gnome-vfs input plugin as shipped with xine");
}

static char
*gnomevfs_plugin_get_identifier (input_plugin_t *this_gen)
{
	return "gnomevfs";
}

static int
gnomevfs_plugin_get_optional_data (input_plugin_t *this_gen, 
		void *data, int data_type)
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;

#ifdef LOG
	LOG_MSG (this->xine,
		_("input_gnomevfs: get optional data, type %08x, sub %p\n"),
		data_type, this->sub);
#endif

	switch (data_type) {
	case INPUT_OPTIONAL_DATA_TEXTSPU0:
		if(this->sub)
		{
			GnomeVFSHandle **tmp;
      
			/* dirty hacks... */
			tmp = data;
			*tmp = this->sub;

			return INPUT_OPTIONAL_SUCCESS;
		}
		break;
	case INPUT_OPTIONAL_DATA_SPULANG:
		sprintf(data, "%3s", (this->sub) ? "on" : "off");
		return INPUT_OPTIONAL_SUCCESS;
		break;
	case INPUT_OPTIONAL_DATA_PREVIEW:
		memcpy (data, this->preview, this->preview_size);
		return this->preview_size;
	default:
		return INPUT_OPTIONAL_UNSUPPORTED;
		break;
	}

	return INPUT_OPTIONAL_UNSUPPORTED;
}

static void
gnomevfs_plugin_dispose (input_plugin_t *this_gen )
{
	gnomevfs_input_plugin_t *this = (gnomevfs_input_plugin_t *) this_gen;

	if (this->mrl)
		g_free (this->mrl);

	g_free (this);
}

input_plugin_t
*init_input_plugin (int iface, xine_t *xine)
{
	gnomevfs_input_plugin_t *this;

	if (iface != 8)
	{
		LOG_MSG(xine,
			_("gnomevfs input plugin doesn't support plugin API "
			"version %d.\nPLUGIN DISABLED.\n"
			"This means there's a version mismatch between xine "
			"and this input plugin.\n"
			"Installing current input plugins should help.\n"),
			iface);

		return NULL;
	}

	if (gnome_vfs_initialized () == FALSE)
		gnome_vfs_init ();
	if (!g_thread_supported ())
		g_thread_init (NULL);

	this = g_new0 (gnomevfs_input_plugin_t, 1);
	this->xine = xine;

	this->input_plugin.interface_version  = INPUT_PLUGIN_IFACE_VERSION;
	this->input_plugin.get_capabilities   =
		gnomevfs_plugin_get_capabilities;
	this->input_plugin.open               = gnomevfs_plugin_open;
	this->input_plugin.read               = gnomevfs_plugin_read;
	this->input_plugin.read_block         = gnomevfs_plugin_read_block;
	this->input_plugin.seek               = gnomevfs_plugin_seek;
	this->input_plugin.get_current_pos    = gnomevfs_plugin_get_current_pos;
	this->input_plugin.get_length         = gnomevfs_plugin_get_length;
	this->input_plugin.get_blocksize      = gnomevfs_plugin_get_blocksize;
	this->input_plugin.get_dir            = NULL;
	this->input_plugin.eject_media        = gnomevfs_plugin_eject_media;
	this->input_plugin.get_mrl            = gnomevfs_plugin_get_mrl;
	this->input_plugin.close              = gnomevfs_plugin_close;
	this->input_plugin.stop               = gnomevfs_plugin_stop;
	this->input_plugin.get_description    = gnomevfs_plugin_get_description;
	this->input_plugin.get_identifier     = gnomevfs_plugin_get_identifier;
	this->input_plugin.get_autoplay_list  = NULL;
	this->input_plugin.get_optional_data  =
		gnomevfs_plugin_get_optional_data;
	this->input_plugin.dispose            = gnomevfs_plugin_dispose;
	this->input_plugin.is_branch_possible = NULL;

	this->fh                     = NULL;
	this->sub                    = NULL;
	this->mrl                    = NULL;
 
	return (input_plugin_t *) this;
}

