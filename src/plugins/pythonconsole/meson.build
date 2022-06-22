plugin_name = 'pythonconsole'

plugin_dir = join_paths(totem_pluginsdir, plugin_name)

plugin_files = files(
  plugin_name + '.py',
  'console.py'
)

install_data(
  plugin_files,
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

schema = 'org.gnome.totem.plugins.' + plugin_name + '.gschema.xml'

configure_file(
  input: schema + '.in',
  output: schema,
  install: true,
  install_dir: totem_schemadir,
  configuration: plugins_conf
)

install_data(
  plugin_name + '.convert',
  install_dir: join_paths(totem_datadir, 'GConf', 'gsettings')
)

if pylint.found()
  test('pylint-' + plugin_name,
       pylint,
       args: pylint_flags + files([ plugin_name + '.py', 'console.py' ]),
       env: nomalloc,
       timeout: 120)
endif
