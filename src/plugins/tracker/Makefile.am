include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/tracker
plugin_LTLIBRARIES = libtracker.la

plugin_in_files = tracker.totem-plugin.in

libtracker_la_SOURCES = totem-tracker.c totem-tracker-widget.h totem-tracker-widget.c
libtracker_la_LDFLAGS = $(plugin_ldflags)
libtracker_la_LIBADD = $(TRACKER_LIBS)
libtracker_la_CFLAGS = \
	$(plugin_cflags)	\
	$(TRACKER_CFLAGS)	\
	$(DBUS_CFLAGS)

-include $(top_srcdir)/git.mk
