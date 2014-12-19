include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/pythonconsole
plugin_PYTHON = pythonconsole.py console.py

plugin_in_files = pythonconsole.plugin.in

gsettings_SCHEMAS = org.gnome.totem.plugins.pythonconsole.gschema.xml
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = pythonconsole.convert
EXTRA_DIST += pythonconsole.convert

-include $(top_srcdir)/git.mk
