
#ifndef __TOTEM_H__
#define __TOTEM_H__

#include <gconf/gconf-client.h>
#include "gtk-xine.h"

#define TOTEM_GCONF_PREFIX "/apps/totem"

typedef struct Totem Totem;

void	totem_action_exit			(Totem *totem);
void	totem_action_play			(Totem *totem, int offset);
void	totem_action_stop			(Totem *totem);
void	totem_action_play_pause			(Totem *totem);
void	totem_action_fullscreen_toggle		(Totem *totem);
void	totem_action_fullscreen			(Totem *totem, gboolean state);
void	totem_action_next			(Totem *totem);
void	totem_action_previous			(Totem *totem);
void	totem_action_seek_relative		(Totem *totem, int off_sec);
void	totem_action_volume_relative		(Totem *totem, int off_pct);
void	totem_action_open_files			(Totem *totem, char **list,
						 gboolean ignore_first);
void	totem_action_set_mrl			(Totem *totem, const char *mrl);

void	totem_action_play_media			(Totem *totem, MediaType type);

void	totem_action_toggle_aspect_ratio	(Totem *totem);
void	totem_action_set_scale_ratio		(Totem *totem, gfloat ratio);

GConfClient *totem_get_gconf_client		(Totem *totem);

#endif /* __TOTEM_H__ */
