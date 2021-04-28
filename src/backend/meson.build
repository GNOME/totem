backend_inc = include_directories('.')

gst_inspect = find_program(
  'gst-inspect-1.0',
  join_paths(gst_dep.get_pkgconfig_variable('toolsdir'), 'gst-inspect-1.0'),
  required: false
)

assert(gst_inspect.found(),
       'Cannot find required GStreamer-1.0 tool "gst-inspect-1.0". It should be part of gstreamer-1_0-utils. Please install it.')

gst_base_plugins = [
  'playbin',
  'videoscale'
]

foreach plugin: gst_base_plugins
  r = run_command(gst_inspect, plugin)
  assert(r.returncode() == 0,
         'Cannot find required GStreamer-1.0 plugin "' + plugin + '". It should be part of gst-plugins-base. Please install it.')
endforeach

gst_good_plugins = [
  'autoaudiosink',
  'goom',
  'scaletempo'
]

foreach plugin: gst_good_plugins
  r = run_command(gst_inspect, plugin)
  assert(r.returncode() == 0,
         'Cannot find required GStreamer-1.0 plugin "' + plugin + '". It should be part of gst-plugins-good. Please install it.')
endforeach

sources = files(
  'bacon-time-label.c',
  'bacon-video-controls-actor.c',
  'bacon-video-spinner-actor.c',
  'bacon-video-widget-gst-missing-plugins.c',
  'bacon-video-widget.c',
  'clock.c',
  'totem-aspect-frame.c'
)

enum = 'bacon-video-widget-enums'
enum_headers = files('bacon-video-widget.h')

enum_sources = gnome.mkenums(
  enum,
  sources: enum_headers,
  c_template: enum + '.c.template',
  h_template: enum + '.h.template'
)

libbacon_video_widget_incs = [
  top_inc,
  gst_inc
]

libbacon_video_widget_deps = [
  gst_dep,
  dependency('gstreamer-base-1.0', version: gst_req_version),
  dependency('gstreamer-plugins-base-1.0', version: gst_req_version),
  gst_pbutils_dep,
  gst_tag_dep,
  dependency('gstreamer-audio-1.0'),
  gst_video_dep,
  dependency('clutter-1.0', version: '>= 1.17.3'),
  dependency('clutter-gst-3.0', version: '>= 2.99.2'),
  clutter_gtk_dep,
  dependency('cairo', version: '>= 1.14.0'),
  dependency('gsettings-desktop-schemas'),
  m_dep,
  libtotem_gst_helpers_dep,
  libtotem_gst_pixbuf_helpers_dep,
  libtotem_time_helpers_dep
]

libbacon_video_widget_cflags = common_flags + warn_flags + [
  '-DDATADIR="@0@"'.format(totem_pkgdatadir)
]

libbacon_video_widget = static_library(
  'baconvideowidget',
  sources: sources + enum_sources,
  include_directories: libbacon_video_widget_incs,
  dependencies: libbacon_video_widget_deps,
  c_args: libbacon_video_widget_cflags
)

libbacon_video_widget_dep = declare_dependency(
  link_with: libbacon_video_widget,
  include_directories: backend_inc,
  dependencies: libbacon_video_widget_deps,
  sources: enum_sources
)

backend_test = 'bvw-test'

executable(
  backend_test,
  backend_test + '.c',
  include_directories: libbacon_video_widget_incs,
  dependencies: libbacon_video_widget_dep,
  c_args: libbacon_video_widget_cflags + [
    '-DLOGO_PATH="@0@"'.format(join_paths(totem_pkgdatadir, meson.project_name(), 'totem_logo.png')),
    '-DG_LOG_DOMAIN="@0@"'.format(backend_test)
  ]
)
