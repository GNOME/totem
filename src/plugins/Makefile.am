SUBDIRS = $(PLUGINS)
DIST_SUBDIRS = $(ALL_PLUGINS)

noinst_LTLIBRARIES = libtotemmodule.la

common_defines =					\
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	-DTOTEM_PLUGIN_DIR=\""$(libdir)/totem/plugins"\"\
	-DG_LOG_DOMAIN=\""Totem"\"			\
	$(DISABLE_DEPRECATED)

modules_flags = -export_dynamic -avoid-version -module

libtotemmodule_la_SOURCES =		\
	totem-plugin.h			\
	totem-plugins-engine.c		\
	totem-plugins-engine.h		\
	totem-dirs.c			\
	totem-dirs.h

libtotemmodule_la_CPPFLAGS = \
	$(common_defines)	\
	$(AM_CPPFLAGS)

libtotemmodule_la_LDFLAGS = $(AM_LDFLAGS) $(modules_flags)

libtotemmodule_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(AM_CFLAGS)		\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/lib 				\
	-I$(top_builddir)/lib 				\
	-I$(top_srcdir)/src				\
	-I$(top_srcdir)/src/backend			\
	-I$(top_srcdir)/src/plugins

