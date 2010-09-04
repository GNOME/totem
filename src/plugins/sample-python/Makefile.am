plugindir = $(PLUGINDIR)/sample-python
#plugin_PYTHON = sample-python.py

plugin_in_files = sample-python.totem-plugin.in

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

# Note: This isn't plugin_DATA so that we don't install the files (this is a _sample_ plugin)
plugin_data = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

EXTRA_DIST = $(plugin_in_files) sample-python.py

CLEANFILES = $(plugin_data)
DISTCLEANFILES = $(plugin_data)

-include $(top_srcdir)/git.mk
