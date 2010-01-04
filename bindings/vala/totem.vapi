[CCode (cprefix = "Totem", lower_case_cprefix = "totem_")]

namespace Totem {
	[CCode (cheader_filename = "totem.h")]
	public class Object : GLib.Object {
		[CCode (cname = "totem_object_get_type")]
		public static GLib.Type get_type ();

		[CCode (cname = "totem_file_opened")]
		public void file_opened (string mrl);
		[CCode (cname = "totem_file_closed")]
		public void file_closed ();
		[CCode (cname = "totem_metadata_updated")]
		public void metadata_updated (string artist, string title, string album, uint track_num);

		[CCode (cname = "totem_action_fullscreen_toggle")]
		public void action_fullscreen_toggle ();
		[CCode (cname = "totem_action_error", instance_pos = 3)]
		public void action_error (string title, string reason);

		[CCode (cname = "totem_add_to_playlist_and_play")]
		public void add_to_playlist_and_play (string uri, string display_name, bool add_to_recent);
		[CCode (cname = "totem_action_play")]
		public void action_play ();

		[CCode (cname = "totem_add_sidebar_page")]
		public void add_sidebar_page (string page_id, string title, Gtk.Widget main_widget);
		[CCode (cname = "totem_remove_sidebar_page")]
		public void remove_sidebar_page (string page_id);
	}

	[CCode (cheader_filename = "totem-cell-renderer-video.h")]
	public class CellRendererVideo : Gtk.CellRenderer {
		[CCode (cname = "totem_cell_renderer_video_get_type")]
		public static GLib.Type get_type ();
		[CCode (cname = "totem_cell_renderer_video_new")]
		public CellRendererVideo (bool use_placeholder);
	}

	[CCode (cheader_filename = "totem-video-list.h")]
	public class VideoList : Gtk.TreeView {
		[CCode (cname = "totem_video_list_get_type")]
		public static GLib.Type get_type ();
		[CCode (cname = "totem_video_list_new")]
		public VideoList ();
		[CCode (cname = "totem_video_list_get_ui_manager")]
		public Gtk.UIManager get_ui_manager ();

		public virtual bool starting_video (Gtk.TreePath path);
	}

	[CCode (cheader_filename = "totem-plugin.h")]
	public abstract class Plugin : GLib.Object {
		[CCode (has_construct_function = false)]
		protected Plugin ();

		[CCode (cname = "totem_plugin_get_type")]
		public static GLib.Type get_type ();

		[CCode (cname = "totem_plugin_activate")]
		public abstract bool activate (Totem.Object totem) throws GLib.Error;
		[CCode (cname = "totem_plugin_deactivate")]
		public abstract void deactivate (Totem.Object totem);

		[CCode (cname = "totem_plugin_is_configurable")]
		public virtual bool is_configurable ();
		[CCode (cname = "totem_plugin_create_configure_dialog")]
		public virtual Gtk.Widget create_configure_dialog ();
		[CCode (cname = "totem_plugin_load_interface")]
		public Gtk.Builder load_interface (string name, bool fatal, Gtk.Window parent);

		[CCode (cname = "totem_plugin_find_file")]
		public virtual weak string find_file (string file);
	}
}
