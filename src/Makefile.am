noinst_PROGRAMS = bvw-test list_v4l test-parser disc-test metadata-test

if TOTEM_VANITY
VANITY=vanity
else
VANITY=
endif

if HAVE_NAUTILUS
TOTEM_PROPERTIES_PAGE = libtotem-properties-page.la
else
TOTEM_PROPERTIES_PAGE =
endif

if HAVE_MOZILLA
TOTEM_MOZILLA_VIEWER = totem-mozilla-viewer
TOTEM_MOZILLA_PLUGIN = libtotem_mozilla.la
else
TOTEM_MOZILLA_VIEWER = 
TOTEM_MOZILLA_PLUGIN =
endif

bin_PROGRAMS = totem totem-video-thumbnailer $(VANITY)
libexec_PROGRAMS = $(TOTEM_MOZILLA_VIEWER)
noinst_LTLIBRARIES = \
	libbaconvideowidget.la libbaconpropertiespage.la
plugin_LTLIBRARIES = $(TOTEM_MOZILLA_PLUGIN)
nautilus_LTLIBRARIES = $(TOTEM_PROPERTIES_PAGE)

modules_flags = -avoid-version -module

if TOTEM_GST
MM = bacon-video-widget-gst.c \
	gstscreenshot.c \
	gstscreenshot.h
else
MM = bacon-video-widget-xine.c
endif

BVWMARSHALFILES = baconvideowidget-marshal.h baconvideowidget-marshal.c
PLPARSERMARSHALFILES = totemplparser-marshal.c totemplparser-marshal.h
MARSHALFILES = $(BVWMARSHALFILES) $(PLPARSERMARSHALFILES)
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`

baconvideowidget-marshal.h: baconvideowidget-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --header > baconvideowidget-marshal.h )
baconvideowidget-marshal.c: baconvideowidget-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --body --header > baconvideowidget-marshal.c )
totemplparser-marshal.c: totemplparser-marshal.h
	 ( $(GLIB_GENMARSHAL) --prefix=totemplparser_marshal $(srcdir)/totemplparser-marshal.list --header --body > totemplparser-marshal.c )
totemplparser-marshal.h: totemplparser-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=totemplparser_marshal $(srcdir)/totemplparser-marshal.list --header > totemplparser-marshal.h )


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

BACON_MESSAGE_CONNECTION = 		\
	bacon-message-connection.c	\
	bacon-message-connection.h

BACON_V4L_SELECTION =			\
	video-dev.c			\
	video-dev.h			\
	bacon-v4l-selection.c		\
	bacon-v4l-selection.h

EGGDIR=$(srcdir)/../../libegg/libegg/recent-files/
BACONDIR=$(srcdir)/../../libbacon/src
regenerate-built-sources:
	EGGFILES="$(EGG_RECENT_FILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh || true
	EGGFILES="$(BACON_MESSAGE_CONNECTION)" EGGDIR="$(BACONDIR)" $(srcdir)/update-from-egg.sh || true

VIDEOWIDGET = \
	baconvideowidget-marshal.list totemplparser-marshal.list \
	$(BVWMARSHALFILES) $(MM) bacon-video-widget.h bacon-resize.h \
	bacon-resize.c video-utils.c video-utils.h debug.h

libbaconvideowidget_la_SOURCES = $(VIDEOWIDGET)
libbaconpropertiespage_la_SOURCES = \
	bacon-video-widget-properties.c \
	bacon-video-widget-properties.h

totem_SOURCES = 					\
	$(EGG_RECENT_FILES)				\
	$(BACON_MESSAGE_CONNECTION)			\
	totem.c totem.h					\
	totem-remote.c totem-remote.h			\
	totem-preferences.c totem-preferences.h		\
	totem-stock-icons.c totem-stock-icons.h		\
	totem-private.h			\
	totem-scrsaver.c		\
	totem-scrsaver.h		\
	totem-options.c totem-options.h	\
	totem-interface.c		\
	totem-interface.h		\
	totem-playlist.c totem-playlist.h \
	totem-screenshot.c		\
	totem-screenshot.h		\
	totem-session.c totem-session.h	\
	totem-skipto.c totem-skipto.h	\
	totem-sublang.c totem-sublang.h	\
	totem-time-label.c totem-time-label.h \
	totem-uri.c totem-uri.h		\
	debug.h				\
	totem-statusbar.c		\
	totem-statusbar.h		\
	totem-gromit.c totem-gromit.h

totem_LDADD = \
	$(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(REMOTE_LIBS) \
	$(XVIDMODE_LIBS) $(NVTV_LIBS) libtotem-plparser.la \
	libbaconvideowidget.la libbaconpropertiespage.la

totem_video_thumbnailer_SOURCES = totem-video-thumbnailer.c
totem_video_thumbnailer_LDADD = \
	$(X_LIBS) $(GTK_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) \
	$(NVTV_LIBS) $(EXTRA_GNOME_LIBS) libbaconvideowidget.la

nautilusdir = $(libdir)/nautilus/extensions-1.0/
libtotem_properties_page_la_CFLAGS = -I$(top_srcdir)
libtotem_properties_page_la_SOURCES = 	\
	totem-properties-main.c		\
	totem-properties-view.c		\
	totem-properties-view.h

libtotem_properties_page_la_LIBADD = \
	$(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) \
	$(NVTV_LIBS) $(NAUTILUS_LIBS) \
	libbaconvideowidget.la libbaconpropertiespage.la
libtotem_properties_page_la_LDFLAGS = $(module_flags)

BUILT_SOURCES = \
	totem_mozilla_scripting.h \
	libtotem_mozilla.xpt

CLEANFILES = $(BUILT_SOURCES)

totem_mozilla_scripting.h: totem_mozilla_scripting.idl
	$(MOZILLA_XPIDL) -m header -I $(MOZILLA_XPCFLAGS) $(srcdir)/totem_mozilla_scripting.idl

libtotem_mozilla.xpt: totem_mozilla_scripting.idl
	$(MOZILLA_XPIDL) -m typelib -I $(MOZILLA_XPCFLAGS) $(srcdir)/totem_mozilla_scripting.idl
	mv totem_mozilla_scripting.xpt libtotem_mozilla.xpt

typelib_DATA = libtotem_mozilla.xpt
typelibdir = $(libdir)/mozilla/plugins

libtotem_mozilla_la_LDFLAGS = $(MOZILLA_LIBS) $(module_flags)
libtotem_mozilla_la_SOURCES = \
	totem-mozilla-plugin.cpp \
	totem_mozilla_scripting.h \
	totem-mozilla-scriptable.h \
	totem-mozilla-scriptable.cpp \
	totem-mozilla-options.h

totem_mozilla_viewer_SOURCES = 		\
	totem-mozilla-viewer.c		\
	totem-mozilla-options.h
totem_mozilla_viewer_LDADD = \
	$(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) \
	$(NVTV_LIBS) libbaconvideowidget.la libbaconpropertiespage.la

plugindir = $(libdir)/mozilla/plugins/
module_flags = -export_dynamic -avoid-version

vanity_SOURCES =					\
	$(BACON_V4L_SELECTION)				\
	totem-screenshot.c totem-screenshot.h		\
	vanity.c

vanity_LDADD = \
	$(X_LIBS) $(EXTRA_GNOME_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) \
	$(NVTV_LIBS) libbaconvideowidget.la

bvw_test_SOURCES = bvw-test.c
bvw_test_LDADD = \
	$(X_LIBS) $(GTK_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) $(NVTV_LIBS) \
	libbaconvideowidget.la

metadata_test_SOURCES = metadata-test.c
metadata_test_LDADD = \
	$(X_LIBS) $(GTK_LIBS) $(XTEST_LIBS) $(XVIDMODE_LIBS) $(NVTV_LIBS) \
	$(EXTRA_GNOME_LIBS) libbaconvideowidget.la

disc_test_SOURCES = disc-test.c
disc_test_LDADD = \
	$(GTK_LIBS) $(EXTRA_GNOME_LIBS) \
	libtotem-plparser.la

list_v4l_SOURCES = list_v4l.c $(BACON_V4L_SELECTION)
list_v4l_LDADD = $(GTK_LIBS)

test_parser_SOURCES = test-parser.c
test_parser_LDADD = \
	$(EXTRA_GNOME_LIBS) \
	libtotem-plparser.la

INCLUDES = 						\
	-D_REENTRANT					\
	-I$(top_srcdir)					\
	$(EXTRA_GNOME_CFLAGS)				\
	$(MOZILLA_CFLAGS)				\
	$(NVTV_CFLAGS)					\
	$(NAUTILUS_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGCONF_PREFIX=\""/apps/totem"\"		\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DLOGO_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"totem_logo.png\"

CLEANFILES = *.bak *.gladep core* *.orig *~ $(MARSHALFILES) $(desktop_DATA)
EXTRA_DIST =	bacon-video-widget-xine.c \
		bacon-video-widget-gst.c \
		gstscreenshot.c \
		gstscreenshot.h \
		$(BACON_V4L_SELECTION) \
		vanity.c \
		totem-mozilla-viewer.c \
		totem-mozilla-plugin.cpp \
		totem-mozilla-protocol.h \
		totem_mozilla_scripting.idl

## libtotem-plparser.la

lib_LTLIBRARIES = libtotem-plparser.la

libtotem_plparser_la_SOURCES =			\
	totem-pl-parser-builtins.c		\
	totemplparser-marshal.c			\
	totemplparser-marshal.h			\
	totem-disc.c				\
	totem-pl-parser.c

plparserincludedir = $(includedir)/totem/1/plparser
plparserinclude_HEADERS =			\
	totem-pl-parser-builtins.h		\
	totem-disc.h				\
	totem-pl-parser.h

libtotem_plparser_la_LDFLAGS =			\
	-version-info $(PLPARSER_LT_VERSION)	\
	-no-undefined				\
	$(TOTEM_PLPARSER_LIBS)


GLIB_MKENUMS=`pkg-config --variable=glib_mkenums glib-2.0`
totem-pl-parser-builtins.h: stamp-totem-pl-parser-builtins.h
	@true

stamp-totem-pl-parser-builtins.h: totem-pl-parser.h Makefile
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __TOTEM_PL_PARSER_BUILTINS_H__\n#define __TOTEM_PL_PARSER_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define TOTEM_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __TOTEM_PL_PARSER_BUILTINS_H__ */" totem-pl-parser.h) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh totem-pl-parser-builtins.h || cp xgen-gtbh totem-pl-parser-builtins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)

totem-pl-parser-builtins.c: totem-pl-parser.h Makefile totem-pl-parser-builtins.h
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include \"totem-pl-parser.h\"\n#include \"totem-pl-parser-builtins.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		totem-pl-parser.h ) > xgen-gtbc \
	&& cp xgen-gtbc totem-pl-parser-builtins.c  \
	&& rm -f xgen-gtbc

CLEANFILES += \
	totem-pl-parser-builtins.h \
	totem-pl-parser-builtins.c \
	stamp-totem-pl-parser-builtins.h
