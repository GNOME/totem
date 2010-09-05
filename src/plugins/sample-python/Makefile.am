include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/sample-python
plugin_PYTHON = sample-python.py

plugin_in_files = sample-python.totem-plugin.in

EXTRA_DIST += sample-python.py

# Override to _not_ install the test plugins
install-pluginPYTHON:
install-pluginDATA:

-include $(top_srcdir)/git.mk
