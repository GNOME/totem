install_data(
  join_paths('scalable', application_id + '.svg'),
  install_dir: join_paths(totem_datadir, 'icons', 'hicolor', 'scalable', 'apps')
)

install_data(
  join_paths('symbolic', 'org.gnome.Totem-symbolic.svg'),
  rename : [application_id + '-symbolic.svg'],
  install_dir: join_paths(totem_datadir, 'icons', 'hicolor', 'symbolic', 'apps')
)

icons = [
  'audio-only-symbolic.svg',
  'broken-video-symbolic.svg'
]

foreach icon: icons
  install_data(
    join_paths('symbolic', icon),
    install_dir: join_paths(totem_datadir, 'icons', 'hicolor', 'scalable', 'status')
  )
endforeach
