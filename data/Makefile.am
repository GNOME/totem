
man_MANS = totem.1

stuffdir = $(datadir)/totem/
stuff_DATA =				\
	filmholes.png			\
	filmholes-big-left.png		\
	filmholes-big-right.png		\
	media-player-48.png		\
	stock_media_next.png		\
	stock_media_previous.png	\
	stock_media_pause.png		\
	stock_media_play.png		\
	playlist-16.png			\
	playlist-24.png			\
	playlist-playing.png		\
	rhythmbox-volume-max.png	\
	rhythmbox-volume-zero.png	\
	stock-tool-brightness-contrast-22.png	\
	totem.glade			\
	playlist.glade			\
	mozilla-viewer.glade		\
	properties.glade		\
	skip_to.glade			\
	uri.glade			\
	screenshot.glade		\
	totem_logo.png			\
	vanity.glade			\
	vanity.png

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = media-player-48.png vanity.png

# totem.desktop.in.in.in = original
# totem.desktop.in.in = by configure, includes the X-Gnome-Bugzilla
# totem.desktop.in = includes the mime-types
# totem.desktop = intltool-ised

desktop_in_in_files_all = totem.desktop.in.in.in vanity.desktop.in.in
desktop_in_files_all = totem.desktop.in.in vanity.desktop.in
if TOTEM_VANITY
desktop_in_files = totem.desktop.in vanity.desktop.in
else
desktop_in_files = totem.desktop.in
endif

desktopdir = $(datadir)/applications/
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

totem.desktop.in: totem.desktop.in.in mime-type-list.txt desktop.sh
	cat $(srcdir)/totem.desktop.in.in > totem.desktop.in &&\
	$(srcdir)/desktop.sh $(srcdir)/mime-type-list.txt	\
	>> totem.desktop.in

totem-mime-types.h: mime-type-include.sh mime-type-list.txt
	$(srcdir)/mime-type-include.sh $(srcdir)/mime-type-list.txt	\
	> totem-mime-types.h

update-gconf.sh: totem-video-thumbnail.schemas.in

totem-video-thumbnail.schemas.in: schemas.sh mime-type-list.txt
	$(srcdir)/schemas.sh $(srcdir)/mime-type-list.txt $(bindir)	\
	> totem-video-thumbnail.schemas.in				\
	2> update-gconf.sh

totem-handlers.schemas.in: handlers.sh
	$(srcdir)/handlers.sh > totem-handlers.schemas.in

schema_in_files = totem.schemas.in totem-video-thumbnail.schemas.in totem-handlers.schemas.in
schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)
else
install-data-local:
endif

EXTRA_DIST = $(stuff_DATA) $(desktop_in_in_files_all) totem.schemas.in lirc_example schemas.sh desktop.sh handlers.sh $(man_MANS) mime-type-list.txt mime-type-include.sh

CLEANFILES = *.bak *.gladep core* *.orig *~ $(desktop_DATA) $(schema_DATA) $(desktop_in_files_all)

