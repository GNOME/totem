include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/publish
plugin_LTLIBRARIES = libpublish.la

plugin_in_files = publish.plugin.in

uidir = $(PLUGINDIR)/publish
ui_DATA = publish-plugin.ui
EXTRA_DIST += $(ui_DATA)

libpublish_la_SOURCES = totem-publish.c
libpublish_la_LDFLAGS = $(plugin_ldflags)
libpublish_la_LIBADD = $(LIBEPC_LIBS)
libpublish_la_CFLAGS = \
	$(plugin_cflags)	\
	$(LIBEPC_CFLAGS)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

gsettings_SCHEMAS = org.gnome.totem.plugins.publish.gschema.xml
CLEANFILES += $(gsettings_SCHEMAS)

convertdir = $(datadir)/GConf/gsettings
convert_DATA = publish.convert
EXTRA_DIST += publish.convert

-include $(top_srcdir)/git.mk
