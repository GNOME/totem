plugins_incs = [
  top_inc,
  plugins_inc
]

plugins_deps = [libtotem_dep]

plugins_cflags = common_flags + warn_flags + [
  '-DG_LOG_DOMAIN="Totem"'
]

plugins_conf = configuration_data()
plugins_conf.set('GETTEXT_PACKAGE', meson.project_name())

allowed_plugins = [
  'apple-trailers',
  'autoload-subtitles',
  'brasero-disc-recorder',
  'dbusservice',
  'gromit',
  'im-status',
  'lirc',
  'media-player-keys',
  'ontop',
  'opensubtitles',
  'properties',
  'pythonconsole',
  'recent',
  'rotation',
  'sample-vala',
  'samplepython',
  'save-file',
  'screensaver',
  'screenshot',
  'skipto',
  'variable-rate',
  'vimeo',
  'zeitgeist-dp'
]

plugins = []

plugins_option = get_option('with-plugins')
if plugins_option != 'none'
  plugins += [
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

  plugin_error = (plugins_option == 'all')

  gdk_targets = gtk_dep.get_pkgconfig_variable('targets')
  if gdk_targets.contains('x11')
    plugins += 'gromit'
  else
    str = 'the gromit plugin is not supported on non-X11 targets'
    if plugin_error
      error(str)
    endif
    message(str)
  endif

  lirc_dep = dependency('lirc', required: false)
  if lirc_dep.found() and cc.has_function('lirc_init', dependencies: lirc_dep) and cc.has_header('lirc/lirc_client.h')
    plugins += 'lirc'
  else
    str = 'you need lirc_client installed for the lirc plugin'
    if plugin_error
      error(str)
    endif
    message(str)
  endif

  libxml_req_version = '>= 2.6.0'
  libxml_dep = dependency('libxml-2.0', version: libxml_req_version, required: false)
  gtk_x11_dep = dependency('gtk+-x11-3.0', version: gtk_req_version, required: false)
  if libxml_dep.found() and gtk_x11_dep.found()
    plugins += 'brasero-disc-recorder'
  else
    str = 'you need libxml-2.0 ' + libxml_req_version + ' and gtk+-x11-3.0 to use the brasero-disc-recorder plugin'
    if plugin_error
      error(str)
    endif
    message(str)
  endif

  if have_nautilus
    plugins += 'save-file'
  else
    str = 'you need libnautilus-extension to use the save-file plugin'
    if plugin_error
      error(str)
    endif
    message(str)
  endif

  if have_python
    plugins += [
      'dbusservice',
      'opensubtitles',
      'pythonconsole',
      'samplepython'
    ]
  endif

  if have_vala
    plugins_vala_deps = [
      declare_dependency(sources: libtotem_gir[0])
    ]

    plugins_vala_flags = [
      '-Wno-unused-but-set-variable',
      '-Wno-unused-function',
      '-Wno-unused-variable'
    ]

    plugins_vala_args = [
      '--girdir=' + join_paths(meson.build_root(), 'src'),
      '--pkg=@0@-@1@'.format(totem_gir_ns, totem_api_version)
    ]

    plugins += [
      'rotation',
      'sample-vala'
    ]

    zeitgeist_dep = dependency('zeitgeist-2.0', version: '>= 0.9.12', required: false)
    if zeitgeist_dep.found()
      plugins += 'zeitgeist-dp'
    else
      str = 'you need zeitgeist-2.0 >= 0.9.12 to use the zeitgeist-dp plugin'
      if plugin_error
        error(str)
      endif
      message(str)
    endif
  endif
endif

pylint_flags = ['-d', 'C0111', '-d', 'W0511', '-d', 'F0401', '-d', 'C0326' ]

foreach plugin: plugins
  subdir(plugin)
endforeach
