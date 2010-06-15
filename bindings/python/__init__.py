# -*- coding: utf-8 -*-
# ex:set ts=4 et sw=4 ai:
#
#    Totem Python bindings
#    Copyright (C) 2009  Steve Frécinaux
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# Import everything from the binary module
from totem._totem import *

import gobject
from totem import _totem

def _method_is_overriden(plugin, method_name):
    child_method = getattr(plugin.__class__, method_name, None)
    parent_method = getattr(_totem.Plugin, method_name, None)
    return child_method != parent_method

def _proxy_plugin_method(method_name):
    def method(self, window):
        if _method_is_overriden(self, method_name):
            return getattr(self, method_name)(window)
    return method

class Plugin(_totem.Plugin):
    do_activate = _proxy_plugin_method('activate')
    do_deactivate = _proxy_plugin_method('deactivate')
    do_update_ui = _proxy_plugin_method('update_ui')

    def do_is_configurable(self):
        return _method_is_overriden(self, 'create_configure_dialog')

    def do_create_configure_dialog(self):
        if _method_is_overriden(self, 'create_configure_dialog'):
            return self.create_configure_dialog()

gobject.type_register(Plugin)
