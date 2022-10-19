plugin_name = 'mpris'

plugin_dir = join_paths(totem_pluginsdir, plugin_name)

shared_module(
  plugin_name,
  sources: 'totem-' + plugin_name + '.c',
  include_directories: plugins_incs,
  dependencies: plugins_deps + [
    gio_dep
  ],
  c_args: plugins_cflags,
  install: true,
  install_dir: plugin_dir
)

plugin_data = plugin_name + '.plugin'

i18n.merge_file(
  input: plugin_data + '.desktop.in',
  output: plugin_data,
  type: 'desktop',
  po_dir: po_dir,
  install_dir: plugin_dir,
  install: true
)
