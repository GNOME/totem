# -*- coding: utf-8 -*-

import totem
import gobject
import gtk

class IplayerPlugin (totem.Plugin):
	def __init__(self):
		totem.Plugin.__init__ (self)
		self.debug = True
		self.totem = None

	def activate (self, totem_object):
		# Build the interface
		self.builder = self.load_interface ("iplayer.ui", True, totem_object.get_main_window (), self)
		container = self.builder.get_object ('iplayer_vbox')

		self.totem = totem_object
		container.show_all ()

		self.totem.add_sidebar_page("iplayer", _("BBC iPlayer"), container)

	def deactivate (self, totem_object):
		totem_object.remove_sidebar_page ("iplayer")
