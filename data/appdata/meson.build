appdata = 'org.gnome.Totem.appdata.xml'

i18n.merge_file ('appdata',
                 input: appdata + '.in',
                 output: appdata,
                 install: true,
                 install_dir: join_paths(totem_datadir, 'metainfo'),
                 po_dir: po_dir,)
