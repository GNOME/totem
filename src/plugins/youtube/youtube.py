import totem
import gobject, gtk, gconf
gobject.threads_init()
import gdata.service
import urllib
import httplib
import atom
import threading
import time
import re
import os
import random

class DownloadThread (threading.Thread):
	def __init__ (self, youtube, url, treeview_name):
		self.youtube = youtube
		self.url = url
		self.treeview_name = treeview_name
		self._done = False
		self._lock = threading.Lock ()
		threading.Thread.__init__ (self)

	def run (self):
		try:
			res = self.youtube.service.Get (self.url).entry
		except gdata.service.RequestError:
			"""Probably a 503 service unavailable. Unfortunately we can't give an error message, as we're not in the GUI thread"""
			"""Just let the lock go and return"""
			res = None
		gobject.idle_add (self.publish_results, res)

	def publish_results(self, res):
		self._lock.acquire (True)
		self.youtube.entry[self.treeview_name] = res
		self._done = True
		self._lock.release ()
		return False

	@property
	def done (self):
		""" Thread-safe property to know whether the query is done or not """
		self._lock.acquire (True)
		res = self._done
		self._lock.release ()
		return res

class CallbackThread (threading.Thread):
	def __init__ (self, callback, *args, **kwargs):
		self.callback = callback
		self.args = args
		self.kwargs = kwargs
		threading.Thread.__init__ (self)

	def run (self):
		res = self.callback (*self.args, **self.kwargs)
		while res == True:
			res = self.callback (*self.args, **self.kwargs)

class YouTube (totem.Plugin):
	def __init__ (self):
		totem.Plugin.__init__ (self)
		self.debug = False
		self.gstreamer_plugins_present = True

		"""Search counters (per search type)"""
		self.in_search = {}
		self.search_token = {} # Used as an ID for a search thread

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
		self.treeview = {}

	def activate (self, totem_object):
		"""Check for the availability of the flvdemux and souphttpsrc GStreamer plugins"""
		bvw_name = totem_object.get_video_widget_backend_name ()

		"""If the user's selected 1.5Mbps or greater as their connection speed, grab higher-quality videos
		   and drop the requirement for the flvdemux plugin."""
		self.gconf_client = gconf.client_get_default ()

		if bvw_name.find ("GStreamer") != -1:
			try:
				import pygst
				pygst.require ("0.10")
				import gst

				registry = gst.registry_get_default ()
				if registry.find_plugin ("soup") == None:
					"""This means an error will be displayed when they try to play anything"""
					self.gstreamer_plugins_present = False
			except ImportError:
				"""Do nothing; either it's using xine or python-gstreamer isn't installed"""

		"""Continue loading the plugin as before"""
		self.builder = self.load_interface ("youtube.ui", True, totem_object.get_main_window (), self)
		self.totem = totem_object

		self.search_entry = self.builder.get_object ("yt_search_entry")
		self.search_entry.connect ("activate", self.on_search_entry_activated)
		self.search_button = self.builder.get_object ("yt_search_button")
		self.search_button.connect ("clicked", self.on_search_button_clicked)
		self.progress_bar = self.builder.get_object ("yt_progress_bar")

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
		self.service = gdata.service.GDataService (account_type = "HOSTED_OR_GOOGLE", server = "gdata.youtube.com")

	def deactivate (self, totem):
		totem.remove_sidebar_page ("youtube")

	def setup_treeview (self, treeview_name):
		self.start_index[treeview_name] = 1
		self.results[treeview_name] = 0
		self.entry[treeview_name] = None
		self.in_search[treeview_name] = False

		"""This is done here rather than in the UI file, because UI files parsed in C and GObjects created in Python apparently don't mix."""
		renderer = totem.CellRendererVideo (use_placeholder = True)
		treeview = self.builder.get_object ("yt_treeview_" + treeview_name)
		treeview.set_property ("totem", self.totem)
		treeview.connect ("row-activated", self.on_row_activated)
		treeview.connect_after ("starting-video", self.on_starting_video)
		treeview.insert_column_with_attributes (0, _("Videos"), renderer, thumbnail=0, title=1)

		"""Add the extra popup menu options. This is done here rather than in the UI file, because it's done for multiple treeviews;
		if it were done in the UI file, the same action group would be used multiple times, which GTK+ doesn't like."""
		ui_manager = treeview.get_ui_manager ()
		action_group = gtk.ActionGroup ("youtube-action-group")
		action = gtk.Action ("open-in-web-browser", _("_Open in Web Browser"), _("Open the video in your web browser"), "gtk-jump-to")
		action_group.add_action_with_accel (action, None)

		ui_manager.insert_action_group (action_group, 1)
		ui_manager.add_ui (ui_manager.new_merge_id (),
				   "/ui/totem-video-list-popup/",
				   "open-in-web-browser",
				   "open-in-web-browser",
				   gtk.UI_MANAGER_MENUITEM,
				   False)
		menu_item = ui_manager.get_action ("/ui/totem-video-list-popup/open-in-web-browser")
		menu_item.connect ("activate", self.on_open_in_web_browser_activated)

		self.vadjust[treeview_name] = treeview.get_vadjustment ()
		self.vadjust[treeview_name].connect ("value-changed", self.on_value_changed)
		vscroll = self.builder.get_object ("yt_scrolled_window_" + treeview_name).get_vscrollbar ()
		vscroll.connect ("button-press-event", self.on_button_press_event)
		vscroll.connect ("button-release-event", self.on_button_release_event)

		self.liststore[treeview_name] = self.builder.get_object ("yt_liststore_" + treeview_name)
		self.treeview[treeview_name] = treeview
		treeview.set_model (self.liststore[treeview_name])

	def on_notebook_page_changed (self, notebook, notebook_page, page_num):
		self.current_treeview_name = self.notebook_pages[page_num]

	def on_row_activated (self, treeview, path, column):
		if self.debug:
			print "Activating row"

		model, rows = treeview.get_selection ().get_selected_rows ()
		iter = model.get_iter (rows[0])
		youtube_id = model.get_value (iter, 3)

		"""Get related videos"""
		self.youtube_id = youtube_id
		self.start_index["related"] = 1
		self.results["related"] = 0
		if self.in_search == False or self.current_treeview_name == "related":
			self.progress_bar.set_text (_("Fetching related videos..."))
		self.get_results ("/feeds/api/videos/" + urllib.quote (youtube_id) + "/related?max-results=" + str (self.max_results), "related")

		if self.debug:
			print "Done activating row"

	def get_fmt_string (self):
		if self.gconf_client.get_int ("/apps/totem/connection_speed") >= 10:
			return "&fmt=18"
		else:
			return ""

	def resolve_t_param (self, youtube_id):
		"""We have to get the t parameter from the actual video page, since Google changed how their URLs work"""
		stream = urllib.urlopen ("http://youtube.com/watch?v=" + urllib.quote (youtube_id))
		regexp1 = re.compile ("swfArgs.*\"t\": \"([^\"]+)\"")
		regexp2 = re.compile ("</head>")

		contents = stream.read ()
		if contents != "":
			"""Check for the t parameter, which is now in a JavaScript array on the video page"""
			matches = regexp1.search (contents)
			if (matches != None):
				stream.close ()
				return matches.group (1)

			"""Check to see if we've come to the end of the <head> tag; in which case, we should give up"""
			if (regexp2.search (contents) != None):
				stream.close ()
				return ""

		stream.close ()
		return ""

	def on_starting_video (self, treeview, path, user_data):
		"""Display an error if the required GStreamer plugins aren't installed"""
		if self.gstreamer_plugins_present == False:
			self.totem.interface_error_with_link (_("Totem cannot play this type of media (%s) because you do not have the appropriate plugins to handle it.") % _("YouTube"),
							      _("Please install the necessary plugins and restart Totem to be able to play this media."),
							      "http://www.gnome.org/projects/totem/#codecs",
							      _("More information about media plugins"),
							      self.totem.get_main_window ())
			return False

		return True

	def on_open_in_web_browser_activated (self, action):
		model, rows = self.treeview[self.current_treeview_name].get_selection ().get_selected_rows ()
		iter = model.get_iter (rows[0])
		youtube_id = model.get_value (iter, 3)

		"""Open the video in the browser"""
		os.spawnlp (os.P_NOWAIT, "xdg-open", "xdg-open", "http://www.youtube.com/watch?v=" + urllib.quote (youtube_id) + self.get_fmt_string ())

	def on_button_press_event (self, widget, event):
		self.button_down = True

	def on_button_release_event (self, widget, event):
		self.button_down = False
		self.on_value_changed (self.vadjust[self.current_treeview_name])

	def on_value_changed (self, adjustment):
		"""Load more results when we get near the bottom of the treeview"""
		if not self.button_down and (adjustment.get_value () + adjustment.page_size) / adjustment.upper > 0.8 and self.results[self.current_treeview_name] >= self.max_results:
			if self.current_treeview_name == "search":
				if self.debug:
					print "Getting more results for search \"" + self.search_terms + "\" from offset " + str (self.start_index["search"])
				self.get_results ("/feeds/api/videos?vq=" + urllib.quote_plus (self.search_terms) + "&max-results=" + str (self.max_results) + "&orderby=relevance&start-index=" + str (self.start_index["search"]), "search", False)
			elif self.current_treeview_name == "related":
				if self.debug:
					print "Getting more related videos for video \"" + self.youtube_id + "\" from offset " + str (self.start_index["related"])
				self.get_results ("/feeds/api/videos/" + urllib.quote_plus (self.youtube_id) + "/related?max-results=" + str (self.max_results) + "&start-index=" + str (self.start_index["related"]), "related", False)

	def convert_url_to_id (self, url):
		"""Find the last clause in the URL; after the last /"""
		return url.split ("/").pop ()

	def populate_list_from_results (self, search_token, treeview_name, thread):
		"""Check to see if this search has been cancelled"""
		if search_token != self.search_token[treeview_name]:
			return False

		"""Check and acquire the lock"""
		if not thread.done:
			if self.current_treeview_name == treeview_name:
				self.progress_bar.pulse ()
			return True

		CallbackThread (self.process_next_thumbnail, search_token, treeview_name).start ()
		return False

	def process_next_thumbnail (self, search_token, treeview_name):
		"""Note that all the calls to gobject.idle_add are so that the respective
		   UI function calls are made in the main thread, since process_next_thumbnail
		   is run in the CallbackThread thread."""

		"""Check to see if this search has been cancelled"""
		if search_token != self.search_token[treeview_name]:
			return False

		"""Return if there are no results (or we've finished)"""
		if self.entry[treeview_name] == None or len (self.entry[treeview_name]) == 0:
			gobject.idle_add (self._clear_ui, treeview_name)

			self.entry[treeview_name] = None
			self.in_search[treeview_name] = False

			return False

		"""Only do one result at a time, as the thumbnail has to be downloaded; give them a temporary MRL until the real one is resolved before playing"""
		entry = self.entry[treeview_name].pop (0)
		self.results[treeview_name] += 1
		self.start_index[treeview_name] += 1
		youtube_id = self.convert_url_to_id (entry.id.text)

		"""Find the content tag"""
		for _element in entry.extension_elements:
			if _element.tag =="group":
				break

		content_elements = _element.FindChildren ("content")
		if len (content_elements) == 0:
			return True
		mrl = content_elements[0].attributes['url']

		"""Download the thumbnail and store it in a temporary location so we can get a pixbuf from it"""
		thumbnail_url = _element.FindChildren ("thumbnail")[0].attributes['url']
		try:
			filename, headers = urllib.urlretrieve (thumbnail_url)
		except IOError:
			return True

		try:
			pixbuf = gtk.gdk.pixbuf_new_from_file (filename)
		except gobject.GError:
			print "Could not open thumbnail " + filename + " for video. It has been left in place for investigation."
			return True

		"""Don't leak the temporary file"""
		os.unlink (filename)

		"""Get the video stream MRL"""
		t_param = self.resolve_t_param (youtube_id)

		if t_param != "":
			mrl = "http://www.youtube.com/get_video?video_id=" + urllib.quote (youtube_id) + "&t=" + urllib.quote (t_param) + self.get_fmt_string ()

		gobject.idle_add (self._append_to_liststore, treeview_name, pixbuf, entry.title.text, mrl, youtube_id, search_token)

		return True

	def _clear_ui (self, treeview_name):
		"""Revert the cursor"""
		window = self.vbox.window
		window.set_cursor (None)

		if self.in_search == True or self.current_treeview_name == treeview_name:
			"""Only blank the progress bar if we're the only search taking place"""
			self.progress_bar.set_fraction (0.0)
			self.progress_bar.set_text ("")

		return False

	def _append_to_liststore (self, treeview_name, pixbuf, title, mrl, id, search_token):
		"""Check to see if this search has been cancelled"""
		if search_token != self.search_token[treeview_name]:
			return False

		if self.in_search == True or self.current_treeview_name == treeview_name:
			self.progress_bar.set_fraction (float (self.results[treeview_name]) / float (self.max_results))
		self.liststore[treeview_name].append ([pixbuf, title, mrl, id])
		return False

	def on_search_button_clicked (self, button):
		search_terms = self.search_entry.get_text ()

		if self.debug:
			print "Searching for \"" + search_terms + "\""

		"""Focus the "Search" tab"""
		self.notebook.set_current_page (self.notebook_pages.index ("search"))

		self.search_terms = search_terms
		self.start_index["search"] = 1
		self.results["search"] = 0
		self.progress_bar.set_text (_("Fetching search results..."))
		self.get_results ("/feeds/api/videos?vq=" + urllib.quote_plus (search_terms) + "&orderby=relevance&max-results=" + str (self.max_results), "search")

	def on_search_entry_activated (self, entry):
		self.search_button.clicked ()

	def get_results (self, url, treeview_name, clear = True):
		if self.in_search[treeview_name] == True and clear == False:
			"""If we're trying to fetch more results while another search is happening, just cancel"""
			if self.debug:
				print "Cancelling getting more results due to existing incomplete search."
			self.in_search[treeview_name] = False
			return
		elif clear == False:
			self.results[self.current_treeview_name] = 0
			self.progress_bar.set_text (_("Fetching more videos..."))

		"""If we're trying to do another full search while one's already happening, just continue as
		   normal. The populate_list_from_results function will notice, and cancel the old search."""

		if clear:
			self.liststore[treeview_name].clear ()

		if self.debug:
			print "Getting results from URL \"" + url + "\""

		self.in_search[treeview_name] = True
		self.search_token[treeview_name] = random.random ()

		"""Give us a nice waiting cursor"""
		window = self.vbox.window
		window.set_cursor (gtk.gdk.Cursor (gtk.gdk.WATCH))
		if self.current_treeview_name == treeview_name:
			self.progress_bar.pulse ()

		thread = DownloadThread (self, url, treeview_name)
		gobject.timeout_add (350, self.populate_list_from_results, self.search_token[treeview_name], treeview_name, thread)
		thread.start()

