
#include "gtk-message.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/X.h>

struct GtkMessagePriv
{
	char *message_id;
	GType type;
	gpointer data;

	guint ref;
};

static GdkWindow *window;

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

static GdkWindow *
gdk_window_new_simple (void)
{
	GtkWidget *win;

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize (win);

	return win->window;
}

static void
selection_get_func (GtkClipboard *clipboard, GtkSelectionData *selection_data,
		                guint info, gpointer user_data_or_owner)
{
	if (GDK_IS_WINDOW (window) == FALSE)
		return;

//FIXME	selection_data = GINT_TO_POINTER (GDK_WINDOW_XID (window));
}

static void
selection_clear_func (GtkClipboard *clipboard, gpointer user_data_or_owner)
{
	return;
}

gboolean
gtk_program_register (const gchar *unique_id)
{
	gboolean result = FALSE;
	GtkClipboard *clipboard;
	Atom clipboard_atom;
	char *selection_name;
	GtkTargetEntry targets[0];

	selection_name = gtk_message_make_selection_name (unique_id);
	clipboard_atom = gdk_x11_get_xatom_by_name (selection_name);

	targets[0].target = selection_name;
	targets[0].flags = 0;
	targets[0].info = 0;

	XGrabServer (GDK_DISPLAY());

	if (XGetSelectionOwner (GDK_DISPLAY(), clipboard_atom) != None)
		goto out;

	clipboard = gtk_clipboard_get (gdk_atom_intern (selection_name, FALSE));
	if (!gtk_clipboard_set_with_data  (clipboard, targets,
				G_N_ELEMENTS (targets),
				selection_get_func,
				selection_clear_func, NULL))
		goto out;

	result = TRUE;

out:
	XUngrabServer (GDK_DISPLAY());
	gdk_flush();

	if (result == TRUE)
	{
		/* Store the GdkWindow, so we can get the XID */
		window = gdk_window_new_simple ();
	} else {
		/* Grab the XID for the already running app */
		//FIXME
	}

	return result;
}

static gchar*
gtk_message_make_selection_name (const gchar *unique_id)
{
	gchar *upper_name, *retval;

	g_return_val_if_fail (unique_id != NULL, NULL);

	upper_name = g_ascii_strup (unique_id, -1);
	retval = g_strdup_printf ("_%s_SELECTION", upper_name);
	g_free (upper_name);

	g_message ("gtk_message_make_selection_name: %s", retval);

	return retval;
}

