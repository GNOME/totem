
bin_PROGRAMS = totem totem-video-thumbnailer

MARSHALFILES = gtkxine-marshal.h gtkxine-marshal.c
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`

gtkxine-marshal.h: gtkxine-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=gtkxine_marshal gtkxine-marshal.list \
	  --header > gtkxine-marshal.tmp \
	  && mv gtkxine-marshal.tmp gtkxine-marshal.h ) \
	|| ( rm -f gtkxine-marshal.tmp && exit 1 )
gtkxine-marshal.c: gtkxine-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=gtkxine_marshal gtkxine-marshal.list \
	  --body > gtkxine-marshal.tmp \
	  && mv gtkxine-marshal.tmp gtkxine-marshal.c ) \
	|| ( rm -f gtkxine-marshal.tmp && exit 1 )

EGG_RECENT_FILES =                      \
	egg-recent-model.c              \
        egg-recent-model.h              \
	egg-recent-item.c               \
	egg-recent-item.h               \
	egg-recent-view.c               \
	egg-recent-view.h               \
	egg-recent-view-gtk.c           \
        egg-recent-view-gtk.h           \
	egg-recent-util.c               \
	egg-recent-util.h

BACON_CD_SELECTION = 			\
	cd-drive.c			\
	cd-drive.h			\
	bacon-cd-selection.c		\
	bacon-cd-selection.h

EGGDIR=$(srcdir)/../../libegg/libegg/recent-files/
BACONDIR=$(srcdir)/../../libbacon/src/
regenerate-built-sources:
	EGGFILES="$(EGG_RECENT_FILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh || true
	EGGFILES="$(BACON_CD_SELECTION)" EGGDIR="$(BACONDIR)" $(srcdir)/update-from-egg.sh || true

totem_SOURCES = 			\
	$(EGG_RECENT_FILES)		\
	$(BACON_CD_SELECTION)		\
	totem.c				\
	totem.h				\
	totem-remote.c			\
	totem-remote.h			\
	gtkxine-marshal.list		\
	$(MARSHALFILES)			\
	gtk-xine.c gtk-xine.h		\
	gtk-playlist.c	gtk-playlist.h	\
	rb-ellipsizing-label.c		\
	rb-ellipsizing-label.h		\
	scrsaver.c  scrsaver.h		\
	video-utils.c video-utils.h	\
	gtk-message.c gtk-message.h	\
	debug.h

totem_LDADD = $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(REMOTE_LIBS)
totem_LDFLAGS = -export-dynamic

totem_video_thumbnailer_SOURCES =	\
	gtkxine-marshal.list		\
	$(MARSHALFILES)			\
	gtk-xine.c gtk-xine.h		\
	scrsaver.c  scrsaver.h          \
	video-utils.c video-utils.h     \
	totem-video-thumbnailer.c

totem_video_thumbnailer_LDADD = $(GTK_LIBS) $(XTEST_LIBS)

INCLUDES = 						\
	-D_REENTRANT					\
	$(EXTRA_GNOME_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem/"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLOGO_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"totem_logo.mpv\"						\
	-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED			\
	-DGNOME_DISABLE_DEPRECATED

CLEANFILES = *.bak *.gladep core* *.orig *~ $(MARSHALFILES) $(desktop_DATA)

