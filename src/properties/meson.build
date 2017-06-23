libbacon_video_widget_properties = static_library(
  'libbaconvideowidgetproperties',
  sources: 'bacon-video-widget-properties.c',
  include_directories: [
    top_inc,
    src_inc
  ],
  dependencies: gtk_dep,
  c_args: common_flags + warn_flags + [
    '-DDATADIR="@0@"'.format(totem_pkgdatadir)
  ]
)

libbacon_video_widget_properties_dep = declare_dependency(
  link_with: libbacon_video_widget_properties,
  include_directories: include_directories('.'),
  dependencies: gtk_dep
)
