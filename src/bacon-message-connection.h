
#include <glib.h>

typedef void (*BaconMessageReceivedFunc) (const char *message,
					  gpointer user_data);

typedef struct BaconMessageConnection BaconMessageConnection;

void bacon_message_connection_free			(BaconMessageConnection *conn);
BaconMessageConnection *bacon_message_connection_new	(const char *prefix);
void bacon_message_connection_free			(BaconMessageConnection *conn);
void bacon_message_connection_set_callback		(BaconMessageConnection *conn,
							 BaconMessageReceivedFunc func,
							 gpointer user_data);
void bacon_message_connection_send			(BaconMessageConnection *conn,
							 const char *message);
gboolean bacon_message_connection_get_is_server		(BaconMessageConnection *conn);

