plugin_name = 'samplepython'

plugin_install = false

plugin_dir = join_paths(totem_pluginsdir, 'sample-python')

if plugin_install
  plugin_data = plugin_name + '.plugin'

  i18n.merge_file(
    input: plugin_data + '.desktop.in',
    output: plugin_data,
    type: 'desktop',
    po_dir: po_dir,
    install_dir: plugin_dir,
    install: true
  )
endif

if pylint.found()
  test('pylint-' + plugin_name,
    pylint,
    args: pylint_flags + files([ plugin_name + '.py' ]),
    suite: ['lint'],
  )
endif
