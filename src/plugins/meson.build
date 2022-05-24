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
  'skipto',
  'variable-rate',
  'vimeo',
]

plugins = []

plugins_option = get_option('with-plugins')
if plugins_option != 'none'
  plugins += [
    'apple-trailers',
    'autoload-subtitles',
    'im-status',
    'mpris',
    'properties',
    'recent',
    'rotation',
    'save-file',
    'screensaver',
    'screenshot',
    'skipto',
    'variable-rate',
    'vimeo'
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

pylint_flags = ['-d', 'C0111', '-d', 'W0511', '-d', 'F0401', '-d', 'C0326' ]

# FIXME: Move custom_target to i18n.merge_file when gettext acquires plugin support.
#        http://lists.gnu.org/archive/html/bug-gettext/2017-06/msg00001.html
msgfmt_plugin_cmd = [
  find_program('msgfmt'),
  '--desktop',
  '--keyword=Name',
  '--keyword=Description',
  '--template=@INPUT@',
  '-d', po_dir,
  '--output=@OUTPUT@'
]

foreach plugin: plugins
  subdir(plugin)
endforeach
