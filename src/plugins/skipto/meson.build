plugin_name = 'skipto'

plugin_dir = join_paths(totem_pluginsdir, plugin_name)

plugin_files = files(
  'totem-' + plugin_name + '-plugin.c',
  'totem-time-entry.c',
  'totem-' + plugin_name + '.c'
)

plugin_files += gnome.compile_resources(
  'skiptoresources',
  'skipto.gresource.xml',
  export: true,
  source_dir: '.',
  c_name: 'skipto'
)

shared_module(
  plugin_name,
  sources: plugin_files,
  include_directories: plugins_incs,
  dependencies: plugins_deps,
  c_args: plugins_cflags,
  install: true,
  install_dir: plugin_dir
)

plugin_data = plugin_name + '.plugin'

custom_target(
  plugin_data,
  input: plugin_data + '.desktop.in',
  output: plugin_data,
  command: msgfmt_plugin_cmd,
  install: true,
  install_dir: plugin_dir
)
