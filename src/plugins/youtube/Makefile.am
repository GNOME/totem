plugindir = $(PLUGINDIR)/youtube
uidir = $(plugindir)
plugin_PYTHON = youtube.py

plugin_in_files = youtube.totem-plugin.in

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)
ui_DATA = youtube.ui

EXTRA_DIST = $(plugin_in_files) $(ui_DATA) youtube.py

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)
