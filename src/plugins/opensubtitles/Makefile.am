plugindir = $(PLUGINDIR)/opensubtitles
uidir = $(plugindir)
plugin_PYTHON = opensubtitles.py hash.py

plugin_in_files = opensubtitles.totem-plugin.in

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)
ui_DATA = opensubtitles.ui

gsettings_SCHEMAS = org.gnome.totem.plugins.opensubtitles.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = opensubtitles.convert

EXTRA_DIST = $(plugin_in_files) $(ui_DATA) opensubtitles.py hash.py org.gnome.totem.plugins.opensubtitles.gschema.xml.in.in opensubtitles.convert

CLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS)
DISTCLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS)
MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)
