plugin_name = 'zeitgeist-dp'

plugin_dir = join_paths(totem_pluginsdir, plugin_name)

plugin_files = files(
  'totem-' + plugin_name + '-plugin.vala',
  'bacon-video.vapi'
)

plugin_deps = [
  clutter_gtk_dep,
  gtk_dep,
  peas_dep,
  zeitgeist_dep
]

shared_module(
  'totem-' + plugin_name + '-plugin',
  sources: plugin_files,
  include_directories: plugins_incs + [
    backend_inc,
    src_inc
  ],
  dependencies: plugin_deps + plugins_vala_deps,
  c_args: plugins_cflags + plugins_vala_flags + [
    '--include=config.h'
  ],
  vala_args: plugins_vala_args,
  link_with: libtotem,
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
