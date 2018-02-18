plugins_incs = [
  top_inc,
  plugins_inc
]

plugins_deps = [
  libbacon_video_widget_dep,
  libpeas_dep,
  libtotem_dep
]

plugins_cflags = common_flags + warn_flags + [
  '-DG_LOG_DOMAIN="Totem"'
]

plugins_conf = configuration_data()
plugins_conf.set('GETTEXT_PACKAGE', meson.project_name())

base_plugins = [
  'apple-trailers',
  'autoload-subtitles',
  'im-status',
  'media-player-keys',
  'ontop',
  'properties',
  'recent',
  'screensaver',
  'screenshot',
  'skipto',
  'variable-rate',
  'vimeo'
]

brasero_plugins = ['brasero-disc-recorder']

lirc_plugins = ['lirc']

nautilus_plugins = ['save-file']

python_plugins = [
  'dbusservice',
  'opensubtitles',
  'pythonconsole',
  'samplepython'
]

vala_plugins = [
  'rotation',
  'sample-vala',
  'zeitgeist-dp'
]

x11_plugins = ['gromit']

allowed_plugins = base_plugins + brasero_plugins + lirc_plugins + nautilus_plugins + python_plugins + vala_plugins + x11_plugins

if get_option('plugins_set') == 'all'
  enable_plugins = allowed_plugins
elif get_option('plugins_set') == 'base'
  enable_plugins = base_plugins
else
  enable_plugins = []
endif

enable_plugins += get_option('plugins')

# Sanity check: Make sure enabled extensions are valid and also remove duplicates
plugins = []
foreach plugin: enable_plugins
  assert(allowed_plugins.contains(plugin), 'Invalid plugin: ' + plugin)
  if not plugins.contains(plugin)
    plugins += plugin
  endif
endforeach

if plugins.contains('gromit')
  assert(gtk_dep.get_pkgconfig_variable('targets').contains('x11'), 'the gromit plugin is not supported on non-X11 targets')
endif

if plugins.contains('lirc')
  lirc_dep = dependency('lirc', required: false)
  assert(lirc_dep.found() and cc.has_function('lirc_init', dependencies: lirc_dep) and cc.has_header('lirc/lirc_client.h'),
         'lirc_client is required for the lirc plugin')
endif

if plugins.contains('brasero-disc-recorder')
  libxml_req_version = '>= 2.6.0'
  libxml_dep = dependency('libxml-2.0', version: libxml_req_version, required: false)
  gtk_x11_dep = dependency('gtk+-x11-3.0', version: gtk_req_version, required: false)
  assert(libxml_dep.found() and gtk_x11_dep.found(), 'libxml-2.0 ' + libxml_req_version + ' and gtk+-x11-3.0 is required for the brasero-disc-recorder plugin')
endif

if plugins.contains('save-file')
  assert(enable_nautilus, 'nautilus support required for the save-file plugin')
endif

foreach plugin: python_plugins
  assert(enable_python or not plugins.contains(plugin), 'python support required for the ' + plugin + ' plugin')
endforeach

foreach plugin: vala_plugins
  assert(enable_vapi or not plugins.contains(plugin), 'vala support required for the ' + plugin + ' plugin')
endforeach

if enable_vapi
  plugins_vala_deps = [declare_dependency(sources: libtotem_gir[0])]

  plugins_vala_flags = [
    '-Wno-unused-but-set-variable',
    '-Wno-unused-function',
    '-Wno-unused-variable'
  ]

  plugins_vala_args = [
    '--girdir=' + join_paths(meson.build_root(), 'src'),
    '--pkg=@0@-@1@'.format(totem_gir_ns, totem_api_version)
  ]

  if plugins.contains('zeitgeist-dp')
    zeitgeist_req_version = '>= 0.9.12'

    zeitgeist_dep = dependency('zeitgeist-2.0', version: zeitgeist_req_version, required: false)
    assert(zeitgeist_dep.found(), 'zeitgeist-2.0 ' + zeitgeist_req_version + ' support required for the zeitgeist-dp plugin')
  endif
endif

foreach plugin: plugins
  subdir(plugin)
endforeach
