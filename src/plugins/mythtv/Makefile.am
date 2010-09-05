include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/mythtv
plugin_LTLIBRARIES = libtotem_mythtv.la

plugin_in_files = mythtv.totem-plugin.in

libtotem_mythtv_la_SOURCES = totem-mythtv.c
libtotem_mythtv_la_LDFLAGS = $(plugin_ldflags)
libtotem_mythtv_la_LIBADD = $(GMYTH_LIBS)
libtotem_mythtv_la_CFLAGS = \
	$(plugin_cflags)	\
	$(GMYTH_CFLAGS)

-include $(top_srcdir)/git.mk
