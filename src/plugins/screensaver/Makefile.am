modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/screensaver
plugin_LTLIBRARIES = libscreensaver.la

plugin_in_files = screensaver.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

libscreensaver_la_SOURCES = totem-screensaver.c
libscreensaver_la_LDFLAGS = $(modules_flags)
libscreensaver_la_LIBADD = $(top_builddir)/lib/libtotemscrsaver.la
libscreensaver_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins

libscreensaver_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(SCREENSAVER_CFLAGS)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
