
man_MANS = totem.1

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
	rhythmbox-volume-zero.png	\
	stock-tool-brightness-contrast-22.png	\
	totem.glade			\
	playlist.glade			\
	properties.glade		\
	uri.glade			\
	screenshot.glade		\
	totem_logo.png			\
	vanity.glade			\
	vanity.png

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = media-player-48.png vanity.png

desktop_in_files_all = totem.desktop.in vanity.desktop.in
if TOTEM_VANITY
desktop_in_files = totem.desktop.in vanity.desktop.in
else
desktop_in_files = totem.desktop.in
endif

desktopdir = $(datadir)/applications/
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

keys_DATA = totem.keys
keysdir = $(datadir)/mime-info/

app_DATA = totem.applications
appdir = $(datadir)/application-registry

serverdir = $(libdir)/bonobo/servers
server_DATA = Totem_properties.server
server_in_files = Totem_properties.server.in.in._1 Totem_properties.server.in.in._3 servers.sh
Totem_properties.server.in.in._2: $(server_in_files) servers.sh totem.keys
	$(srcdir)/servers.sh $(srcdir)/totem.keys		\
	> Totem_properties.server.in.in._2
Totem_properties.server.in.in: $(server_in_files) Totem_properties.server.in.in._2
	cat $(srcdir)/Totem_properties.server.in.in._1		\
	Totem_properties.server.in.in._2			\
	$(srcdir)/Totem_properties.server.in.in._3		\
	> Totem_properties.server.in.in
Totem_properties.server.in: Totem_properties.server.in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@
@INTLTOOL_SERVER_RULE@

update-gconf.sh: totem-video-thumbnail.schemas.in

totem-video-thumbnail.schemas.in: schemas.sh totem.keys
	$(srcdir)/schemas.sh $(srcdir)/totem.keys $(bindir)	\
	> totem-video-thumbnail.schemas.in			\
	2> update-gconf.sh

schema_in_files = totem.schemas.in totem-video-thumbnail.schemas.in
schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

# For the sake of some users
user-install: $(app_DATA) $(keys_DATA)
	mkdir -p $(HOME)/.gnome/application-registry $(HOME)/.gnome/mime-info/
	cp $(app_DATA) $(HOME)/.gnome/application-registry
	cp $(keys_DATA) $(HOME)/.gnome/mime-info/

if GCONF_SCHEMAS_INSTALL
install-data-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)
else
install-data-local:
endif

EXTRA_DIST = $(stuff_DATA) $(desktop_in_files_all) $(keys_DATA) $(app_DATA) $(server_in_files) totem.schemas.in lirc_example schemas.sh $(man_MANS)

CLEANFILES = *.bak *.gladep core* *.orig *~ $(desktop_DATA) $(schema_DATA)

