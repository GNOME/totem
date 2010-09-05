plugindir = $(PLUGINDIR)/iplayer
plugin_PYTHON = iplayer.py iplayer2.py listparser.py

plugin_in_files = iplayer.totem-plugin.in
plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

uidir = $(plugindir)
ui_DATA = iplayer.ui

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

EXTRA_DIST = $(plugin_in_files) $(ui_DATA) $(plugin_PYTHON)

CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

-include $(top_srcdir)/git.mk
