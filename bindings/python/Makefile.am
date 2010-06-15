## Process this file with automake to produce Makefile.in

plugindir = $(PLUGINDIR)/python/totem
plugin_PYTHON = __init__.py

pymoduledir = $(libdir)/totem/plugins/python/totem
pymodule_LTLIBRARIES = _totem.la

nodist__totem_la_SOURCES = totem.c

_totem_la_SOURCES = override_common.c override_common.h totemmodule.c

_totem_la_LDFLAGS = \
	-module -avoid-version \
	-export-symbol-regex init_totem

_totem_la_LIBADD = \
	$(DEPENDENCY_LIBS)	\
	$(PYTHON_LIBS)		\
	$(PYTHON_EXTRA_LIBS)	\
	$(PYGTK_LIBS)

_totem_la_CFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins	\
	$(DEPENDENCY_CFLAGS)		\
	$(PEAS_CFLAGS)			\
	$(PYGTK_CFLAGS)			\
	$(PYTHON_CFLAGS)		\
	$(AM_CFLAGS)			\
	$(WNOERROR_CFLAGS)		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

totem.c: totem.defs totem.override
	( cd $(srcdir) && $(PYGTK_CODEGEN) \
		--register $(PYGTK_DEFSDIR)/gtk-types.defs \
		--register $(PYGTK_DEFSDIR)/libpeas.defs \
		--override $*.override \
		--prefix py$* $(<F) ) > $@ 

BINDING_HEADERS_SRCDIR_IN = \
	src/totem.h				\
	src/totem-cell-renderer-video.h		\
	src/plugins/totem-plugin.h		\
	$(NULL)

BINDING_HEADERS_BUILDDIR_IN = 

BINDING_HEADERS_SRCDIR		:= $(addprefix $(top_srcdir)/,$(BINDING_HEADERS_SRCDIR_IN))
BINDING_HEADERS_BUILDDIR	:= $(addprefix $(top_builddir)/,$(BINDING_HEADERS_BUILDDIR_IN))

regenerate-python-binding:
	$(PYGTK_H2DEF) $(sort $(BINDING_HEADERS_SRCDIR) $(BINDING_HEADERS_BUILDDIR)) > totem.defs.new

BUILT_SOURCES = totem.c

EXTRA_DIST = \
	totem.override		\
	totem.defs

CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
