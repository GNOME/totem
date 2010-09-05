SUBDIRS = icons

bugreportdir = $(libexecdir)/totem
bugreport_SCRIPTS = totem-bugreport.py

man_MANS = totem.1 totem-video-thumbnailer.1

stuffdir = $(pkgdatadir)
stuff_DATA =				\
	filmholes.png			\
	filmholes-big-left.png		\
	filmholes-big-right.png		\
	totem.ui			\
	fullscreen.ui			\
	playlist.ui			\
	mozilla-viewer.ui		\
	properties.ui			\
	uri.ui				\
	video-list.ui

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

# totem.desktop.in.in.in = original
# totem.desktop.in.in = by configure, includes the X-Gnome-Bugzilla
# totem.desktop.in = includes the mime-types
# totem.desktop = intltool-ised

desktop_in_files = totem.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

totem.desktop.in: totem.desktop.in.in mime-type-list.txt desktop.sh
	$(AM_V_GEN) cat totem.desktop.in.in | sed 's,@FULL_LIBEXECDIR@,$(FULL_LIBEXECDIR),' > $@ &&\
	$(SHELL) $(srcdir)/desktop.sh $(srcdir)/mime-type-list.txt	\
	>> $@

nodist_noinst_HEADERS = totem-mime-types.h nautilus-audio-mime-types.h
totem-mime-types.h: mime-type-include.sh mime-type-list.txt mime-functions.sh
	$(AM_V_GEN) $(srcdir)/mime-type-include.sh $(srcdir)/mime-type-list.txt	\
	> $@
nautilus-audio-mime-types.h: mime-type-include.sh mime-type-list.txt mime-functions.sh
	$(AM_V_GEN) $(srcdir)/mime-type-include.sh --nautilus $(srcdir)/mime-type-list.txt	\
	> $@

gsettings_ENUM_NAMESPACE = org.gnome.totem
gsettings_ENUM_FILES = $(top_srcdir)/src/*.h $(top_srcdir)/src/backend/*.h
gsettings_SCHEMAS = org.gnome.totem.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = totem.convert

# Man pages
totem-video-thumbnailer.1: totem-video-thumbnailer.pod
	$(AM_V_GEN) pod2man -c "" -s 1 -q none -n totem-video-thumbnailer -r GNOME $< > $@

# pkg-config file for the plugin API
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = totem.pc

EXTRA_DIST = \
	$(man_MANS)			\
	$(stuff_DATA)			\
	$(bugreport_SCRIPTS)		\
	desktop.sh			\
	mime-type-include.sh		\
	mime-type-list.txt		\
	mime-functions.sh		\
	totem.convert			\
	totem-video-thumbnailer.1	\
	totem-video-thumbnailer.pod

CLEANFILES = \
	*.bak *.uip core* *.orig *~ 	\
	$(desktop_in_files)		\
	$(desktop_DATA)			\
	$(gsettings_SCHEMAS)		\
	totem-mime-types.h		\
	nautilus-audio-mime-types.h

-include $(top_srcdir)/git.mk
