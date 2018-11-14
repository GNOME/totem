# -*- coding: utf-8 -*-

# pythonconsole.py -- plugin object
#
# Copyright (C) 2006 - Steve Frécinaux
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

# Parts from "Interactive Python-GTK Console" (stolen from epiphany's
# console.py)
#     Copyright (C), 1998 James Henstridge <james@daa.com.au>
#     Copyright (C), 2005 Adam Hooper <adamh@densi.com>
# Bits from gedit Python Console Plugin
#     Copyrignt (C), 2005 Raphaël Slinckx
#
# The Totem project hereby grant permission for non-gpl compatible GStreamer
# plugins to be used and distributed together with GStreamer and Totem. This
# permission are above and beyond the permissions granted by the GPL license
# Totem is covered by.
#
# Monday 7th February 2005: Christian Schaller: Add exception clause.
# See license_change file for details.

import gettext
import gi

gi.require_version('Gtk', '3.0')
gi.require_version('Peas', '1.0')
gi.require_version('Pango', '1.0')
gi.require_version('Totem', '1.0')

from gi.repository import GObject, Peas, Gtk, Totem # pylint: disable=wrong-import-position,no-name-in-module
from gi.repository import Gio # pylint: disable=wrong-import-position

from console import PythonConsole, OutFile # pylint: disable=wrong-import-position

__all__ = ('PythonConsolePlugin', 'PythonConsole', 'OutFile') # pylint: disable=E0603

try:
    import rpdb2
    HAVE_RPDB2 = True
except ImportError:
    HAVE_RPDB2 = False

gettext.textdomain ("totem")

D_ = gettext.dgettext
_ = gettext.gettext

class PythonConsolePlugin (GObject.Object, Peas.Activatable):
    __gtype_name__ = 'PythonConsolePlugin'

    object = GObject.property (type = GObject.Object)

    def __init__ (self):
        GObject.Object.__init__ (self)

        self.totem = None
        self.window = None

    def do_activate (self):
        self.totem = self.object

        action = Gio.SimpleAction.new ("python-console", None)
        action.connect ('activate', self._show_console)
        self.totem.add_action (action) # pylint: disable=no-member

        menu = self.totem.get_menu_section ("python-console-placeholder") # pylint: disable=no-member
        menu.append (_('_Python Console'), "app.python-console")

        if HAVE_RPDB2:
            action = Gio.SimpleAction.new ("python-debugger", None)
            action.connect ('activate', self._enable_debugging)
            self.totem.add_action (action) # pylint: disable=no-member
            menu.append (_('Python Debugger'), "app.python-debugger")

    def _show_console (self, parameter, _action): # pylint: disable=W0613
        if not self.window:
            console = PythonConsole (namespace = {
                '__builtins__' : __builtins__,
                'Totem' : Totem,
                'totem_object' : self.totem
            }, destroy_cb = self._destroy_console)

            console.set_size_request (600, 400) # pylint: disable=E1101
            console.eval ('print("%s" %% totem_object)' % _("You can access "\
                "the Totem.Object through “totem_object” :\\n%s"), False)

            self.window = Gtk.Window ()
            self.window.set_title (_('Totem Python Console'))
            self.window.add (console)
            self.window.connect ('destroy', self._destroy_console)
            self.window.show_all ()
        else:
            self.window.show_all ()
            self.window.grab_focus ()

    @classmethod
    def _enable_debugging (cls, param, _action): # pylint: disable=W0613
        msg = _("After you press OK, Totem will wait until you connect to it "\
                 "with winpdb or rpdb2. If you have not set a debugger "\
                 "password in DConf, it will use the default password "\
                 "(“totem”).")
        dialog = Gtk.MessageDialog (None, 0, Gtk.MessageType.INFO,
                                    Gtk.ButtonsType.OK_CANCEL, msg)
        if dialog.run () == Gtk.ResponseType.OK:
            schema = 'org.gnome.totem.plugins.pythonconsole'
            settings = Gio.Settings.new (schema)
            password = settings.get_string ('rpdb2-password') or "totem"
            def start_debugger (password):
                rpdb2.start_embedded_debugger (password)
                return False

            GObject.idle_add (start_debugger, password)
        dialog.destroy ()

    def _destroy_console (self, *_args): # pylint: disable=W0613
        self.window.destroy ()
        self.window = None

    def do_deactivate (self):
        self.totem.empty_menu_section ("python-console-placeholder") # pylint: disable=no-member

        if self.window is not None:
            self.window.destroy ()
