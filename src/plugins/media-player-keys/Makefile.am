include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/media-player-keys
plugin_LTLIBRARIES = libmedia_player_keys.la

plugin_in_files = media-player-keys.plugin.in

# Marshal files
marshal_files = \
	totem-marshal.h		\
	totem-marshal.c

totem-marshal.c: totem-marshal.list totem-marshal.h
	$(AM_V_GEN) @GLIB_GENMARSHAL@ --prefix=totem_marshal $< --header --body > $@

totem-marshal.h: totem-marshal.list
	$(AM_V_GEN) @GLIB_GENMARSHAL@ --prefix=totem_marshal $< --header > $@

EXTRA_DIST += totem-marshal.list
CLEANFILES += $(marshal_files)

libmedia_player_keys_la_SOURCES = \
	$(marshal_files)		\
	totem-media-player-keys.c
libmedia_player_keys_la_LDFLAGS = $(plugin_ldflags)
libmedia_player_keys_la_LIBADD = $(MEDIA_PLAYER_KEYS_LIBS)
libmedia_player_keys_la_CFLAGS = \
	$(plugin_cflags)		\
	$(MEDIA_PLAYER_KEYS_CFLAGS)

-include $(top_srcdir)/git.mk
