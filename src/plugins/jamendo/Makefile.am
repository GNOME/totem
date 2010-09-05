include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/jamendo
plugin_PYTHON = jamendo.py

plugin_in_files = jamendo.totem-plugin.in

uidir = $(plugindir)
ui_DATA = jamendo.ui
EXTRA_DIST += $(ui_DATA)

gsettings_SCHEMAS = org.gnome.totem.plugins.jamendo.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = jamendo.convert

EXTRA_DIST += jamendo.py org.gnome.totem.plugins.jamendo.gschema.xml.in.in jamendo.convert

CLEANFILES += $(gsettings_SCHEMAS)
DISTCLEANFILES += $(gsettings_SCHEMAS)
MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)

-include $(top_srcdir)/git.mk
