plugin_name = 'dbusservice'

plugin_dir = join_paths(totem_pluginsdir, 'dbus')

install_data(
  plugin_name + '.py',
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

if pylint.found()
  test('pylint-' + plugin_name, pylint, args: pylint_flags + files([ plugin_name + '.py' ]))
endif
