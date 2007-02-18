NULL =

BASIC_IDLS = \
	totemIBasicPlayer.idl

COMPLEX_IDLS = \
	totemIComplexPlayer.idl

GMP_IDLS = \
	totemIGMPCdromCollection.idl	\
	totemIGMPClosedCaption.idl	\
	totemIGMPControls.idl		\
	totemIGMPDVD.idl		\
	totemIGMPError.idl		\
	totemIGMPErrorItem.idl		\
	totemIGMPMediaCollection.idl	\
	totemIGMPMedia.idl		\
	totemIGMPNetwork.idl		\
	totemIGMPPlayerApplication.idl	\
	totemIGMPPlayer.idl		\
	totemIGMPPlaylistArray.idl	\
	totemIGMPPlaylistCollection.idl	\
	totemIGMPPlaylist.idl		\
	totemIGMPSettings.idl		\
	$(NULL)

NARROWSPACE_IDLS = \
	totemINarrowSpacePlayer.idl

MULLY_IDLS = \
	totemIMullYPlayer.idl

xptdir = $(libdir)/mozilla/plugins
xpt_DATA =

ALL_IDLS =

if ENABLE_BASIC_PLUGIN
ALL_IDLS += $(BASIC_IDLS)
xpt_DATA += libtotem-basic-plugin.xpt
endif

if ENABLE_GMP_PLUGIN
ALL_IDLS += $(GMP_IDLS)
xpt_DATA += libtotem-gmp-plugin.xpt
endif

if ENABLE_COMPLEX_PLUGIN
ALL_IDLS += $(COMPLEX_IDLS)
xpt_DATA += libtotem-complex-plugin.xpt
endif

if ENABLE_NARROWSPACE_PLUGIN
ALL_IDLS += $(NARROWSPACE_IDLS)
xpt_DATA += libtotem-narrowspace-plugin.xpt
endif

if ENABLE_MULLY_PLUGIN
ALL_IDLS += $(MULLY_IDLS)
xpt_DATA += libtotem-mully-plugin.xpt
endif

noinst_DATA = \
	$(ALL_IDLS:.idl=.xpt)	\
	$(ALL_IDLS:.idl=.h)

docdir = $(pkgdatadir)/doc
doc_DATA = $(ALL_IDLS:.idl=.html)

CLEANFILES = \
	$(noinst_HEADERS)	\
	$(noinst_DATA)		\
	$(xpt_DATA)		\
	$(doc_DATA)

EXTRA_DIST = \
	$(BASIC_IDLS)		\
	$(GMP_IDLS)		\
	$(COMPLEX_IDLS)		\
	$(NARROWSPACE_IDLS)	\
	$(MULLY_IDLS)

%.h: %.idl
	$(MOZILLA_XPIDL) -w -m header -I $(MOZILLA_IDLDIR) -I $(srcdir) $<

%.html: %.idl
	$(MOZILLA_XPIDL) -w -m doc -I $(MOZILLA_IDLDIR) -I $(srcdir) $<

%.java: %.idl
	$(MOZILLA_XPIDL) -w -m java -I $(MOZILLA_IDLDIR) -I $(srcdir) $<

%.xpt: %.idl
	$(MOZILLA_XPIDL) -w -m typelib -I $(MOZILLA_IDLDIR) -I $(srcdir) $<

libtotem-basic-plugin.xpt: $(BASIC_IDLS:.idl=.xpt)
	$(MOZILLA_XPT_LINK) $@ $^

libtotem-complex-plugin.xpt: $(COMPLEX_IDLS:.idl=.xpt)
	$(MOZILLA_XPT_LINK) $@ $^

libtotem-gmp-plugin.xpt: $(GMP_IDLS:.idl=.xpt)
	$(MOZILLA_XPT_LINK) $@ $^

libtotem-narrowspace-plugin.xpt: $(NARROWSPACE_IDLS:.idl=.xpt)
	$(MOZILLA_XPT_LINK) $@ $^

libtotem-mully-plugin.xpt: $(MULLY_IDLS:.idl=.xpt)
	$(MOZILLA_XPT_LINK) $@ $^

