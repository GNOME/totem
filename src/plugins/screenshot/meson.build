plugin_name = 'screenshot'

plugin_dir = join_paths(totem_pluginsdir, plugin_name)

plugin_files = files(
  'totem-' + plugin_name + '-plugin.c',
  'totem-gallery.c',
  'totem-gallery-progress.c'
)

plugin_files += gnome.compile_resources(
  'screenshotresources',
  'screenshot.gresource.xml',
  export: true,
  source_dir: '.',
  c_name: 'screenshot'
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

i18n.merge_file(
  input: plugin_data + '.desktop.in',
  output: plugin_data,
  type: 'desktop',
  po_dir: po_dir,
  install_dir: plugin_dir,
  install: true
)
