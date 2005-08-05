SUBDIRS = backend plparse

noinst_PROGRAMS = list_v4l disc-test metadata-test

if TOTEM_VANITY
VANITY=vanity
else
VANITY=
endif

if HAVE_NAUTILUS
TOTEM_PROPERTIES_PAGE = libtotem-properties-page.la
else
TOTEM_PROPERTIES_PAGE =
endif

if HAVE_MOZILLA
TOTEM_MOZILLA_VIEWER = totem-mozilla-viewer
TOTEM_MOZILLA_PLUGIN = libtotem_mozilla.la

plugin_LTLIBRARIES = $(TOTEM_MOZILLA_PLUGIN)
plugindir = $(MOZILLA_PLUGINDIR)/plugins
else
TOTEM_MOZILLA_VIEWER = 
TOTEM_MOZILLA_PLUGIN =
endif

bin_PROGRAMS = totem totem-video-thumbnailer $(VANITY)
libexec_PROGRAMS = $(TOTEM_MOZILLA_VIEWER)
noinst_LTLIBRARIES = \
	libbaconpropertiespage.la \
	libbaconmessageconnection.la
nautilus_LTLIBRARIES = $(TOTEM_PROPERTIES_PAGE)

modules_flags = -avoid-version -module

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

BACON_MESSAGE_CONNECTION = \
	bacon-message-connection.c \
	bacon-message-connection.h

libbaconmessageconnection_la_SOURCES = $(BACON_MESSAGE_CONNECTION)

BACON_V4L_SELECTION =			\
	video-dev.c			\
	video-dev.h			\
	bacon-v4l-selection.c		\
	bacon-v4l-selection.h

EGGDIR=$(srcdir)/../../libegg/libegg/recent-files/
BACONDIR=$(srcdir)/../../libbacon/src
regenerate-built-sources:
	EGGFILES="$(EGG_RECENT_FILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh || true
	EGGFILES="$(BACON_MESSAGE_CONNECTION)" EGGDIR="$(BACONDIR)" $(srcdir)/update-from-egg.sh || true

libbaconpropertiespage_la_SOURCES = \
	bacon-video-widget-properties.c \
	bacon-video-widget-properties.h \
	totem-interface.c		\
	totem-interface.h

totem_SOURCES = 					\
	$(EGG_RECENT_FILES)				\
	totem.c totem.h					\
	totem-remote.c totem-remote.h			\
	totem-preferences.c totem-preferences.h		\
	totem-stock-icons.c totem-stock-icons.h		\
	totem-private.h			\
	totem-scrsaver.c		\
	totem-scrsaver.h		\
	totem-options.c totem-options.h	\
	totem-playlist.c totem-playlist.h \
	totem-screenshot.c		\
	totem-screenshot.h		\
	totem-session.c totem-session.h	\
	totem-sidebar.c totem-sidebar.h \
	totem-skipto.c totem-skipto.h	\
	totem-menu.c totem-menu.h	\
	totem-time-label.c totem-time-label.h \
	totem-uri.c totem-uri.h		\
	totem-statusbar.c		\
	totem-statusbar.h		\
	totem-gromit.c totem-gromit.h	\
	bacon-volume.c bacon-volume.h	\
	ev-sidebar.c ev-sidebar.h

totem_LDADD = \
	$(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(REMOTE_LIBS) \
	$(XVIDMODE_LIBS) $(NVTV_LIBS) plparse/libtotem-plparser.la \
	backend/libbaconvideowidget.la libbaconpropertiespage.la \
	libbaconmessageconnection.la

totem_video_thumbnailer_SOURCES = totem-video-thumbnailer.c
totem_video_thumbnailer_LDADD = \
	$(X_LIBS) $(GTK_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) \
	$(NVTV_LIBS) $(EXTRA_GNOME_LIBS) backend/libbaconvideowidget.la

nautilusdir = $(libdir)/nautilus/extensions-1.0/
libtotem_properties_page_la_CFLAGS = -I$(top_srcdir)
libtotem_properties_page_la_SOURCES = 	\
	totem-properties-main.c		\
	totem-properties-view.c		\
	totem-properties-view.h

libtotem_properties_page_la_LIBADD = \
	$(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) \
	$(NVTV_LIBS) $(NAUTILUS_LIBS) \
	backend/libbaconvideowidget.la libbaconpropertiespage.la
libtotem_properties_page_la_LDFLAGS = $(module_flags)

if HAVE_MOZILLA

BUILT_SOURCES = \
	totem_mozilla_scripting.h \
	libtotem_mozilla.xpt \
	totem-mozilla-interface.h

totem_mozilla_scripting.h: totem_mozilla_scripting.idl
	$(MOZILLA_XPIDL) -m header -I $(MOZILLA_IDLDIR) $<

libtotem_mozilla.xpt: totem_mozilla_scripting.idl
	$(MOZILLA_XPIDL) -m typelib -I $(MOZILLA_IDLDIR) $< && \
	mv totem_mozilla_scripting.xpt libtotem_mozilla.xpt

typelib_DATA = libtotem_mozilla.xpt
typelibdir = $(MOZILLA_PLUGINDIR)/plugins

libtotem_mozilla_la_LDFLAGS = \
	$(DBUS_LIBS) $(MOZILLA_LIBS) $(module_flags)
libtotem_mozilla_la_SOURCES = \
	totem-mozilla-plugin.cpp \
	totem_mozilla_scripting.h \
	totem-mozilla-scriptable.h \
	totem-mozilla-scriptable.cpp \
	totem-mozilla-options.h
libtotem_mozilla_la_LIBADD = \
	libbaconmessageconnection.la

totem-mozilla-interface.h: totem-mozilla-interface.xml
	$(DBUS_BIND) --prefix=totem_embedded --mode=glib-server \
		--output=totem-mozilla-interface.h $<

totem_mozilla_viewer_SOURCES = 		\
	totem-mozilla-viewer.c		\
	totem-mozilla-options.h		\
	bacon-volume.c bacon-volume.h
totem_mozilla_viewer_LDADD = \
	$(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) \
	$(DBUS_LIBS) $(NVTV_LIBS) \
	backend/libbaconvideowidget.la libbaconpropertiespage.la

module_flags = -export_dynamic -avoid-version

else

BUILT_SOURCES = 

endif

vanity_SOURCES =					\
	$(BACON_V4L_SELECTION)				\
	totem-screenshot.c totem-screenshot.h		\
	vanity.c

vanity_LDADD = \
	$(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) \
	$(NVTV_LIBS) backend/libbaconvideowidget.la

metadata_test_SOURCES = metadata-test.c
metadata_test_LDADD = \
	$(X_LIBS) $(GTK_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) $(NVTV_LIBS) \
	$(EXTRA_GNOME_LIBS) backend/libbaconvideowidget.la

disc_test_SOURCES = disc-test.c
disc_test_LDADD = \
	$(GTK_LIBS) $(EXTRA_GNOME_LIBS) \
	plparse/libtotem-plparser.la

list_v4l_SOURCES = list_v4l.c $(BACON_V4L_SELECTION)
list_v4l_LDADD = $(GTK_LIBS)

INCLUDES = 						\
	-D_REENTRANT					\
	-I$(top_srcdir)					\
	-I$(srcdir)/plparse				\
	-I$(srcdir)/backend				\
	$(EXTRA_GNOME_CFLAGS)				\
	$(DBUS_CFLAGS)					\
	$(MOZILLA_CFLAGS)				\
	$(MOZILLA_XPCOM_CFLAGS)				\
	$(NVTV_CFLAGS)					\
	$(NAUTILUS_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DLOGO_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"totem_logo.png\"

CLEANFILES = *.bak *.gladep core* *.orig *~ $(desktop_DATA) $(BUILT_SOURCES) 
EXTRA_DIST = \
	$(BACON_V4L_SELECTION) \
	vanity.c \
	totem-mozilla-viewer.c \
	totem-mozilla-plugin.cpp \
	totem_mozilla_scripting.idl \
	totem-mozilla-scriptable.cpp \
	totem-mozilla-scriptable.h \
	totem-mozilla-interface.xml
