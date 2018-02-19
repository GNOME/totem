sources = files(
  'bacon-video.vapi',
  'totem-@0@-plugin.vala'.format(plugin)
)

deps = [
  libbacon_video_widget_dep,
  libpeas_dep
]

shared_module(
  plugin,
  sources: sources,
  include_directories: incs + [src_inc],
  dependencies: deps + vala_deps,
  c_args: cflags + vala_flags + ['--include=config.h'],
  vala_args: vala_args,
  install: true,
  install_dir: plugin_dir
)
