src_inc = include_directories('.')
plugins_inc = include_directories('plugins')

subdir('gst')
subdir('backend')
subdir('properties')

enum_headers = files(
  'backend/bacon-time-label.h',
  'backend/bacon-video-controls-actor.h',
  'backend/bacon-video-spinner-actor.h',
  'backend/bacon-video-widget-gst-missing-plugins.h',
  'backend/bacon-video-widget.h',
  'backend/clock.h',
  'backend/totem-aspect-frame.h',
  'icon-helpers.h',
  'totem-grilo.h',
  'totem-interface.h',
  'totem-main-toolbar.h',
  'totem-menu.h',
  'totem-open-location.h',
  'totem-options.h',
  'totem-playlist.h',
  'totem-preferences.h',
  'totem-private.h',
  'totem-profile.h',
  'totem-properties-view.h',
  'totem-resources.h',
  'totem-search-entry.h',
  'totem-selection-toolbar.h',
  'totem-session.h',
  'totem-subtitle-encoding.h',
  'totem-time-label.h',
  'totem-uri.h',
  'totem.h'
)

gnome.mkenums(
  'org.gnome.totem.enums.xml',
  sources: enum_headers,
  comments: '<!-- @comment@ -->',
  fhead: '<schemalist>',
  vhead: '  <@type@ id="org.gnome.totem.@EnumName@">',
  vprod: '    <value nick="@valuenick@" value="@valuenum@"/>',
  vtail: '  </@type@>',
  ftail: '</schemalist>',
  install_header: true,
  install_dir: totem_schemadir
)

headers = files(
  'plugins/totem-plugin.h',
  'plugins/totem-dirs.h',
  'totem-interface.h',
  'totem.h'
)

install_headers(
  headers,
  subdir: join_paths(totem_includedir, totem_api_path)
)

common_cflags = warn_flags + [
  '-DDBUS_API_SUBJECT_TO_CHANGE',
  '-DDATADIR="@0@"'.format(totem_datadir),
  '-DGNOMELOCALEDIR="@0@"'.format(totem_localedir),
  '-DTOTEM_PLUGIN_DIR="@0@"'.format(totem_pluginsdir)
]

libtotem_player_sources = files(
  'totem-interface.c',
  'totem-time-label.c'
)

deps = [
  gtk_dep,
  libtotem_time_helpers_dep
]

cflags = common_cflags + ['-DG_LOG_DOMAIN="Totem"']

libtotem_player = static_library(
  'totem_player',
  sources: libtotem_player_sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

libtotem_sources = files(
  'plugins/totem-dirs.c',
  'plugins/totem-plugins-engine.c',
  'icon-helpers.c',
  'totem-grilo.c',
  'totem-main-toolbar.c',
  'totem-menu.c',
  'totem-object.c',
  'totem-open-location.c',
  'totem-options.c',
  'totem-playlist.c',
  'totem-preferences.c',
  'totem-search-entry.c',
  'totem-selection-toolbar.c',
  'totem-session.c',
  'totem-subtitle-encoding.c',
  'totem-uri.c'
)

libtotem_sources += gnome.compile_resources(
  'griloresources',
  'grilo.gresource.xml',
  export: true,
  c_name: '_' + meson.project_name()
)

incs = [
  top_inc,
  plugins_inc
]

deps = [
  gir_dep,
  gnome_desktop_dep,
  grilo_pls_dep,
  gst_tag_dep,
  libbacon_video_widget_dep,
  libgd_dep,
  libpeas_gtk_dep,
  libtotem_time_helpers_dep,
  totem_plparser_dep
]

libtotem = shared_library(
  meson.project_name(),
  sources: libtotem_sources + gen_sources,
  version: libversion,
  soversion: soversion,
  include_directories: incs,
  dependencies: deps,
  c_args: cflags,
  link_with: libtotem_player,
  install: true
)

libtotem_dep = declare_dependency(
  include_directories: src_inc,
  dependencies: [
    grilo_dep,
    gtk_dep,
    totem_plparser_dep
  ],
  link_with: libtotem
)

pkg.generate(
  libraries: libtotem,
  version: totem_version,
  name: 'Totem',
  description: 'Totem Movie Player plugin API',
  filebase: meson.project_name(),
  subdirs: totem_api_path,
  requires: [
    'gio-2.0 ' + glib_req_version,
    'gtk+-3.0 ' + gtk_req_version,
    'totem-plparser ' + totem_plparser_req_version
  ],
  variables: [
    'exec_prefix=' + totem_prefix,
    'pluginsdir=' + totem_pluginsdir
  ]
)

deps = [
  libbacon_video_widget_dep,
  libpeas_dep,
  totem_plparser_dep
]

executable(
  meson.project_name(),
  'totem.c',
  include_directories: incs,
  dependencies: deps,
  c_args: cflags,
  link_with: libtotem,
  install: true
)

programs = [
  ['Gallery', [], totem_libexecdir],
  ['Video', ['totem-resources.c'], totem_bindir]
]

deps = [
  libtotem_gst_helpers_dep,
  m_dep,
  totem_plparser_dep,
  dependency('gdk-3.0')
]

foreach program: programs
  name = 'totem-@0@-thumbnailer'.format(program[0].to_lower())

  executable(
    name,
    [name + '.c'] + program[1],
    include_directories: top_inc,
    dependencies: deps,
    c_args: common_cflags + ['-DG_LOG_DOMAIN="Totem@0@Thumbnailer"'.format(program[0])],
    install: true,
    install_dir: program[2]
  )
endforeach

sources = files(
  'icon-helpers.c',
  'test-icons.c'
)

deps = [
  gnome_desktop_dep,
  grilo_dep,
  gtk_dep
]

executable(
  'test-icons',
  sources,
  dependencies: deps,
  c_args: common_cflags,
  link_with: libtotem_player
)

if enable_nautilus
  sources = files(
    'totem-properties-main.c',
    'totem-properties-view.c'
  )

  deps = [
    gst_tag_dep,
    libbacon_video_widget_properties_dep,
    libnautilus_ext_dep
  ]

  if enable_easy_codec_installation
    deps += gst_pbutils_dep
  endif

  shared_module(
    'totem-properties-page',
    sources: sources + gen_sources,
    include_directories: top_inc,
    dependencies: deps,
    c_args: common_cflags + ['-DG_LOG_DOMAIN="TotemPropertiesPage"'],
    install: true,
    install_dir: nautilusdir
  )

  sources = files(
    'totem-properties-main.c',
    'totem-properties-view.c',
    'test-properties-page.c'
  )

  executable(
    'test-properties-page',
    sources + gen_sources,
    include_directories: top_inc,
    dependencies: deps,
    c_args: cflags,
    link_with: libtotem_player
  )
endif

if enable_introspection
  gir_incs = [
    'Gio-2.0',
    'GLib-2.0',
    'GObject-2.0',
    'Gtk-3.0',
    'TotemPlParser-1.0'
  ]

  libtotem_gir = gnome.generate_gir(
    libtotem,
    sources: libtotem_sources + libtotem_player_sources + headers,
    nsversion: totem_api_version,
    namespace: totem_gir_ns,
    symbol_prefix: totem_gir_ns.to_lower(),
    identifier_prefix: totem_gir_ns,
    includes: gir_incs,
    include_directories: src_inc,
    header: 'totem.h',
    export_packages: meson.project_name(),
    install: true
  )
endif

subdir('plugins')
