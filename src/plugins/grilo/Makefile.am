include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/grilo
plugin_LTLIBRARIES = libgrilo.la

plugin_in_files = grilo.plugin.in

EXTRA_DIST = grilo.gresource.xml totemmaintoolbar.ui totemselectiontoolbar.ui grilo.ui totem-grilo.conf

griloresources.h: grilo.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/grilo.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _totem --generate-header
griloresources.c: $(EXTRA_DIST) griloresources.h
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/grilo.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _totem --generate-source


BUILT_SOURCES = griloresources.h griloresources.c


libgrilo_la_SOURCES =				\
	totem-grilo.c				\
	totem-search-entry.c			\
	totem-search-entry.h			\
	totem-main-toolbar.h			\
	totem-main-toolbar.c			\
	totem-selection-toolbar.h		\
	totem-selection-toolbar.c		\
	icon-helpers.c				\
	icon-helpers.h				\
	griloresources.h			\
	griloresources.c

libgrilo_la_LDFLAGS = $(plugin_ldflags)
libgrilo_la_LIBADD =			\
        $(plugin_libadd)		\
        $(LIBGRILO_LIBS)		\
        $(top_builddir)/libgd/libgd.la
libgrilo_la_CFLAGS =			\
        $(plugin_cflags)		\
        $(LIBGRILO_CFLAGS)		\
	-I $(top_srcdir)/src/gst/	\
	-I $(top_srcdir)/libgd/

-include $(top_srcdir)/git.mk

