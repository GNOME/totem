appdata = 'org.gnome.Totem.appdata.xml'

appdata_file = i18n.merge_file ('appdata',
                 input: appdata + '.in',
                 output: appdata,
                 install: true,
                 install_dir: join_paths(totem_datadir, 'metainfo'),
                 po_dir: po_dir,)
# Validate Appdata
appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test(
    'validate-appdata', appstream_util,
    args: [
      'validate', '--nonet', appdata_file.full_path()
    ]
  )
endif
