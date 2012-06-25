SUBDIRS = gst backend properties . plugins

bin_PROGRAMS = totem totem-video-thumbnailer totem-audio-preview
lib_LTLIBRARIES = libtotem.la
noinst_LTLIBRARIES = libtotem_player.la

AM_CPPFLAGS = \
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DTOTEM_PLUGIN_DIR=\""$(libdir)/totem/plugins"\"\
	-DBINDIR=\""$(bindir)"\"			\
	-DGTKBUILDERDIR=\"""\"				\
	$(DISABLE_DEPRECATED)				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/src/backend			\
	-I$(top_builddir)/src/backend			\
	-I$(srcdir)/plugins

AM_CFLAGS = $(WARN_CFLAGS)

# Header files installed for use by plugins (and consequently introspected using gobject-introspection; see the bottom of the Makefile)
INST_H_FILES = \
	totem.h				\
	totem-video-list.h		\
	totem-cell-renderer-video.h	\
	totem-interface.h		\
	plugins/totem-plugin.h		\
	plugins/totem-dirs.h

headerdir = $(includedir)/totem/$(TOTEM_API_VERSION)
header_DATA = $(INST_H_FILES)

# Totem UI ltlibrary (used by browser plugins)
libtotem_player_la_SOURCES = \
	$(INST_H_FILES)		\
	totem-statusbar.c	\
	totem-statusbar.h	\
	totem-interface.c	\
	totem-fullscreen.c	\
	totem-fullscreen.h	\
	gsd-media-keys-window.c	\
	gsd-media-keys-window.h	\
	gsd-osd-window.c	\
	gsd-osd-window.h	\
	totem-time-label.c	\
	totem-time-label.h

libtotem_player_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(AM_CPPFLAGS)

libtotem_player_la_LIBADD = $(DEPENDENCY_LIBS)
libtotem_player_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(AM_CFLAGS)

# Totem main library (used for main player; separate to allow gtk-doc to be used)
libtotem_la_SOURCES = \
	$(TOTEM_MARSHAL_FILES)		\
	$(INST_H_FILES)			\
	totem-object.c			\
	totem-private.h			\
	totem-cell-renderer-video.c	\
	totem-video-list.c		\
	totem-preferences.c		\
	totem-preferences.h		\
	totem-dnd-menu.c		\
	totem-dnd-menu.h		\
	totem-options.c			\
	totem-options.h			\
	totem-playlist.c		\
	totem-playlist.h		\
	eggfileformatchooser.c		\
	eggfileformatchooser.h		\
	egg-macros.h			\
	totem-session.c			\
	totem-session.h			\
	totem-sidebar.c			\
	totem-sidebar.h			\
	totem-open-location.c		\
	totem-open-location.h		\
	totem-menu.c			\
	totem-menu.h			\
	totem-uri.c			\
	totem-uri.h			\
	totem-subtitle-encoding.c	\
	totem-subtitle-encoding.h	\
	totem-profile.h			\
	plugins/totem-plugins-engine.c	\
	plugins/totem-plugins-engine.h	\
	plugins/totem-dirs.c

libtotem_la_CPPFLAGS = \
	-I$(top_builddir)/data		\
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(AM_CPPFLAGS)

libtotem_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)		\
	$(PEAS_CFLAGS)			\
	$(PEASGTK_CFLAGS)		\
	$(AM_CFLAGS)

libtotem_la_LDFLAGS = \
	-export-dynamic	\
	-no-undefined \
	$(AM_LDFLAGS)

libtotem_la_LIBADD = \
	libtotem_player.la		\
	backend/libbaconvideowidget.la	\
	$(PEAS_LIBS)			\
	$(PEASGTK_LIBS)			\
	$(DEPENDENCY_LIBS)

if WITH_SMCLIENT
libtotem_la_SOURCES += \
	eggsmclient.c \
	eggsmclient.h \
	eggsmclient-private.h \
	$(NULL)
libtotem_la_CFLAGS += $(SMCLIENT_CFLAGS)
libtotem_la_LIBADD += $(SMCLIENT_LIBS)

if WITH_SMCLIENT_XSMP
libtotem_la_SOURCES += \
	eggdesktopfile.c \
	eggdesktopfile.h \
	eggsmclient-xsmp.c
libtotem_la_CPPFLAGS += -DEGG_SM_CLIENT_BACKEND_XSMP
endif
if WITH_SMCLIENT_WIN32
libtotem_la_SOURCES += eggsmclient-win32.c
endif
if WITH_SMCLIENT_QUARTZ
libtotem_la_SOURCES += eggsmclient-osx.c
endif
endif

# Marshal files
TOTEM_MARSHAL_FILES = \
	totemobject-marshal.c totemobject-marshal.h		\
	totemplaylist-marshal.c totemplaylist-marshal.h		\
	totemvideolist-marshal.c totemvideolist-marshal.h

totemobject-marshal.h: totemobject-marshal.list
	$(AM_V_GEN) ($(GLIB_GENMARSHAL) --prefix=totemobject_marshal $(srcdir)/totemobject-marshal.list --header > $@)
totemobject-marshal.c: totemobject-marshal.h totemobject-marshal.list
	$(AM_V_GEN) ($(GLIB_GENMARSHAL) --prefix=totemobject_marshal $(srcdir)/totemobject-marshal.list --header --body > $@)
totemplaylist-marshal.h: totemplaylist-marshal.list
	$(AM_V_GEN) ($(GLIB_GENMARSHAL) --prefix=totemplaylist_marshal $(srcdir)/totemplaylist-marshal.list --header > $@)
totemplaylist-marshal.c: totemplaylist-marshal.h totemobject-marshal.list
	$(AM_V_GEN) ($(GLIB_GENMARSHAL) --prefix=totemplaylist_marshal $(srcdir)/totemplaylist-marshal.list --header --body > $@)
totemvideolist-marshal.h: totemvideolist-marshal.list
	$(AM_V_GEN) ($(GLIB_GENMARSHAL) --prefix=totemvideolist_marshal $(srcdir)/totemvideolist-marshal.list --header > $@)
totemvideolist-marshal.c: totemvideolist-marshal.h totemobject-marshal.list
	$(AM_V_GEN) ($(GLIB_GENMARSHAL) --prefix=totemvideolist_marshal $(srcdir)/totemvideolist-marshal.list --header --body > $@)

EXTRA_DIST = \
	totemobject-marshal.list	\
	totemvideolist-marshal.list	\
	totemplaylist-marshal.list
CLEANFILES = $(TOTEM_MARSHAL_FILES)

# Totem
totem_SOURCES = totem.c

totem_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(AM_CPPFLAGS)

totem_CFLAGS = \
	$(PEAS_CFLAGS)		\
	$(DEPENDENCY_CFLAGS)	\
	$(AM_CFLAGS)

totem_LDADD = \
	libtotem.la		\
	$(DEPENDENCY_LIBS)	\
	$(X_LIBS)	\
	$(PEAS_LIBS)

# Totem video thumbnailer
totem_video_thumbnailer_SOURCES = \
	totem-video-thumbnailer.c	\
	totem-resources.c		\
	totem-resources.h

totem_video_thumbnailer_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""TotemVideoThumbnailer"\"	\
	$(AM_CPPFLAGS)

totem_video_thumbnailer_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(AM_CFLAGS)

totem_video_thumbnailer_LDADD = \
	backend/libbaconvideowidget.la		\
	$(DEPENDENCY_LIBS)

# Nautilus Property Page
if HAVE_NAUTILUS
nautilusdir = $(NAUTILUSDIR)
nautilus_LTLIBRARIES = libtotem-properties-page.la

libtotem_properties_page_la_SOURCES = \
	totem-properties-main.c		\
	totem-properties-view.c		\
	totem-properties-view.h

libtotem_properties_page_la_CPPFLAGS = \
	-I$(top_srcdir)/src/properties		\
	-I$(top_builddir)/data			\
	-DG_LOG_DOMAIN=\""TotemPropertiesPage"\"\
	$(AM_CPPFLAGS)

libtotem_properties_page_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(NAUTILUS_CFLAGS)	\
	$(AM_CFLAGS)

libtotem_properties_page_la_LDFLAGS = \
	-export_dynamic -avoid-version -module -no-undefined	\
	$(AM_LDFLAGS)

libtotem_properties_page_la_LIBADD = \
	backend/libbaconvideowidget.la				\
	properties/libbaconvideowidgetproperties.la		\
	libtotem_player.la					\
	$(DEPENDENCY_LIBS)					\
	$(NAUTILUS_LIBS)

noinst_PROGRAMS = test-properties-page

test_properties_page_SOURCES = \
	totem-properties-main.c		\
	totem-properties-view.c		\
	totem-properties-view.h		\
	test-properties-page.c

test_properties_page_CPPFLAGS = \
	-I$(top_builddir)/data			\
	-I$(top_srcdir)/src/properties		\
	$(AM_CPPFLAGS)

test_properties_page_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(NAUTILUS_CFLAGS)	\
	$(AM_CFLAGS)

test_properties_page_LDADD = \
	backend/libbaconvideowidget.la				\
	properties/libbaconvideowidgetproperties.la		\
	libtotem_player.la					\
	$(DEPENDENCY_LIBS)					\
	$(NAUTILUS_LIBS)
endif # HAVE_NAUTILUS

# Totem Audio Preview for Nautilus
totem_audio_preview_SOURCES = \
	totem-audio-preview.c		\
	totem-resources.c		\
	totem-resources.h

totem_audio_preview_CPPFLAGS = \
	-I$(top_builddir)/data			\
	-I$(top_srcdir)/src/gst			\
	-DG_LOG_DOMAIN=\""TotemAudioPreview"\"	\
	$(AM_CPPFLAGS)

totem_audio_preview_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(AM_CFLAGS)

totem_audio_preview_LDADD =			\
	gst/libtotemgsthelpers.la		\
	backend/libbaconvideowidget.la		\
	$(DEPENDENCY_LIBS)

# Introspection
-include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION

introspection_sources = $(INST_H_FILES) $(filter %.c,$(libtotem_player_la_SOURCES) $(libtotem_la_SOURCES))

# NOTE: This should change with $(TOTEM_API_VERSION)
Totem-1.0.gir: libtotem.la
Totem_1_0_gir_INCLUDES = GLib-2.0 GObject-2.0 Gtk-3.0 TotemPlParser-1.0
Totem_1_0_gir_SCANNERFLAGS = --c-include=totem.h --pkg-export=totem
Totem_1_0_gir_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(libtotem_la_CPPFLAGS)
Totem_1_0_gir_LIBS = libtotem.la
Totem_1_0_gir_FILES = $(introspection_sources)
Totem_1_0_gir_EXPORT_PACKAGES = totem
INTROSPECTION_GIRS += Totem-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

endif

-include $(top_srcdir)/git.mk
