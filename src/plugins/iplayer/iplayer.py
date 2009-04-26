import totem
import gobject
import gtk
import iplayer2
import threading

class IplayerPlugin (totem.Plugin):
	def __init__ (self):
		totem.Plugin.__init__ (self)
		self.debug = True
		self.totem = None

	def activate (self, totem_object):
		# Build the interface
		builder = self.load_interface ("iplayer.ui", True, totem_object.get_main_window (), self)
		container = builder.get_object ('iplayer_vbox')
		self.tree_store = builder.get_object ('iplayer_programme_store')

		self.totem = totem_object
		container.show_all ()

		self.tv = iplayer2.feed ('tv')
		# TODO: Radio support
		#self.radio = feed ('radio')

		self.totem.add_sidebar_page ("iplayer", _("BBC iPlayer"), container)

		self.populate_channel_list ()

	def deactivate (self, totem_object):
		totem_object.remove_sidebar_page ("iplayer")

	def populate_channel_list (self):
		# Add all the channels as top-level rows in the tree store
		channels = self.tv.channels ()
		for channel_id, title in channels.items ():
			parent_iter = self.tree_store.append (None, [title, channel_id, None])

		# Add the channels' categories in a thread, since they each require a network request
		parent_path = self.tree_store.get_path (parent_iter)
		thread = PopulateChannelsThread (self, parent_path, self.tv, self.tree_store)
        	thread.start ()

	def _populate_channel_list_cb (self, parent_path, values):
		# Callback from PopulateChannelsThread to add stuff to the tree store
		parent_iter = self.tree_store.get_iter (parent_path)
		self.tree_store.append (parent_iter, values)
		return False

class PopulateChannelsThread (threading.Thread):
	# Class to populate the channel list from the Internet
	def __init__ (self, plugin, parent_path, feed, tree_model):
		self.plugin = plugin
		self.feed = feed
		self.tree_model = tree_model
		threading.Thread.__init__ (self)

	def run (self):
		tree_iter = self.tree_model.get_iter_first ()
		while (tree_iter != None):
			channel_id = self.tree_model.get_value (tree_iter, 1)
			parent_path = self.tree_model.get_path (tree_iter)

			# Add this channel's categories as sub-rows
			# We have to pass a path because the model could theoretically be modified
			# while the idle function is waiting in the queue, invalidating an iter
			for name, category_id in self.feed.get (channel_id).categories ():
				gobject.idle_add (self.plugin._populate_channel_list_cb, parent_path, [name, category_id, None])

			tree_iter = self.tree_model.iter_next (tree_iter)

