include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/screenshot
plugin_LTLIBRARIES = libscreenshot.la

plugin_in_files = screenshot.plugin.in

uidir = $(plugindir)
ui_DATA = gallery.ui
EXTRA_DIST += $(ui_DATA)

libscreenshot_la_SOURCES = \
	totem-screenshot-plugin.c	\
	totem-screenshot-plugin.h	\
	screenshot-filename-builder.c	\
	screenshot-filename-builder.h	\
	totem-gallery.c			\
	totem-gallery.h			\
	totem-gallery-progress.c	\
	totem-gallery-progress.h
libscreenshot_la_LDFLAGS = $(plugin_ldflags)
# format-nonliteral causes failures for the totem_screenshot_plugin_setup_file_chooser() function
libscreenshot_la_CFLAGS = $(plugin_cflags) -Wno-error=format-nonliteral -Wno-format-nonliteral
libscreenshot_la_LIBADD = $(plugin_libadd)

-include $(top_srcdir)/git.mk
