# From code by James Livingston

from gi.repository import GObject, Totem # pylint: disable=no-name-in-module,unused-import

class SamplePython (GObject.Object, Totem.PluginActivatable):
    __gtype_name__ = 'SamplePython'

    object = GObject.property (type = GObject.Object)

    def __init__ (self):
        GObject.Object.__init__ (self)

    def do_activate (self):
        print("Activating sample Python plugin")
        self.object.action_fullscreen_toggle () # pylint: disable=no-member

    def do_deactivate (self):
        print("Deactivating sample Python plugin")
        self.object.action_fullscreen_toggle () # pylint: disable=no-member
