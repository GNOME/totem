
#include <config.h>
#include <glib.h>
#include "totem.h"

#ifdef HAVE_IRMAN

#include <stdio.h>
#include <errno.h>
#include <gconf/gconf-client.h>
#include <irman.h>


#define TOTEM_IRMAN_KEY_PLAY "play"
#define TOTEM_IRMAN_KEY_PAUSE "pause"
#define TOTEM_IRMAN_KEY_SEEK_FORWARD "seek_forward"
#define TOTEM_IRMAN_KEY_SEEK_BACKWARD "seek_backward"
#define TOTEM_IRMAN_KEY_FULLSCREEN "fullscreen"
#define TOTEM_IRMAN_KEY_VOL_UP "volume_up"
#define TOTEM_IRMAN_KEY_VOL_DOWN "volume_down"
#define TOTEM_IRMAN_KEY_NEXT "next"
#define TOTEM_IRMAN_KEY_PREVIOUS "previous"
#define TOTEM_IRMAN_KEY_QUIT "quit"

static gboolean
totem_irman_match_key (Totem *totem, const gchar *val, const gchar *key)
{
	gchar *gconf_val;
	gchar *key_name;
	gboolean ret;

	key_name = g_strdup_printf (TOTEM_GCONF_PREFIX "/ir/%s", key);

	gconf_val = gconf_client_get_string (totem_get_gconf_client (totem),
					     key_name, NULL);

	if (gconf_val == NULL) {
		g_free (key_name);
		return FALSE;
	} else if (strcmp (gconf_val, val) == 0)
		ret = TRUE;
	else
		ret = FALSE;

	g_free (key_name);
	g_free (gconf_val);

	return ret;
}

static gboolean
totem_irman_idle (Totem *totem)
{
	unsigned char *code;
	gchar *text;

	code = ir_poll_code ();

	if (code == NULL)
		return TRUE;

	text = ir_code_to_text (code);

	if (totem_irman_match_key (totem, text, TOTEM_IRMAN_KEY_PLAY))
		totem_action_play (totem, 0);
	else if (totem_irman_match_key (totem, text, TOTEM_IRMAN_KEY_PAUSE))
		totem_action_play_pause (totem);
	else if (totem_irman_match_key (totem, text,
					TOTEM_IRMAN_KEY_SEEK_FORWARD))
		totem_action_seek_relative (totem, 60);
	else if (totem_irman_match_key (totem, text,
					TOTEM_IRMAN_KEY_SEEK_BACKWARD))
		totem_action_seek_relative (totem, -15);
	else if (totem_irman_match_key (totem, text,
					TOTEM_IRMAN_KEY_FULLSCREEN))
		totem_action_fullscreen_toggle (totem);
	else if (totem_irman_match_key (totem, text,
					TOTEM_IRMAN_KEY_VOL_UP))
		totem_action_volume_relative (totem, 8);
	else if (totem_irman_match_key (totem, text,
					TOTEM_IRMAN_KEY_VOL_DOWN))
		totem_action_volume_relative (totem, -8);
	else if (totem_irman_match_key (totem, text,
					TOTEM_IRMAN_KEY_NEXT))
		totem_action_next (totem);
	else if (totem_irman_match_key (totem, text,
					TOTEM_IRMAN_KEY_PREVIOUS))
		totem_action_previous (totem);
	else if (totem_irman_match_key (totem, text,
					TOTEM_IRMAN_KEY_QUIT))
		totem_action_exit (totem);

	return TRUE;
}

gboolean
totem_irman_init (Totem *totem)
{
	gchar *device;


	device = gconf_client_get_string (totem_get_gconf_client (totem),
				TOTEM_GCONF_PREFIX "/ir/device", NULL);

	if (device == NULL)
		device = g_strdup ("/dev/ttyS0");

	if (ir_init (device) < 0) {
		g_warning ("Could not initialize ir support: %s",
			   ir_strerror (errno));
		g_free (device);
		return FALSE;
	}

	
	/* poll for an IR code every 50 millis */
	g_timeout_add (50, (GSourceFunc)totem_irman_idle, totem);


	g_free (device);
	return TRUE;
}

#else

gboolean
totem_irman_init (Totem *totem)
{
	return FALSE;
}


#endif /* HAVE_IRMAN */
