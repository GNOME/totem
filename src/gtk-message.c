
#include "gtk-message.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/X.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct GtkMessagePriv
{
	char *message_id;
	GType type;
	gpointer data;

	guint ref;
};

struct GtkMessageQueuePriv
{
	gboolean is_server;
	key_t key;
	int msqid;

	guint ref;
};

static gchar* gtk_message_make_selection_name (const gchar *unique_id);

GtkMessage *
gtk_message_new (char *message_id, GType type, gpointer data)
{
	GtkMessage *msg;

	g_return_val_if_fail (message_id != NULL, NULL);
	g_return_val_if_fail (type != 0, NULL);
	g_return_val_if_fail (data != NULL, NULL);

	msg = g_new0 (GtkMessage, 1);
	msg->priv = g_new0 (GtkMessagePriv, 1);
	msg->priv->message_id = message_id;
	msg->priv->type = type;
	msg->priv->data = data;
	msg->priv->ref = 1;

	return msg;
}
/*
void
gtk_message_unref (GtkMessage *message)
{
	msg->priv->ref --;

	if (msg->priv->ref == 0)
	{
		g_free (msg->priv->message_id);
		g_free (msg->priv->data);
		g_free (msg->priv);
		g_free (msg);
	}
	
}

gboolean gtk_message_send_to (GtkMessage *message, gchar *destination);
*/

void
gtk_message_queue_unref (GtkMessageQueue *queue)
{
	if (queue == NULL)
		return;

	g_return_if_fail (queue->priv != NULL);
	queue->priv->ref --;

	if (queue->priv->ref == 0)
	{
		g_free (queue->priv);
		g_free (queue);
	}
}

static void
selection_get_func (GtkClipboard *clipboard, GtkSelectionData *selection_data,
		                guint info, gpointer user_data_or_owner)
{
}

static void
selection_clear_func (GtkClipboard *clipboard, gpointer user_data_or_owner)
{
}

static gboolean
gtk_program_register (const gchar *unique_id)
{
	gboolean result = FALSE;
	GtkClipboard *clipboard;
	Atom clipboard_atom;
	GtkTargetEntry targets[1];
	char *selection_name = NULL;

	selection_name = gtk_message_make_selection_name (unique_id);
	clipboard_atom = gdk_x11_get_xatom_by_name (selection_name);

	XGrabServer (GDK_DISPLAY());

	if (XGetSelectionOwner (GDK_DISPLAY(), clipboard_atom) != None)
		goto out;

	clipboard = gtk_clipboard_get (gdk_atom_intern (selection_name, FALSE));

	targets[0].target = selection_name;
	targets[0].flags = 0;
	targets[0].info = 0;

	if (!gtk_clipboard_set_with_data  (clipboard, targets,
				G_N_ELEMENTS (targets),
				selection_get_func,
				selection_clear_func, NULL))
		goto out;

	result = TRUE;

out:
	XUngrabServer (GDK_DISPLAY());
	gdk_flush();

	return result;
}

GtkMessageQueue *
gtk_message_queue_new (const gchar *unique_id, const gchar *binary_path)
{
	GtkMessageQueue *q;

	g_return_val_if_fail (unique_id != NULL, NULL);
	g_return_val_if_fail (binary_path != NULL, NULL);

	q = g_new0 (GtkMessageQueue, 1);
	q->priv = g_new0 (GtkMessageQueuePriv, 1);
	q->priv->ref = 1;

	q->priv->is_server = gtk_program_register (unique_id);

	/* Create the key */
	q->priv->key = ftok (binary_path, unique_id[0]);
	if (q->priv->key == -1)
	{
		g_message ("ftok: failed");
		gtk_message_queue_unref (q);
		return NULL;
	}

	/* Connect to the queue */
	if (q->priv->is_server == TRUE)
	{
		/* destroy the msgctl if it's around */
		q->priv->msqid = msgget(q->priv->key, 0600);
		if (q->priv->msqid != -1)
			msgctl (q->priv->msqid, IPC_RMID, NULL);

		q->priv->msqid = msgget(q->priv->key, 0600 | IPC_CREAT);
	} else {
		q->priv->msqid = msgget(q->priv->key, 0600);
	}

	if (q->priv->msqid == -1)
	{
		g_message ("msgget: failed");
		gtk_message_queue_unref (q);
		return NULL;
	}

	return q;
}

gboolean
gtk_message_queue_is_server (GtkMessageQueue *queue)
{
	g_return_val_if_fail (queue != NULL, FALSE);
	g_return_val_if_fail (queue->priv != NULL, FALSE);

	return queue->priv->is_server;
}

static gchar*
gtk_message_make_selection_name (const gchar *unique_id)
{
	gchar *upper_name, *retval;

	g_return_val_if_fail (unique_id != NULL, NULL);

	upper_name = g_ascii_strup (unique_id, -1);
	retval = g_strdup_printf ("_%s_SELECTION", upper_name);
	g_free (upper_name);

	return retval;
}

