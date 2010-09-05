modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/tracker
plugin_LTLIBRARIES = libtracker.la

plugin_in_files = tracker.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

libtracker_la_SOURCES = totem-tracker.c totem-tracker-widget.h totem-tracker-widget.c
libtracker_la_LDFLAGS = $(modules_flags)
libtracker_la_LIBADD = $(TRACKER_LIBS)
libtracker_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins

libtracker_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(TRACKER_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
