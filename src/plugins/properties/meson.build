plugin_name = 'movie-properties'

plugin_dir = join_paths(totem_pluginsdir, 'properties')

resources = gnome.compile_resources(
  'properties-resources',
  'properties.gresource.xml',
  c_name: 'totem_properties',
)

shared_module(
  plugin_name,
  sources: [
    'totem-' + plugin_name + '.c',
    'bacon-video-widget-properties.c',
    resources[0]
  ],
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
