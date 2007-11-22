SUBDIRS = plugins backend

bin_PROGRAMS = totem totem-video-thumbnailer totem-video-indexer totem-audio-preview
libexec_PROGRAMS =
noinst_LTLIBRARIES =				\
	libbaconmessageconnection.la		\
	libtotem_player.la

common_defines = \
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	-DLOGO_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"totem_logo.png\"	\
	$(DISABLE_DEPRECATED)

modules_flags = -export_dynamic -avoid-version -module -no-undefined

# Bacon message connection ltlibrary

BACON_MESSAGE_CONNECTION = \
	bacon-message-connection.c \
	bacon-message-connection.h

libbaconmessageconnection_la_SOURCES =	\
	$(BACON_MESSAGE_CONNECTION)

libbaconmessageconnection_la_CPPFLAGS = \
	$(common_defines)	\
	$(AM_CPPFLAGS)

libbaconmessageconnection_la_CFLAGS = \
	$(EXTRA_GNOME_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(AM_CFLAGS)

libbaconmessageconnection_la_LDFLAGS = \
	$(AM_LDFLAGS)


EGGDIR=$(srcdir)/../../libegg/libegg/recent-files/
BACONDIR=$(srcdir)/../../libbacon/src
regenerate-built-sources:
	EGGFILES="$(BACON_MESSAGE_CONNECTION)" EGGDIR="$(BACONDIR)" $(srcdir)/update-from-egg.sh || true

# Totem UI ltlibrary

libtotem_player_la_SOURCES =		\
	totem-statusbar.c		\
	totem-statusbar.h		\
	totem-interface.c		\
	totem-interface.h		\
	totem-fullscreen.c		\
	totem-fullscreen.h		\
	totem-time-label.c		\
	totem-time-label.h

libtotem_player_la_CPPFLAGS = \
	-I$(srcdir)/backend	\
	-I$(top_srcdir)/plparse	\
	-I$(top_builddir)/src/plparse	\
	$(common_defines)	\
	$(AM_CPPFLAGS)

libtotem_player_la_CFLAGS = \
	$(EXTRA_GNOME_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(AM_CFLAGS)

libtotem_player_la_LDFLAGS = \
	$(AM_LDFLAGS)

# Totem

TOTEMMARSHALFILES = 						\
	totemobject-marshal.c totemobject-marshal.h		\
	totemplaylist-marshal.c totemplaylist-marshal.h		\
	totemvideolist-marshal.c totemvideolist-marshal.h
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
BUILT_SOURCES = $(TOTEMMARSHALFILES)

totemobject-marshal.h: totemobject-marshal.list Makefile
	( $(GLIB_GENMARSHAL) --prefix=totemobject_marshal $(srcdir)/totemobject-marshal.list --header > totemobject-marshal.h )
totemobject-marshal.c: totemobject-marshal.h Makefile
	( $(GLIB_GENMARSHAL) --prefix=totemobject_marshal $(srcdir)/totemobject-marshal.list --header --body > totemobject-marshal.c )
totemplaylist-marshal.h: totemplaylist-marshal.list Makefile
	( $(GLIB_GENMARSHAL) --prefix=totemplaylist_marshal $(srcdir)/totemplaylist-marshal.list --header > totemplaylist-marshal.h )
totemplaylist-marshal.c: totemplaylist-marshal.h Makefile
	( $(GLIB_GENMARSHAL) --prefix=totemplaylist_marshal $(srcdir)/totemplaylist-marshal.list --header --body > totemplaylist-marshal.c )
totemvideolist-marshal.h: totemvideolist-marshal.list Makefile
	( $(GLIB_GENMARSHAL) --prefix=totemvideolist_marshal $(srcdir)/totemvideolist-marshal.list --header > totemvideolist-marshal.h )
totemvideolist-marshal.c: totemvideolist-marshal.h Makefile
	( $(GLIB_GENMARSHAL) --prefix=totemvideolist_marshal $(srcdir)/totemvideolist-marshal.list --header --body > totemvideolist-marshal.c )

totem_SOURCES = 					\
	totem.c totem.h					\
	totem-object.c					\
	$(TOTEMMARSHALFILES)				\
	totem-private.h					\
	totem-preferences.c totem-preferences.h		\
	totem-options.c totem-options.h	\
	totem-playlist.c totem-playlist.h \
	totem-screenshot.c		\
	totem-screenshot.h		\
	totem-session.c totem-session.h	\
	totem-sidebar.c totem-sidebar.h \
	totem-open-location.c totem-open-location.h \
	totem-menu.c totem-menu.h	\
	totem-missing-plugins.c totem-missing-plugins.h \
	totem-uri.c totem-uri.h		\
	ev-sidebar.c ev-sidebar.h	\
	totem-subtitle-encoding.c	\
	totem-subtitle-encoding.h	\
	totem-cell-renderer-video.c	\
	totem-cell-renderer-video.h	\
	totem-video-list.c		\
	totem-video-list.h

totem_CPPFLAGS = \
	-I$(top_srcdir)/plparse		\
	-I$(srcdir)/backend		\
	-I$(srcdir)/plugins		\
	-I$(top_builddir)/data		\
	-I$(top_builddir)/src/plparse	\
	$(common_defines)		\
	$(AM_CPPFLAGS)

totem_CFLAGS = \
	$(WARN_CFLAGS)			\
	$(EXTRA_GNOME_CFLAGS)		\
	$(DBUS_CFLAGS)			\
	$(NVTV_CFLAGS)			\
	$(MISSING_PLUGINS_CFLAGS) \
	$(AM_CFLAGS)

totem_LDFLAGS = \
	$(AM_LDFLAGS)

totem_LDADD =						\
	$(top_builddir)/plparse/libtotem-plparser.la	\
	backend/libbaconvideowidget.la			\
	plugins/libtotemmodule.la			\
	libbaconmessageconnection.la			\
	libtotem_player.la				\
	$(EXTRA_GNOME_LIBS)				\
	$(XVIDMODE_LIBS)				\
	$(NVTV_LIBS)					\
	$(DBUS_LIBS)					\
	$(XTEST_LIBS)					\
	$(X_LIBS)					\
	$(MISSING_PLUGINS_LIBS)

if ENABLE_PYTHON
totem_LDADD += \
	$(PYTHON_LIBS)			\
	$(top_builddir)/bindings/python/totem-python.la
endif

# Totem video thumbnailer

totem_video_thumbnailer_SOURCES =	\
	totem-video-thumbnailer.c	\
	totem-resources.c		\
	totem-resources.h

totem_video_thumbnailer_CPPFLAGS = \
	-I$(top_srcdir)/plparse	\
	-I$(srcdir)/backend	\
	$(common_defines)	\
	$(AM_CPPFLAGS)

totem_video_thumbnailer_CFLAGS = \
	$(EXTRA_GNOME_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(NVTV_CFLAGS)		\
	$(AM_CFLAGS)

totem_video_thumbnailer_LDFLAGS = \
	$(AM_LDFLAGS)

totem_video_thumbnailer_LDADD =			\
	backend/libbaconvideowidget.la		\
	$(GTK_LIBS) 				\
	$(EXTRA_GNOME_LIBS)			\
	$(NVTV_LIBS)				\
	$(XTEST_LIBS)				\
	$(XVIDMODE_LIBS)			\
	$(X_LIBS)

# Nautilus Property Page

if HAVE_NAUTILUS

nautilusdir = $(libdir)/nautilus/extensions-1.0
nautilus_LTLIBRARIES = libtotem-properties-page.la

libtotem_properties_page_la_SOURCES = 	\
	totem-properties-main.c		\
	totem-properties-view.c		\
	totem-properties-view.h

libtotem_properties_page_la_CPPFLAGS =		\
	-I$(srcdir)/backend			\
	-I$(top_srcdir)/plparse			\
	-I$(srcdir)/plugins/properties		\
	-I$(top_builddir)/data			\
	$(common_defines)			\
	$(AM_CPPFLAGS)

libtotem_properties_page_la_CFLAGS = \
	$(WARN_CFLAGS)		\
	$(EXTRA_GNOME_CFLAGS)	\
	$(NAUTILUS_CFLAGS)	\
	$(NVTV_CFLAGS)		\
	$(AM_CFLAGS)

libtotem_properties_page_la_LDFLAGS = \
	$(modules_flags)	\
	$(AM_LDFLAGS)

libtotem_properties_page_la_LIBADD =				\
	backend/libbaconvideowidget.la				\
	plugins/properties/libbaconvideowidgetproperties.la	\
	libtotem_player.la					\
	$(EXTRA_GNOME_LIBS)					\
	$(NAUTILUS_LIBS)					\
	$(NVTV_LIBS)						\
	$(XTEST_LIBS)						\
	$(XVIDMODE_LIBS)					\
	$(X_LIBS)

noinst_PROGRAMS = test-properties-page

test_properties_page_SOURCES =		\
	totem-properties-main.c		\
	totem-properties-view.c		\
	totem-properties-view.h		\
	test-properties-page.c

test_properties_page_CPPFLAGS = 		\
	-I$(srcdir)/backend			\
	-I$(top_srcdir)/plparse			\
	-I$(top_builddir)/data			\
	-I$(srcdir)/plugins/properties		\
	$(common_defines)			\
	$(AM_CPPFLAGS)

test_properties_page_CFLAGS = \
	$(WARN_CFLAGS)		\
	$(EXTRA_GNOME_CFLAGS)	\
	$(NAUTILUS_CFLAGS)	\
	$(NVTV_CFLAGS)		\
	$(AM_CFLAGS)

test_properties_page_LDFLAGS = \
	$(AM_LDFLAGS)

test_properties_page_LDADD = 					\
	backend/libbaconvideowidget.la				\
	plugins/properties/libbaconvideowidgetproperties.la	\
	libtotem_player.la					\
	$(EXTRA_GNOME_LIBS)					\
	$(NAUTILUS_LIBS)					\
	$(NVTV_LIBS)						\
	$(XTEST_LIBS)						\
	$(XVIDMODE_LIBS)					\
	$(X_LIBS)

endif # HAVE_NAUTILUS

# Totem Video Indexer

totem_video_indexer_SOURCES =		\
	totem-video-indexer.c		\
	totem-resources.c		\
	totem-resources.h

totem_video_indexer_CPPFLAGS = \
	-I$(top_srcdir)/plparse	\
	-I$(srcdir)/backend	\
	-I$(top_builddir)/data	\
	$(common_defines)	\
	$(AM_CPPFLAGS)

totem_video_indexer_CFLAGS = \
	$(WARN_CFLAGS)		\
	$(EXTRA_GNOME_CFLAGS)	\
	$(NVTV_CFLAGS)		\
	$(AM_CFLAGS)

totem_video_indexer_LDFLAGS = \
	$(AM_LDFLAGS)

totem_video_indexer_LDADD = \
	backend/libbaconvideowidget.la	\
	$(EXTRA_GNOME_LIBS)		\
	$(GTK_LIBS)			\
	$(NVTV_LIBS)			\
	$(XTEST_LIBS)			\
	$(XVIDMODE_LIBS)		\
	$(X_LIBS)

# Totem Audio Preview for nautilus
totem_audio_preview_SOURCES =		\
	totem-audio-preview.c		\
	totem-resources.c		\
	totem-resources.h

totem_audio_preview_CPPFLAGS =	\
	-I$(top_srcdir)/plparse	\
	-I$(srcdir)/backend	\
	-I$(top_builddir)/data	\
	$(common_defines)	\
	$(AM_CPPFLAGS)

totem_audio_preview_CFLAGS = \
	$(WARN_CFLAGS)		\
	$(EXTRA_GNOME_CFLAGS)	\
	$(NVTV_CFLAGS)		\
	$(AM_CFLAGS)

totem_audio_preview_LDFLAGS = \
	$(AM_LDFLAGS)

totem_audio_preview_LDADD = \
	backend/libbaconvideowidget.la	\
	$(EXTRA_GNOME_LIBS)		\
	$(GTK_LIBS)			\
	$(NVTV_LIBS)			\
	$(XTEST_LIBS)			\
	$(XVIDMODE_LIBS)		\
	$(X_LIBS)

CLEANFILES =			\
	*.bak			\
	core*			\
	*.orig			\
	*~			\
	$(desktop_DATA)		\
	$(BUILT_SOURCES)

EXTRA_DIST = 				\
	totemobject-marshal.list	\
	totemvideolist-marshal.list
		
