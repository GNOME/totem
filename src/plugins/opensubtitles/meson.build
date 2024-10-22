plugin_name = 'opensubtitles'

plugin_dir = join_paths(totem_pluginsdir, plugin_name)

gnome.compile_resources(
  'opensubtitles',
  'opensubtitles.gresource.xml',
  c_name: 'totem_opensubtitles',
  install_dir: plugin_dir,
  gresource_bundle: true,
  install: true,
)

plugin_files = files(
  plugin_name + '.py',
  'hash.py',
)

install_data(
  plugin_files,
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
       args: pylint_flags + files([ plugin_name + '.py', 'hash.py' ]),
       env: nomalloc,
       suite: ['lint'],
       timeout: 120)
endif
