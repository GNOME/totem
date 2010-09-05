modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/media-player-keys
plugin_LTLIBRARIES = libmedia_player_keys.la

plugin_in_files = media-player-keys.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

marshal_files = \
	totem-marshal.h		\
	totem-marshal.c

libmedia_player_keys_la_SOURCES = totem-media-player-keys.c $(marshal_files)
libmedia_player_keys_la_LDFLAGS = $(modules_flags)
libmedia_player_keys_la_LIBADD = $(MEDIA_PLAYER_KEYS_LIBS)
libmedia_player_keys_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\""Totem"\"	\
	$(DISABLE_DEPRECATED)		\
	-I$(top_srcdir)/		\
	-I$(top_srcdir)/lib		\
	-I$(top_srcdir)/src		\
	-I$(top_srcdir)/src/plugins

libmedia_player_keys_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(MEDIA_PLAYER_KEYS_CFLAGS) \
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)

#Rule to generate the marshal files
totem-marshal.c: totem-marshal.list
	$(AM_V_GEN) @GLIB_GENMARSHAL@ --prefix=totem_marshal $< --header --body > $@

totem-marshal.h: totem-marshal.list
	$(AM_V_GEN) @GLIB_GENMARSHAL@ --prefix=totem_marshal $< --header > $@

EXTRA_DIST = totem-marshal.list $(plugin_in_files)

CLEANFILES = $(plugin_DATA) $(marshal_files)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
