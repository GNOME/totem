include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/bemused
plugin_LTLIBRARIES = libbemused.la

plugin_in_files = bemused.totem-plugin.in

libbemused_la_SOURCES = totem-bemused.c
libbemused_la_LDFLAGS = $(plugin_ldflags)
libbemused_la_LIBADD = $(BEMUSED_LIBS)
libbemused_la_CFLAGS = \
	$(plugin_cflags)	\
	$(BEMUSED_CFLAGS)

-include $(top_srcdir)/git.mk
