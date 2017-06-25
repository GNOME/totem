plugin_name = 'sample-vala'

plugin_install = false

plugin_dir = join_paths(totem_pluginsdir, plugin_name)

plugin_deps = [
  gtk_dep,
  peas_dep
]

shared_module(
  plugin_name,
  sources: 'totem-' + plugin_name + '-plugin.vala',
  include_directories: plugins_incs,
  dependencies: plugin_deps + plugins_vala_deps,
  c_args: plugins_cflags + plugins_vala_flags,
  vala_args: plugins_vala_args,
  install: plugin_install,
  install_dir: plugin_dir
)

plugin_data = plugin_name + '.plugin'

custom_target(
  plugin_data,
  input: plugin_data + '.in',
  output: plugin_data,
  command: intltool_desktop_cmd,
  install: plugin_install,
  install_dir: plugin_dir
)
