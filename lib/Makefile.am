noinst_LTLIBRARIES = libtotemscrsaver.la

libtotemscrsaver_la_SOURCES = \
	totem-scrsaver.c	\
	totem-scrsaver.h

libtotemscrsaver_la_CPPFLAGS = $(DISABLE_DEPRECATED)

libtotemscrsaver_la_CFLAGS = \
	$(DEPENDENCY_CFLAGS)	\
	$(WARN_CFLAGS)		\
	$(DBUS_CFLAGS)

libtotemscrsaver_la_LIBADD = \
	$(DEPENDENCY_LIBS)	\
	$(DBUS_LIBS)		\
	$(XTEST_LIBS)

-include $(top_srcdir)/git.mk
