# -*- coding: utf-8 -*-

import xmlrpc.client
import threading
import zlib
from os import sep
import os
import gettext

import gi
gi.require_version('Peas', '1.0')
gi.require_version('Gtk', '3.0')
gi.require_version('Totem', '1.0')
from gi.repository import GLib, GObject # pylint: disable=wrong-import-position
from gi.repository import Peas, Gtk, Gdk # pylint: disable=wrong-import-position,
from gi.repository import Gio, Pango, Totem # pylint: disable=wrong-import-position,no-name-in-module

from hash import hash_file # pylint: disable=wrong-import-position

gettext.textdomain ("totem")

D_ = gettext.dgettext
_ = gettext.gettext

USER_AGENT = 'Totem'
OK200 = '200 OK'
TOTEM_REMOTE_COMMAND_REPLACE = 14

SUBTITLES_EXT = [
    "asc",
    "txt",
    "sub",
    "vtt",
    "srt",
    "smi",
    "ssa",
    "ass",
    "mpl",
]

# Map of the language codes used by opensubtitles.org's API to their
# human-readable name
LANGUAGES_STR = [ (D_('iso_639_3', 'Albanian'), 'sq'),
         (D_('iso_639_3', 'Arabic'), 'ar'),
         (D_('iso_639_3', 'Armenian'), 'hy'),
         (D_('iso_639_3', 'Neo-Aramaic, Assyrian'), 'ay'),
         (D_('iso_639_3', 'Basque'), 'eu'),
         (D_('iso_639_3', 'Bosnian'), 'bs'),
         (_('Brazilian Portuguese'), 'pb'),
         (D_('iso_639_3', 'Bulgarian'), 'bg'),
         (D_('iso_639_3', 'Catalan'), 'ca'),
         (D_('iso_639_3', 'Chinese'), 'zh'),
         (D_('iso_639_3', 'Croatian'), 'hr'),
         (D_('iso_639_3', 'Czech'), 'cs'),
         (D_('iso_639_3', 'Danish'), 'da'),
         (D_('iso_639_3', 'Dutch'), 'nl'),
         (D_('iso_639_3', 'English'), 'en'),
         (D_('iso_639_3', 'Esperanto'), 'eo'),
         (D_('iso_639_3', 'Estonian'), 'et'),
         (D_('iso_639_3', 'Finnish'), 'fi'),
         (D_('iso_639_3', 'French'), 'fr'),
         (D_('iso_639_3', 'Galician'), 'gl'),
         (D_('iso_639_3', 'Georgian'), 'ka'),
         (D_('iso_639_3', 'German'), 'de'),
         (D_('iso_639_3', 'Greek, Modern (1453-)'), 'el'),
         (D_('iso_639_3', 'Hebrew'), 'he'),
         (D_('iso_639_3', 'Hindi'), 'hi'),
         (D_('iso_639_3', 'Hungarian'), 'hu'),
         (D_('iso_639_3', 'Icelandic'), 'is'),
         (D_('iso_639_3', 'Indonesian'), 'id'),
         (D_('iso_639_3', 'Italian'), 'it'),
         (D_('iso_639_3', 'Japanese'), 'ja'),
         (D_('iso_639_3', 'Kazakh'), 'kk'),
         (D_('iso_639_3', 'Korean'), 'ko'),
         (D_('iso_639_3', 'Latvian'), 'lv'),
         (D_('iso_639_3', 'Lithuanian'), 'lt'),
         (D_('iso_639_3', 'Luxembourgish'), 'lb'),
         (D_('iso_639_3', 'Macedonian'), 'mk'),
         (D_('iso_639_3', 'Malay (macrolanguage)'), 'ms'),
         (D_('iso_639_3', 'Norwegian'), 'no'),
         (D_('iso_639_3', 'Occitan (post 1500)'), 'oc'),
         (D_('iso_639_3', 'Persian'), 'fa'),
         (D_('iso_639_3', 'Polish'), 'pl'),
         (D_('iso_639_3', 'Portuguese'), 'pt'),
         (D_('iso_639_3', 'Romanian'), 'ro'),
         (D_('iso_639_3', 'Russian'), 'ru'),
         (D_('iso_639_3', 'Serbian'), 'sr'),
         (D_('iso_639_3', 'Slovak'), 'sk'),
         (D_('iso_639_3', 'Slovenian'), 'sl'),
         (D_('iso_639_3', 'Spanish'), 'es'),
         (D_('iso_639_3', 'Swedish'), 'sv'),
         (D_('iso_639_3', 'Thai'), 'th'),
         (D_('iso_639_3', 'Turkish'), 'tr'),
         (D_('iso_639_3', 'Ukrainian'), 'uk'),
         (D_('iso_639_3', 'Vietnamese'), 'vi'),]

# Map of ISO 639-1 language codes to the codes used by opensubtitles.org's API
LANGUAGES = {'sq':'alb',
         'ar':'ara',
         'hy':'arm',
         'ay':'ass',
         'bs':'bos',
         'pb':'pob',
         'bg':'bul',
         'ca':'cat',
         'zh':'chi',
         'hr':'hrv',
         'cs':'cze',
         'da':'dan',
         'nl':'dut',
         'en':'eng',
         'eo':'epo',
         'eu':'eus',
         'et':'est',
         'fi':'fin',
         'fr':'fre',
         'gl':'glg',
         'ka':'geo',
         'de':'ger',
         'el':'ell',
         'he':'heb',
         'hi':'hin',
         'hu':'hun',
         'is':'ice',
         'id':'ind',
         'it':'ita',
         'ja':'jpn',
         'kk':'kaz',
         'ko':'kor',
         'lv':'lav',
         'lt':'lit',
         'lb':'ltz',
         'mk':'mac',
         'ms':'may',
         'no':'nor',
         'oc':'oci',
         'fa':'per',
         'pl':'pol',
         'pt':'por',
         'ro':'rum',
         'ru':'rus',
         'sr':'scc',
         'sk':'slo',
         'sl':'slv',
         'es':'spa',
         'sv':'swe',
         'th':'tha',
         'tr':'tur',
         'uk':'ukr',
         'vi':'vie',}

def _cache_subtitles_dir ():
    bpath = GLib.get_user_cache_dir() + sep
    bpath += 'totem' + sep + 'subtitles' + sep
    return bpath

class SearchThread (threading.Thread):
    """
    This is the thread started when the dialog is searching for subtitles
    """
    def __init__ (self, model, movie_hash, movie_size, movie_title):
        self._model = model
        self._movie_hash = movie_hash
        self._movie_size = movie_size
        self._movie_title = movie_title
        self._done = False
        self._results = []
        self._lock = threading.Lock ()
        self._message = ''
        threading.Thread.__init__ (self)

    def run (self):
        self._lock.acquire (True)
        (self._results,
         self._message) = self._model.search_subtitles (self._movie_hash,
                                                        self._movie_size,
                                                        self._movie_title)
        self._done = True
        self._lock.release ()

    def get_results (self):
        results = []

        self._lock.acquire (True)
        if self._done:
            results = self._results
        self._lock.release ()

        return results

    def get_message (self):
        message = _(u'Searching for subtitles…')

        self._lock.acquire (True)
        if self._done:
            message = self._message
        self._lock.release ()

        return message

    @property
    def done (self):
        """ Thread-safe property to know whether the query is done or not """
        self._lock.acquire (True)
        res = self._done
        self._lock.release ()
        return res

class DownloadThread (threading.Thread):
    """
    This is the thread started when the dialog is downloading the subtitles.
    """
    def __init__ (self, model, subtitle_id):
        self._model = model
        self._subtitle_id = subtitle_id
        self._done = False
        self._lock = threading.Lock ()
        self._subtitles = ''
        self._message = ''
        threading.Thread.__init__ (self)

    def run (self):
        self._lock.acquire (True)
        (self._subtitles,
         self._message) = self._model.download_subtitles (self._subtitle_id)
        self._done = True
        self._lock.release ()

    def get_subtitles (self):
        subtitles = ''

        self._lock.acquire (True)
        if self._done:
            subtitles = self._subtitles
        self._lock.release ()

        return subtitles

    def get_message (self):
        message = _(u'Downloading the subtitles…')

        self._lock.acquire (True)
        if self._done:
            message = self._message
        self._lock.release ()

        return message

    @property
    def done (self):
        """ Thread-safe property to know whether the query is done or not """
        self._lock.acquire (True)
        res = self._done
        self._lock.release ()
        return res

# OpenSubtitles.org API abstraction

class OpenSubtitlesModel:
    """
    This contains the logic of the opensubtitles service.
    """
    def __init__ (self, server):
        self._server = server
        self._token = None

        try:
            import locale # pylint: disable=import-outside-toplevel
            (language_code, _) = locale.getlocale ()
            self.lang = LANGUAGES[language_code.split ('_')[0]]
        except (ImportError, IndexError, AttributeError, KeyError):
            self.lang = 'eng'

        self._lock = threading.Lock ()

    def _log_in (self, username='', password=''):
        """
        Non-locked version of log_in() for internal use only.

        @rtype : (bool, string)
        """

        result = None

        if self._token:
            # We have already logged-in before, check the connection
            try:
                result = self._server.NoOperation (self._token)
            except (xmlrpc.client.Fault, xmlrpc.client.ProtocolError):
                pass
            if result and result['status'] != OK200:
                return (True, '')

        try:
            result = self._server.LogIn (username, password, self.lang,
                                         USER_AGENT)
        except (xmlrpc.client.Fault, xmlrpc.client.ProtocolError):
            pass

        if result and result.get ('status') == OK200:
            self._token = result.get ('token')
            if self._token:
                return (True, '')

        return (False, _(u'Could not contact the OpenSubtitles website'))

    def log_in (self, username='', password=''):
        """
        Logs into the opensubtitles web service and gets a valid token for
        the comming comunications. If we are already logged it only checks
        the if the token is still valid. It returns a tuple of success boolean
        and error message (if appropriate).

        @rtype : (bool, string)
        """

        self._lock.acquire (True)
        result = self._log_in (username, password)
        self._lock.release ()

        return result

    def search_subtitles (self, movie_hash, movie_size, movie_title):
        self._lock.acquire (True)

        message = ''

        (log_in_success, log_in_message) = self._log_in ()

        if log_in_success:
            searchdata = {'sublanguageid': self.lang,
                          'moviehash'    : movie_hash,
                          'moviebytesize': str (movie_size)}
            try:
                result = self._server.SearchSubtitles (self._token,
                                                       [searchdata])
            except xmlrpc.client.ProtocolError:
                message = _(u'Could not contact the OpenSubtitles website.')

            if result.get ('data'):
                self._lock.release ()
                return (result['data'], message)

            """Not found by movie_hash and moviebytesize, fallback to title"""
            searchdata = {'sublanguageid': self.lang,
                          'query'   : movie_title}
            try:
                result = self._server.SearchSubtitles (self._token,
                                                       [searchdata])
            except xmlrpc.client.ProtocolError:
                message = _(u'Could not contact the OpenSubtitles website.')

            if result.get ('data'):
                self._lock.release ()
                return (result['data'], message)

            message = _(u'No results found.')
        else:
            message = log_in_message

        self._lock.release ()

        return (None, message)

    def download_subtitles (self, subtitle_id):
        self._lock.acquire (True)

        message = ''
        error_message = _(u'Could not contact the OpenSubtitles website.')

        (log_in_success, log_in_message) = self._log_in ()

        if log_in_success:
            result = None
            try:
                result = self._server.DownloadSubtitles (self._token,
                                                         [subtitle_id])
            except xmlrpc.client.ProtocolError:
                message = error_message

            if result and result.get ('status') == OK200:
                try:
                    subtitle64 = result['data'][0]['data']
                except LookupError:
                    self._lock.release ()
                    return (None, error_message)

                subtitle_unzipped = zlib.decompress(GLib.base64_decode (subtitle64), 47)

                self._lock.release ()

                return (subtitle_unzipped, message)
        else:
            message = log_in_message

        self._lock.release ()

        return (None, message)

class OpenSubtitles (GObject.Object, # pylint: disable=R0902
                     Peas.Activatable):
    __gtype_name__ = 'OpenSubtitles'

    object = GObject.Property (type = GObject.Object)

    def __init__ (self):
        GObject.Object.__init__ (self)

        self._dialog = None
        self._totem = None
        schema = 'org.gnome.totem.plugins.opensubtitles'
        self._settings = Gio.Settings.new (schema)

        self._action = None

        self._find_button = None
        self._apply_button = None
        self._close_button = None

        self._list_store = None
        self._model = None
        self._tree_view = None

        self._filename = None
        self._progress = None

    # totem.Plugin methods

    def do_activate (self):
        """
        Called when the plugin is activated.
        Here the sidebar page is initialized (set up the treeview, connect
        the callbacks, ...) and added to totem.
        """
        self._totem = self.object

        # Name of the movie file which the most-recently-downloaded subtitles
        # are related to.
        self._filename = None

        self._append_menu ()

        self._totem.connect ('file-opened', self.__on_totem__file_opened) # pylint: disable=no-member
        self._totem.connect ('file-closed', self.__on_totem__file_closed) # pylint: disable=no-member

        # Obtain the ServerProxy and init the model
        server = xmlrpc.client.Server ('http://api.opensubtitles.org/xml-rpc')
        self._model = OpenSubtitlesModel (server)

        mrl = self._totem.get_current_mrl ()
        if mrl:
            self.__on_totem__file_opened(self._totem, self._totem.get_current_mrl ())

    def do_deactivate (self):
        if self._dialog:
            self._dialog.destroy ()
        self._dialog = None

        self._delete_menu ()

    # UI related code

    def _build_dialog (self):
        ui_file_path = os.path.dirname (os.path.abspath (__file__)) + sep + 'opensubtitles.ui'
        builder = Gtk.Builder.new_from_file (ui_file_path)

        # Obtain all the widgets we need to initialize
        combobox = builder.get_object ('language_combobox')
        languages = builder.get_object ('language_model')
        self._progress = builder.get_object ('progress_bar')
        self._tree_view = builder.get_object ('subtitle_treeview')
        self._list_store = builder.get_object ('subtitle_model')
        self._dialog = builder.get_object ('subtitles_dialog')
        self._find_button = builder.get_object ('find_button')
        self._apply_button = builder.get_object ('apply_button')
        self._close_button = builder.get_object ('close_button')

        # Set up and populate the languages combobox
        renderer = Gtk.CellRendererText ()
        sorted_languages = Gtk.TreeModelSort (model = languages)
        sorted_languages.set_sort_column_id (0, Gtk.SortType.ASCENDING)
        combobox.set_model (sorted_languages)
        combobox.pack_start (renderer, True)
        combobox.add_attribute (renderer, 'text', 0)

        lang = self._settings.get_string ('language')
        if lang is not None:
            self._model.lang = lang

        for lang in LANGUAGES_STR:
            itera = languages.append (lang)
            if LANGUAGES[lang[1]] == self._model.lang:
                (success,
                 parentit) = sorted_languages.convert_child_iter_to_iter (itera)
                if success:
                    combobox.set_active_iter (parentit)

        # Set up the results treeview
        renderer = Gtk.CellRendererText ()
        self._tree_view.set_model (self._list_store)
        renderer.set_property ('ellipsize', Pango.EllipsizeMode.END)
        column = Gtk.TreeViewColumn (_(u"Subtitles"), renderer, text=0)
        column.set_resizable (True)
        column.set_expand (True)
        self._tree_view.append_column (column)
        # translators comment:
        # This is the file-type of the subtitle file detected
        column = Gtk.TreeViewColumn (_(u"Format"), renderer, text=1)
        column.set_resizable (False)
        self._tree_view.append_column (column)
        # translators comment:
        # This is a rating of the quality of the subtitle
        column = Gtk.TreeViewColumn (_(u"Rating"), renderer, text=2)
        column.set_resizable (False)
        self._tree_view.append_column (column)

        self._apply_button.set_sensitive (False)

        self._apply_button.connect ('clicked', self.__on_apply_clicked)
        self._find_button.connect ('clicked', self.__on_find_clicked)
        self._close_button.connect ('clicked', self.__on_close_clicked)

        # Set up signals

        combobox.connect ('changed', self.__on_combobox__changed)
        self._dialog.set_transient_for (self._totem.get_main_window ()) # pylint: disable=no-member
        self._dialog.set_position (Gtk.WindowPosition.CENTER_ON_PARENT)

        # Connect the callbacks
        self._dialog.connect ('key-press-event',
                             self.__on_window__key_press_event)
        self._tree_view.get_selection ().connect ('changed',
                                                self.__on_treeview__row_change)
        self._tree_view.connect ('row-activated',
                               self.__on_treeview__row_activate)

    def _show_dialog (self, *_):
        if not self._dialog:
            self._build_dialog ()

        self._dialog.show_all ()

        self._progress.set_fraction (0.0)

    def _append_menu (self):
        self._action = Gio.SimpleAction.new ("opensubtitles", None)
        self._action.connect ('activate', self._show_dialog)
        self._totem.add_action (self._action) # pylint: disable=no-member
        self._totem.set_accels_for_action ("app.opensubtitles",  # pylint: disable=no-member
                                           ["<Primary><Shift>s"])

        menu = self._totem.get_menu_section ("subtitle-download-placeholder") # pylint: disable=no-member
        menu.append (_(u'_Download Movie Subtitles…'), "app.opensubtitles")

        self._action.set_enabled (self._totem.is_playing () and # pylint: disable=no-member
                  self._check_allowed_scheme () and
                                  not self._check_is_audio ())

    def _check_allowed_scheme (self):
        current_file = Gio.file_new_for_uri (self._totem.get_current_mrl ()) # pylint: disable=no-member
        scheme = current_file.get_uri_scheme ()

        if scheme in ('dvd', 'http', 'dvb', 'vcd'):
            return False

        return True

    def _check_is_audio (self):
        # FIXME need to use something else here
        # I think we must use video widget metadata but I don't found a way
        # to get this info from python
        filename = self._totem.get_current_mrl () # pylint: disable=no-member
        if Gio.content_type_guess (filename, '')[0].split ('/')[0] == 'audio':
            return True
        return False

    def _delete_menu (self):
        self._totem.empty_menu_section ("subtitle-download-placeholder") # pylint: disable=no-member

    def _get_results (self, movie_hash, movie_size, movie_title):
        self._list_store.clear ()
        self._apply_button.set_sensitive (False)
        self._find_button.set_sensitive (False)

        cursor = Gdk.Cursor.new (Gdk.CursorType.WATCH)
        self._dialog.get_window ().set_cursor (cursor)

        thread = SearchThread (self._model, movie_hash, movie_size, movie_title)
        thread.start ()
        GLib.idle_add (self._populate_treeview, thread)

        self._progress.set_text (_(u'Searching subtitles…'))
        GLib.timeout_add (350, self._progress_bar_increment, thread)

    def _populate_treeview (self, search_thread):
        if not search_thread.done:
            return True

        results = search_thread.get_results ()
        if results:
            for sub_data in results:
                if not SUBTITLES_EXT.count (sub_data['SubFormat']):
                    continue
                self._list_store.append ([sub_data['SubFileName'],
                                        sub_data['SubFormat'],
                                        sub_data['SubRating'],
                                        sub_data['IDSubtitleFile'],])

        self._dialog.get_window ().set_cursor (None)

        return False

    def _save_selected_subtitle (self):
        cursor = Gdk.Cursor.new (Gdk.CursorType.WATCH)
        self._dialog.get_window ().set_cursor (cursor)

        model, rows = self._tree_view.get_selection ().get_selected_rows ()
        if rows:
            subtitle_iter = model.get_iter (rows[0])
            subtitle_id = model.get_value (subtitle_iter, 3)
            subtitle_format = model.get_value (subtitle_iter, 1)

            bpath = _cache_subtitles_dir()

            directory = Gio.file_new_for_path (bpath)
            try:
                directory.make_directory_with_parents (None)
            except: # pylint: disable=bare-except
                pass

            thread = DownloadThread (self._model, subtitle_id)
            thread.start ()
            GLib.idle_add (self._save_subtitles, thread, subtitle_format)

            self._progress.set_text (_(u'Downloading the subtitles…'))
            GLib.timeout_add (350, self._progress_bar_increment, thread)
        else:
            #warn user!
            pass

    def _movie_dir (self):
        directory = Gio.file_new_for_uri (self._filename)
        parent = directory.get_parent()
        return parent.get_path ()

    def _save_subtitles (self, download_thread, extension):
        if not download_thread.done:
            return True

        subtitles = download_thread.get_subtitles ()
        suburi = None
        if subtitles:
            subtitle_file = Gio.file_new_for_uri (self._filename)
            movie_name = subtitle_file.get_basename ().rpartition ('.')[0]

            # Delete all previous cached subtitle for this file
            for ext in SUBTITLES_EXT:
                # In the cache dir
                old_subtitle_file = Gio.file_new_for_path (_cache_subtitles_dir() +
                        sep + movie_name + '.' + ext)
                try:
                    old_subtitle_file.delete (None)
                except: # pylint: disable=bare-except
                    pass

                # In the movie dir
                old_subtitle_file = Gio.file_new_for_path (self._movie_dir() +
                        sep + movie_name + '.' + ext)
                try:
                    old_subtitle_file.delete (None)
                except: # pylint: disable=bare-except
                    pass

            flags = Gio.FileCreateFlags.REPLACE_DESTINATION
            try:
                subtitle_file = Gio.file_new_for_path (self._movie_dir() +
                        sep + movie_name + '.' + extension)
                suburi = subtitle_file.get_uri ()

                sub_file = subtitle_file.replace ('', False, flags, None)
                sub_file.write (subtitles, None)
                sub_file.close (None)
            except: # pylint: disable=bare-except
                subtitle_file = Gio.file_new_for_path (_cache_subtitles_dir() +
                        sep + movie_name + '.' + extension)
                suburi = subtitle_file.get_uri ()

                sub_file = subtitle_file.replace ('', False, flags, None)
                sub_file.write (subtitles, None)
                sub_file.close (None)

        self._dialog.get_window ().set_cursor (None)
        self._close_dialog ()

        if suburi:
            self._totem.set_current_subtitle (suburi) # pylint: disable=no-member

        return False

    def _progress_bar_increment (self, thread):
        if not thread.done:
            self._progress.pulse ()
            return True

        message = thread.get_message ()
        if message:
            self._progress.set_text (message)
        else:
            self._progress.set_text (' ')

        self._progress.set_fraction (0.0)
        self._find_button.set_sensitive (True)
        self._apply_button.set_sensitive (False)
        self._tree_view.set_sensitive (True)
        return False

    def _download_and_apply (self):
        self._apply_button.set_sensitive (False)
        self._find_button.set_sensitive (False)
        self._action.set_enabled (False)
        self._tree_view.set_sensitive (False)
        self._save_selected_subtitle ()

    def _close_dialog (self):
        # We hide the dialogue instead of closing it so that we still have the
        # last set of search results up if we re-open the dialogue without
        # changing the movie
        self._dialog.hide ()

    # Callbacks

    def __on_window__key_press_event (self, _, event):
        if event.keyval == Gdk.KEY_Escape:
            self._close_dialog ()
            return True
        return False

    def __on_treeview__row_change (self, selection):
        if selection.count_selected_rows () > 0:
            self._apply_button.set_sensitive (True)
        else:
            self._apply_button.set_sensitive (False)

    def __on_treeview__row_activate (self,
                                     _tree_path, # pylint: disable=W0613
                                     _column, # pylint: disable=W0613
                                     _data): # pylint: disable=W0613
        self._download_and_apply ()

    def __on_totem__file_opened (self, _, new_mrl):
        # Check if allows subtitles
        if self._check_allowed_scheme () and not self._check_is_audio ():
            self._action.set_enabled (True)
            if self._dialog:
                self._find_button.set_sensitive (True)
                # Check we're not re-opening the same file; if we are, don't
                # clear anything. This happens when we re-load the file with a
                # new set of subtitles, for example
                if self._filename != new_mrl:
                    self._filename = new_mrl
                    self._list_store.clear ()
                    self._apply_button.set_sensitive (False)
        else:
            self._action.set_enabled (False)
            if self._dialog and self._dialog.is_active ():
                self._filename = None
                self._list_store.clear ()
                self._apply_button.set_sensitive (False)
                self._find_button.set_sensitive (False)

    def __on_totem__file_closed (self, _):
        self._action.set_enabled (False)
        if self._dialog:
            self._apply_button.set_sensitive (False)
            self._find_button.set_sensitive (False)

    def __on_combobox__changed (self, combobox):
        combo_iter = combobox.get_active_iter ()
        combo_model = combobox.get_model ()
        self._model.lang = LANGUAGES[combo_model.get_value (combo_iter, 1)]
        self._settings.set_string ('language', self._model.lang)

    def __on_close_clicked (self, _):
        self._close_dialog ()

    def __on_apply_clicked (self, _):
        self._download_and_apply ()

    def __on_find_clicked (self, _):
        self._apply_button.set_sensitive (False)
        self._find_button.set_sensitive (False)
        self._filename = self._totem.get_current_mrl () # pylint: disable=no-member
        (movie_hash, movie_size) = hash_file (self._filename)
        movie_title = self._totem.get_title_at_playlist_pos(self._totem.get_playlist_pos())

        self._get_results (movie_hash, movie_size, movie_title)
