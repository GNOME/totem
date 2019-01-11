plugin_name = 'samplepython'

plugin_install = false

plugin_dir = join_paths(totem_pluginsdir, 'sample-python')

if plugin_install
  install_data(
    plugin_name + '.py',
    install_dir: plugin_dir
  )
endif

plugin_data = plugin_name + '.plugin'

custom_target(
  plugin_data,
  input: plugin_data + '.desktop.in',
  output: plugin_data,
  command: msgfmt_plugin_cmd,
  install: plugin_install,
  install_dir: plugin_dir
)

if pylint.found()
  test('pylint-' + plugin_name, pylint, args: pylint_flags + files([ plugin_name + '.py' ]))
endif
