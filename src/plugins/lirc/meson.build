deps [
  lirc_dep,
  plugins_dep
]

shared_module(
  plugin,
  sources: 'totem-@0@.c'.format(plugin),
  dependencies: deps,
  install: true,
  install_dir: plugin_dir
)

install_data(
  'totem_lirc_default',
  install_dir: plugin_dir
)
