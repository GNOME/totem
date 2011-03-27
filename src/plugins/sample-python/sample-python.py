# From code by James Livingston

import gobject
from gi.repository import Peas
from gi.repository import Totem

class SamplePython (gobject.GObject, Peas.Activatable):
    __gtype_name__ = 'SamplePython'

    object = gobject.property (type = gobject.GObject)

    def do_activate (self):
        print "Activating sample Python plugin"
        self.object.action_fullscreen_toggle ()

    def do_deactivate (self):
        print "Deactivating sample Python plugin"
