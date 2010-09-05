include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/pythonconsole
plugin_PYTHON = pythonconsole.py console.py

plugin_in_files = pythonconsole.totem-plugin.in

gsettings_SCHEMAS = org.gnome.totem.plugins.pythonconsole.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = pythonconsole.convert

EXTRA_DIST += pythonconsole.py console.py org.gnome.totem.plugins.pythonconsole.gschema.xml.in.in pythonconsole.convert

CLEANFILES += $(gsettings_SCHEMAS)
DISTCLEANFILES += $(gsettings_SCHEMAS)
MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)

-include $(top_srcdir)/git.mk
