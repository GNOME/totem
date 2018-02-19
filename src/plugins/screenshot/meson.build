sources = files(
  'totem-@0@-plugin.c'.format(plugin),
  'screenshot-filename-builder.c',
  'totem-gallery.c',
  'totem-gallery-progress.c'
)

deps = [
  libbacon_video_widget_dep,
  plugins_dep
]

test_cflags = [
  '-Wno-error=format-nonliteral',
  '-Wno-format-nonliteral'
]

cflags = cc.get_supported_arguments(test_cflags) + ['-DLIBEXECDIR="@0@"'.format(totem_libexecdir)]

shared_module(
  plugin,
  sources: sources,
  dependencies: deps,
  c_args: cflags,
  install: true,
  install_dir: plugin_dir
)

install_data(
  'gallery.ui',
  install_dir: plugin_dir
)
