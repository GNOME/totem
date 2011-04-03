noinst_LTLIBRARIES = libtotemgsthelpers.la

libtotemgsthelpers_la_SOURCES =				\
	totem-gst-helpers.c				\
	totem-gst-helpers.h

libtotemgsthelpers_la_CPPFLAGS = \
	-D_REENTRANT				\
	$(DISABLE_DEPRECATED)			\
	$(AM_CPPFLAGS)

libtotemgsthelpers_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)		\
	$(MM_CFLAGS)			\
	$(MISSING_PLUGINS_CFLAGS)	\
	$(AM_CFLAGS)

libtotemgsthelpers_la_LIBADD = \
	$(MM_LIBS)		\
	$(GST_LIBS)		\
	$(EXTRA_BACKEND_LIBS)	\
	$(X_LIBS)		\
	$(MISSING_PLUGINS_LIBS)	\
	-lm

libtotemgsthelpers_la_LDFLAGS= \
	-no-undefined

-include $(top_srcdir)/git.mk
