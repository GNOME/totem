SUBDIRS = lib bindings po data src help

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
	xmldocs.make		\
	gnome-doc-utils.make

CLEANFILES = totem.spec

DISTCLEANFILES = intltool-extract intltool-merge intltool-update \
		 gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper
