/* Totem Basic Plugin
 *
 * Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2002 David A. Schleef <ds@schleef.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id: totemScriptablePlugin.h 3717 2006-11-15 17:21:16Z chpe $
 */

#ifndef __GMP_PLAYLIST_H__
#define __GMP_PLAYLIST_H__

#include <nsIClassInfo.h>

#include "totemIGMPPlaylist.h"

#include "totemGMPPlugin.h"

class totemGMPPlaylist : public totemIGMPPlaylist,
			 public nsIClassInfo
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_TOTEMIGMPPLAYLIST
    NS_DECL_NSICLASSINFO

    totemGMPPlaylist (totemScriptablePlugin *aScriptable);
  
  private:
    ~totemGMPPlaylist ();

    totemScriptablePlugin *mScriptable;
    nsCString mName;
};

#endif /* __GMP_PLAYLIST_H__ */
