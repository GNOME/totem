# -*- coding: utf-8 -*-

# Licensed under the MIT license
# http://opensource.org/licenses/mit-license.php

# Copyright 2008, Frank Scholz <coherence@beebits.net>

import gettext

import gobject
from gi.repository import Peas
from gi.repository import Gtk
from gi.repository import Totem

from coherence.ui.av_widgets import TreeWidget
from coherence.ui.av_widgets import UDN_COLUMN, UPNP_CLASS_COLUMN
from coherence.ui.av_widgets import SERVICE_COLUMN

gettext.textdomain ("totem")

D_ = gettext.dgettext
_ = gettext.gettext

class UPnPClient (gobject.GObject, Peas.Activatable):
    __gtype_name__ = 'UPnPClient'

    object = gobject.property (type = gobject.GObject)

    def __init__ (self):
        self.totem_object = None
        self.uiw = TreeWidget ()
        self.uiw.window.set_shadow_type (gtk.SHADOW_IN)
        self.uiw.cb_item_right_click = self.button_pressed
        self.uiw.window.show_all ()
        selection = self.uiw.treeview.get_selection ()
        selection.set_mode (gtk.SELECTION_MULTIPLE)

    def button_pressed (self, widget, event):
        if event.button == 3:
            event_x = int (event.x)
            event_y = int (event.y)
            try:
                row_path, column, _cell_x, _cell_y = \
                    self.uiw.treeview.get_path_at_pos (event_x, event_y)
                selection = self.uiw.treeview.get_selection ()
                if not selection.path_is_selected (row_path):
                    self.uiw.treeview.set_cursor (row_path, column, False)
                print "button_pressed", row_path, (row_path[0],)
                itera = self.uiw.store.get_iter ((row_path[0],))
                udn, = self.uiw.store.get (itera, UDN_COLUMN)
                itera = self.uiw.store.get_iter (row_path)
                upnp_class, url = self.uiw.store.get (itera, UPNP_CLASS_COLUMN,
                                                      SERVICE_COLUMN)
                print udn, upnp_class, url
                if (not upnp_class.startswith ('object.container') and
                    not upnp_class == 'root'):
                    has_delete = self.uiw.device_has_action (udn,
                                                             'ContentDirectory',
                                                             'DestroyObject')
                    self.create_item_context (has_delete = has_delete)
                    self.context.popup (None, None, None, event.button,
                                        event.time)
                    return 1
            except TypeError:
                pass
            return 1

    def create_item_context (self, has_delete = False):
        """ create context menu for right click in treeview item"""

        def action (menu, text):
            selection = self.uiw.treeview.get_selection ()
            model, selected_rows = selection.get_selected_rows ()
            if text == 'item.delete':
                for row_path in selected_rows:
                    self.uiw.destroy_object (row_path)
                return
            if (len (selected_rows) > 0 and text ==' item.play'):
                row_path = selected_rows.pop (0)
                itera = self.uiw.store.get_iter (row_path)
                url, = self.uiw.store.get (itera, SERVICE_COLUMN)
                self.totem_object.action_remote (totem.REMOTE_COMMAND_REPLACE,
                                                 url)
                self.totem_object.action_remote (totem.REMOTE_COMMAND_PLAY, url)
            for row_path in selected_rows:
                itera = self.uiw.store.get_iter (row_path)
                url, = self.uiw.store.get (itera, SERVICE_COLUMN)
                self.totem_object.action_remote (totem.REMOTE_COMMAND_ENQUEUE,
                                                 url)
                self.totem_object.action_remote (totem.REMOTE_COMMAND_PLAY, url)

        if not hasattr (self, 'context_no_delete'):
            self.context_no_delete = gtk.Menu ()
            # Translators: this refers to a media file
            play_menu = gtk.MenuItem (_(u"Play"))
            play_menu.connect ("activate", action, 'item.play')
            # Translators: this refers to a media file
            enqueue_menu = gtk.MenuItem (_(u"Enqueue"))
            enqueue_menu.connect ("activate", action, 'item.enqueue')
            self.context_no_delete.append (play_menu)
            self.context_no_delete.append (enqueue_menu)
            self.context_no_delete.show_all ()

        if not hasattr (self, 'context_with_delete'):
            self.context_with_delete = gtk.Menu ()
            # Translators: this refers to a media file
            play_menu = gtk.MenuItem (_(u"Play"))
            play_menu.connect ("activate", action, 'item.play')
            # Translators: this refers to a media file
            enqueue_menu = gtk.MenuItem (_(u"Enqueue"))
            enqueue_menu.connect ("activate", action, 'item.enqueue')
            self.context_with_delete.append (play_menu)
            self.context_with_delete.append (enqueue_menu)
            self.context_with_delete.append (gtk.SeparatorMenuItem ())
            # Translators: this refers to a media file
            menu = gtk.MenuItem (_(u"Delete"))
            menu.connect ("activate", action, 'item.delete')
            self.context_with_delete.append (menu)
            self.context_with_delete.show_all ()

        if has_delete:
            self.context = self.context_with_delete
        else:
            self.context = self.context_no_delete

    def do_activate (self):
        self.totem_object = self.object
        self.totem_object.add_sidebar_page ("upnp-coherence",
                                            _(u"Coherence DLNA/UPnP Client"),
                                            self.uiw.window)

        def load_and_play (url):
            self.totem_object.add_to_playlist_and_play (url, '', True)

        self.uiw.cb_item_dbl_click = load_and_play

    def do_deactivate (self):
        self.totem_object.remove_sidebar_page ("upnp-coherence")
