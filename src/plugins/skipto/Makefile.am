modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/skipto
plugin_LTLIBRARIES = libskipto.la

plugin_in_files = skipto.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

uidir = $(plugindir)
ui_DATA = skipto.ui

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

libskipto_la_SOURCES = \
	totem-skipto-plugin.c	\
	totem-time-entry.c	\
	totem-time-entry.h	\
	totem-skipto.c		\
	totem-skipto.h
libskipto_la_LDFLAGS = $(modules_flags)
libskipto_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/backend	\
	-I$(top_srcdir)/src/plugins	\
	-I$(srcdir)/

libskipto_la_CFLAGS = 			\
	$(DEPENDENCY_CFLAGS)		\
	$(PEAS_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(DBUS_CFLAGS)

EXTRA_DIST = $(plugin_in_files) $(ui_DATA)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
