SUBDIRS = lib bindings po data src help

if ENABLE_BROWSER_PLUGINS
SUBDIRS += browser-plugin
endif

EXTRA_DIST =			\
	license_change		\
	autogen.sh		\
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
