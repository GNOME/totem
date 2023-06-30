/* totem-session.h

   Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>

   SPDX-License-Identifier: GPL-3-or-later

   Author: Bastien Nocera <hadess@hadess.net>
 */

#pragma once

#include "totem.h"

gboolean totem_session_try_restore (Totem *totem);
void totem_session_save (Totem *totem);
void totem_session_cleanup (Totem *totem);
