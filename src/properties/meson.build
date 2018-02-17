libbacon_video_widget_properties = static_library(
  'baconvideowidgetproperties',
  sources: 'bacon-video-widget-properties.c',
  include_directories: [top_inc, src_inc],
  dependencies: gtk_dep,
  c_args: warn_flags + ['-DDATADIR="@0@"'.format(totem_pkgdatadir)]
)

libbacon_video_widget_properties_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: gtk_dep,
  link_with: libbacon_video_widget_properties
)
