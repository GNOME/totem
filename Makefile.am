SUBDIRS = po src data help

install-data-local:
	@$(NORMAL_INSTALL)

EXTRA_DIST =			\
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	AUTHORS NEWS		\
	totem.spec.in		\
	totem.spec		\
	omf.make		\
	totem-plparser.pc.in	\
	xmldocs.make

CLEANFILES = totem.spec totem-plparser.pc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = totem-plparser.pc
