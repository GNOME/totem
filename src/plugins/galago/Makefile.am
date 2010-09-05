modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/galago
plugin_LTLIBRARIES = libtgp.la

plugin_in_files = galago.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

libtgp_la_SOURCES = totem-galago.c
libtgp_la_LDFLAGS = $(modules_flags)
libtgp_la_LIBADD = $(LIBGALAGO_LIBS)
libtgp_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins

libtgp_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(LIBGALAGO_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
