SUBDIRS = icons

man_MANS = totem.1 totem-video-thumbnailer.1

stuffdir = $(pkgdatadir)
stuff_DATA =				\
	filmholes.png			\
	filmholes-big-left.png		\
	filmholes-big-right.png		\
	stock-tool-brightness-contrast-22.png	\
	totem.glade			\
	playlist.glade			\
	playlist-ui.xml			\
	mozilla-viewer.glade		\
	properties.glade		\
	skip_to.glade			\
	uri.glade			\
	screenshot.glade		\
	totem-ui.xml			\
	totem_logo.png			\
	vanity.glade			\
	vanity.png

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = vanity.png

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

# totem.desktop.in.in.in = original
# totem.desktop.in.in = by configure, includes the X-Gnome-Bugzilla
# totem.desktop.in = includes the mime-types
# totem.desktop = intltool-ised

desktop_in_files = totem.desktop.in
if TOTEM_VANITY
desktop_in_files += vanity.desktop.in
endif

desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

totem.desktop.in: totem.desktop.in.in mime-type-list.txt desktop.sh
	cat totem.desktop.in.in > $@ &&\
	$(SHELL) $(srcdir)/desktop.sh $(srcdir)/mime-type-list.txt	\
	>> $@

nodist_noinst_HEADERS = totem-mime-types.h
totem-mime-types.h: mime-type-include.sh mime-type-list.txt mime-functions.sh
	$(srcdir)/mime-type-include.sh $(srcdir)/mime-type-list.txt	\
	> $@

update-gconf.sh: totem-video-thumbnail.schemas.in

totem-video-thumbnail.schemas.in: schemas.sh mime-type-list.txt mime-functions.sh
	$(srcdir)/schemas.sh $(srcdir)/mime-type-list.txt $(bindir)	\
	> $@								\
	2> update-gconf.sh

totem-handlers.schemas.in: handlers.sh
	$(srcdir)/handlers.sh > $@

generated_schema_in_files = totem-video-thumbnail.schemas.in totem-handlers.schemas.in
schema_in_files = totem.schemas.in $(generated_schema_in_files)
schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

totem-video-thumbnailer.1: totem-video-thumbnailer.pod
	pod2man -c "" -s 1 -q none -n totem-video-thumbnailer -r GNOME $< > $@

install-data-local: $(schema_DATA)
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
	for p in $^ ; do \
		GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p 2>&1 > /dev/null; \
	done \
	fi
endif

EXTRA_DIST = \
	$(man_MANS)			\
	$(stuff_DATA)			\
	desktop.sh			\
	handlers.sh			\
	lirc_example			\
	mime-type-include.sh		\
	mime-type-list.txt		\
	schemas.sh			\
	totem.schemas.in		\
	totem-video-thumbnailer.1	\
	totem-video-thumbnailer.pod

CLEANFILES = \
 	*.bak *.gladep core* *.orig *~ 	\
	$(desktop_in_files)		\
	$(desktop_DATA)			\
	$(generated_schema_in_files)	\
	$(schema_DATA)			\
	totem-handlers.schemas.in 	\
	totem-mime-types.h		\
	totem-video-thumbnail.schemas.in\
	update-gconf.sh

DISTCLEANFILES =
