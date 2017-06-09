plugin_name = 'ontop'

plugin_dir = join_paths(totem_pluginsdir, plugin_name)

shared_module(
  plugin_name,
  sources: 'totem-' + plugin_name + '.c',
  include_directories: plugins_incs,
  dependencies: plugins_deps,
  c_args: plugins_cflags,
  install: true,
  install_dir: plugin_dir
)

plugin_data = plugin_name + '.plugin'

custom_target(
  plugin_data,
  input: plugin_data + '.in',
  output: plugin_data,
  command: intltool_desktop_cmd,
  install: true,
  install_dir: plugin_dir
)
