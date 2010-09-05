modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/brasero-disc-recorder
plugin_LTLIBRARIES = libbrasero-disc-recorder.la

plugin_in_files = brasero-disc-recorder.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

libbrasero_disc_recorder_la_SOURCES = totem-disc-recorder.c
libbrasero_disc_recorder_la_LDFLAGS = $(modules_flags)
libbrasero_disc_recorder_la_LIBADD = $(DISC_RECORDER_LIBS)
libbrasero_disc_recorder_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins

libbrasero_disc_recorder_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(DISC_RECORDER_CFLAGS)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
