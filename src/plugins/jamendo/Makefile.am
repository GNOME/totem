plugindir = $(PLUGINDIR)/jamendo
uidir = $(plugindir)
plugin_PYTHON = jamendo.py
plugin_in_files = jamendo.totem-plugin.in

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)
ui_DATA = jamendo.ui

gsettings_SCHEMAS = org.gnome.totem.plugins.jamendo.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = jamendo.convert

EXTRA_DIST = $(plugin_in_files) $(ui_DATA) jamendo.py org.gnome.totem.plugins.jamendo.gschema.xml.in.in jamendo.convert

CLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS)
DISTCLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS)
MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)
