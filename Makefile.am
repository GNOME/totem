ACLOCAL_AMFLAGS = -I m4 -I libgd ${ACLOCAL_FLAGS}

NORMAL_SUBDIRS = libgd po data help src docs

SUBDIRS = $(NORMAL_SUBDIRS)

if ENABLE_BROWSER_PLUGINS
SUBDIRS += browser-plugin
endif

EXTRA_DIST =			\
	license_change		\
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	AUTHORS NEWS		\
	totem.spec.in		\
	totem.spec		\
	ChangeLog.pre-gitlog	\
	MAINTAINERS		\
	totem.doap

CLEANFILES = totem.spec

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(srcdir)/INSTALL \
	$(srcdir)/config.h.in \
	$(srcdir)/py-compile \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/ChangeLog \
	$(srcdir)/omf.make \
	$(srcdir)/xmldocs.make

DISTCLEANFILES = intltool-extract intltool-merge intltool-update

DIST_SUBDIRS = $(NORMAL_SUBDIRS) browser-plugin
DISTCHECK_CONFIGURE_FLAGS = \
	--disable-scrollkeeper \
	--enable-gtk-doc \
	--with-nautilusdir='$${libdir}/nautilus/extensions-2.0-distcheck' \
	--enable-vala=no \
	--enable-introspection \
	--enable-appdata-validate

# Build ChangeLog from GIT history
ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --stat --after="Mon Jul 20 23:47:57 2009" > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
