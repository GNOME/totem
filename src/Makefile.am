noinst_PROGRAMS = bvw-test list_v4l test-parser disc-test metadata-test

if TOTEM_VANITY
VANITY=vanity
else
VANITY=
endif

if HAVE_GNOME
TOTEM_PROPERTIES_PAGE = totem-properties-page
else
TOTEM_PROPERTIES_PAGE =
endif

if HAVE_MOZILLA
TOTEM_MOZILLA_VIEWER = totem-mozilla-viewer
TOTEM_MOZILLA_PLUGIN = libtotem_mozilla.la
else
TOTEM_MOZILLA_VIEWER = 
TOTEM_MOZILLA_PLUGIN =
endif

bin_PROGRAMS = totem totem-video-thumbnailer $(VANITY)
libexec_PROGRAMS = $(TOTEM_MOZILLA_VIEWER) $(TOTEM_PROPERTIES_PAGE)
plugin_LTLIBRARIES = $(TOTEM_MOZILLA_PLUGIN)

if TOTEM_GST
MM = bacon-video-widget-gst.c \
	gstscreenshot.c \
	gstscreenshot.h
else
MM = bacon-video-widget-xine.c
endif

BVWMARSHALFILES = baconvideowidget-marshal.h baconvideowidget-marshal.c
PLPARSERMARSHALFILES = totemplparser-marshal.c totemplparser-marshal.h
MARSHALFILES = $(BVWMARSHALFILES) $(PLPARSERMARSHALFILES)
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`

baconvideowidget-marshal.h: baconvideowidget-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --header > baconvideowidget-marshal.h )
baconvideowidget-marshal.c: baconvideowidget-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --body --header > baconvideowidget-marshal.c )
totemplparser-marshal.c: totemplparser-marshal.h
	 ( $(GLIB_GENMARSHAL) --prefix=totemplparser_marshal $(srcdir)/totemplparser-marshal.list --header --body > totemplparser-marshal.c )
totemplparser-marshal.h: totemplparser-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=totemplparser_marshal $(srcdir)/totemplparser-marshal.list --header > totemplparser-marshal.h )


EGG_RECENT_FILES =                      \
	egg-recent-model.c              \
        egg-recent-model.h              \
	egg-recent-item.c               \
	egg-recent-item.h               \
	egg-recent-view.c               \
	egg-recent-view.h               \
	egg-recent-view-gtk.c           \
        egg-recent-view-gtk.h           \
	egg-recent-util.c               \
	egg-recent-util.h

BACON_CD_SELECTION = 			\
	bacon-message-connection.c	\
	bacon-message-connection.h

BACON_V4L_SELECTION =			\
	video-dev.c			\
	video-dev.h			\
	bacon-v4l-selection.c		\
	bacon-v4l-selection.h

EGGDIR=$(srcdir)/../../libegg/libegg/recent-files/
BACONDIR=$(srcdir)/../../nautilus-cd-burner/
regenerate-built-sources:
	EGGFILES="$(EGG_RECENT_FILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh || true
	EGGFILES="$(BACON_CD_SELECTION)" EGGDIR="$(BACONDIR)" $(srcdir)/update-from-egg.sh || true

VIDEOWIDGET = baconvideowidget-marshal.list totemplparser-marshal.list $(BVWMARSHALFILES) $(MM) bacon-video-widget.h bacon-resize.h bacon-resize.c video-utils.c video-utils.h debug.h

totem_SOURCES = 					\
	$(EGG_RECENT_FILES)				\
	$(BACON_CD_SELECTION)				\
	$(VIDEOWIDGET)					\
	$(PLPARSERMARSHALFILES)				\
	totem.c totem.h					\
	totem-remote.c totem-remote.h			\
	totem-preferences.c totem-preferences.h		\
	totem-stock-icons.c totem-stock-icons.h		\
	totem-private.h			\
	bacon-video-widget-properties.c	\
	bacon-video-widget-properties.h	\
	scrsaver.c scrsaver.h		\
	totem-options.c totem-options.h	\
	totem-pl-parser.c totem-pl-parser.h \
	totem-playlist.c totem-playlist.h \
	totem-screenshot.c		\
	totem-screenshot.h		\
	totem-session.c totem-session.h	\
	totem-skipto.c totem-skipto.h	\
	totem-sublang.c totem-sublang.h	\
	totem-time-label.c totem-time-label.h \
	totem-uri.c totem-uri.h		\
	debug.h				\
	totem-statusbar.c		\
	totem-statusbar.h		\
	totem-disc.c totem-disc.h	\
	totem-gromit.c totem-gromit.h

totem_LDADD = $(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(REMOTE_LIBS) $(XVIDMODE_LIBS)  $(NVTV_LIBS)

totem_video_thumbnailer_SOURCES = $(VIDEOWIDGET) totem-video-thumbnailer.c
totem_video_thumbnailer_LDADD = $(X_LIBS) $(GTK_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS)  $(NVTV_LIBS) $(EXTRA_GNOME_LIBS)

totem_properties_page_SOURCES =		\
	$(VIDEOWIDGET)			\
	bacon-video-widget-properties.c	\
	bacon-video-widget-properties.h	\
	totem-properties-page.c
totem_properties_page_LDADD = $(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS)  $(NVTV_LIBS)

libtotem_mozilla_la_LDFLAGS = $(GTK_LIBS) $(module_flags)
libtotem_mozilla_la_SOURCES = totem-mozilla-plugin.c

totem_mozilla_viewer_SOURCES = 		\
	$(VIDEOWIDGET)			\
	bacon-video-widget-properties.c	\
	bacon-video-widget-properties.h	\
	totem-mozilla-viewer.c
totem_mozilla_viewer_LDADD = $(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) $(NVTV_LIBS)

plugindir = $(libdir)/mozilla/plugins/
module_flags = -export_dynamic -avoid-version

vanity_SOURCES =					\
	$(VIDEOWIDGET)					\
	$(BACON_V4L_SELECTION)				\
	totem-screenshot.c totem-screenshot.h		\
	vanity.c

vanity_LDADD = $(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS)  $(NVTV_LIBS)

bvw_test_SOURCES = $(VIDEOWIDGET) bvw-test.c
bvw_test_LDADD = $(X_LIBS) $(GTK_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS)  $(NVTV_LIBS)

metadata_test_SOURCES = $(VIDEOWIDGET) metadata-test.c
metadata_test_LDADD = $(X_LIBS) $(GTK_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS)  $(NVTV_LIBS)

disc_test_SOURCES = disc-test.c totem-disc.c
disc_test_LDADD = $(GTK_LIBS) $(EXTRA_GNOME_LIBS)

list_v4l_SOURCES = list_v4l.c $(BACON_V4L_SELECTION)
list_v4l_LDADD = $(GTK_LIBS)

test_parser_SOURCES = test-parser.c totem-disc.c totem-disc.h $(PLPARSERMARSHALFILES)
test_parser_LDADD = $(EXTRA_GNOME_LIBS)

INCLUDES = 						\
	-D_REENTRANT					\
	$(EXTRA_GNOME_CFLAGS)				\
	$(MOZILLA_CFLAGS)				\
	$(NVTV_CFLAGS)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBDIR=\""$(libdir)"\"			\
	-DLOGO_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"totem_logo.png\"

CLEANFILES = *.bak *.gladep core* *.orig *~ $(MARSHALFILES) $(desktop_DATA)
EXTRA_DIST =	bacon-video-widget-xine.c \
		bacon-video-widget-gst.c \
		gstscreenshot.c \
		gstscreenshot.h \
		$(BACON_V4L_SELECTION) \
		vanity.c \
		totem-mozilla-viewer.c \
		totem-mozilla-plugin.c

