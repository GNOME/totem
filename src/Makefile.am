
bin_PROGRAMS = totem totem-video-thumbnailer

if TOTEM_GST
MM=bacon-video-widget-gst.c
else
MM=bacon-video-widget-xine.c
endif

MARSHALFILES = baconvideowidget-marshal.h baconvideowidget-marshal.c
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`

baconvideowidget-marshal.h: baconvideowidget-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal baconvideowidget-marshal.list \
	  --header > baconvideowidget-marshal.tmp \
	  && mv baconvideowidget-marshal.tmp baconvideowidget-marshal.h ) \
	|| ( rm -f baconvideowidget-marshal.tmp && exit 1 )
baconvideowidget-marshal.c: baconvideowidget-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal baconvideowidget-marshal.list \
	  --body > baconvideowidget-marshal.tmp \
	  && mv baconvideowidget-marshal.tmp baconvideowidget-marshal.c ) \
	|| ( rm -f baconvideowidget-marshal.tmp && exit 1 )

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
	bacon-cd-selection.h		\
	bacon-message-connection.c	\
	bacon-message-connection.h

EGGDIR=$(srcdir)/../../libegg/libegg/recent-files/
BACONDIR=$(srcdir)/../../libbacon/src/
regenerate-built-sources:
	EGGFILES="$(EGG_RECENT_FILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh || true
	EGGFILES="$(BACON_CD_SELECTION)" EGGDIR="$(BACONDIR)" $(srcdir)/update-from-egg.sh || true

totem_SOURCES = 					\
	$(EGG_RECENT_FILES)				\
	$(BACON_CD_SELECTION)				\
	totem.c totem.h					\
	totem-remote.c totem-remote.h			\
	totem-preferences.c totem-preferences.h		\
	totem-private.h			\
	baconvideowidget-marshal.list	\
	$(MARSHALFILES)			\
	$(MM)				\
	bacon-video-widget.h		\
	bacon-video-widget-properties.c	\
	bacon-video-widget-properties.h	\
	gtk-playlist.c	gtk-playlist.h	\
	rb-ellipsizing-label.c		\
	rb-ellipsizing-label.h		\
	scrsaver.c  scrsaver.h		\
	video-utils.c video-utils.h	\
	debug.h	languages.h		\
	gnome-authn-manager.c		\
	gnome-authn-manager.h		\
	gnome-caption-table.c		\
	gnome-caption-table.h		\
	gnome-password-dialog.c		\
	gnome-password-dialog.h		\
	totem-statusbar.c		\
	totem-statusbar.h

totem_LDADD = $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(REMOTE_LIBS)

totem_video_thumbnailer_SOURCES =				\
	baconvideowidget-marshal.list				\
	$(MARSHALFILES)	$(MM)					\
	bacon-video-widget.h					\
	scrsaver.c  scrsaver.h          			\
	video-utils.c video-utils.h     			\
	totem-video-thumbnailer.c

totem_video_thumbnailer_LDADD = $(GTK_LIBS) $(XTEST_LIBS)

INCLUDES = 						\
	-D_REENTRANT					\
	$(EXTRA_GNOME_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLOGO_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"totem_logo.png\"						\
	-DICON_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"media-player-48.png\"					\
	-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED			\
	-DGNOME_DISABLE_DEPRECATED			\
        -DGCONF_DISABLE_DEPRECATED=1 			\
	-DLIBGLADE_DISABLE_DEPRECATED

CLEANFILES = *.bak *.gladep core* *.orig *~ $(MARSHALFILES) $(desktop_DATA)
EXTRA_DIST = bacon-video-widget-xine.c bacon-video-widget-gst.c

