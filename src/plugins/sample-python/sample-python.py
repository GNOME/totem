# From code by James Livingston

import gobject
from gi.repository import Peas
from gi.repository import Totem

class SamplePython(gobject.GObject, Peas.Activatable):
	__gtype_name__ = 'SamplePython'

	def do_activate(self, totem):
		print "Activating sample Python plugin"
		totem.action_fullscreen_toggle()
	
	def do_deactivate(self, totem):
		print "Deactivating sample Python plugin"
