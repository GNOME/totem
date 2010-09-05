include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/opensubtitles
plugin_PYTHON = opensubtitles.py hash.py

plugin_in_files = opensubtitles.totem-plugin.in

uidir = $(plugindir)
ui_DATA = opensubtitles.ui
EXTRA_DIST += $(ui_DATA)

gsettings_SCHEMAS = org.gnome.totem.plugins.opensubtitles.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = opensubtitles.convert

EXTRA_DIST += opensubtitles.py hash.py org.gnome.totem.plugins.opensubtitles.gschema.xml.in.in opensubtitles.convert

CLEANFILES += $(gsettings_SCHEMAS)
DISTCLEANFILES += $(gsettings_SCHEMAS)
MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)

-include $(top_srcdir)/git.mk
