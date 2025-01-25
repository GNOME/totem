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
  'autoload-subtitles',
  'im-status',
  'mpris',
  'open-directory',
  'opensubtitles',
  'properties',
  'pythonconsole',
  'recent',
  'rotation',
  'samplepython',
  'save-file',
  'screensaver',
  'screenshot',
  'variable-rate',
]

plugins = []

plugins_option = get_option('with-plugins')
if plugins_option != 'none'
  plugins += [
    'autoload-subtitles',
    'im-status',
    'mpris',
    'properties',
    'recent',
    'rotation',
    'save-file',
    'screensaver',
    'screenshot',
    'variable-rate',
  ]

  plugin_error = (plugins_option == 'all')

  if have_python
    plugins += [
      'opensubtitles',
      'pythonconsole',
      'samplepython'
    ]
  endif

  if have_libportal
    plugins += [
      'open-directory'
    ]
  endif
endif

pylint_flags = ['-d', 'C0111', '-d', 'W0511', '-d', 'F0401' ]

foreach plugin: plugins
  subdir(plugin)
endforeach
