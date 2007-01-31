SUBDIRS = idl

plugindir = $(libdir)/mozilla/plugins
plugin_LTLIBRARIES =

BUILT_SOURCES =

EXTRA_DIST = \
	plugin.symbols	\
	TODO

CLEANFILES = $(BUILT_SOURCES)

gecko_include_subdirs = \
	.		\
	docshell	\
	dom		\
	necko		\
	xpcom

# Marshaller

totempluginviewer-marshal.h: totempluginviewer-marshal.list
	$(GLIB_GENMARSHAL) --prefix=totempluginviewer_marshal --header $< > $@
totempluginviewer-marshal.c: totempluginviewer-marshal.list
	$(GLIB_GENMARSHAL) --prefix=totempluginviewer_marshal --body --header $< > $@

noinst_LTLIBRARIES = libtotempluginviewer_marshal.la

libtotempluginviewer_marshal_la_SOURCES =	\
	totempluginviewer-marshal.h		\
	totempluginviewer-marshal.c

libtotempluginviewer_marshal_la_CPPFLAGS = \
	$(DISABLE_DEPRECATED)	\
	$(AM_CPPFLAGS)

libtotempluginviewer_marshal_la_CFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)

BUILT_SOURCES += $(libtotempluginviewer_marshal_la_SOURCES)

EXTRA_DIST += \
	totempluginviewer-marshal.list

# Totem Plugin Viewer

libexec_PROGRAMS = totem-plugin-viewer

totem_plugin_viewer_SOURCES = \
	totem-plugin-viewer.c		\
	totem-plugin-viewer-interface.h	\
	totem-plugin-viewer-commands.h	\
	totem-plugin-viewer-options.h

totem_plugin_viewer_CPPFLAGS = \
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/src/plparse			\
	-I$(top_srcdir)/src/backend			\
	-I$(top_builddir)/src				\
	-I$(top_builddir)/src/plparse			\
	-I$(top_builddir)/src/backend			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGCONF_PREFIX="\"/apps/totem\""		\
	-DDATADIR="\"$(datadir)\""			\
	-DLIBEXECDIR="\"$(libexecdir)\""		\
	-DBINDIR="\"$(bindir)\""			\
	-DSHAREDIR="\"$(pkgdatadir)\""			\
	-DLOGO_NAME="\"totem_logo.png\""		\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

totem_plugin_viewer_CFLAGS = \
	$(EXTRA_GNOME_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(NVTV_CFLAGS)		\
	$(AM_CFLAGS)

totem_plugin_viewer_LDADD =						\
	$(top_builddir)/src/backend/libbaconvideowidget.la		\
	$(top_builddir)/src/libbaconpropertiespage.la			\
	$(top_builddir)/src/libtotem_player.la				\
	$(top_builddir)/src/plparse/libtotem-plparser.la		\
	libtotempluginviewer_marshal.la					\
	$(EXTRA_GNOME_LIBS)	\
	$(XVIDMODE_LIBS) 	\
	$(DBUS_LIBS)		\
	$(NVTV_LIBS) 		\
	$(XTEST_LIBS)		\
	$(X_LIBS)

BUILT_SOURCES += \
	totem-plugin-viewer-interface.h

totem-plugin-viewer-interface.h: org_gnome_totem_PluginViewer.xml
	$(DBUS_BIND) --prefix=totem_embedded --mode=glib-server $< > $@

EXTRA_DIST += \
	org_gnome_totem_PluginViewer.xml

# Totem Basic Plugin

if ENABLE_BASIC_PLUGIN

plugin_LTLIBRARIES += libtotem-basic-plugin.la

libtotem_basic_plugin_la_SOURCES =	\
	totemPluginGlue.cpp		\
	totemPluginGlue.h		\
	totemPlugin.cpp			\
	totemPlugin.h			\
	totemStringGlue.h		\
	totemBasicPlugin.cpp		\
	totemBasicPlugin.h		\
	totemClassInfo.h		\
	totem-plugin-viewer-commands.h

libtotem_basic_plugin_la_CPPFLAGS = \
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/src/backend			\
	-I$(top_srcdir)/src/plparse			\
	-I$(top_builddir)/src				\
	-I$(top_builddir)/src/backend			\
	-I$(top_builddir)/src/plparse			\
	-Iidl						\
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(gecko_include_subdirs))	\
	-DTOTEM_BASIC_PLUGIN				\
	-DTOTEM_PL_PARSER_MINI				\
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGCONF_PREFIX="\"/apps/totem\""		\
	-DDATADIR="\"$(pkgdatadir)\""			\
	-DLIBEXECDIR="\"$(libexecdir)\""		\
	-DBINDIR="\"$(bindir)\""			\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

libtotem_basic_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CXXFLAGS)

libtotem_basic_plugin_la_LIBADD =					\
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la		\
	libtotempluginviewer_marshal.la					\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)	\
	$(LIBXPCOMGLUE_S)

libtotem_basic_plugin_la_LDFLAGS = \
	-avoid-version	\
	-export-symbols $(srcdir)/plugin.symbols \
	-module		\
	$(AM_LDFLAGS)

endif

# Totem GMP Plugin

if ENABLE_GMP_PLUGIN

plugin_LTLIBRARIES += libtotem-gmp-plugin.la

libtotem_gmp_plugin_la_SOURCES =	\
	totemGMPError.cpp		\
	totemGMPError.h			\
	totemGMPPlaylist.cpp		\
	totemGMPPlaylist.h		\
	totemGMPSettings.cpp		\
	totemGMPSettings.h		\
	totemPluginGlue.cpp		\
	totemPluginGlue.h		\
	totemPlugin.cpp			\
	totemPlugin.h			\
	totemStringGlue.h		\
	totemGMPPlugin.cpp		\
	totemGMPPlugin.h		\
	totemClassInfo.h		\
	totem-plugin-viewer-commands.h

libtotem_gmp_plugin_la_CPPFLAGS = \
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/src/backend			\
	-I$(top_srcdir)/src/plparse			\
	-I$(top_builddir)/src				\
	-I$(top_builddir)/src/backend			\
	-I$(top_builddir)/src/plparse			\
	-Iidl						\
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(gecko_include_subdirs))	\
	-DTOTEM_GMP_PLUGIN				\
	-DTOTEM_PL_PARSER_MINI				\
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGCONF_PREFIX="\"/apps/totem\""		\
	-DDATADIR="\"$(pkgdatadir)\""			\
	-DLIBEXECDIR="\"$(libexecdir)\""		\
	-DBINDIR="\"$(bindir)\""			\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

libtotem_gmp_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CXXFLAGS)

libtotem_gmp_plugin_la_LIBADD =						\
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la		\
	libtotempluginviewer_marshal.la	\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)	\
	$(LIBXPCOMGLUE_S)

libtotem_gmp_plugin_la_LDFLAGS = \
	-avoid-version	\
	-export-symbols $(srcdir)/plugin.symbols \
	-module		\
	$(AM_LDFLAGS)

endif

# Totem Complex Plugin

if ENABLE_COMPLEX_PLUGIN

plugin_LTLIBRARIES += libtotem-complex-plugin.la

libtotem_complex_plugin_la_SOURCES =	\
	totemPluginGlue.cpp		\
	totemPluginGlue.h		\
	totemPlugin.cpp			\
	totemPlugin.h			\
	totemStringGlue.h		\
	totemComplexPlugin.cpp		\
	totemComplexPlugin.h		\
	totemClassInfo.h		\
	totem-plugin-viewer-commands.h

libtotem_complex_plugin_la_CPPFLAGS = \
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/src/backend			\
	-I$(top_srcdir)/src/plparse			\
	-I$(top_builddir)/src				\
	-I$(top_builddir)/src/backend			\
	-I$(top_builddir)/src/plparse			\
	-Iidl						\
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(gecko_include_subdirs))	\
	-DTOTEM_COMPLEX_PLUGIN				\
	-DTOTEM_PL_PARSER_MINI				\
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGCONF_PREFIX="\"/apps/totem\""		\
	-DDATADIR="\"$(pkgdatadir)\""			\
	-DLIBEXECDIR="\"$(libexecdir)\""		\
	-DBINDIR="\"$(bindir)\""			\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

libtotem_complex_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CXXFLAGS)

libtotem_complex_plugin_la_LIBADD =					\
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la		\
	libtotempluginviewer_marshal.la					\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)	\
	$(LIBXPCOMGLUE_S)

libtotem_complex_plugin_la_LDFLAGS = \
	-avoid-version	\
	-export-symbols $(srcdir)/plugin.symbols \
	-module		\
	$(AM_LDFLAGS)

endif

# Totem NarrowSpace Plugin

if ENABLE_NARROWSPACE_PLUGIN

plugin_LTLIBRARIES += libtotem-narrowspace-plugin.la

libtotem_narrowspace_plugin_la_SOURCES = \
	totemPluginGlue.cpp		\
	totemPluginGlue.h		\
	totemPlugin.cpp			\
	totemPlugin.h			\
	totemStringGlue.h		\
	totemNarrowSpacePlugin.cpp	\
	totemNarrowSpacePlugin.h	\
	totemClassInfo.h		\
	totem-plugin-viewer-commands.h

libtotem_narrowspace_plugin_la_CPPFLAGS = \
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/src/backend			\
	-I$(top_srcdir)/src/plparse			\
	-I$(top_builddir)/src				\
	-I$(top_builddir)/src/backend			\
	-I$(top_builddir)/src/plparse			\
	-Iidl						\
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(gecko_include_subdirs))	\
	-DTOTEM_NARROWSPACE_PLUGIN			\
	-DTOTEM_PL_PARSER_MINI				\
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGCONF_PREFIX="\"/apps/totem\""		\
	-DDATADIR="\"$(pkgdatadir)\""			\
	-DLIBEXECDIR="\"$(libexecdir)\""		\
	-DBINDIR="\"$(bindir)\""			\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

libtotem_narrowspace_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CXXFLAGS)

libtotem_narrowspace_plugin_la_LIBADD =					\
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la		\
	libtotempluginviewer_marshal.la					\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)	\
	$(LIBXPCOMGLUE_S)

libtotem_narrowspace_plugin_la_LDFLAGS = \
	-avoid-version	\
	-export-symbols $(srcdir)/plugin.symbols \
	-module		\
	$(AM_LDFLAGS)

endif

# Totem MULLY Plugin

if ENABLE_MULLY_PLUGIN

plugin_LTLIBRARIES += libtotem-mully-plugin.la

libtotem_mully_plugin_la_SOURCES =	\
	totemPluginGlue.cpp		\
	totemPluginGlue.h		\
	totemPlugin.cpp			\
	totemPlugin.h			\
	totemStringGlue.h		\
	totemMullYPlugin.cpp		\
	totemMullYPlugin.h		\
	totemClassInfo.h		\
	totem-plugin-viewer-commands.h

libtotem_mully_plugin_la_CPPFLAGS = \
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/src/backend			\
	-I$(top_srcdir)/src/plparse			\
	-I$(top_builddir)/src				\
	-I$(top_builddir)/src/backend			\
	-I$(top_builddir)/src/plparse			\
	-Iidl						\
	$(addprefix -I$(MOZILLA_INCLUDE_ROOT)/,$(gecko_include_subdirs))	\
	-DTOTEM_MULLY_PLUGIN				\
	-DTOTEM_PL_PARSER_MINI				\
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGCONF_PREFIX="\"/apps/totem\""		\
	-DDATADIR="\"$(pkgdatadir)\""			\
	-DLIBEXECDIR="\"$(libexecdir)\""		\
	-DBINDIR="\"$(bindir)\""			\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

libtotem_mully_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CXXFLAGS)

libtotem_mully_plugin_la_LIBADD =					\
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la		\
	libtotempluginviewer_marshal.la					\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)	\
	$(LIBXPCOMGLUE_S)

libtotem_mully_plugin_la_LDFLAGS = \
	-avoid-version	\
	-export-symbols $(srcdir)/plugin.symbols \
	-module		\
	$(AM_LDFLAGS)

endif
