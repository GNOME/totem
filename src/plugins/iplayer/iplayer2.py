#!/usr/bin/python
# -*- coding: utf-8 -*-

# Python libs
import re, os
import urllib2
#import logging
#from pprint import pformat
from socket import timeout as SocketTimeoutError
#from time import time
import gettext

# external libs
import httplib2
import listparser
from BeautifulSoup import BeautifulStoneSoup

gettext.textdomain ("totem")

D_ = gettext.dgettext
_ = gettext.gettext

IMG_DIR = os.path.join (os.getcwd (), 'resources', 'media')

#try:
#    logging.basicConfig (
#        filename='iplayer2.log',
#        filemode='w',
#        format='%(asctime)s %(levelname)4s %(message)s',
#        level=logging.DEBUG
#    )
#except IOError:
#    #print "iplayer2 logging to stdout"
#    logging.basicConfig (
#        stream=sys.stdout,
#        level=logging.DEBUG,
#        format='iplayer2.py: %(asctime)s %(levelname)4s %(message)s',
#    )
# me want 2.5!!!
def any (iterable):
    for element in iterable:
        if element:
            return True
    return False


# http://colinm.org/blog/on-demand-loading-of-flickr-photo-metadata
# returns immediately for all previously-called functions
def call_once (function):
    called_by = {}
    def result (self):
        if self in called_by:
            return
        called_by[self] = True
        function (self)
    return result

# runs loader before decorated function
def loaded_by (loader):
    def decorator (function):
        def result (self, *args, **kwargs):
            loader (self)
            return function (self, *args, **kwargs)
        return result
    return decorator

CHANNELS_TV_LIST = [
    ('bbc_one', 'BBC One'),
    ('bbc_two', 'BBC Two'),
    ('bbc_three', 'BBC Three'),
    ('bbc_four', 'BBC Four'),
    ('cbbc', 'CBBC'),
    ('cbeebies', 'CBeebies'),
    ('bbc_news24', 'BBC News Channel'),
    ('bbc_parliament', 'BBC Parliament'),
    ('bbc_hd', 'BBC HD'),
    ('bbc_alba', 'BBC Alba'),
]
CHANNELS_TV = dict (CHANNELS_TV_LIST)
CHANNELS_NATIONAL_RADIO_LIST = [
    ('bbc_radio_one', 'Radio 1'),
    ('bbc_1xtra', '1 Xtra'),
    ('bbc_radio_two', 'Radio 2'),
    ('bbc_radio_three', 'Radio 3'),
    ('bbc_radio_four', 'Radio 4'),
    ('bbc_radio_five_live', '5 Live'),
    ('bbc_radio_five_live_sports_extra', '5 Live Sports Extra'),
    ('bbc_6music', '6 Music'),
    ('bbc_7', 'BBC 7'),
    ('bbc_asian_network', 'Asian Network'),
    ('bbc_radio_scotland', 'BBC Scotland'),
    ('bbc_radio_ulster', 'BBC Ulster'),
    ('bbc_radio_foyle', 'Radio Foyle'),
    ('bbc_radio_wales', 'BBC Wales'),
    ('bbc_radio_cymru', 'BBC Cymru'),
    ('bbc_world_service', 'World Service'),
    ('bbc_radio_nan_gaidheal', 'BBC nan Gaidheal')
]
CHANNELS_LOCAL_RADIO_LIST = [
    ('bbc_radio_cumbria', 'BBC Cumbria'),
    ('bbc_radio_newcastle', 'BBC Newcastle'),
    ('bbc_tees', 'BBC Tees'),
    ('bbc_radio_lancashire', 'BBC Lancashire'),
    ('bbc_radio_merseyside', 'BBC Merseyside'),
    ('bbc_radio_manchester', 'BBC Manchester'),
    ('bbc_radio_leeds', 'BBC Leeds'),
    ('bbc_radio_sheffield', 'BBC Sheffield'),
    ('bbc_radio_york', 'BBC York'),
    ('bbc_radio_humberside', 'BBC Humberside'),
    ('bbc_radio_lincolnshire', 'BBC Lincolnshire'),
    ('bbc_radio_nottingham', 'BBC Nottingham'),
    ('bbc_radio_leicester', 'BBC Leicester'),
    ('bbc_radio_derby', 'BBC Derby'),
    ('bbc_radio_stoke', 'BBC Stoke'),
    ('bbc_radio_shropshire', 'BBC Shropshire'),
    ('bbc_wm', 'BBC WM'),
    ('bbc_radio_coventry_warwickshire', 'BBC Coventry Warwickshire'),
    ('bbc_radio_hereford_worcester', 'BBC Hereford/Worcester'),
    ('bbc_radio_northampton', 'BBC Northampton'),
    ('bbc_three_counties_radio', 'BBC Three Counties Radio'),
    ('bbc_radio_cambridge', 'BBC Cambridge'),
    ('bbc_radio_norfolk', 'BBC Norfolk'),
    ('bbc_radio_suffolk', 'BBC Suffolk'),
    ('bbc_radio_essex', 'BBC Essex'),
    ('bbc_london', 'BBC London'),
    ('bbc_radio_kent', 'BBC Kent'),
    ('bbc_southern_counties_radio', 'BBC Southern Counties Radio'),
    ('bbc_radio_oxford', 'BBC Oxford'),
    ('bbc_radio_berkshire', 'BBC Berkshire'),
    ('bbc_radio_solent', 'BBC Solent'),
    ('bbc_radio_gloucestershire', 'BBC Gloucestershire'),
    ('bbc_radio_swindon', 'BBC Swindon'),
    ('bbc_radio_wiltshire', 'BBC Wiltshire'),
    ('bbc_radio_bristol', 'BBC Bristol'),
    ('bbc_radio_somerset_sound', 'BBC Somerset'),
    ('bbc_radio_devon', 'BBC Devon'),
    ('bbc_radio_cornwall', 'BBC Cornwall'),
    ('bbc_radio_guernsey', 'BBC Guernsey'),
    ('bbc_radio_jersey', 'BBC Jersey')
]

LOGO_URI = 'http://www.bbc.co.uk/englandcms/'
CHANNELS_LOGOS = {
    'bbc_radio_cumbria': LOGO_URI + 'localradio/images/cumbria.gif',
    'bbc_radio_newcastle': LOGO_URI + 'localradio/images/newcastle.gif',
    'bbc_tees': LOGO_URI + 'localradio/images/tees.gif',
    'bbc_radio_lancashire': LOGO_URI + 'images/rh_nav170_lancs.gif',
    'bbc_radio_merseyside': LOGO_URI + 'localradio/images/merseyside.gif',
    'bbc_radio_manchester': os.path.join (IMG_DIR, 'bbc_local_radio.png'),
    'bbc_radio_leeds': LOGO_URI + 'images/rh_nav170_leeds.gif',
    'bbc_radio_sheffield': LOGO_URI + 'images/rh_nav170_sheffield.gif',
    'bbc_radio_york': LOGO_URI + 'localradio/images/york.gif',
    'bbc_radio_humberside': 'http://www.bbc.co.uk/radio/images/home/'\
                            'r-home-nation-regions.gif',
    'bbc_radio_lincolnshire': LOGO_URI + 'localradio/images/lincs.gif',
    'bbc_radio_nottingham': os.path.join (IMG_DIR, 'bbc_local_radio.png'),
    'bbc_radio_leicester': LOGO_URI + 'localradio/images/leicester.gif',
    'bbc_radio_derby': LOGO_URI + 'derby/images/rh_nav170_derby.gif',
    'bbc_radio_stoke': LOGO_URI + 'localradio/images/stoke.gif',
    'bbc_radio_shropshire': LOGO_URI + 'localradio/images/shropshire.gif',
    'bbc_wm': os.path.join (IMG_DIR, 'bbc_local_radio.png'),
    'bbc_radio_coventry_warwickshire': LOGO_URI + 'localradio/images/'\
                                                  'cov_warks.gif',
    'bbc_radio_hereford_worcester': LOGO_URI + 'localradio/images/'\
                                               'hereford_worcester.gif',
    'bbc_radio_northampton': LOGO_URI + 'localradio/images/northampton.gif',
    'bbc_three_counties_radio': LOGO_URI + 'images/rh_nav170_3counties.gif',
    'bbc_radio_cambridge': LOGO_URI + 'localradio/images/cambridgeshire.gif',
    'bbc_radio_norfolk': LOGO_URI + 'localradio/images/norfolk.gif',
    'bbc_radio_suffolk': LOGO_URI + 'localradio/images/suffolk.gif',
    'bbc_radio_essex': LOGO_URI + 'images/rh_nav170_essex.gif',
    'bbc_london': os.path.join (IMG_DIR, 'bbc_local_radio.png'),
    'bbc_radio_kent': 'http://www.bbc.co.uk/radio/images/home/'\
                      'r-home-nation-regions.gif',
    'bbc_southern_counties_radio': os.path.join (IMG_DIR,
                                                 'bbc_local_radio.png'),
    'bbc_radio_oxford': LOGO_URI + 'images/rh_nav170_oxford.gif',
    'bbc_radio_berkshire': LOGO_URI + 'images/rh_nav170_berks.gif',
    'bbc_radio_solent': LOGO_URI + 'localradio/images/solent.gif',
    'bbc_radio_gloucestershire': LOGO_URI + 'localradio/images/'\
                                            'gloucestershire.gif',
    'bbc_radio_swindon': os.path.join (IMG_DIR, 'bbc_local_radio.png'),
    'bbc_radio_wiltshire': os.path.join (IMG_DIR, 'bbc_local_radio.png'),
    'bbc_radio_bristol': LOGO_URI + 'localradio/images/bristol.gif',
    'bbc_radio_somerset_sound': LOGO_URI + 'images/rh_nav170_somerset.gif',
    'bbc_radio_devon': LOGO_URI + 'images/rh_nav170_devon.gif',
    'bbc_radio_cornwall': LOGO_URI + 'localradio/images/cornwall.gif',
    'bbc_radio_guernsey': LOGO_URI + 'localradio/images/guernsey.gif',
    'bbc_radio_jersey': LOGO_URI + 'localradio/images/jersey.gif'
}


CHANNELS_NATIONAL_RADIO = dict (CHANNELS_NATIONAL_RADIO_LIST)
CHANNELS_LOCAL_RADIO = dict (CHANNELS_LOCAL_RADIO_LIST)
CHANNELS_RADIO_LIST = CHANNELS_NATIONAL_RADIO_LIST + CHANNELS_LOCAL_RADIO_LIST
CHANNELS_RADIO = dict (CHANNELS_RADIO_LIST)

CHANNELS = dict (CHANNELS_TV_LIST + CHANNELS_RADIO_LIST)
CATEGORIES_LIST = [
    ('childrens', 'Children\'s'),
    ('comedy', 'Comedy'),
    ('drama', 'Drama'),
    ('entertainment', 'Entertainment'),
    ('factual', 'Factual'),
    ('films', 'Films'),
    ('music', 'Music'),
    ('news', 'News'),
    ('religion_and_ethics', 'Religion & Ethics'),
    ('sport', 'Sport'),
    ('signed', 'Sign Zone'),
    ('northern_ireland', 'Northern Ireland'),
    ('scotland', 'Scotland'),
    ('wales', 'Wales')
]
CATEGORIES = dict (CATEGORIES_LIST)

ENGLAND_RADIO_URI = 'http://www.bbc.co.uk/england/'
LIVE_RADIO_STATIONS = {
    'Radio 1': 'http://www.bbc.co.uk/radio1/wm_asx/aod/radio1_hi.asx',
    '1 Xtra':  'http://www.bbc.co.uk/1xtra/realmedia/1xtra_hi.asx',
    'Radio 2': 'http://www.bbc.co.uk/radio2/wm_asx/aod/radio2_hi.asx',
    'BBC 3': 'http://www.bbc.co.uk/radio3/wm_asx/aod/radio3_hi.asx',
    'BBC 4': 'http://www.bbc.co.uk/radio4/wm_asx/aod/radio4.asx',
    '5 Live':  'http://www.bbc.co.uk/fivelive/live/live.asx',
    '5 Live Sports Extra': 'http://www.bbc.co.uk/fivelive/live/'\
                           'live_sportsextra.asx',
    '6 Music': 'http://www.bbc.co.uk/6music/ram/6music_hi.asx',
    'BBC 7':   'http://www.bbc.co.uk/bbc7/realplayer/bbc7_hi.asx',
    'Asian Network': 'http://www.bbc.co.uk/asiannetwork/rams/asiannet_hi.asx',
    'Radio Scotland': 'http://www.bbc.co.uk/scotland/radioscotland/media/'\
                      'radioscotland.ram',
    'World Service': 'http://www.bbc.co.uk/worldservice/meta/tx/nb/'\
                     'live_eneuk_au_nb.asx',
    'BBC nan Gaidheal': 'http://www.bbc.co.uk/scotland/alba/media/live/'\
                        'radio_ng.ram',
    'BBC London': ENGLAND_RADIO_URI + 'london.ram',
    'BBC Berkshire': ENGLAND_RADIO_URI + 'radioberkshire.ram',
    'BBC Bristol': ENGLAND_RADIO_URI + 'bristol.ram',
    'BBC Cambridgeshire': ENGLAND_RADIO_URI + 'cambridgeshire.ram',
    'BBC Cornwall': ENGLAND_RADIO_URI + 'cornwall.ram',
    'BBC Cumbria': ENGLAND_RADIO_URI + 'cumbria.ram',
    'BBC Derby': ENGLAND_RADIO_URI + 'derby.ram',
    'BBC Devon': ENGLAND_RADIO_URI + 'devon.ram',
    'BBC Essex': ENGLAND_RADIO_URI + 'essex.ram',
    'BBC Gloucestershire': ENGLAND_RADIO_URI + 'gloucestershire.ram',
    'BBC Guernsey': ENGLAND_RADIO_URI + 'guernsey.ram',
    'BBC Hereford/Worcester': ENGLAND_RADIO_URI + 'herefordandworcester.ram',
    'BBC Humberside': ENGLAND_RADIO_URI + 'humberside.ram',
    'BBC Jersey': ENGLAND_RADIO_URI + 'jersey.ram',
    'BBC Kent': ENGLAND_RADIO_URI + 'kent.ram',
    'BBC Lancashire': ENGLAND_RADIO_URI + 'lancashire.ram',
    'BBC Leeds': ENGLAND_RADIO_URI + 'leeds.ram',
    'BBC Leicester': ENGLAND_RADIO_URI + 'leicester.ram',
    'BBC Lincolnshire': ENGLAND_RADIO_URI + 'lincolnshire.ram',
    'BBC Manchester': ENGLAND_RADIO_URI + 'manchester.ram',
    'BBC Merseyside': ENGLAND_RADIO_URI + 'merseyside.ram',
    'BBC Newcastle': ENGLAND_RADIO_URI + 'newcastle.ram',
    'BBC Norfolk': ENGLAND_RADIO_URI + 'norfolk.ram',
    'BBC Northampton': ENGLAND_RADIO_URI + 'northampton.ram',
    'BBC Nottingham': ENGLAND_RADIO_URI + 'nottingham.ram',
    'BBC Oxford': ENGLAND_RADIO_URI + 'radiooxford.ram',
    'BBC Sheffield': ENGLAND_RADIO_URI + 'sheffield.ram',
    'BBC Shropshire': ENGLAND_RADIO_URI + 'shropshire.ram',
    'BBC Solent': ENGLAND_RADIO_URI + 'solent.ram',
    'BBC Somerset Sound': ENGLAND_RADIO_URI + 'somerset.ram',
    'BBC Southern Counties Radio': ENGLAND_RADIO_URI + 'southerncounties.ram',
    'BBC Stoke': ENGLAND_RADIO_URI + 'stoke.ram',
    'BBC Suffolk': ENGLAND_RADIO_URI + 'suffolk.ram',
    'BBC Swindon': ENGLAND_RADIO_URI + 'swindon.ram',
    'BBC Three Counties Radio': ENGLAND_RADIO_URI + 'threecounties.ram',
    'BBC Wiltshire': ENGLAND_RADIO_URI + 'wiltshire.ram',
    'BBC York': ENGLAND_RADIO_URI + 'york.ram',
    'BBC WM': ENGLAND_RADIO_URI + 'wm.ram',
    'BBC Cymru': 'http://www.bbc.co.uk/cymru/live/rc-live.ram',
    'Radio Foyle': 'http://www.bbc.co.uk/northernireland/realmedia/rf-live.ram',
    'BBC Scotland': 'http://www.bbc.co.uk/scotland/radioscotland/media/'\
                    'radioscotland.ram',
    'BBC nan Gaidheal': 'http://www.bbc.co.uk/scotland/alba/media/live/'\
                        'radio_ng.ram',
    'BBC Ulster': 'http://www.bbc.co.uk/ni/realmedia/ru-live.ram',
    'BBC Wales': 'http://www.bbc.co.uk/wales/live/rwg2.ram',
    'BBC Tees': ENGLAND_RADIO_URI + 'cleveland.ram',
}

LIVE_WEBCAMS = {
    'Radio 1': 'http://www.bbc.co.uk/radio1/webcam/images/live/webcam.jpg',
    '1 Xtra':  'http://www.bbc.co.uk/1xtra/webcam/live/1xtraa.jpg',
    'Radio 2': 'http://www.bbc.co.uk/radio2/webcam/live/radio2.jpg',
    '5 Live':  'http://www.bbc.co.uk/fivelive/inside/webcam/5Lwebcam1.jpg',
    '6 Music': 'http://www.bbc.co.uk/6music/webcam/live/6music.jpg',
    'Asian Network': 'http://www.bbc.co.uk/asiannetwork/webcams/birmingham.jpg'
}

RSS_CACHE = {}

SELF_CLOSING_TAGS = ['alternate', 'mediator']

HTTP_OBJECT = httplib2.Http ()

RE_SELF_CLOSE = re.compile ('< ([a-zA-Z0-9]+) ( ?.*)/>', re.M | re.S)

def fix_selfclosing (xml):
    return RE_SELF_CLOSE.sub ('<\\1\\2></\\1>', xml)

def set_http_cache_dir (directory):
    file_cache = httplib2.FileCache (directory)
    HTTP_OBJECT.cache = file_cache

def set_http_cache (cache):
    HTTP_OBJECT.cache = cache

class NoItemsError (Exception):
    def __init__ (self, reason=None):
        self.reason = reason

    def __str__ (self):
        reason = self.reason or _(u'<no reason given>')
        return _(u'Programme unavailable ("%s")') % (reason)

class Memoize (object):
    def __init__ (self, func):
        self.func = func
        self._cache = {}
    def __call__ (self, *args, **kwds):
        key = args
        if kwds:
            items = kwds.items ()
            items.sort ()
            key = key + tuple (items)
        if key in self._cache:
            return self._cache[key]
        self._cache[key] = result = self.func (*args, **kwds)
        return result

def httpretrieve (url, filename):
    _response, data = HTTP_OBJECT.request (url, 'GET')
    data_file = open (filename, 'wb')
    data_file.write (data)
    data_file.close ()

def httpget (url):
    _response, data = HTTP_OBJECT.request (url, 'GET')
    return data

def parse_entry_id (entry_id):
    # tag:bbc.co.uk,2008:PIPS:b00808sc
    regexp = re.compile ('PIPS: ([0-9a-z]{8})')
    matches = regexp.findall (entry_id)
    if not matches:
        return None
    return matches[0]

class Media (object):
    def __init__ (self, item, media_node):
        self.item = item
        self.href = None
        self.kind = None
        self.method = None
        self.width, self.height = None, None
        self.read_media_node (media_node)

    @property
    def url (self):
        if self.connection_method == 'resolve':
            #logging.info ("Resolving URL %s", self.connection_href)
            page = urllib2.urlopen (self.connection_href)
            page.close ()
            url = page.geturl ()
            #logging.info ("URL resolved to %s", url)
            return url
        else:
            return self.connection_href

    @property
    def application (self):
        """
        The type of stream represented as a string.
        i.e. 'captions', 'flashhigh', 'flashmed', 'flashwii', 'mobile', 'mp3'
        or 'real'
        """
        tep = {}
        tep['captions', 'application/ttaf+xml', None, 'http'] = 'captions'
        tep['video', 'video/mp4', 'h264', 'rtmp'] = 'flashhigh'
        tep['video', 'video/x-flv', 'vp6', 'rtmp'] = 'flashmed'
        tep['video', 'video/x-flv', 'spark', 'rtmp'] = 'flashwii'
        tep['video', 'video/mpeg', 'h264', 'http'] = 'mobile'
        tep['audio', 'audio/mpeg', 'mp3', 'rtmp'] = 'mp3'
        tep['audio', 'audio/real', 'real', 'http'] = 'real'
        media = (self.kind, self.mimetype, self.encoding,
                 self.connection_protocol)
        return tep.get (media, None)

    def read_media_node (self, media):
        """
        Reads media info from a media XML node
        media: media node from BeautifulStoneSoup
        """
        self.kind = media.get ('kind')
        self.mimetype = media.get ('type')
        self.encoding = media.get ('encoding')
        self.width, self.height = media.get ('width'), media.get ('height')
        self.live = media.get ('live') == 'true'

        conn = media.find ('connection')
        self.connection_kind = conn.get ('kind')
        self.connection_live = conn.get ('live') == 'true'
        self.connection_protocol = None
        self.connection_href = None
        self.connection_method = None

        if self.connection_kind in ['http', 'sis']: # http
            self.connection_href = conn.get ('href')
            self.connection_protocol = 'http'
            if self.mimetype == 'video/mp4' and self.encoding == 'h264':
                # iPhone, don't redirect or it goes to license failure page
                self.connection_method = 'iphone'
            elif self.kind == 'captions':
                self.connection_method = None
            else:
                self.connection_method = 'resolve'
        elif self.connection_kind in ['level3', 'akamai']: #rtmp
            self.connection_protocol = 'rtmp'
            server = conn.get ('server')
            identifier = conn.get ('identifier')

            if not self.connection_live:
                #logging.error ("No support for live streams!")
                auth = conn.get ('authstring')
                params = dict (ip=server, server=server, auth=auth,
                               identifier=identifier)
                self.connection_href = "rtmp://%(ip)s:1935/ondemand"\
                                       "?_fcs_vhost=%(server)s&auth=%(auth)s"\
                                       "&aifp=v001"\
                                       "&slist=%(identifier)s" % params
        #else:
        #    logging.error ("connectionkind %s unknown", self.connection_kind)

        #if self.connection_protocol:
        #    logging.info ("conn protocol: %s - conn kind: %s - media type: "\
        #                  "%s - media encoding: %s" %
        #                 (self.connection_protocol, self.connection_kind,
        #                  self.mimetype, self.encoding))
        #    logging.info ("conn href: %s", self.connection_href)

    @property
    def programme (self):
        return self.item.programme

class Item (object):
    """
    Represents an iPlayer programme item. Most programmes consist of 2 such
    items, (1) the ident, and (2) the actual programme. The item specifies the
    properties of the media available, such as whether it's a radio/TV
    programme, if it's live, signed, etc.
    """

    def __init__ (self, programme, item_node):
        """
        programme: a programme object that represents the 'parent' of this item.
        item_node: an XML &lt;item&gt; node representing this item.
        """
        self.programme = programme
        self.identifier = None
        self.service = None
        self.guidance = None
        self.masterbrand = None
        self.alternate = None
        self.duration = ''
        self.medias = None
        self.read_item_node (item_node)

    def read_item_node (self, node):
        """
        Reads the specified XML &lt;item&gt; node and sets this instance's
        properties.
        """
        self.kind = node.get ('kind')
        self.identifier = node.get ('identifier')
        #logging.info ('Found item: %s, %s', self.kind, self.identifier)
        if self.kind in ['programme', 'radioProgramme']:
            self.live = node.get ('live') == 'true'
            #self.title = node.get ('title')
            self.group = node.get ('group')
            self.duration = node.get ('duration')
            #self.broadcast = node.broadcast
            self.service = node.service and node.service.get ('id')
            self.masterbrand = node.masterbrand and node.masterbrand.get ('id')
            self.alternate = node.alternate and node.alternate.get ('id')
            self.guidance = node.guidance

    @property
    def is_radio (self):
        """ True if this stream is a radio programme. """
        return self.kind == 'radioProgramme'

    @property
    def is_tv (self):
        """ True if this stream is a TV programme. """
        return self.kind == 'programme'

    @property
    def is_ident (self):
        """ True if this stream is an ident. """
        return self.kind == 'ident'

    @property
    def is_programme (self):
        """ True if this stream is a programme (TV or Radio). """
        return self.is_radio or self.is_tv

    @property
    def is_live (self):
        """ True if this stream is being broadcast live. """
        return self.live

    @property
    def is_signed (self):
        """ True if this stream is 'signed' for the hard-of-hearing. """
        return self.alternate == 'signed'

    @property
    def mediaselector_url (self):
        url = "http://www.bbc.co.uk/mediaselector/4/mtis/stream/%s"
        return url % self.identifier

    @property
    def media (self):
        """
        Returns a list of all the media available for this item.
        """
        if self.medias:
            return self.medias
        url = self.mediaselector_url
        #logging.info ("Stream XML URL: %s", str (url))
        _response, xml = HTTP_OBJECT.request (url)
        entities = BeautifulStoneSoup.XML_ENTITIES
        soup = BeautifulStoneSoup (xml, convertEntities = entities)
        medias = [Media (self, m) for m in soup ('media')]
        #logging.info ('Found media: %s', pformat (medias, indent=8))
        self.medias = medias
        return medias

    def get_media_for (self, application):
        """
        Returns a media object for the given application type.
        """
        medias = [m for m in self.media if m.application == application]
        if not medias:
            return None
        return medias[0]

    def get_medias_for (self, applications):
        """
        Returns a dictionary of media objects for the given application types.
        """
        medias = [m for m in self.media if m.application in applications]
        dictionary = {}.fromkeys (applications)
        for media in medias:
            dictionary[media.application] = media
        return dictionary

class Programme (object):
    """
    Represents an individual iPlayer programme, as identified by an 8-letter
    PID, and contains the programme title, subtitle, broadcast time and list of
    playlist items (e.g. ident and then the actual programme.)
    """

    def __init__ (self, pid):
        self.pid = pid
        self.meta = {}
        self._items = []
        self._related = []

    @call_once
    def read_playlist (self):
        #logging.info ('Read playlist for %s...', self.pid)
        self.parse_playlist (self.playlist)

    def get_playlist_xml (self):
        """ Downloads and returns the XML for a PID from the iPlayer site. """
        try:
            url = self.playlist_url
            #logging.info ("Getting XML playlist at URL: %s", url)
            _response, xml = HTTP_OBJECT.request (url, 'GET')
            return xml
        except SocketTimeoutError:
            #logging.error ("Timed out trying to download programme XML")
            raise

    def parse_playlist (self, xml):
        #logging.info ('Parsing playlist XML... %s', xml)
        #xml.replace ('<summary/>', '<summary></summary>')
        #xml = fix_selfclosing (xml)

        entities = BeautifulStoneSoup.XML_ENTITIES
        soup = BeautifulStoneSoup (xml, selfClosingTags = SELF_CLOSING_TAGS,
                                   convertEntities = entities)

        self.meta = {}
        self._items = []
        self._related = []

        #logging.info ('  Found programme: %s', soup.playlist.title.string)
        self.meta['title'] = soup.playlist.title.string.encode ('utf-8')
        self.meta['summary'] = soup.playlist.summary.string.encode ('utf-8')
        self.meta['updated'] = soup.playlist.updated.string

        if soup.playlist.noitems:
            #logging.info ('No playlist items: %s',
            #              soup.playlist.noitems.get ('reason'))
            self.meta['reason'] = soup.playlist.noitems.get ('reason')

        self._items = [Item (self, i) for i in soup ('item')]
        #for i in self._items:
        #    print i, i.alternate , " ",
        #print

        id_regexp = re.compile ('concept_pid: ([a-z0-9]{8})')
        for link in soup ('relatedlink'):
            i = {}
            i['title'] = link.title.string
            #i['summary'] = item.summary # FIXME looks like a bug in BSS
            i['pid'] = (id_regexp.findall (link.id.string) or [None])[0]
            i['programme'] = Programme (i['pid'])
            self._related.append (i)

    def get_thumbnail (self, size='large', tvradio='tv'):
        """
        Returns the URL of a thumbnail.
        size: '640x360'/'biggest'/'largest' or '512x288'/'big'/'large' or None
        """
        url_format = "http://www.bbc.co.uk/iplayer/images/episode/%s_%d_%d.jpg"

        if size in ['640x360', '640x', 'x360', 'biggest', 'largest']:
            return url_format % (self.pid, 640, 360)
        elif size in ['512x288', '512x', 'x288', 'big', 'large']:
            return url_format % (self.pid, 512, 288)
        elif size in ['178x100', '178x', 'x100', 'small']:
            return url_format % (self.pid, 178, 100)
        elif size in ['150x84', '150x', 'x84', 'smallest']:
            return url_format % (self.pid, 150, 84)
        else:
            return os.path.join (IMG_DIR, '%s.png' % tvradio)


    def get_url (self):
        """
        Returns the programmes episode page.
        """
        return "http://www.bbc.co.uk/iplayer/episode/%s" % (self.pid)

    @property
    def playlist_url (self):
        return "http://www.bbc.co.uk/iplayer/playlist/%s" % self.pid

    @property
    def playlist (self):
        return self.get_playlist_xml ()

    def get_updated (self):
        return self.meta['updated']

    @loaded_by (read_playlist)
    def get_title (self):
        return self.meta['title']

    @loaded_by (read_playlist)
    def get_summary (self):
        return self.meta['summary']

    @loaded_by (read_playlist)
    def get_related (self):
        return self._related

    @loaded_by (read_playlist)
    def get_items (self):
        if not self._items:
            raise NoItemsError (self.meta['reason'])
        return self._items

    @property
    def programme (self):
        for i in self.items:
            if i.is_programme:
                return i
        return None

    title = property (get_title)
    summary = property (get_summary)
    updated = property (get_updated)
    thumbnail = property (get_thumbnail)
    related = property (get_related)
    items = property (get_items)

#programme = Memoize (programme)


class ProgrammeSimple (object):
    """
    Represents an individual iPlayer programme, as identified by an 8-letter
    PID, and contains the programme pid, title, subtitle etc
    """

    def __init__ (self, pid, entry):
        self.pid = pid
        self.meta = {}
        self.meta['title'] = entry.title
        self.meta['summary'] = entry.summary
        self.meta['updated'] = entry.updated
        self.categories = []
        for category in entry.categories:
            if category != 'TV':
                self.categories.append (category.rstrip ())
        self._items = []
        self._related = []

    @call_once
    def read_playlist (self):
        pass

    def get_playlist_xml (self):
        pass

    def parse_playlist (self, xml):
        pass

    def get_thumbnail (self, size='large', tvradio='tv'):
        """
        Returns the URL of a thumbnail.
        size: '640x360'/'biggest'/'largest' or '512x288'/'big'/'large' or None
        """
        url_format = "http://www.bbc.co.uk/iplayer/images/episode/%s_%d_%d.jpg"

        if size in ['640x360', '640x', 'x360', 'biggest', 'largest']:
            return url_format % (self.pid, 640, 360)
        elif size in ['512x288', '512x', 'x288', 'big', 'large']:
            return url_format % (self.pid, 512, 288)
        elif size in ['178x100', '178x', 'x100', 'small']:
            return url_format % (self.pid, 178, 100)
        elif size in ['150x84', '150x', 'x84', 'smallest']:
            return url_format % (self.pid, 150, 84)
        else:
            return os.path.join (IMG_DIR, '%s.png' % tvradio)


    def get_url (self):
        """
        Returns the programmes episode page.
        """
        return "http://www.bbc.co.uk/iplayer/episode/%s" % (self.pid)

    @property
    def playlist_url (self):
        return "http://www.bbc.co.uk/iplayer/playlist/%s" % self.pid

    @property
    def playlist (self):
        return self.get_playlist_xml ()

    def get_updated (self):
        return self.meta['updated']

    @loaded_by (read_playlist)
    def get_title (self):
        return self.meta['title']

    @loaded_by (read_playlist)
    def get_summary (self):
        return self.meta['summary']

    @loaded_by (read_playlist)
    def get_related (self):
        return self._related

    @loaded_by (read_playlist)
    def get_items (self):
        if not self._items:
            raise NoItemsError (self.meta['reason'])
        return self._items

    @property
    def programme (self):
        for i in self.items:
            if i.is_programme:
                return i
        return None

    title = property (get_title)
    summary = property (get_summary)
    updated = property (get_updated)
    thumbnail = property (get_thumbnail)
    related = property (get_related)
    items = property (get_items)


class Feed (object):
    def __init__ (self, tvradio = None, channel = None, category = None,
                  subcategory = None, atoz = None, searchterm = None):
        """
        Creates a feed for the specified channel/category/whatever.
        tvradio: type of channel - 'tv' or 'radio'. If a known channel is
                 specified, use 'auto'.
        channel: name of channel, e.g. 'bbc_one'
        category: category name, e.g. 'drama'
        subcategory: subcategory name, e.g. 'period_drama'
        atoz: A-Z listing for the specified letter
        """
        if tvradio == 'auto':
            if not channel and not searchterm:
                raise Exception, "Must specify channel or searchterm when "\
                                 "using 'auto'"
            elif channel in CHANNELS_TV:
                self.tvradio = 'tv'
            elif channel in CHANNELS_RADIO:
                self.tvradio = 'radio'
            else:
                raise Exception, "TV channel '%s' not "\
                                 "recognised." % self.channel

        elif tvradio in ['tv', 'radio']:
            self.tvradio = tvradio
        else:
            self.tvradio = None
        self.channel = channel
        self.category = category
        self.subcategory = subcategory
        self.atoz = atoz
        self.searchterm = searchterm

    def create_url (self, listing):
        """
        <channel>/['list'|'popular'|'highlights']
        'categories'/<category> (/<subcategory>) \
            (/['tv'/'radio'])/['list'|'popular'|'highlights']
        """
        assert listing in ['list', 'popular', 'highlights'], "Unknown "\
                                                             "listing type"

        if self.searchterm:
            path = ['search']
            if self.tvradio:
                path += [self.tvradio]
            path += ['?q=%s' % self.searchterm]
        elif self.channel:
            path = [self.channel]
            if self.atoz:
                path += ['atoz', self.atoz]
            if self.category:
                path += [self.category]
            path += [listing]
        elif self.category:
            path = [self.category]
            if self.atoz:
                path += ['atoz', self.atoz]
            path += [listing]
        elif self.atoz:
            path = ['atoz', self.atoz, listing]
            if self.tvradio:
                path += [self.tvradio]
        else:
            assert listing != 'list', "Can't list at tv/radio level'"
            path = [listing, self.tvradio]

        return "http://feeds.bbc.co.uk/iplayer/" + '/'.join (path)

    def get_name (self, separator=' '):
        """
        A readable title for this feed, e.g. 'BBC One' or 'TV Drama' or
        'BBC One Drama'
        separator: string to separate name parts with, defaults to ' '.
                   Use None to return a list (e.g. ['TV', 'Drama']).
        """
        path = []

        # TODO: This is not i18n-friendly whatsoever
        # if got a channel, don't need tv/radio distinction
        if self.channel:
            assert (self.channel in CHANNELS_TV or
                    self.channel in CHANNELS_RADIO), 'Unknown channel'
            #print self.tvradio
            if self.tvradio == 'tv':
                path.append (CHANNELS_TV.get (self.channel, ' (TV)'))
            else:
                path.append (CHANNELS_RADIO.get (self.channel, ' (Radio)'))
        elif self.tvradio:
            # no channel
            medium = 'TV'
            if self.tvradio == 'radio':
                medium = 'Radio'
            path.append (medium)

        if self.searchterm:
            path += ['Search results for %s' % self.searchterm]

        if self.category:
            assert self.category in CATEGORIES, 'Unknown category'
            path.append (CATEGORIES.get (self.category, ' (Category)'))

        if self.atoz:
            path.append ("beginning with %s" % self.atoz.upper ())

        if separator != None:
            return separator.join (path)
        else:
            return path

    def channels (self):
        """
        Return a list of available channels.
        """
        if self.channel:
            return None
        if self.tvradio == 'tv':
            return CHANNELS_TV
        if self.tvradio == 'radio':
            return CHANNELS_RADIO
        return None

    def channels_feed (self):
        """
        Return a list of available channels as a list of feeds.
        """
        if self.channel:
            #logging.warning ("%s doesn\'t have any channels!", self.channel)
            return None
        if self.tvradio == 'tv':
            return [Feed ('tv', channel = ch)
                    for (ch, _title) in CHANNELS_TV_LIST]
        if self.tvradio == 'radio':
            return [Feed ('radio', channel = ch)
                    for (ch, _title) in CHANNELS_RADIO_LIST]
        return None

    def subcategories (self):
        raise NotImplementedError ('Sub-categories not yet supported')

    @classmethod
    def is_atoz (self, letter):
        """
        Return False if specified letter is not a valid 'A to Z' directory
        entry. Otherwise returns the directory name.

        >>> feed.is_atoz ('a'), feed.is_atoz ('z')
        ('a', 'z')
        >>> feed.is_atoz ('0'), feed.is_atoz ('9')
        ('0-9', '0-9')
        >>> feed.is_atoz ('123'), feed.is_atoz ('abc')
        (False, False)
        >>> feed.is_atoz ('big british castle'), feed.is_atoz ('')
        (False, False)
        """
        letter = letter.lower ()
        if len (letter) != 1 and letter != '0-9':
            return False
        if letter in '0123456789':
            letter = "0-9"
        if letter not in 'abcdefghijklmnopqrstuvwxyz0-9':
            return False
        return letter

    def sub (self, *_args, **kwargs):
        """
        Clones this feed, altering the specified parameters.

        >>> feed ('tv').sub (channel='bbc_one').channel
        'bbc_one'
        >>> feed ('tv', channel='bbc_one').sub (channel='bbc_two').channel
        'bbc_two'
        >>> feed ('tv', channel='bbc_one').sub (category='drama').category
        'drama'
        >>> feed ('tv', channel='bbc_one').sub (channel=None).channel
        >>>
        """
        dictionary = self.__dict__.copy ()
        dictionary.update (kwargs)
        return Feed (**dictionary)

    def get (self, subfeed):
        """
        Returns a child/subfeed of this feed.
        child: can be channel/cat/subcat/letter, e.g. 'bbc_one'
        """
        if self.channel:
            return self.sub (category = subfeed)
        elif self.category:
            # no children: TODO support subcategories
            return None
        elif self.is_atoz (subfeed):
            return self.sub (atoz=self.is_atoz (subfeed))
        else:
            if subfeed in CHANNELS_TV:
                return Feed ('tv', channel = subfeed)
            if subfeed in CHANNELS_RADIO:
                return Feed ('radio', channel = subfeed)
        # TODO handle properly oh pants
        return None

    @classmethod
    def read_rss (self, url):
        #logging.info ('Read RSS: %s', url)
        if url not in RSS_CACHE:
            #logging.info ('Feed URL not in cache, requesting...')
            xml = httpget (url)
            progs = listparser.parse (xml)
            if not progs:
                return []
            cached_programmes = []
            for entry in progs.entries:
                pid = parse_entry_id (entry.id)
                programme = Programme (pid)
                cached_programmes.append (programme)
            #logging.info ('Found %d entries', len (d))
            RSS_CACHE[url] = cached_programmes
        #else:
        #    logging.info ('RSS found in cache')
        return RSS_CACHE[url]

    def popular (self):
        return self.read_rss (self.create_url ('popular'))

    def highlights (self):
        return self.read_rss (self.create_url ('highlights'))

    def list (self):
        return self.read_rss (self.create_url ('list'))

    def categories (self):
        # quick and dirty category extraction and count
        xml_url = self.create_url ('list')
        xml = httpget (xml_url)
        cat = re.findall ("<category .*term=\" (.*?)\"", xml)
        categories = {}
        for category in cat:
            if category != 'TV':
                if not categories.has_key (category):
                    categories[category] = 0
                categories[category] += 1
        alist = []
        category_keys = categories.keys ()
        category_keys.sort ()
        for category in category_keys:
            name = categories[category]
            category = category.replace ('&amp;', '&')
            category = category.replace ('&gt;', '>')
            category = category.replace ('&lt;', '<')
            alist.append ((category, name))
        return alist

    @property
    def is_radio (self):
        """ True if this feed is for radio. """
        return self.tvradio == 'radio'

    @property
    def is_tv (self):
        """ True if this feed is for tv. """
        return self.tvradio == 'tv'

    name = property (get_name)


TV = Feed ('tv')
RADIO = Feed ('radio')

def test ():
    tv_feed = Feed ('tv')
    print tv_feed.popular ()
    print tv_feed.channels ()
    print tv_feed.get ('bbc_one')
    print tv_feed.get ('bbc_one').list ()
    for category in tv_feed.get ('bbc_one').categories ():
        print category
    #print tv_feed.get ('bbc_one').channels ()
    #print tv_feed.categories ()
    #print tv_feed.get ('drama').list ()
    #print tv_feed.get ('drama').get_subcategory ('period').list ()

if __name__ == '__main__':
    test ()
