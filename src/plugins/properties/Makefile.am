include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/properties
plugin_LTLIBRARIES = libmovie-properties.la
noinst_LTLIBRARIES = libbaconvideowidgetproperties.la

plugin_in_files = movie-properties.totem-plugin.in

libmovie_properties_la_SOURCES = totem-movie-properties.c
libmovie_properties_la_LDFLAGS = $(plugin_ldflags)
libmovie_properties_la_LIBADD = libbaconvideowidgetproperties.la
libmovie_properties_la_CFLAGS = $(plugin_cflags)

libbaconvideowidgetproperties_la_SOURCES = \
	bacon-video-widget-properties.c	\
	bacon-video-widget-properties.h
libbaconvideowidgetproperties_la_CPPFLAGS = \
	$(AM_CPPFLAGS)			\
	-I$(top_srcdir)/src/backend
libbaconvideowidgetproperties_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(WARN_CFLAGS)

-include $(top_srcdir)/git.mk
