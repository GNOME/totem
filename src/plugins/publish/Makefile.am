include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/publish
plugin_LTLIBRARIES = libpublish.la

plugin_in_files = publish.totem-plugin.in

uidir = $(PLUGINDIR)/publish
ui_DATA = publish-plugin.ui
EXTRA_DIST += $(ui_DATA)

libpublish_la_SOURCES = totem-publish.c
libpublish_la_LDFLAGS = $(plugin_ldflags)
libpublish_la_LIBADD = $(LIBEPC_LIBS)
libpublish_la_CFLAGS = \
	$(plugin_cflags)	\
	$(LIBEPC_CFLAGS)

gsettings_SCHEMAS = org.gnome.totem.plugins.publish.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = publish.convert

EXTRA_DIST += org.gnome.totem.plugins.publish.gschema.xml.in.in publish.convert

CLEANFILES += $(gsettings_SCHEMAS)
DISTCLEANFILES += $(gsettings_SCHEMAS)
MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)

-include $(top_srcdir)/git.mk
