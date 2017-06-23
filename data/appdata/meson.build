info_name = 'org.gnome.Totem.metainfo.xml'

custom_target(
  info_name,
  input: 'org.gnome.Totem.appdata.xml.in',
  output: info_name,
  command: intltool_xml_cmd,
  install: true,
  install_dir: join_paths(totem_datadir, 'appdata')
)
