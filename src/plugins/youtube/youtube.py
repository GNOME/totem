import totem
import gobject, gtk
import gdata.service
import urllib
import httplib
import atom
import threading
import re
from os import unlink

class DownloadThread (threading.Thread):
	def __init__ (self, youtube, url):
		self.youtube = youtube
		self.url = url
		threading.Thread.__init__ (self)
	def run (self):
		self.youtube.entry = self.youtube.service.Get (self.url).entry

class YouTube (totem.Plugin):
	def __init__ (self):
		totem.Plugin.__init__(self)
		self.debug = False
		self.search_mode = True
		self.query = ""
		self.start_index = 1
		self.max_results = 20
		"""Note that this is just the number of results from the last pagination query"""
		self.results = 0
		self.entry = None
		self.button_down = False
	def activate (self, totem_object):
		self.builder = self.load_interface ("youtube.ui", True, totem_object.get_main_window (), self)

		self.search_entry = self.builder.get_object ("yt_search_entry")
		self.search_entry.connect ("activate", self.on_search_entry_activated)
		self.search_button = self.builder.get_object ("yt_search_button")
		self.search_button.connect ("clicked", self.on_search_button_clicked)
		self.status_label = self.builder.get_object ("yt_status_label")

		"""This is created here rather than in the UI file, because UI files parsed in C and GObjects created in Python don't mix."""
		self.renderer = totem.CellRendererVideo ()
		self.treeview = self.builder.get_object ("yt_treeview")
		self.treeview.connect ("row-activated", self.on_row_activated)
		self.treeview.insert_column_with_attributes (0, _("Videos"), self.renderer, thumbnail=0, title=1)

		self.vadjust = self.treeview.get_vadjustment ()
		self.vadjust.connect ("value-changed", self.on_value_changed)
		vscroll = self.builder.get_object ("yt_scrolled_window").get_vscrollbar ()
		vscroll.connect ("button-press-event", self.on_button_press_event)
		vscroll.connect ("button-release-event", self.on_button_release_event)

		self.liststore = gtk.ListStore (gobject.TYPE_OBJECT, gobject.TYPE_STRING, gobject.TYPE_STRING)

		self.treeview.set_model (self.liststore)

		vbox = self.builder.get_object ("yt_vbox")
		vbox.show_all ()
		totem_object.add_sidebar_page ("youtube", _("YouTube"), vbox)

		"""Set up the service"""
		self.service = gdata.service.GDataService (None, None, "HOSTED_OR_GOOGLE", None, None, "gdata.youtube.com")
		self.totem = totem_object
	def deactivate (self, totem):
		totem.remove_sidebar_page ("youtube")
	def on_row_activated (self, treeview, path, column):
		model, iter = treeview.get_selection ().get_selected ()
		youtube_id = model.get_value (iter, 2)

		"""Play the video"""
		conn = httplib.HTTPConnection ("www.youtube.com")
		conn.request ("GET", "/v/" + urllib.quote (youtube_id))
		response = conn.getresponse ()
		if response.status == 303:
			location = response.getheader("location")
			url = "http://www.youtube.com/get_video?video_id=" + urllib.quote (youtube_id) + "&t=" + urllib.quote (re.match (".*[?&]t=([^&]+)", location).groups ()[0])
		else:
			url = "http://www.youtube.com/v/" + urllib.quote (youtube_id)
		conn.close ()

		if self.debug:
			print "Playing: " + url
		else:
			self.totem.action_set_mrl_and_play (url)
		
		"""Get related videos"""
		self.search_mode = False
		self.query = youtube_id
		self.start_index = 1
		self.results = 0
		self.get_results ("/feeds/videos/" + urllib.quote (youtube_id) + "/related?max-results=" + str (self.max_results), _("Related Videos:"))
	def on_button_press_event (self, widget, event):
		self.button_down = True
	def on_button_release_event (self, widget, event):
		self.button_down = False
		self.on_value_changed (self.vadjust)
	def on_value_changed (self, adjustment):
		"""Load more results when we get near the bottom of the treeview"""
		if not self.button_down and (adjustment.get_value () + adjustment.page_size) / adjustment.upper > 0.8 and self.results >= self.max_results:
			if self.debug:
				print "Getting more results for query \"" + self.query + "\" from offset " + str (self.start_index)
			self.results = 0
			if self.search_mode:
				self.get_results ("/feeds/videos?vq=" + urllib.quote_plus (self.query) + "&max-results=" + str (self.max_results) + "&orderby=relevance&start-index=" + str (self.start_index), _("Search Results:"), False)
			else:
				self.get_results ("/feeds/videos/" + urllib.quote_plus (self.query) + "/related?max-results=" + str (self.max_results) + "&start-index=" + str (self.start_index), _("Related Videos:"), False)
	def convert_url_to_id (self, url):
		return url.rpartition ("/")[2]
	def populate_list_from_results (self, status_text):
		"""Wait until we have some results to display"""
		if self.entry == None or len (self.entry) == 0:
			return True

		"""Only do one result at a time, as the thumbnail has to be downloaded"""
		entry = self.entry.pop (0)
		self.results += 1
		self.start_index += 1

		"""Find the thumbnail tag"""
		for _element in entry.extension_elements:
			if _element.tag == "group":
				break

		"""Download the thumbnail and store it in a temporary location so we can get a pixbuf from it"""
		thumbnail_url = _element.FindChildren ("thumbnail")[0].attributes['url']
		try:
			filename, headers = urllib.urlretrieve (thumbnail_url)
		except IOError:
			print "Could not load thumbnail " + thumbnail_url + " for video."
			return True

		try:
			pixbuf = gtk.gdk.pixbuf_new_from_file (filename)
		except gobject.GError:
			print "Could not load thumbnail " + filename + " for video. It has been left in place for investigation."
			return True

		"""Don't leak the temporary file"""
		unlink (filename)

		self.liststore.append ([pixbuf, entry.title.text, self.convert_url_to_id (entry.id.text)])
		self.treeview.window.process_updates (True)

		"""Have we finished?"""
		if len (self.entry) == 0:
			self.status_label.set_text (status_text)
			self.entry = None
			return False

		return True
	def on_search_button_clicked (self, button):
		search_terms = self.search_entry.get_text ()

		if self.debug:
			print "Searching for \"" + search_terms + "\""

		self.search_mode = True
		self.query = search_terms
		self.start_index = 1
		self.results = 0
		self.get_results ("/feeds/videos?vq=" + urllib.quote_plus (search_terms) + "&orderby=relevance&max-results=" + str (self.max_results), _("Search Results:"))
	def on_search_entry_activated (self, entry):
		self.search_button.clicked ()
	def get_results (self, url, status_text, clear = True):
		if clear:
			self.liststore.clear ()

		self.status_label.set_text (_("Loading..."))

		"""Make sure things are redrawn"""
		self.status_label.window.process_updates (True)
		self.treeview.window.process_updates (True)

		if self.debug:
				print "Getting results from URL \"" + url + "\""

		DownloadThread (self, url).start ()
		gobject.idle_add (self.populate_list_from_results, status_text)
