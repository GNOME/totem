noinst_PROGRAMS = test-parser disc-test

test_parser_SOURCES = test-parser.c

test_parser_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_builddir)/src/plparser	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"\
	$(DISABLE_DEPRECATED)		\
	$(AM_CPPFLAGS)

test_parser_CFLAGS = \
	$(EXTRA_GNOME_CFLAGS)	\
	$(HAL_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(WARN_CFLAGS)		\
	$(AM_CFLAGS)

test_parser_LDADD = \
	$(EXTRA_GNOME_LIBS) \
	libtotem-plparser.la

disc_test_SOURCES = \
	disc-test.c

disc_test_CPPFLAGS = \
	$(common_defines)	\
	$(AM_CPPFLAGS)

disc_test_CFLAGS = \
	$(WARN_CFLAGS)		\
	$(GTK_CFLAGS)		\
	$(EXTRA_GNOME_CFLAGS)	\
	$(AM_CFLAGS)

disc_test_LDFLAGS = \
	$(AM_LDFLAGS)

disc_test_LDADD = \
	libtotem-plparser.la	\
	$(GTK_LIBS)			\
	$(EXTRA_GNOME_LIBS)

lib_LTLIBRARIES = libtotem-plparser.la
noinst_LTLIBRARIES = libtotem-plparser-mini.la

MARSHALFILES = totemplparser-marshal.c totemplparser-marshal.h
BUILT_SOURCES = $(MARSHALFILES)

totemplparser-marshal.c: totemplparser-marshal.h
	 ( $(GLIB_GENMARSHAL) --prefix=totemplparser_marshal $(srcdir)/totemplparser-marshal.list --header --body > totemplparser-marshal.c )
totemplparser-marshal.h: totemplparser-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=totemplparser_marshal $(srcdir)/totemplparser-marshal.list --header > totemplparser-marshal.h )

plparserincludedir = $(pkgincludedir)/1/plparser
plparserinclude_HEADERS =			\
	totem-pl-parser-builtins.h		\
	totem-pl-parser-features.h		\
	totem-pl-parser.h			\
	totem-disc.h

libtotem_plparser_la_SOURCES =			\
	totem-disc.c				\
	totem-disc.h				\
	totem-pl-parser-builtins.c		\
	totem-pl-parser-builtins.h		\
	totem-pl-parser.c			\
	totem-pl-parser-features.h		\
	totem-pl-parser.h			\
	totem-pl-parser-lines.c			\
	totem-pl-parser-lines.h			\
	totemplparser-marshal.c			\
	totemplparser-marshal.h			\
	totem-pl-parser-media.c			\
	totem-pl-parser-media.h			\
	totem-pl-parser-misc.c			\
	totem-pl-parser-misc.h			\
	totem-pl-parser-pla.c			\
	totem-pl-parser-pla.h			\
	totem-pl-parser-pls.c			\
	totem-pl-parser-pls.h			\
	totem-pl-parser-podcast.c		\
	totem-pl-parser-podcast.h		\
	totem-pl-parser-private.h		\
	totem-pl-parser-qt.c			\
	totem-pl-parser-qt.h			\
	totem-pl-parser-smil.c			\
	totem-pl-parser-smil.h			\
	totem-pl-parser-wm.c			\
	totem-pl-parser-wm.h			\
	totem-pl-parser-xspf.c			\
	totem-pl-parser-xspf.h			\
	xmlparser.h xmlparser.c			\
	xmllexer.h xmllexer.c

libtotem_plparser_la_CPPFLAGS = \
	-I$(top_srcdir)				\
	-I$(top_srcdir)/lib			\
	-I$(top_builddir)/src/plparser		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"\
	$(DISABLE_DEPRECATED)			\
	$(AM_CPPFLAGS)

libtotem_plparser_la_CFLAGS = \
	$(TOTEM_PLPARSER_CFLAGS)	\
	$(HAL_CFLAGS)			\
	$(DBUS_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)			\
	-I$(top_srcdir)/lib		\
	-D_GNU_SOURCE

libtotem_plparser_la_LIBADD = \
	$(TOTEM_PLPARSER_LIBS)			\
	$(top_builddir)/lib/libtotem_glibc.la	\
	$(HAL_LIBS)

libtotem_plparser_la_LDFLAGS =			\
	-version-info $(PLPARSER_LT_VERSION)	\
	-export-symbols $(srcdir)/plparser.symbols \
	-no-undefined				\
	$(AM_LDFLAGS)

libtotem_plparser_mini_la_SOURCES =		\
	totem-pl-parser-mini.h			\
	totem-pl-parser.c			\
	totem-pl-parser-lines.c			\
	totem-pl-parser-lines.h			\
	totem-pl-parser-misc.c			\
	totem-pl-parser-misc.h			\
	totem-pl-parser-pls.c			\
	totem-pl-parser-pls.h			\
	totem-pl-parser-podcast.c		\
	totem-pl-parser-podcast.h		\
	totem-pl-parser-private.h		\
	totem-pl-parser-qt.c			\
	totem-pl-parser-qt.h			\
	totem-pl-parser-smil.c			\
	totem-pl-parser-smil.h			\
	totem-pl-parser-wm.c			\
	totem-pl-parser-wm.h			\
	totem-pl-parser-xspf.c			\
	totem-pl-parser-xspf.h

libtotem_plparser_mini_la_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_builddir)/src/plparser	\
	-DTOTEM_PL_PARSER_MINI		\
	$(DISABLE_DEPRECATED)		\
	$(AM_CPPFLAGS)

libtotem_plparser_mini_la_CFLAGS = \
	$(TOTEM_PLPARSER_MINI_CFLAGS)	\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)

libtotem_plparser_mini_la_LIBADD = \
	$(TOTEM_PLPARSER_MINI_LIBS)

libtotem_plparser_mini_la_LDFLAGS = \
	-no-undefined	\
	$(AM_LDFLAGS)

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

CLEANFILES = \
	totem-pl-parser-builtins.h \
	totem-pl-parser-builtins.c \
	stamp-totem-pl-parser-builtins.h \
	$(BUILT_SOURCES)

EXTRA_DIST =				\
	totemplparser-marshal.list	\
	totem-pl-parser-features.h.in	\
	plparser.symbols
