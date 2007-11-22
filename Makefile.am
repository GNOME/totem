SUBDIRS = lib plparse bindings po data src help

if ENABLE_BROWSER_PLUGINS
SUBDIRS += browser-plugin
endif

EXTRA_DIST =			\
	COPYING.LIB		\
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	AUTHORS NEWS		\
	totem.spec.in		\
	totem.spec		\
	omf.make		\
	totem-plparser.pc.in	\
	xmldocs.make		\
	gnome-doc-utils.make

CLEANFILES = totem.spec totem-plparser.pc

DISTCLEANFILES = intltool-extract intltool-merge intltool-update \
		 gnome-doc-utils.make

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = totem-plparser.pc

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper
