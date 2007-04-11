SUBDIRS = $(PLUGINS)
DIST_SUBDIRS = $(ALL_PLUGINS)

noinst_LTLIBRARIES = libtotemmodule.la

common_defines = \
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	-DTOTEM_PLUGIN_DIR=\""$(libdir)/totem/plugins"\"\
	$(DISABLE_DEPRECATED)

modules_flags = -export_dynamic -avoid-version -module

libtotemmodule_la_SOURCES =		\
	totem-module.c			\
	totem-module.h			\
	totem-plugin.c			\
	totem-plugin.h			\
	totem-plugins-engine.c		\
	totem-plugins-engine.h		\
	totem-plugin-manager.c		\
	totem-plugin-manager.h

libtotemmodule_la_CPPFLAGS = \
	$(common_defines)	\
	$(AM_CPPFLAGS)

libtotemmodule_la_CFLAGS = \
	$(EXTRA_GNOME_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(AM_CFLAGS)		\
	-I$(top_srcdir)/src

libtotemmodule_la_LDFLAGS = \
	$(AM_LDFLAGS)			

