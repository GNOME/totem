include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/iplayer
plugin_PYTHON = iplayer.py iplayer2.py listparser.py

plugin_in_files = iplayer.totem-plugin.in

uidir = $(plugindir)
ui_DATA = iplayer.ui
EXTRA_DIST += $(ui_DATA)

EXTRA_DIST += $(plugin_PYTHON)

-include $(top_srcdir)/git.mk
