
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "bacon-message-connection.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

struct BaconMessageConnection {
	gboolean is_server;
	int fd;
	char *path;
	GIOChannel *chan;
	/* callback */
	void (*func) (const char *message, gpointer user_data);
	gpointer data;
};

static gboolean
test_is_socket (const char *path)
{
	struct stat s;

	if (stat (path, &s) == -1)
		return FALSE;

	if (S_ISSOCK (s.st_mode))
		return TRUE;

	return FALSE;
}

#define BUF_SIZE 1024

static gboolean
server_cb (GIOChannel *source, GIOCondition condition, gpointer data)
{
	BaconMessageConnection *conn = (BaconMessageConnection *)data;
	char *message, buf[BUF_SIZE];
	int cd, alen, rc, offset;

	message = NULL;
	offset = 0;
	cd = accept (g_io_channel_unix_get_fd (source), NULL, &alen);

	memset (buf, sizeof (buf), '\0');
	rc = read (cd, buf, BUF_SIZE);

	while (rc != 0)
	{
		message = g_realloc (message, rc + offset);
		memcpy (message + offset, buf, MIN(rc, BUF_SIZE));

		offset = offset + rc;
		memset (buf, sizeof (buf), '\0');
		rc = read (cd, buf, BUF_SIZE);
	}

	if (message != NULL && conn->func != NULL)
			(*conn->func) (message, conn->data);

	g_free (message);

	return TRUE;
}

static gboolean
try_server (BaconMessageConnection *conn)
{
	struct sockaddr_un uaddr;
	GError *err = NULL;

	uaddr.sun_family = AF_UNIX;
	strncpy (uaddr.sun_path, conn->path,
			MIN (strlen(conn->path)+1, UNIX_PATH_MAX));
	conn->fd = socket (PF_UNIX, SOCK_STREAM, 0);
	if (bind (conn->fd, (struct sockaddr *) &uaddr, sizeof (uaddr)) == -1)
	{
		conn->fd = -1;
		return FALSE;
	}

	listen (conn->fd, 5);
	conn->chan = g_io_channel_unix_new (conn->fd);
	if (conn->chan == NULL || err != NULL)
	{
		conn->fd = -1;
		return FALSE;
	}

	g_io_add_watch (conn->chan, G_IO_IN, server_cb, conn);

	return TRUE;
}

static gboolean
try_client (BaconMessageConnection *conn)
{
	struct sockaddr_un uaddr;

	uaddr.sun_family = AF_UNIX;
	strncpy (uaddr.sun_path, conn->path,
			MIN(strlen(conn->path)+1, UNIX_PATH_MAX));
	conn->fd = socket (PF_UNIX, SOCK_STREAM, 0);
	if (connect (conn->fd, (struct sockaddr *) &uaddr,
				sizeof (uaddr)) == -1)
	{
		conn->fd = -1;
		return FALSE;
	}

	return TRUE;
}

BaconMessageConnection *
bacon_message_connection_new (const char *prefix)
{
	BaconMessageConnection *conn;
	char *filename, *path;

	g_return_val_if_fail (prefix != NULL, NULL);

	filename = g_strdup_printf ("%s.%s", prefix, g_get_user_name ());
	path = g_build_filename (G_DIR_SEPARATOR_S, g_get_tmp_dir (),
			filename, NULL);
	g_free (filename);

	conn = g_new0 (BaconMessageConnection, 1);
	conn->path = path;

	if (test_is_socket (conn->path) == FALSE)
	{
		try_server (conn);
		if (conn->fd == -1)
		{
			bacon_message_connection_free (conn);
			return NULL;
		}

		conn->is_server = TRUE;
		return conn;
	}

	if (try_client (conn) == FALSE)
	{
		unlink (path);
		try_server (conn);
		if (conn->fd == -1)
		{
			bacon_message_connection_free (conn);
			return NULL;
		}

		conn->is_server = TRUE;
		return conn;
	}

	conn->is_server = FALSE;
	return conn;
}

void
bacon_message_connection_free (BaconMessageConnection *conn)
{
	g_return_if_fail (conn != NULL);
	g_return_if_fail (conn->path != NULL);

	close (conn->fd);
	if (conn->is_server == TRUE)
	{
		g_io_channel_shutdown (conn->chan, FALSE, NULL);
		g_io_channel_unref (conn->chan);
		unlink (conn->path);
	}
	g_free (conn->path);
	g_free (conn);
}

void
bacon_message_connection_set_callback (BaconMessageConnection *conn,
				       BaconMessageReceivedFunc func,
				       gpointer user_data)
{
	g_return_if_fail (conn != NULL);
	g_assert (conn->is_server == TRUE);

	conn->func = func;
	conn->data = user_data;
}

void
bacon_message_connection_send (BaconMessageConnection *conn,
			       const char *message)
{
	g_return_if_fail (conn != NULL);
	g_assert (conn->is_server == FALSE);

	write (conn->fd, message, strlen (message) + 1);
}

gboolean
bacon_message_connection_get_is_server (BaconMessageConnection *conn)
{
	g_return_val_if_fail (conn != NULL, FALSE);
	return conn->is_server;
}

