
#include <glib.h>
#include <glib-object.h>

typedef struct GtkMessage GtkMessage;
typedef struct GtkMessagePriv GtkMessagePriv;
struct GtkMessage {
	GtkMessagePriv *priv;
};

typedef struct GtkMessageQueue GtkMessageQueue;
typedef struct GtkMessageQueuePriv GtkMessageQueuePriv;
struct GtkMessageQueue {
	GtkMessageQueuePriv *priv;
};

GtkMessage *gtk_message_new (char *message_id, GType type, gpointer data);
gboolean gtk_message_send_to (GtkMessage *message, GtkMessageQueue *queue);

/* Try to register the program as the unique instance running on this display
 * Returns FALSE if a program with the same ID is running
 * Returns TRUE if the program was successfully registered as the unique (first)
 * instance
 *
 * If you're running a gnome program, it is advised you use this snippet:
 * GnomeProgram *program;
 * const char *unique_id;
 *
 * program = gnome_program_get ();
 * unique_id = gnome_program_get_app_id (program);
 */

GtkMessageQueue *gtk_message_queue_new (const gchar *unique_id,
					const gchar *binary_path);
gboolean gtk_message_queue_is_server (GtkMessageQueue *queue);
void gtk_message_queue_unref (GtkMessageQueue *queue);

