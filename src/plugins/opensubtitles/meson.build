data_files = files(
  'hash.py',
  plugin + '.py',
  plugin + '.ui'
)

install_data(
  data_files,
  install_dir: plugin_dir
)

schema = 'org.gnome.totem.plugins.' + plugin + '.gschema.xml'

configure_file(
  input: schema + '.in',
  output: schema,
  configuration: plugins_conf,
  install: true,
  install_dir: totem_schemadir
)

install_data(
  plugin + '.convert',
  install_dir: join_paths(totem_datadir, 'GConf', 'gsettings')
)
