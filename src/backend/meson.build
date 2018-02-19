backend_inc = include_directories('.')

gst_inspect_name = 'gst-inspect-1.0'

gst_inspect = find_program(
  gst_inspect_name,
  join_paths(gst_dep.get_pkgconfig_variable('toolsdir'), gst_inspect_name),
  required: false
)

assert(gst_inspect.found(),
       'Cannot find required GStreamer-1.0 tool "@0@". It should be part of gstreamer-1_0-utils. Please install it.'.format(gst_inspect_name))

gst_plugins = [
  ['playbin', 'base'],
  ['videoscale', 'base'],
  ['autoaudiosink', 'good'],
  ['goom', 'good'],
  ['scaletempo', 'good']
]

foreach plugin: gst_plugins
  assert(run_command(gst_inspect, plugin[0]).returncode() == 0,
         'Cannot find required GStreamer-1.0 plugin "@0@". It should be part of gst-plugins-@1@. Please install it.'.format(plugin[0], plugin[1]))
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

deps = [
  clutter_gst_dep,
  clutter_gtk_dep,
  libtotem_gst_helpers_dep,
  m_dep,
  dependency('gsettings-desktop-schemas'),
  dependency('gstreamer-audio-1.0')
]

cflags = warn_flags + ['-DDATADIR="@0@"'.format(totem_pkgdatadir)]

if enable_easy_codec_installation
  deps += gst_pbutils_dep
endif

libbacon_video_widget = static_library(
  'baconvideowidget',
  sources: sources + enum_sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

libbacon_video_widget_dep = declare_dependency(
  sources: enum_sources[1],
  include_directories: backend_inc,
  dependencies: clutter_gtk_dep,
  link_with: libbacon_video_widget
)

if enable_easy_codec_installation
  backend_test = 'bvw-test'

  cflags = warn_flags + [
    '-DG_LOG_DOMAIN="@0@"'.format(backend_test),
    '-DLOGO_PATH="@0@"'.format(join_paths(totem_pkgdatadir, meson.project_name(), 'totem_logo.png'))
  ]

  executable(
    backend_test,
    backend_test + '.c',
    include_directories: top_inc,
    dependencies: clutter_gtk_dep,
    c_args: cflags,
    link_with: libbacon_video_widget
  )
endif
