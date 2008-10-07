#!/bin/sh

SVN_URI=http://svn.gnome.org/svn/libegg/trunk/libegg/smclient
FILES="eggdesktopfile.c
       eggdesktopfile.h
       eggsmclient.c
       eggsmclient.h
       eggsmclient-osx.c
       eggsmclient-private.h
       eggsmclient-win32.c
       eggsmclient-xsmp.c"
PATCHES="eggsmclient-1.patch
         eggsmclient-2.patch
         eggsmclient-3.patch"

echo "Obtaining latest version of the sources"
for FILE in $FILES
do
  svn export $SVN_URI/$FILE
done

echo "Applying patches"
for PATCH in $PATCHES
do
  patch -p3 -i $PATCH
done
