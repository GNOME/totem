modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/youtube
plugin_LTLIBRARIES = libyoutube.la

plugin_in_files = youtube.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

uidir = $(plugindir)
ui_DATA = youtube.ui

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

libyoutube_la_SOURCES = totem-youtube.c
libyoutube_la_LDFLAGS = $(modules_flags)
libyoutube_la_LIBADD = \
	$(LIBGDATA_LIBS)	\
	$(LIBSOUP_LIBS)
libyoutube_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins

libyoutube_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(LIBGDATA_CFLAGS)	\
	$(LIBSOUP_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)

EXTRA_DIST = $(plugin_in_files) $(ui_DATA)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
