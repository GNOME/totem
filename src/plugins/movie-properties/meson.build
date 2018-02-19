deps = [
  libbacon_video_widget_dep,
  libbacon_video_widget_properties_dep,
  plugins_dep
]

shared_module(
  plugin,
  sources: 'totem-@0@.c'.format(plugin),
  dependencies: deps,
  install: true,
  install_dir: plugin_dir
)
