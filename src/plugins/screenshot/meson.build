plugin_name = 'screenshot'

plugin_dir = join_paths(totem_pluginsdir, plugin_name)

plugin_files = files(
  'totem-' + plugin_name + '-plugin.c',
  'screenshot-filename-builder.c',
  'totem-gallery.c',
  'totem-gallery-progress.c'
)

test_cflags = [
  '-Wno-error=format-nonliteral',
  '-Wno-format-nonliteral'
]

plugin_cflags = plugins_cflags

foreach cflag: test_cflags
  if cc.has_argument(cflag)
    plugin_cflags += [cflag]
  endif
endforeach

shared_module(
  plugin_name,
  sources: plugin_files,
  include_directories: plugins_incs,
  dependencies: plugins_deps,
  c_args: plugin_cflags + ['-DLIBEXECDIR="@0@"'.format(totem_libexecdir)],
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

install_data(
  'gallery.ui',
  install_dir: plugin_dir
)