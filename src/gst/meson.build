gst_inc = include_directories('.')

libtotem_gst_helpers_deps = [
  gst_dep,
  gst_tag_dep,
]

libtotem_gst_helpers = static_library(
  'totemgsthelpers',
  sources: 'totem-gst-helpers.c',
  dependencies: libtotem_gst_helpers_deps
)

libtotem_gst_helpers_dep = declare_dependency(
  link_with: libtotem_gst_helpers,
  include_directories: gst_inc,
  dependencies: libtotem_gst_helpers_deps
)

libtotem_gst_pixbuf_helpers_deps = [
  dependency('gdk-pixbuf-2.0'),
  gst_tag_dep,
  gst_video_dep
]

libtotem_gst_pixbuf_helpers = static_library(
  'totemgstpixbufhelpers',
  sources: 'totem-gst-pixbuf-helpers.c',
  dependencies: libtotem_gst_pixbuf_helpers_deps
)

libtotem_gst_pixbuf_helpers_dep = declare_dependency(
  link_with: libtotem_gst_pixbuf_helpers,
  include_directories: gst_inc,
  dependencies: libtotem_gst_pixbuf_helpers_deps
)

libtotem_time_helpers = static_library(
  'totemtimehelpers',
  sources: 'totem-time-helpers.c',
  dependencies: [ glib_dep, m_dep ]
)

libtotem_time_helpers_dep = declare_dependency(
  link_with: libtotem_time_helpers,
  include_directories: gst_inc,
  dependencies: glib_dep
)
