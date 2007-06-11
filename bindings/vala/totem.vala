[Import ()]
[CCode (cprefix = "Totem", lower_case_cprefix = "totem_")]

namespace Totem {
	[CCode (cheader_filename = "totem.h")]
	public class Object {
		[NoArrayLength]
		[CCode (cname = "totem_object_get_type")]
		public static GLib.Type get_type ();
	}

	[CCode (cheader_filename = "totem-plugin.h")]
	public class Plugin {
		[NoArrayLength]
		[CCode (cname = "totem_plugin_get_type")]
		public static GLib.Type get_type ();

		[NoArrayLength]
		[CCode (cname = "totem_plugin_activate")]
		public abstract void activate (Totem.Object totem);
		[NoArrayLength]
		[CCode (cname = "totem_plugin_deactivate")]
		public abstract void deactivate (Totem.Object totem);

		[NoArrayLength]
		[CCode (cname = "totem_plugin_is_configurable")]
		public virtual bool is_configurable ();
		[NoArrayLength]
		[CCode (cname = "totem_plugin_create_configure_dialog")]
		public virtual Gtk.Widget create_configure_dialog ();
		
		[NoArrayLength]
		[CCode (cname = "totem_plugin_find_file")]
		public virtual weak string find_file (string file);
	}

}
