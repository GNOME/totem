include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/galago
plugin_LTLIBRARIES = libtgp.la

plugin_in_files = galago.plugin.in

libtgp_la_SOURCES = totem-galago.c
libtgp_la_LDFLAGS = $(plugin_ldflags)
libtgp_la_LIBADD = $(LIBGALAGO_LIBS)
libtgp_la_CFLAGS = \
	$(plugin_cflags)	\
	$(LIBGALAGO_CFLAGS)

-include $(top_srcdir)/git.mk
