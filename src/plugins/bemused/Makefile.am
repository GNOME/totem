modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/bemused
plugin_LTLIBRARIES = libbemused.la

plugin_in_files = bemused.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

libbemused_la_SOURCES = totem-bemused.c
libbemused_la_LDFLAGS = $(modules_flags)
libbemused_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins	\
	-I$(top_srcdir)/src/backend
libbemused_la_LIBADD = $(BEMUSED_LIBS)

libbemused_la_CFLAGS =			\
	$(DEPENDENCY_CFLAGS)		\
	$(PEAS_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(BEMUSED_CFLAGS)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
