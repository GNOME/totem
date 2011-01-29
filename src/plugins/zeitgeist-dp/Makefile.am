include $(top_srcdir)/src/plugins/Makefile.plugins

plugindir = $(PLUGINDIR)/zeitgeist-dp
plugin_LTLIBRARIES = libtotem-zeitgeist-dp-plugin.la

plugin_in_files = zeitgeist-dp.plugin.in

# here we are explicitly specifying gtk+-3.0 to use the vapi because vala still
# cannot parse the gir
VALAFLAGS = \
	--girdir=$(top_srcdir)/src	\
	--pkg Totem-1.0 --pkg Peas-1.0 --pkg gtk+-3.0 \
	--pkg zeitgeist-1.0 \
	bacon-video.vapi

libtotem_zeitgeist_dp_plugin_la_SOURCES = totem-zeitgeist-dp-plugin.vala
libtotem_zeitgeist_dp_plugin_la_LDFLAGS = $(plugin_ldflags) $(LIBZEITGEIST_LIBS)
libtotem_zeitgeist_dp_plugin_la_CFLAGS = \
	$(plugin_cflags) \
	$(LIBZEITGEIST_CFLAGS) \
	-I $(top_srcdir)/src/backend

-include $(top_srcdir)/git.mk
