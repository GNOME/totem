noinst_PROGRAMS = bvw-test

noinst_LTLIBRARIES = libbaconvideowidget.la

bvw_test_SOURCES = bvw-test.c

bvw_test_CPPFLAGS = \
	-DDATADIR=\"$(pkgdatadir)\"	\
	-DLOGO_PATH=DATADIR\"\"G_DIR_SEPARATOR_S\"totem\"G_DIR_SEPARATOR_S\"totem_logo.png\"	\
	$(DISABLE_DEPRECATED)		\
	$(AM_CPPFLAGS)

bvw_test_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(AM_CFLAGS)		\
	-I$(top_srcdir)/

bvw_test_LDADD = \
	libbaconvideowidget.la \
	$(DEPENDENCY_LIBS) $(XVIDMODE_LIBS) $(XTEST_LIBS)

BVWMARSHALFILES = baconvideowidget-marshal.c baconvideowidget-marshal.h
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
BUILT_SOURCES = $(BVWMARSHALFILES)

baconvideowidget-marshal.h: baconvideowidget-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --header > baconvideowidget-marshal.h )
baconvideowidget-marshal.c: baconvideowidget-marshal.h
	( $(GLIB_GENMARSHAL) --prefix=baconvideowidget_marshal $(srcdir)/baconvideowidget-marshal.list --body --header > baconvideowidget-marshal.c )

# Enums
BVW_ENUM_FILES = bacon-video-widget-enums.c bacon-video-widget-enums.h

bacon-video-widget-enums.h: bacon-video-widget.h Makefile
	$(AM_V_GEN)($(GLIB_MKENUMS) \
			--fhead "#ifndef BVW_ENUMS_H\n#define BVW_ENUMS_H\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define BVW_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* !BVW_ENUMS_H */" $(srcdir)/bacon-video-widget.h > $@)

bacon-video-widget-enums.c: bacon-video-widget.h Makefile bacon-video-widget-enums.h
	$(AM_V_GEN)($(GLIB_MKENUMS) \
			--fhead "#include \"bacon-video-widget.h\"\n#include \"bacon-video-widget-enums.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(srcdir)/bacon-video-widget.h > $@)

BUILT_SOURCES += $(BVW_ENUM_FILES)

libbaconvideowidget_la_SOURCES = \
	$(BVWMARSHALFILES) 		\
	$(BVW_ENUM_FILES)		\
	bacon-video-widget.h		\
	bacon-resize.h bacon-resize.c 	\
	video-utils.c video-utils.h 	\
	bacon-video-widget-gst-0.10.c			\
	bacon-video-widget-gst-missing-plugins.c	\
	bacon-video-widget-gst-missing-plugins.h	\
	gstscreenshot.c					\
	gstscreenshot.h

libbaconvideowidget_la_CPPFLAGS =		\
	-D_REENTRANT				\
	-DGCONF_PREFIX=\""/apps/totem"\"	\
	$(DISABLE_DEPRECATED)			\
	$(AM_CPPFLAGS)

libbaconvideowidget_la_CFLAGS =		\
	$(DEPENDENCY_CFLAGS)		\
	$(MM_CFLAGS)			\
	$(MISSING_PLUGINS_CFLAGS)	\
	$(AM_CFLAGS)			\
	-I$(top_srcdir)/


libbaconvideowidget_la_LIBADD =	\
	$(XVIDMODE_LIBS)	\
	$(MM_LIBS)		\
	$(GST_LIBS)		\
	$(EXTRA_BACKEND_LIBS)	\
	$(X_LIBS)		\
	$(MISSING_PLUGINS_LIBS)	\
	-lm

libbaconvideowidget_la_LDFLAGS=	\
	-no-undefined

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	baconvideowidget-marshal.list
