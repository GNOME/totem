include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/gromit
plugin_LTLIBRARIES = libgromit.la

plugin_in_files = gromit.totem-plugin.in

libgromit_la_SOURCES = totem-gromit.c
libgromit_la_LDFLAGS = $(plugin_ldflags)
libgromit_la_CFLAGS = $(plugin_cflags)

-include $(top_srcdir)/git.mk
