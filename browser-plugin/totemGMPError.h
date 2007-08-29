/* Totem GMP plugin
 *
 * Copyright © 2006, 2007 Christian Persch
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
 * $Id: totemGMPPlugin.h 3717 2006-11-15 17:21:16Z chpe $
 */

#ifndef __GMP_ERROR_H__
#define __GMP_ERROR_H__

#include <nsIClassInfo.h>

#include "totemIGMPError.h"
#include "totemIGMPErrorItem.h"

class totemScriptablePlugin;

class totemGMPError : public totemIGMPError,
		      public totemIGMPErrorItem,
		      public nsIClassInfo
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_TOTEMIGMPERROR
    NS_DECL_TOTEMIGMPERRORITEM
    NS_DECL_NSICLASSINFO

    totemGMPError (totemScriptablePlugin *aPlugin);

  private:
    ~totemGMPError ();

    totemScriptablePlugin *mPlugin;
    PRInt32 mCount;
};

#endif /* __GMP_ERROR_H__ */
