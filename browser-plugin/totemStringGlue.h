/* Totem browser plugin
 *
 * Copyright © 2006 Bastien Nocera <hadess@hadess.net>
 * Copyright © 2006 Christian Persch
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
 */

#include <nsEmbedString.h>

#ifndef NS_LITERAL_CSTRING
#define NS_LITERAL_CSTRING(x) (x)
#endif

class nsString : public nsEmbedString {
  public:
    PRBool IsEmpty () { return !Length (); }
};

class nsCString : public nsEmbedCString {
  public:
    nsCString () { ::nsEmbedCString (); }
    explicit nsCString (const char *aData, PRUint32 aLength) { ::nsEmbedCString (aData, aLength); }
    explicit nsCString (const abstract_string_type& aOther) { Assign (aOther); }
    explicit nsCString (const char *&aOther) { Assign (aOther); }
    explicit nsCString (const nsCString& aData, int aStartPos, PRUint32 aLength) { ::nsEmbedCString (aData.get() + aStartPos, aLength-aStartPos); }
    PRBool IsEmpty () { return !Length (); }
    PRBool Equals (const self_type& aOther) { return !strcmp (aOther.get (), get ()); }
    PRBool Equals (const char_type *aOther) { return !strcmp (aOther, get ()); }
    void SetLength (PRUint32 aLen) { Assign (""); }
    self_type& operator=(const abstract_string_type& aOther) { Assign (aOther); return *this; }
    self_type& operator=(const char_type* aOther) { Assign (aOther); return *this; }
};

class NS_ConvertUTF16toUTF8 : public nsCString {
  public:
    explicit NS_ConvertUTF16toUTF8 (const nsAString& aString) {
	 NS_UTF16ToCString (aString, NS_CSTRING_ENCODING_UTF8, *this);
      }
    void SetLength (PRUint32 aLen) { Assign (""); }
};

typedef nsString nsDependentString;
typedef nsCString nsDependentCString;
typedef nsCString nsDependentCSubstring;

