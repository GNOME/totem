include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/sample-vala
plugin_LTLIBRARIES = libsample-vala.la

plugin_in_files = sample-vala.plugin.in

VALAFLAGS = \
	--vapidir=$(top_srcdir)/bindings/vala	\
	--pkg=totem

libsample_vala_la_SOURCES = totem-sample-vala-plugin.vala
libsample_vala_la_LDFLAGS = $(plugin_ldflags)
libsample_vala_la_CFLAGS = $(plugin_cflags)

# Override to _not_ install the test plugins
install-pluginLTLIBRARIES:
install-pluginDATA:
uninstall-pluginLTLIBRARIES:
uninstall-pluginDATA:

-include $(top_srcdir)/git.mk
