#!/bin/sh

if test x"$MESON_DIST_ROOT" != "x" && test -d $MESON_DIST_ROOT; then
  rm -rf $MESON_DIST_ROOT/flatpak
  exit $?
fi

echo MESON_DIST_ROOT must be set
exit 1
