include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/ontop
plugin_LTLIBRARIES = libontop.la

plugin_in_files = ontop.totem-plugin.in

libontop_la_SOURCES = totem-ontop.c
libontop_la_LDFLAGS = $(plugin_ldflags)
libontop_la_LIBADD = $(top_builddir)/lib/libtotemscrsaver.la
libontop_la_CFLAGS = $(plugin_cflags)

-include $(top_srcdir)/git.mk
