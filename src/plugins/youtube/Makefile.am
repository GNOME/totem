include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/youtube
plugin_LTLIBRARIES = libyoutube.la

plugin_in_files = youtube.plugin.in

uidir = $(plugindir)
ui_DATA = youtube.ui
EXTRA_DIST += $(ui_DATA)

libyoutube_la_SOURCES = totem-youtube.c
libyoutube_la_LDFLAGS = $(plugin_ldflags)
libyoutube_la_LIBADD = \
	$(plugin_libadd)		\
	$(LIBGDATA_LIBS)		\
	$(LIBSOUP_LIBS)
libyoutube_la_CFLAGS = \
	$(plugin_cflags)	\
	$(LIBGDATA_CFLAGS)	\
	$(LIBSOUP_CFLAGS)

-include $(top_srcdir)/git.mk
