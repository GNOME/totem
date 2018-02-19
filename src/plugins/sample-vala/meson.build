deps = [
  gtk_dep,
  libpeas_dep
]

shared_module(
  plugin,
  sources: 'totem-@0@-plugin.vala'.format(plugin),
  dependencies: deps + vala_deps,
  c_args: vala_flags,
  vala_args: vala_args,
  install: true,
  install_dir: plugin_dir
)
