
stuffdir = $(datadir)/totem/
stuff_DATA =				\
	filmholes.png			\
	media-player-48.png		\
	stock_media_next.png		\
	stock_media_previous.png	\
	stock_media_pause.png		\
	stock_media_play.png		\
	playlist-16.png			\
	playlist-24.png			\
	playlist-playing.png		\
	rhythmbox-volume-max.png	\
	rhythmbox-volume-medium.png	\
	rhythmbox-volume-min.png	\
	rhythmbox-volume-zero.png	\
	totem.glade			\
	playlist.glade			\
	properties.glade		\
	uri.glade			\
	totem_logo.png			\
	vanity.glade			\
	vanity.png

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = media-player-48.png

desktop_in_files = totem.desktop.in
desktopdir = $(datadir)/applications/
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

keys_DATA = totem.keys
keysdir = $(datadir)/mime-info/

app_DATA = totem.applications
appdir = $(datadir)/application-registry

update-gconf.sh: totem-video-thumbnail.schemas.in

totem-video-thumbnail.schemas.in: schemas.sh
	$(srcdir)/schemas.sh $(srcdir)/totem.keys $(bindir)	\
	> totem-video-thumbnail.schemas.in			\
	2> update-gconf.sh

schema_in_files = totem.schemas.in totem-video-thumbnail.schemas.in
schemadir = $(sysconfdir)/gconf/schemas
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

# For the sake of some users
user-install: $(app_DATA) $(keys_DATA)
	mkdir -p $(HOME)/.gnome/application-registry $(HOME)/.gnome/mime-info/
	cp $(app_DATA) $(HOME)/.gnome/application-registry
	cp $(keys_DATA) $(HOME)/.gnome/mime-info/

install-data-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

EXTRA_DIST = $(stuff_DATA) $(desktop_in_files) $(keys_DATA) $(app_DATA) totem.schemas.in lirc_example schemas.sh

CLEANFILES = *.bak *.gladep core* *.orig *~ $(desktop_DATA) $(schema_DATA)

