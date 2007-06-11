using GLib;
using Totem;

class SampleValaPlugin: Totem.Plugin {
	public override void activate (Totem.Object totem) {
		stdout.printf ("Hello world\n");
	}

	public override void deactivate (Totem.Object totem) {
		stdout.printf ("Goodbye world\n");
	}
}


[ModuleInit]
public GLib.Type register_totem_plugin (GLib.TypeModule module)
{
	stdout.printf ("Registering plugin %s\n", "SampleValaPlugin");

	return typeof (SampleValaPlugin);
}
