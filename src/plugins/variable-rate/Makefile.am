include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/variable-rate
plugin_LTLIBRARIES = libvariable-rate.la

plugin_in_files = variable-rate.plugin.in

libvariable_rate_la_SOURCES =		\
	totem-variable-rate-plugin.c
libvariable_rate_la_LDFLAGS = $(plugin_ldflags)
libvariable_rate_la_CFLAGS = $(plugin_cflags)
libvariable_rate_la_LIBADD = $(plugin_libadd)

-include $(top_srcdir)/git.mk
