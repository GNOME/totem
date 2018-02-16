libtotem_gst_helpers = static_library(
  'totemgsthelpers',
  sources: 'totem-gst-helpers.c',
  dependencies: gst_dep
)

deps = [
  gdk_pixbuf_dep,
  gst_tag_dep
]

libtotem_gst_pixbuf_helpers = static_library(
  'totemgstpixbufhelpers',
  sources: 'totem-gst-pixbuf-helpers.c',
  dependencies: deps
)

libtotem_time_helpers = static_library(
  'totemtimehelpers',
  sources: 'totem-time-helpers.c',
  dependencies: glib_dep
)

libtotem_time_helpers_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: glib_dep,
  link_with: libtotem_time_helpers
)

libtotem_gst_helpers_dep = declare_dependency(
  dependencies: [
    gdk_pixbuf_dep,
    gst_tag_dep,
    libtotem_time_helpers_dep
  ],
  link_with: [
    libtotem_gst_helpers,
    libtotem_gst_pixbuf_helpers
  ]
)

test_unit = 'test-time'

exe = executable(
  test_unit,
  test_unit + '.c',
  include_directories: top_inc,
  dependencies: glib_dep,
  link_with: libtotem_time_helpers
)

test(test_unit, exe)
