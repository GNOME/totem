source_metainfo = 'org.gnome.Totem.metainfo.xml'
dest_metainfo = application_id + '.metainfo.xml'

metainfo_in = configure_file(
  input: source_metainfo + '.in.in',
  output: source_metainfo + '.in',
  configuration: desktop_conf
)

metainfo_file = i18n.merge_file (
    input: metainfo_in,
    output: dest_metainfo,
    install: true,
    install_dir: join_paths(totem_datadir, 'metainfo'),
    po_dir: po_dir,)

# Validate Metainfo
appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test(
      'validate-metainfo', appstreamcli,
      depends: metainfo_file,
      args: ['validate', '--no-net', '--explain', metainfo_file.full_path()]
      )
endif
