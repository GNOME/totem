
bin_PROGRAMS = totem

totem_SOURCES = 			\
	totem.c				\
	gtk-xine.c gtk-xine.h		\
	gtk-playlist.c	gtk-playlist.h	\
	rb-ellipsizing-label.c		\
	rb-ellipsizing-label.h		\
	debug.h

totem_LDADD = $(EXTRA_GNOME_LIBS) $(XINE_LIBS)
totem_LDFLAGS = -export-dynamic
INCLUDES = 						\
	$(EXTRA_GNOME_CFLAGS)				\
	$(XINE_CFLAGS)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\""$(datadir)"\"			\
	-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED

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

EXTRA_DIST = $(stuff_DATA) $(desktop_DATA) $(keys_DATA) $(app_DATA)

CLEANFILES = *.bak *.gladep core*
