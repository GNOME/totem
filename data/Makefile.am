
stuffdir = $(datadir)/totem/
stuff_DATA =				\
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
	playlist.glade

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

# For the sake of some users
user-install: $(app_DATA) $(keys_DATA)
	mkdir -p ~/.gnome/application-registry ~/.gnome/mime-info/
	cp $(app_DATA) ~/.gnome/application-registry
	cp $(keys_DATA) ~/.gnome/mime-info/

EXTRA_DIST = $(stuff_DATA) $(desktop_DATA) $(keys_DATA) $(app_DATA)

CLEANFILES = *.bak *.gladep core* *.orig *~ $(desktop_DATA)
