plugindir = $(BROWSER_PLUGIN_DIR)
plugin_LTLIBRARIES =

EXTRA_DIST = \
	plugin.symbols	\
	TODO		\
	README.browser-plugin

AM_CPPFLAGS = \
	-I$(top_srcdir)					\
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/src/backend			\
	-I$(top_builddir)/src				\
	-I$(top_builddir)/src/backend			\
	-DSYSCONFDIR="\"$(sysconfdir)\""		\
	-DLIBEXECDIR="\"$(libexecdir)\""		\
	$(DISABLE_DEPRECATED)

plugin_sources = \
	npapi.h				\
	npruntime.h			\
	npupp.h				\
	totemNPNGlue.cpp		\
	totemNPClass.cpp		\
	totemNPClass.h			\
	totemNPObject.cpp		\
	totemNPObject.h			\
	totemNPObjectWrapper.h		\
	totemNPVariantWrapper.h		\
	totemPluginGlue.cpp		\
	totemPlugin.cpp			\
	totemPlugin.h			\
	totem-plugin-viewer-constants.h

plugin_cxxflags = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CXXFLAGS)

plugin_libadd = $(BROWSER_PLUGIN_LIBS)

plugin_ldflags = \
	-avoid-version					\
	-export-symbols $(srcdir)/plugin.symbols	\
	-module						\
	-no-undefined					\
	$(AM_LDFLAGS)

# Marshaller
MARSHAL_FILES = \
	marshal.h	\
	marshal.c

marshal.h: marshal.list
	$(AM_V_GEN)($(GLIB_GENMARSHAL) --prefix=totempluginviewer_marshal --internal --header $< > $@)
marshal.c: marshal.list marshal.h
	$(AM_V_GEN)($(GLIB_GENMARSHAL) --prefix=totempluginviewer_marshal --internal --body --header $< > $@)

CLEANFILES = $(MARSHAL_FILES)
EXTRA_DIST += marshal.list

# The Glow button test program
noinst_PROGRAMS = test-glow-button

test_glow_button_SOURCES = \
	totem-glow-button.c	\
	totem-glow-button.h	\
	test-glow-button.c

test_glow_button_LDADD = \
	$(DEPENDENCY_LIBS)			\
	-lm

test_glow_button_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(AM_CFLAGS)

# Totem Plugin Viewer
libexec_PROGRAMS = totem-plugin-viewer

# Are we a bit thick, Mister?
totem-plugin-viewer.c: totem-plugin-viewer-interface.h

totem_plugin_viewer_SOURCES = \
	totem-glow-button.c		\
	totem-glow-button.h		\
	totem-plugin-viewer-interface.h	\
	totem-plugin-viewer.c		\
	totem-plugin-viewer-constants.h	\
	totem-plugin-viewer-options.h	\
	$(MARSHAL_FILES)

totem_plugin_viewer_CPPFLAGS = \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DG_LOG_DOMAIN=\""TotemEmbedded\""		\
	$(AM_CPPFLAGS)

totem_plugin_viewer_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(SN_CFLAGS)		\
	$(AM_CFLAGS)

totem_plugin_viewer_LDADD = \
	$(top_builddir)/src/backend/libbaconvideowidget.la		\
	$(top_builddir)/src/libtotem_player.la				\
	$(DEPENDENCY_LIBS)	\
	$(SN_LIBS)		\
	$(DBUS_LIBS)		\
	$(X_LIBS)

totem-plugin-viewer-interface.h: org_gnome_totem_PluginViewer.xml
	$(DBUS_BIND) --prefix=totem_embedded --mode=glib-server $< > $@

CLEANFILES += totem-plugin-viewer-interface.h
EXTRA_DIST += org_gnome_totem_PluginViewer.xml

# Totem GMP Plugin
if ENABLE_GMP_PLUGIN
plugin_LTLIBRARIES += libtotem-gmp-plugin.la

libtotem_gmp_plugin_la_SOURCES = \
	$(plugin_sources)		\
	totemGMPControls.cpp		\
	totemGMPControls.h		\
	totemGMPError.cpp		\
	totemGMPError.h			\
	totemGMPNetwork.cpp		\
	totemGMPNetwork.h		\
	totemGMPPlayer.cpp		\
	totemGMPPlayer.h		\
	totemGMPPlaylist.cpp		\
	totemGMPPlaylist.h		\
	totemGMPSettings.cpp		\
	totemGMPSettings.h

libtotem_gmp_plugin_la_CPPFLAGS = \
	-DXP_UNIX					\
	-DMOZ_X11					\
	-DTOTEM_GMP_PLUGIN				\
	-DTOTEM_PL_PARSER_MINI				\
	$(AM_CPPFLAGS)

libtotem_gmp_plugin_la_CXXFLAGS = $(plugin_cxxflags)
libtotem_gmp_plugin_la_LIBADD = $(plugin_libadd)
libtotem_gmp_plugin_la_LDFLAGS = $(plugin_ldflags)
endif

# Totem NarrowSpace Plugin
if ENABLE_NARROWSPACE_PLUGIN
plugin_LTLIBRARIES += libtotem-narrowspace-plugin.la

libtotem_narrowspace_plugin_la_SOURCES = \
	$(plugin_sources)		\
	totemNarrowSpacePlugin.cpp	\
	totemNarrowSpacePlugin.h

libtotem_narrowspace_plugin_la_CPPFLAGS = \
	-DXP_UNIX					\
	-DMOZ_X11					\
	-DTOTEM_NARROWSPACE_PLUGIN			\
	-DTOTEM_PL_PARSER_MINI				\
	$(AM_CPPFLAGS)

libtotem_narrowspace_plugin_la_CXXFLAGS = $(plugin_cxxflags)
libtotem_narrowspace_plugin_la_LIBADD = $(plugin_libadd)
libtotem_narrowspace_plugin_la_LDFLAGS = $(plugin_ldflags)
endif

# Totem Vegas Plugin
if ENABLE_VEGAS_PLUGIN
plugin_LTLIBRARIES += libtotem-vegas-plugin.la

libtotem_vegas_plugin_la_SOURCES = \
	$(plugin_sources)		\
	totemVegasPlugin.cpp	\
	totemVegasPlugin.h

libtotem_vegas_plugin_la_CPPFLAGS = \
	-DXP_UNIX					\
	-DMOZ_X11					\
	-DTOTEM_VEGAS_PLUGIN			\
	-DTOTEM_PL_PARSER_MINI				\
	$(AM_CPPFLAGS)

libtotem_vegas_plugin_la_CXXFLAGS = $(plugin_cxxflags)
libtotem_vegas_plugin_la_LIBADD = $(plugin_libadd)
libtotem_vegas_plugin_la_LDFLAGS = $(plugin_ldflags)
endif

# Totem MULLY Plugin
if ENABLE_MULLY_PLUGIN
plugin_LTLIBRARIES += libtotem-mully-plugin.la

libtotem_mully_plugin_la_SOURCES = \
	$(plugin_sources)		\
	totemMullYPlugin.cpp		\
	totemMullYPlugin.h

libtotem_mully_plugin_la_CPPFLAGS = \
	-DXP_UNIX					\
	-DMOZ_X11					\
	-DTOTEM_MULLY_PLUGIN				\
	-DTOTEM_PL_PARSER_MINI				\
	$(AM_CPPFLAGS)

libtotem_mully_plugin_la_CXXFLAGS = $(plugin_cxxflags)
libtotem_mully_plugin_la_LIBADD = $(plugin_libadd)
libtotem_mully_plugin_la_LDFLAGS = $(plugin_ldflags)
endif

# Totem Cone Plugin
if ENABLE_CONE_PLUGIN
plugin_LTLIBRARIES += libtotem-cone-plugin.la

libtotem_cone_plugin_la_SOURCES = \
	$(plugin_sources)		\
	totemConeAudio.cpp		\
	totemConeAudio.h		\
	totemCone.cpp			\
	totemCone.h			\
	totemConeInput.cpp		\
	totemConeInput.h		\
	totemConePlaylist.cpp		\
	totemConePlaylist.h		\
	totemConePlaylistItems.cpp	\
	totemConePlaylistItems.h	\
	totemConeVideo.cpp		\
	totemConeVideo.h

libtotem_cone_plugin_la_CPPFLAGS = \
	-DXP_UNIX					\
	-DMOZ_X11					\
	-DTOTEM_CONE_PLUGIN				\
	-DTOTEM_PL_PARSER_MINI				\
	$(AM_CPPFLAGS)

libtotem_cone_plugin_la_CXXFLAGS = $(plugin_cxxflags)
libtotem_cone_plugin_la_LIBADD = $(plugin_libadd)
libtotem_cone_plugin_la_LDFLAGS = $(plugin_ldflags)
endif

-include $(top_srcdir)/git.mk
