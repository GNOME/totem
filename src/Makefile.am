
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

totem_SOURCES = 			\
	totem.c				\
	gtkxine-marshal.list		\
	$(MARSHALFILES)			\
	gtk-xine.c gtk-xine.h		\
	gtk-playlist.c	gtk-playlist.h	\
	rb-ellipsizing-label.c		\
	rb-ellipsizing-label.h		\
	scrsaver.c  scrsaver.h		\
	video-utils.c video-utils.h	\
	debug.h

totem_LDADD = $(EXTRA_GNOME_LIBS) $(XINE_LIBS) $(XTEST_LIBS)
totem_LDFLAGS = -export-dynamic
INCLUDES = 						\
	$(EXTRA_GNOME_CFLAGS)				\
	$(XINE_CFLAGS)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\""$(datadir)"\"			\
	-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED

CLEANFILES = *.bak *.gladep core* *.orig *~ $(MARSHALFILES) $(desktop_DATA)
