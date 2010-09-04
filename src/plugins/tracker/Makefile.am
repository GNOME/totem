modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/tracker
plugin_LTLIBRARIES = libtracker.la

plugin_in_files = tracker.totem-plugin.in

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

common_defines = \
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	-DTOTEM_PLUGIN_DIR=\""$(libdir)/totem/plugins"\"\
	-DG_LOG_DOMAIN=\""Totem"\"			\
	$(DISABLE_DEPRECATED)

libtracker_la_SOURCES = totem-tracker.c totem-tracker-widget.h totem-tracker-widget.c
libtracker_la_LDFLAGS = $(modules_flags)
libtracker_la_LIBADD = $(TRACKER_LIBS)
libtracker_la_CPPFLAGS = $(common_defines)

libtracker_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(TRACKER_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(AM_CFLAGS)		\
	-I$(top_srcdir)/	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/src/plugins

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA) $(BUILT_SOURCES)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
