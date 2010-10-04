include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/jamendo
plugin_PYTHON = jamendo.py

plugin_in_files = jamendo.plugin.in

uidir = $(plugindir)
ui_DATA = jamendo.ui
EXTRA_DIST += $(ui_DATA)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

gsettings_SCHEMAS = org.gnome.totem.plugins.jamendo.gschema.xml
CLEANFILES += $(gsettings_SCHEMAS)

convertdir = $(datadir)/GConf/gsettings
convert_DATA = jamendo.convert
EXTRA_DIST += jamendo.convert

-include $(top_srcdir)/git.mk
