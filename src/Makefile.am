
bin_PROGRAMS = totem

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
	egg-recent-util.h		\
	egg-recent-vfs-utils.c		\
	egg-recent-vfs-utils.h

EGGDIR=$(srcdir)/../../libegg/libegg/recent-files/
regenerate-built-sources:
	EGGFILES="$(EGG_RECENT_FILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh

totem_SOURCES = 			\
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
        debug.h                         \
        $(EGG_RECENT_FILES)

totem_LDADD = $(EXTRA_GNOME_LIBS) $(XINE_LIBS) $(XTEST_LIBS) $(REMOTE_LIBS)
totem_LDFLAGS = -export-dynamic
INCLUDES = 						\
	$(EXTRA_GNOME_CFLAGS)				\
	$(XINE_CFLAGS)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\""$(datadir)"\"			\
	-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED			\
	-DGNOME_DISABLE_DEPRECATED			\
	-DUSE_STABLE_LIBGNOMEUI

CLEANFILES = *.bak *.gladep core* *.orig *~ $(MARSHALFILES) $(desktop_DATA)

LIBTOOL = $(SHELL) $(top_builddir)/libtool-nofpic

libdir = ${exec_prefix}/lib/xine/plugins
lib_LTLIBRARIES = xineplug_inp_gnomevfs.la

xineplug_inp_gnomevfs_la_SOURCES = input_gnomevfs.c
xineplug_inp_gnomevfs_la_LIBADD = -lxine
xineplug_inp_gnomevfs_la_LDFLAGS = -avoid-version -module $(VFS_LIBS)

