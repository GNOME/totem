noinst_PROGRAMS = test-parser

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir)/src/plparser \
	$(EXTRA_GNOME_CFLAGS)

test_parser_SOURCES = test-parser.c
test_parser_LDADD = \
	$(EXTRA_GNOME_LIBS) \
	libtotem-plparser.la

lib_LTLIBRARIES = libtotem-plparser.la

MARSHALFILES = totemplparser-marshal.c totemplparser-marshal.h
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
BUILT_SOURCES = $(MARSHALFILES)

totemplparser-marshal.c: totemplparser-marshal.h
	 ( $(GLIB_GENMARSHAL) --prefix=totemplparser_marshal $(srcdir)/totemplparser-marshal.list --header --body > totemplparser-marshal.c )
totemplparser-marshal.h: totemplparser-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=totemplparser_marshal $(srcdir)/totemplparser-marshal.list --header > totemplparser-marshal.h )

libtotem_plparser_la_SOURCES =			\
	totem-pl-parser-builtins.c		\
	totemplparser-marshal.c			\
	totemplparser-marshal.h			\
	totem-disc.c				\
	totem-pl-parser.c

plparserincludedir = $(includedir)/totem/1/plparser
plparserinclude_HEADERS =			\
	totem-pl-parser-builtins.h		\
	totem-pl-parser-features.h		\
	totem-pl-parser.h			\
	totem-disc.h

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

CLEANFILES = \
	totem-pl-parser-builtins.h \
	totem-pl-parser-builtins.c \
	stamp-totem-pl-parser-builtins.h \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	totemplparser-marshal.list \
	totem-pl-parser-features.h.in
