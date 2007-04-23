modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/media-player-keys
plugin_LTLIBRARIES = libmedia_player_keys.la

plugin_in_files = media-player-keys.totem-plugin.desktop.in

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.totem-plugin.desktop.in=.totem-plugin)

common_defines = \
	-D_REENTRANT					\
	-DDBUS_API_SUBJECT_TO_CHANGE			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DBINDIR=\""$(bindir)"\"			\
	-DTOTEM_PLUGIN_DIR=\""$(libdir)/totem/plugins"\"\
	$(DISABLE_DEPRECATED)

libmedia_player_keys_la_SOURCES = totem-media-player-keys.c $(BUILT_SOURCES)
libmedia_player_keys_la_LDFLAGS = $(modules_flags)
libmedia_player_keys_la_LIBADD = $(MEDIA_PLAYER_KEYS_LIBS)
libmedia_player_keys_la_CPPFLAGS = $(common_defines)

libmedia_player_keys_la_CFLAGS = \
	$(EXTRA_GNOME_CFLAGS)	\
	$(MEDIA_PLAYER_KEYS_CFLAGS) \
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(AM_CFLAGS)		\
	-I$(top_srcdir)/lib	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/src/plugins

BUILT_SOURCES =			\
	totem-marshal.h		\
	totem-marshal.c

#Rule to generate the marshal files
totem-marshal.c: totem-marshal.list
	@GLIB_GENMARSHAL@ --prefix=totem_marshal $< --header --body > $@

totem-marshal.h: totem-marshal.list
	@GLIB_GENMARSHAL@ --prefix=totem_marshal $< --header > $@

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA) $(BUILT_SOURCES)
DISTCLEANFILES = $(plugin_DATA)

