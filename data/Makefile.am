SUBDIRS = icons appdata

EXTRA_DIST =
CLEANFILES =

# Bug reporting information script
bugreportdir = $(libexecdir)/totem
bugreport_SCRIPTS = totem-bugreport.py
EXTRA_DIST += $(bugreport_SCRIPTS)

# Man pages
man_MANS = totem.1 totem-video-thumbnailer.1
totem-video-thumbnailer.1: totem-video-thumbnailer.pod
	$(AM_V_GEN) pod2man -c "" -s 1 -q none -n totem-video-thumbnailer -r GNOME $< > $@
EXTRA_DIST += \
	$(man_MANS)			\
	totem-video-thumbnailer.pod

# UI files and images
stuffdir = $(pkgdatadir)
stuff_DATA =				\
	totem.ui			\
	playlist.ui			\
	preferences.ui			\
	properties.ui			\
	uri.ui				\
	controls.ui
EXTRA_DIST += $(stuff_DATA)

# Icons
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

# Desktop file
# org.gnome.Totem.desktop.in.in.in = original
# org.gnome.Totem.desktop.in.in = by configure, includes the X-Gnome-Bugzilla
# org.gnome.Totem.desktop.in = includes the mime-types
# org.gnome.Totem.desktop = intltool-ised
desktop_in_files = org.gnome.Totem.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

org.gnome.Totem.desktop.in: org.gnome.Totem.desktop.in.in mime-type-list.txt uri-schemes-list.txt desktop.sh
	$(AM_V_GEN) cat org.gnome.Totem.desktop.in.in | sed 's,@FULL_LIBEXECDIR@,$(FULL_LIBEXECDIR),' > $@ &&\
	$(SHELL) $(srcdir)/desktop.sh $(srcdir)/mime-type-list.txt $(srcdir)/uri-schemes-list.txt >> $@

EXTRA_DIST += desktop.sh

servicedir = $(datadir)/dbus-1/services
service_DATA = org.gnome.Totem.service

%.service: %.service.in
	$(AM_V_GEN) sed -e "s|@bindir[@]|$(bindir)|" \
	$< > $@ || rm $@

EXTRA_DIST += org.gnome.Totem.service.in

CLEANFILES += \
	$(desktop_in_files)	\
	$(desktop_DATA)		\
	$(service_DATA)

# Thumbnailer
thumbnailerdir = $(datadir)/thumbnailers
thumbnailer_in_files = totem.thumbnailer.in
thumbnailer_DATA = $(thumbnailer_in_files:.thumbnailer.in=.thumbnailer)

totem.thumbnailer: $(thumbnailer_in_files) mime-type-list.txt thumbnailer.sh
	$(AM_V_GEN)sed -e "s|\@BINDIR\@|$(bindir)|" $< > $@ &&\
	$(SHELL) $(srcdir)/thumbnailer.sh $(srcdir)/mime-type-list.txt >> $@

EXTRA_DIST += \
	$(thumbnailer_in_files) \
	thumbnailer.sh

CLEANFILES += \
	$(thumbnailer_DATA)

# Content type handling
nodist_noinst_HEADERS = totem-mime-types.h nautilus-video-mime-types.h totem-uri-schemes.h totemMimeTypes.js
totem-mime-types.h: mime-type-include.sh mime-type-list.txt mime-functions.sh
	$(AM_V_GEN) $(srcdir)/mime-type-include.sh $(srcdir)/mime-type-list.txt	> $@
nautilus-video-mime-types.h: mime-type-include.sh mime-type-list.txt mime-functions.sh
	$(AM_V_GEN) $(srcdir)/mime-type-include.sh --nautilus $(srcdir)/mime-type-list.txt > $@
totemMimeTypes.js: mime-type-imports.sh mime-type-list.txt mime-functions.sh
	$(AM_V_GEN) $(srcdir)/mime-type-imports.sh $(srcdir)/mime-type-list.txt	> $@
totem-uri-schemes.h: mime-type-include.sh mime-type-list.txt mime-functions.sh
	$(AM_V_GEN) $(srcdir)/uri-scheme-include.sh $(srcdir)/uri-schemes-list.txt > $@

EXTRA_DIST += \
	mime-type-imports.sh	\
	mime-type-include.sh	\
	mime-type-list.txt	\
	uri-scheme-include.sh	\
	uri-schemes-list.txt	\
	mime-functions.sh
CLEANFILES += \
	totem-mime-types.h		\
	totem-uri-schemes.h		\
	nautilus-video-mime-types.h	\
	totem-uri-schemes.h		\
	totemMimeTypes.js

# GSettings schemas, enum files and conversion file
gsettings_ENUM_NAMESPACE = org.gnome.totem
gsettings_ENUM_FILES = $(top_srcdir)/src/*.h $(top_srcdir)/src/backend/*.h
gsettings_SCHEMAS = org.gnome.totem.gschema.xml
CLEANFILES += $(gsettings_SCHEMAS)

@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = totem.convert
EXTRA_DIST += $(convert_DATA)

# pkg-config file for the plugin API
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = totem.pc

-include $(top_srcdir)/git.mk
