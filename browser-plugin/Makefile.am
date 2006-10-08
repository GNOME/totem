SUBDIRS = idl

plugindir = $(libdir)/mozilla/plugins
plugin_LTLIBRARIES =

EXTRA_DIST = \
	plugin.symbols	\
	TODO

gecko_include_subdirs = \
	.		\
	docshell	\
	dom		\
	necko

# Totem Basic Plugin

if ENABLE_BASIC_PLUGIN

plugin_LTLIBRARIES += libtotem-basic-plugin.la

libtotem_basic_plugin_la_SOURCES = \
	totemPluginGlue.cpp	\
	totemPluginGlue.h	\
	totemPlugin.cpp		\
	totemPlugin.h		\
	totemBasicPlugin.cpp	\
	totemBasicPlugin.h

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
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	$(AM_CPPFLAGS)

libtotem_basic_plugin_la_CFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)

libtotem_basic_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CFLAGS)

libtotem_basic_plugin_la_LIBADD = \
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la	\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)

libtotem_basic_plugin_la_LDFLAGS = \
	-avoid-version	\
	-export-symbols $(srcdir)/plugin.symbols \
	-module		\
	$(AM_LDFLAGS)

endif

# Totem GMP Plugin

if ENABLE_GMP_PLUGIN

plugin_LTLIBRARIES += libtotem-gmp-plugin.la

libtotem_gmp_plugin_la_SOURCES = \
	totemPluginGlue.cpp	\
	totemPluginGlue.h	\
	totemPlugin.cpp		\
	totemPlugin.h		\
	totemGMPPlugin.cpp	\
	totemGMPPlugin.h

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
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	$(AM_CPPFLAGS)

libtotem_gmp_plugin_la_CFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)

libtotem_gmp_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CFLAGS)

libtotem_gmp_plugin_la_LIBADD = \
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la	\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)

libtotem_gmp_plugin_la_LDFLAGS = \
	-avoid-version	\
	-export-symbols $(srcdir)/plugin.symbols \
	-module		\
	$(AM_LDFLAGS)

endif

# Totem Complex Plugin

if ENABLE_COMPLEX_PLUGIN

plugin_LTLIBRARIES += libtotem-complex-plugin.la

libtotem_complex_plugin_la_SOURCES = \
	totemPluginGlue.cpp	\
	totemPluginGlue.h	\
	totemPlugin.cpp		\
	totemPlugin.h		\
	totemComplexPlugin.cpp	\
	totemComplexPlugin.h

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
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	$(AM_CPPFLAGS)

libtotem_complex_plugin_la_CFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)

libtotem_complex_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CFLAGS)

libtotem_complex_plugin_la_LIBADD = \
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la	\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)

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
	totemNarrowSpacePlugin.cpp	\
	totemNarrowSpacePlugin.h

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
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	$(AM_CPPFLAGS)

libtotem_narrowspace_plugin_la_CFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)

libtotem_narrowspace_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CFLAGS)

libtotem_narrowspace_plugin_la_LIBADD = \
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la	\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)

libtotem_narrowspace_plugin_la_LDFLAGS = \
	-avoid-version	\
	-export-symbols $(srcdir)/plugin.symbols \
	-module		\
	$(AM_LDFLAGS)

endif

# Totem MULLY Plugin

if ENABLE_MULLY_PLUGIN

plugin_LTLIBRARIES += libtotem-mully-plugin.la

libtotem_mully_plugin_la_SOURCES = \
	totemPluginGlue.cpp	\
	totemPluginGlue.h	\
	totemPlugin.cpp		\
	totemPlugin.h		\
	totemMullYPlugin.cpp	\
	totemMullYPlugin.h

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
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	$(AM_CPPFLAGS)

libtotem_mully_plugin_la_CFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)

libtotem_mully_plugin_la_CXXFLAGS = \
	$(BROWSER_PLUGIN_CFLAGS)	\
	$(MOZILLA_XPCOM_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(WARN_CXXFLAGS)		\
	$(PLUGIN_EXTRA_WARN_CXXFLAGS)	\
	$(AM_CFLAGS)

libtotem_mully_plugin_la_LIBADD = \
	$(top_builddir)/src/plparse/libtotem-plparser-mini.la	\
	$(DBUS_LIBS)	\
 	$(MOZILLA_LIBS)

libtotem_mully_plugin_la_LDFLAGS = \
	-avoid-version	\
	-export-symbols $(srcdir)/plugin.symbols \
	-module		\
	$(AM_LDFLAGS)

endif
