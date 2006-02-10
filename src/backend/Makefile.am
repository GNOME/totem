noinst_PROGRAMS = bvw-test

noinst_LTLIBRARIES = libbaconvideowidget.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(srcdir)/../plparse \
	$(EXTRA_GNOME_CFLAGS)

bvw_test_SOURCES = bvw-test.c
bvw_test_CFLAGS = \
	-DDATADIR=\""$(datadir)"\" \
	-DLOGO_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"totem_logo.png\"
bvw_test_LDADD = \
	$(GTK_LIBS) $(XVIDMODE_LIBS) $(NVTV_LIBS) $(XTEST_LIBS) $(X_LIBS) \
	libbaconvideowidget.la

if TOTEM_GST_010
MM = bacon-video-widget-gst-0.10.c \
	gstscreenshot.c \
	gstscreenshot.h
else
if TOTEM_GST
MM = bacon-video-widget-gst.c \
	gstscreenshot.c \
	gstscreenshot.h
else
MM = bacon-video-widget-xine.c
endif
endif

BVWMARSHALFILES = baconvideowidget-marshal.c baconvideowidget-marshal.h
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
BUILT_SOURCES = $(BVWMARSHALFILES)

baconvideowidget-marshal.h: baconvideowidget-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --header > baconvideowidget-marshal.h )
baconvideowidget-marshal.c: baconvideowidget-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --body --header > baconvideowidget-marshal.c )

libbaconvideowidget_la_SOURCES = \
	$(BVWMARSHALFILES) $(MM) bacon-video-widget.h \
	bacon-video-widget-private.h bacon-resize.h \
	bacon-resize.c video-utils.c video-utils.h debug.h
libbaconvideowidget_la_CFLAGS = \
	-D_REENTRANT \
	$(NVTV_CFLAGS) \
	-DGCONF_PREFIX=\""/apps/totem"\"

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	bacon-video-widget-xine.c \
	bacon-video-widget-gst.c \
	bacon-video-widget-gst-0.10.c \
	gstscreenshot.c \
	gstscreenshot.h \
	baconvideowidget-marshal.list
