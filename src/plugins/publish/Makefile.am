modules_flags = -export_dynamic -avoid-version -module

plugindir = $(PLUGINDIR)/publish
plugin_LTLIBRARIES = libpublish.la

uidir = $(PLUGINDIR)/publish
ui_DATA = publish-plugin.ui

plugin_in_files = publish.totem-plugin.in

%.totem-plugin: %.totem-plugin.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

plugin_DATA = $(plugin_in_files:.totem-plugin.in=.totem-plugin)

common_defines = \
	-DG_LOG_DOMAIN=\""Totem"\"			\
	$(DISABLE_DEPRECATED)

libpublish_la_SOURCES = totem-publish.c
libpublish_la_LDFLAGS = $(modules_flags)
libpublish_la_LIBADD = $(LIBEPC_LIBS)
libpublish_la_CPPFLAGS = $(common_defines)

libpublish_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(PEAS_CFLAGS)		\
	$(LIBEPC_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)		\
	$(AM_CFLAGS)		\
	-I$(top_srcdir)/	\
	-I$(top_srcdir)/src	\
	-I$(top_srcdir)/src/backend	\
	-I$(top_srcdir)/src/plugins

gsettings_SCHEMAS = org.gnome.totem.plugins.publish.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = publish.convert

EXTRA_DIST = $(plugin_in_files) $(ui_DATA) org.gnome.totem.plugins.publish.gschema.xml.in.in publish.convert

CLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS)
DISTCLEANFILES = $(plugin_DATA) $(gsettings_SCHEMAS)
MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)

-include $(top_srcdir)/git.mk
