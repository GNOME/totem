modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/screenshot
uidir = $(plugindir)
plugin_LTLIBRARIES = libscreenshot.la

plugin_in_files = screenshot.totem-plugin.in

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)
ui_DATA = gallery.ui gnome-screenshot.ui

common_defines = \
	-DG_LOG_DOMAIN=\""Totem"\"			\
	$(DISABLE_DEPRECATED)

libscreenshot_la_SOURCES = \
	gnome-screenshot-widget.c	\
	gnome-screenshot-widget.h	\
	totem-screenshot-plugin.c	\
	totem-screenshot-plugin.h	\
	totem-screenshot.c		\
	totem-screenshot.h		\
	totem-gallery.c			\
	totem-gallery.h			\
	totem-gallery-progress.c	\
	totem-gallery-progress.h
libscreenshot_la_LDFLAGS = $(modules_flags)
libscreenshot_la_CPPFLAGS = \
	$(common_defines)		\
	-I$(top_srcdir)/src/backend

libscreenshot_la_CFLAGS = 		\
	$(DEPENDENCY_CFLAGS)		\
	$(PEAS_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)			\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins	\
	-I$(srcdir)/

EXTRA_DIST = $(plugin_in_files) $(ui_DATA)

CLEANFILES = $(plugin_DATA) $(BUILT_SOURCES)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
