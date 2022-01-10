#!/bin/sh

GST_DEBUG=*:5 gst-inspect-1.0 "$1" > $MESON_BUILD_ROOT/meson-logs/gst-inspect-$1-log.txt
