plugindir = $(PLUGINDIR)/pythonconsole
plugin_PYTHON = pythonconsole.py console.py

plugin_in_files = pythonconsole.totem-plugin.in

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

gsettings_SCHEMAS = org.gnome.totem.plugins.pythonconsole.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = pythonconsole.convert

EXTRA_DIST = $(plugin_in_files) pythonconsole.py console.py org.gnome.totem.plugins.pythonconsole.gschema.xml.in.in pythonconsole.convert

CLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS)
DISTCLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS)
MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)
