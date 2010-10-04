include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/thumbnail
plugin_LTLIBRARIES = libthumbnail.la

plugin_in_files = thumbnail.plugin.in

libthumbnail_la_SOURCES = totem-thumbnail.c
libthumbnail_la_LDFLAGS = $(plugin_ldflags)
libthumbnail_la_CFLAGS = $(plugin_cflags)

-include $(top_srcdir)/git.mk
