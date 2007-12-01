noinst_PROGRAMS = bvw-test

noinst_LTLIBRARIES = libbaconvideowidget.la

bvw_test_SOURCES = bvw-test.c

bvw_test_CPPFLAGS = \
	-I$(top_srcdir)/plparse		\
	-DDATADIR=\"$(pkgdatadir)\"	\
	-DLOGO_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"totem_logo.png\"
	$(DISABLE_DEPRECATED)		\
	$(AM_CPPFLAGS)

bvw_test_CFLAGS = \
	$(EXTRA_GNOME_CFLAGS)	\
	$(AM_CFLAGS)		\
	-I$(top_srcdir)/

bvw_test_LDADD = \
	libbaconvideowidget.la \
	$(EXTRA_GNOME_LIBS) $(GTK_LIBS) $(XVIDMODE_LIBS) $(NVTV_LIBS) $(XTEST_LIBS) $(X_LIBS)

BVWMARSHALFILES = baconvideowidget-marshal.c baconvideowidget-marshal.h
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
BUILT_SOURCES = $(BVWMARSHALFILES)

baconvideowidget-marshal.h: baconvideowidget-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --header > baconvideowidget-marshal.h )
baconvideowidget-marshal.c: baconvideowidget-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --body --header > baconvideowidget-marshal.c )

libbaconvideowidget_la_SOURCES = \
	$(BVWMARSHALFILES) 		\
	bacon-video-widget.h		\
	bacon-video-widget-common.h bacon-video-widget-common.c \
	bacon-resize.h bacon-resize.c 	\
	video-utils.c video-utils.h 	\
	debug.h

if TOTEM_GST
libbaconvideowidget_la_SOURCES += \
	bacon-video-widget-gst-0.10.c \
	gstscreenshot.c \
	gstscreenshot.h
else
libbaconvideowidget_la_SOURCES += \
	bacon-video-widget-xine.c
endif

libbaconvideowidget_la_CPPFLAGS =		\
	-D_REENTRANT				\
	-DGCONF_PREFIX=\""/apps/totem"\"	\
	$(DISABLE_DEPRECATED)			\
	$(AM_CPPFLAGS)

libbaconvideowidget_la_CFLAGS = \
	$(EXTRA_GNOME_CFLAGS)	\
	$(NVTV_CFLAGS) 		\
	$(AM_CFLAGS)		\
	-I$(top_srcdir)/


libbaconvideowidget_la_LIBADD = \
	$(XVIDMODE_LIBS)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	baconvideowidget-marshal.list
