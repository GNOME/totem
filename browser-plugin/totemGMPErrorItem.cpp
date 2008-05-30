/* Totem GMP plugin
 *
 * Copyright © 2004 Bastien Nocera <hadess@hadess.net>
 * Copyright © 2002 David A. Schleef <ds@schleef.org>
 * Copyright © 2006, 2008 Christian Persch
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 */

#include <config.h>

#include <string.h>

#include <glib.h>

#include "totemGMPErrorItem.h"

static const char *propertyNames[] = {
  "condition",
  "customURL",
  "errorCode",
  "errorContext",
  "errorDescription",
};

TOTEM_IMPLEMENT_NPCLASS (totemGMPErrorItem,
                         propertyNames, G_N_ELEMENTS (propertyNames),
                         NULL, 0,
                         NULL);

totemGMPErrorItem::totemGMPErrorItem (NPP aNPP)
  : totemNPObject (aNPP)
{
  TOTEM_LOG_CTOR ();
}

totemGMPErrorItem::~totemGMPErrorItem ()
{
  TOTEM_LOG_DTOR ();
}

bool
totemGMPErrorItem::GetPropertyByIndex (int aIndex,
                                       NPVariant *_result)
{
  TOTEM_LOG_GETTER (aIndex, totemGMPErrorItem);

  switch (Properties (aIndex)) {
    case eCondition:
      /* readonly attribute long condition; */
    case eErrorCode:
      /* readonly attribute long errorCode; */
      return Int32Variant (_result, 0);

    case eErrorContext:
      /* readonly attribute AUTF8String errorContext; */
    case eErrorDescription:
      /* readonly attribute AUTF8String errorDescription; */
      return StringVariant (_result, "Error<1>");

    case eCustomURL:
      /* readonly attribute AUTF8String customURL; */
      return StringVariant (_result, "http://www.gnome.org/projects/totem");
  }

  return false;
}

bool
totemGMPErrorItem::SetPropertyByIndex (int aIndex,
                                       const NPVariant *aValue)
{
  TOTEM_LOG_SETTER (aIndex, totemGMPErrorItem);

  return ThrowPropertyNotWritable ();
}
