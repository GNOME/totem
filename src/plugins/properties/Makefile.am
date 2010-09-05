modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/properties
plugin_LTLIBRARIES = libmovie-properties.la
noinst_LTLIBRARIES = libbaconvideowidgetproperties.la

plugin_in_files = movie-properties.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

common_defines = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/backend	\
	-I$(top_srcdir)/src/plugins

libmovie_properties_la_SOURCES = totem-movie-properties.c
libmovie_properties_la_LDFLAGS = $(modules_flags)
libmovie_properties_la_LIBADD = libbaconvideowidgetproperties.la
libmovie_properties_la_CPPFLAGS = $(common_defines)

libmovie_properties_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)

libbaconvideowidgetproperties_la_SOURCES =		\
	bacon-video-widget-properties.c			\
	bacon-video-widget-properties.h

libbaconvideowidgetproperties_la_CPPFLAGS = $(common_defines)
libbaconvideowidgetproperties_la_CFLAGS =	\
	$(DEPENDENCY_CFLAGS)			\
	$(WARN_CFLAGS)				\
	$(DBUS_CFLAGS)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
