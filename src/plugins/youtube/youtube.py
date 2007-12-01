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
	def __init__ (self, youtube, url, treeview_name):
		self.youtube = youtube
		self.url = url
		self.treeview_name = treeview_name
		threading.Thread.__init__ (self)
	def run (self):
		self.youtube.entry[self.treeview_name] = self.youtube.service.Get (self.url).entry

class YouTube (totem.Plugin):
	def __init__ (self):
		totem.Plugin.__init__(self)
		self.debug = False

		self.max_results = 20
		self.button_down = False

		self.search_terms = ""
		self.youtube_id = ""

		self.start_index = {}
		self.results = {} # This is just the number of results from the last pagination query
		self.entry = {}

		self.current_treeview_name = ""
		self.notebook_pages = []

		self.vadjust = {}
		self.liststore = {}
	def activate (self, totem_object):
		self.builder = self.load_interface ("youtube.ui", True, totem_object.get_main_window (), self)
		self.totem = totem_object

		self.search_entry = self.builder.get_object ("yt_search_entry")
		self.search_entry.connect ("activate", self.on_search_entry_activated)
		self.search_button = self.builder.get_object ("yt_search_button")
		self.search_button.connect ("clicked", self.on_search_button_clicked)

		self.notebook = self.builder.get_object ("yt_notebook")
		self.notebook.connect ("switch-page", self.on_notebook_page_changed)

		self.notebook_pages = ["search", "related"]
		for page in self.notebook_pages:
			self.setup_treeview (page)
		self.current_treeview_name = "search"

		self.vbox = self.builder.get_object ("yt_vbox")
		self.vbox.show_all ()
		totem_object.add_sidebar_page ("youtube", _("YouTube"), self.vbox)

		"""Set up the service"""
		self.service = gdata.service.GDataService (None, None, "HOSTED_OR_GOOGLE", None, None, "gdata.youtube.com")
	def deactivate (self, totem):
		totem.remove_sidebar_page ("youtube")
	def setup_treeview (self, treeview_name):
		self.start_index[treeview_name] = 1
		self.results[treeview_name] = 0
		self.entry[treeview_name] = None

		"""This is done here rather than in the UI file, because UI files parsed in C and GObjects created in Python apparently don't mix."""
		renderer = totem.CellRendererVideo (use_placeholder = True)
		treeview = self.builder.get_object ("yt_treeview_" + treeview_name)
		treeview.set_property ("totem", self.totem)
		treeview.connect ("row-activated", self.on_row_activated)
		treeview.connect ("starting-video", self.on_starting_video)
		treeview.insert_column_with_attributes (0, _("Videos"), renderer, thumbnail=0, title=1)

		self.vadjust[treeview_name] = treeview.get_vadjustment ()
		self.vadjust[treeview_name].connect ("value-changed", self.on_value_changed)
		vscroll = self.builder.get_object ("yt_scrolled_window_" + treeview_name).get_vscrollbar ()
		vscroll.connect ("button-press-event", self.on_button_press_event)
		vscroll.connect ("button-release-event", self.on_button_release_event)

		self.liststore[treeview_name] = self.builder.get_object ("yt_liststore_" + treeview_name)
		treeview.set_model (self.liststore[treeview_name])
	def on_notebook_page_changed (self, notebook, notebook_page, page_num):
		self.current_treeview_name = self.notebook_pages[page_num]
	def on_row_activated (self, treeview, path, column):
		model, rows = treeview.get_selection ().get_selected_rows ()
		iter = model.get_iter (rows[0])
		youtube_id = model.get_value (iter, 3)
		
		"""Get related videos"""
		self.youtube_id = youtube_id
		self.start_index["related"] = 1
		self.results["related"] = 0
		self.get_results ("/feeds/videos/" + urllib.quote (youtube_id) + "/related?max-results=" + str (self.max_results), "related")
	def on_starting_video (self, treeview, path, user_data):
		model, rows = treeview.get_selection ().get_selected_rows ()
		iter = model.get_iter (rows[0])
		youtube_id = model.get_value (iter, 3)

		"""Get the video stream MRL"""
		try:
			conn = httplib.HTTPConnection ("www.youtube.com")
			conn.request ("GET", "/v/" + urllib.quote (youtube_id))
			response = conn.getresponse ()
		except:
			print "Could not resolve stream MRL for YouTube video \"" + youtube_id + "\"."
			return True

		if response.status == 303:
			location = response.getheader("location")
			mrl = "http://www.youtube.com/get_video?video_id=" + urllib.quote (youtube_id) + "&t=" + urllib.quote (re.match (".*[?&]t=([^&]+)", location).groups ()[0])
		else:
			mrl = "http://www.youtube.com/v/" + urllib.quote (youtube_id)
		conn.close ()

		model.set_value (iter, 2, mrl)

		return True
	def on_button_press_event (self, widget, event):
		self.button_down = True
	def on_button_release_event (self, widget, event):
		self.button_down = False
		self.on_value_changed (self.vadjust[self.current_treeview_name])
	def on_value_changed (self, adjustment):
		"""Load more results when we get near the bottom of the treeview"""
		if not self.button_down and (adjustment.get_value () + adjustment.page_size) / adjustment.upper > 0.8 and self.results[self.current_treeview_name] >= self.max_results:
			self.results[self.current_treeview_name] = 0
			if self.current_treeview_name == "search":
				self.get_results ("/feeds/videos?vq=" + urllib.quote_plus (self.search_terms) + "&max-results=" + str (self.max_results) + "&orderby=relevance&start-index=" + str (self.start_index["search"]), "search", False)
				if self.debug:
					print "Getting more results for search \"" + self.search_terms + "\" from offset " + str (self.start_index["search"])
			elif self.current_treeview_name == "related":
				self.get_results ("/feeds/videos/" + urllib.quote_plus (self.youtube_id) + "/related?max-results=" + str (self.max_results) + "&start-index=" + str (self.start_index["related"]), "related", False)
				if self.debug:
					print "Getting more related videos for video \"" + self.youtube_id + "\" from offset " + str (self.start_index["related"])
	def convert_url_to_id (self, url):
		"""Find the last clause in the URL; after the last /"""
		return url.split ("/").pop ()
	def populate_list_from_results (self, treeview_name):
		"""Wait until we have some results to display"""
		if self.entry[treeview_name] == None or len (self.entry[treeview_name]) == 0:
			return True
		"""Only do one result at a time, as the thumbnail has to be downloaded; give them a temporary MRL until the real one is resolved before playing"""
		entry = self.entry[treeview_name].pop (0)
		self.results[treeview_name] += 1
		self.start_index[treeview_name] += 1
		youtube_id = self.convert_url_to_id (entry.id.text)
		mrl = "http://www.youtube.com/v/" + urllib.quote (youtube_id)

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

		self.liststore[treeview_name].append ([pixbuf, entry.title.text, mrl, youtube_id])

		"""Have we finished?"""
		if len (self.entry[treeview_name]) == 0:
			"""Revert the cursor"""
			window = self.vbox.window
			window.set_cursor (None)

			self.entry[treeview_name] = None
			return False

		return True
	def on_search_button_clicked (self, button):
		search_terms = self.search_entry.get_text ()

		if self.debug:
			print "Searching for \"" + search_terms + "\""

		"""Focus the "Search" tab"""
		self.notebook.set_current_page (self.notebook_pages.index ("search"))

		self.search_terms = search_terms
		self.start_index["search"] = 1
		self.results["search"] = 0
		self.get_results ("/feeds/videos?vq=" + urllib.quote_plus (search_terms) + "&orderby=relevance&max-results=" + str (self.max_results), "search")
	def on_search_entry_activated (self, entry):
		self.search_button.clicked ()
	def get_results (self, url, treeview_name, clear = True):
		if clear:
			self.liststore[treeview_name].clear ()

		if self.debug:
			print "Getting results from URL \"" + url + "\""

		"""Give us a nice waiting cursor"""
		window = self.vbox.window
		window.set_cursor (gtk.gdk.Cursor (gtk.gdk.WATCH))

		DownloadThread (self, url, treeview_name).start ()
		gobject.idle_add (self.populate_list_from_results, treeview_name)

