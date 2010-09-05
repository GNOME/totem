modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/sidebar-test
plugin_LTLIBRARIES = libsidebar-test.la
# override to _not_ install the test plugins
install-pluginLTLIBRARIES:

plugin_in_files = sidebar-test.totem-plugin.in
noinst_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

libsidebar_test_la_SOURCES = totem-sidebar-test.c
libsidebar_test_la_LDFLAGS = $(modules_flags)
libsidebar_test_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins

libsidebar_test_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(noinst_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
