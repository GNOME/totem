# -*- coding: utf-8 -*-

import gettext
import gobject
from gi.repository import Peas
from gi.repository import Totem
import iplayer2
import threading

gettext.textdomain ("totem")

D_ = gettext.dgettext
_ = gettext.gettext

class IplayerPlugin (gobject.GObject, Peas.Activatable):
    __gtype_name__ = 'IplayerPlugin'

    object = gobject.property (type = gobject.GObject)

    def __init__ (self):
        self.debug = False
        self.totem = None
        self.programme_download_lock = threading.Lock ()

        self.tv_feed = None
        self.tv_tree_store = None

    def do_activate (self):
        self.totem = self.object
        # Build the interface
        builder = Totem.plugin_load_interface ("iplayer", "iplayer.ui", True,
                                               self.totem.get_main_window (),
                                               self)
        container = builder.get_object ('iplayer_vbox')

        self.tv_tree_store = builder.get_object ('iplayer_programme_store')
        programme_list = builder.get_object ('iplayer_programme_list')
        programme_list.connect ('row-expanded', self._row_expanded_cb)
        programme_list.connect ('row-activated', self._row_activated_cb)

        container.show_all ()

        self.tv_feed = iplayer2.Feed ('tv')

        # Add the interface to Totem's sidebar
        self.totem.add_sidebar_page ("iplayer", _(u"BBC iPlayer"), container)

        # Get the channel category listings
        self.populate_channel_list (self.tv_feed, self.tv_tree_store)

    def do_deactivate (self):
        self.totem.remove_sidebar_page ("iplayer")

    def populate_channel_list (self, feed, tree_store):
        if self.debug:
            print "Populating channel list…"

        # Add all the channels as top-level rows in the tree store
        channels = feed.channels ()
        for channel_id, title in channels.items ():
            parent_iter = tree_store.append (None, (title, channel_id, None))

        # Add the channels' categories in a thread, since they each require a
        # network request
        parent_path = tree_store.get_path (parent_iter)
        thread = PopulateChannelsThread (self, feed, tree_store)
        thread.start ()

    def _populate_channel_list_cb (self, tree_store, parent_path, values):
        # Callback from PopulateChannelsThread to add stuff to the tree store
        if values == None:
            self.totem.action_error (_(u'Error listing channel categories'),
                                     _(u'There was an unknown error getting '\
                                        'the list of television channels '\
                                        'available on BBC iPlayer.'))
            return False

        parent_iter = tree_store.get_iter (parent_path)
        category_iter = tree_store.append (parent_iter, values)

        # Append a dummy child row so that the expander's visible; we can
        # then queue off the expander to load the programme listing for this
        # category
        tree_store.append (category_iter, [_(u'Loading…'), None, None])

        return False

    def _row_expanded_cb (self, tree_view, row_iter, path):
        tree_model = tree_view.get_model ()

        if self.debug:
            print "_row_expanded_cb called."

        # We only care about the category level (level 1), and only when
        # it has the "Loading..." placeholder child row
        if (get_iter_level (tree_model, row_iter) != 1 or
            tree_model.iter_n_children (row_iter) != 1):
            return

        # Populate it with programmes asynchronously
        self.populate_programme_list (self.tv_feed, tree_model, row_iter)

    def _row_activated_cb (self, tree_view, path, view_column):
        tree_store = tree_view.get_model ()
        tree_iter = tree_store.get_iter (path)
        if tree_iter == None:
            return

        mrl = tree_store.get_value (tree_iter, 2)

        # Only allow programme rows to be activated, not channel or category
        # rows
        if mrl == None:
            return

        # Add the programme to the playlist and play it
        title = tree_store.get_value (tree_iter, 0)
        self.totem.add_to_playlist_and_play (mrl, title, True)

    def populate_programme_list (self, feed, tree_store, category_iter):
        if self.debug:
            print "Populating programme list…"

        category_path = tree_store.get_path (category_iter)
        thread = PopulateProgrammesThread (self, feed, tree_store,
                                           category_path)
        thread.start ()

    def _populate_programme_list_cb (self, tree_store, category_path, values,
                                     remove_placeholder):
        # Callback from PopulateProgrammesThread to add stuff to the tree store
        if values == None:
            # Translators: the "programme feed" is the list of TV shows
            # available to watch online
            self.totem.action_error (_(u'Error getting programme feed'),
                                     _(u'There was an error getting the list '\
                                        'of programmes for this channel and '\
                                        'category combination.'))
            return False

        category_iter = tree_store.get_iter (category_path)
        if category_iter != None:
            tree_store.append (category_iter, values)

        # Remove the placeholder row
        children = tree_store.iter_children (category_iter)
        if remove_placeholder and children != None:
            tree_store.remove (children)

        return False

def get_iter_level (tree_model, tree_iter):
    i = 0
    while True:
        tree_iter = tree_model.iter_parent (tree_iter)
        if tree_iter == None:
            break
        i += 1
    return i

def category_name_to_id (category_name):
    return category_name.lower ().replace (' ', '_').replace ('&', 'and')

class PopulateChannelsThread (threading.Thread):
    # Class to populate the channel list from the Internet
    def __init__ (self, plugin, feed, tree_model):
        self.plugin = plugin
        self.feed = feed
        self.tree_model = tree_model
        threading.Thread.__init__ (self)

    def run (self):
        shown_error = False
        tree_iter = self.tree_model.get_iter_first ()
        while (tree_iter != None):
            channel_id = self.tree_model.get_value (tree_iter, 1)
            parent_path = self.tree_model.get_path (tree_iter)

            try:
                # Add this channel's categories as sub-rows
                # We have to pass a path because the model could theoretically
                # be modified while the idle function is waiting in the queue,
                # invalidating an iter
                for name, _count in self.feed.get (channel_id).categories ():
                    category_id = category_name_to_id (name)
                    gobject.idle_add (self.plugin._populate_channel_list_cb,
                                      self.tree_model, parent_path,
                                      [name, category_id, None])
            except:
                # Only show the error once, rather than for each channel
                # (it gets a bit grating)
                if not shown_error:
                    gobject.idle_add (self.plugin._populate_channel_list_cb,
                                      self.tree_model, parent_path, None)
                    shown_error = True

            tree_iter = self.tree_model.iter_next (tree_iter)

class PopulateProgrammesThread (threading.Thread):
    # Class to populate the programme list for a channel/category combination
    # from the Internet
    def __init__ (self, plugin, feed, tree_model, category_path):
        self.plugin = plugin
        self.feed = feed
        self.tree_model = tree_model
        self.category_path = category_path
        threading.Thread.__init__ (self)

    def run (self):
        self.plugin.programme_download_lock.acquire ()

        category_iter = self.tree_model.get_iter (self.category_path)
        if category_iter == None:
            gobject.idle_add (self.plugin._populate_programme_list_cb,
                              self.tree_model, self.category_path, None, False)
            self.plugin.programme_download_lock.release ()
            return

        category_id = self.tree_model.get_value (category_iter, 1)
        parent_iter = self.tree_model.iter_parent (category_iter)
        channel_id = self.tree_model.get_value (parent_iter, 1)

        # Retrieve the programmes and return them
        feed = self.feed.get (channel_id).get (category_id)
        if feed == None:
            gobject.idle_add (self.plugin._populate_programme_list_cb,
                              self.tree_model, self.category_path, None, False)
            self.plugin.programme_download_lock.release ()
            return

        # Get the programmes
        try:
            programmes = feed.list ()
        except:
            gobject.idle_add (self.plugin._populate_programme_list_cb,
                              self.tree_model, self.category_path, None, False)
            self.plugin.programme_download_lock.release ()
            return

        # Add the programmes to the tree store
        remove_placeholder = True
        for programme in programmes:
            programme_item = programme.programme

            # Get the media, which gives the stream URI.
            # We go for mobile quality, since the higher-quality streams are
            # RTMP-only which isn't currently supported by GStreamer or xine
            # TODO: Use higher-quality streams once
            # http://bugzilla.gnome.org/show_bug.cgi?id=566604 is fixed
            media = programme_item.get_media_for ('mobile')
            if media == None:
                # Not worth displaying an error in the interface for this
                print "Programme has no HTTP streams"
                continue

            gobject.idle_add (self.plugin._populate_programme_list_cb,
                              self.tree_model, self.category_path,
                              [programme.get_title (), programme.get_summary (),
                               media.url],
                              remove_placeholder)
            remove_placeholder = False

        self.plugin.programme_download_lock.release ()
