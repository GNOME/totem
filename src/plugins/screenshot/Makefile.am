include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/screenshot
plugin_LTLIBRARIES = libscreenshot.la

plugin_in_files = screenshot.plugin.in

uidir = $(plugindir)
ui_DATA = gallery.ui gnome-screenshot.ui
EXTRA_DIST += $(ui_DATA)

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
libscreenshot_la_LDFLAGS = $(plugin_ldflags)
libscreenshot_la_CFLAGS = $(plugin_cflags)

-include $(top_srcdir)/git.mk
