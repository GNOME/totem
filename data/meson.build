desktop_sh = find_program('desktop.sh')
thumbnailer_sh = find_program('thumbnailer.sh')
mime_type_include_sh = find_program('mime-type-include.sh')
mime_type_imports_sh = find_program('mime-type-imports.sh')
uri_scheme_include_sh = find_program('uri-scheme-include.sh')

mime_type_list = files('mime-type-list.txt')
uri_schemes_list = files('uri-schemes-list.txt')

pod2man = find_program('pod2man', required: false)

if pod2man.found()
  thumbnailer_name = 'totem-video-thumbnailer'
  pod2man_opts = [
    '-c', '""',
    '-s', '1',
    '-q', 'none',
    '-n', thumbnailer_name,
    '-r', 'GNOME'
  ]

  custom_target(
    thumbnailer_name + '.1',
    input: thumbnailer_name + '.pod',
    output: thumbnailer_name + '.1',
    command: [pod2man, pod2man_opts, '@INPUT@', '@OUTPUT@'],
    install: true,
    install_dir: join_paths(totem_mandir, 'man1')
  )

  install_data(
    'totem.1',
    install_dir: join_paths(totem_mandir, 'man1')
  )
else
  message('Man page generation disabled')
endif

ui_resources = gnome.compile_resources(
  'totemresources',
  'totem.gresource.xml',
  export: true,
  source_dir: '.',
  c_name: 'totem'
)

r = run_command(desktop_sh, mime_type_list, uri_schemes_list, check: true)

desktop_conf = configuration_data()
desktop_conf.set('APPLICATION_ID', application_id)
desktop_conf.set('APPLICATION_NAME', application_name)
desktop_conf.set('MIME_TYPE', r.stdout().strip())

source_desktop = 'org.gnome.Totem.desktop'
dest_desktop = application_id + '.desktop'

desktop_in = configure_file(
  input: source_desktop + '.in.in',
  output: source_desktop + '.in',
  configuration: desktop_conf
)

desktop_file = i18n.merge_file (
    type: 'desktop',
    input: desktop_in,
    output: dest_desktop,
    po_dir: po_dir,
    install: true,
    install_dir: join_paths(totem_datadir, 'applications')
)
# Validate Desktop file
desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test(
      'validate-desktop',
      desktop_file_validate,
      depends: desktop_file,
      args: [desktop_file.full_path()]
      )
endif

service_conf = configuration_data()
service_conf.set('bindir', totem_bindir)
service_conf.set('application_id', application_id)

service = application_id + '.service'

configure_file(
  input: 'org.gnome.Totem.service.in',
  output: service,
  install: true,
  install_dir: join_paths(totem_datadir, 'dbus-1', 'services'),
  configuration: service_conf
)

r = run_command(thumbnailer_sh, mime_type_list, check: true)

thumbnailer_conf = configuration_data()
thumbnailer_conf.set('BINDIR', totem_bindir)
thumbnailer_conf.set('MIME_TYPE', r.stdout().strip())

thumbnailer = 'totem.thumbnailer'

configure_file(
  input: thumbnailer + '.in',
  output: thumbnailer,
  install: true,
  install_dir: join_paths(totem_datadir, 'thumbnailers'),
  configuration: thumbnailer_conf
)

gen_sources = []

totem_mime_header = 'totem-mime-types.h'

gen_sources += custom_target(
  totem_mime_header,
  output: totem_mime_header,
  capture: true,
  command: [mime_type_include_sh, mime_type_list]
)

nautilus_mime_header = 'nautilus-video-mime-types.h'

custom_target(
  nautilus_mime_header,
  output: nautilus_mime_header,
  capture: true,
  command: [mime_type_include_sh, '--nautilus', mime_type_list]
)

totem_mime_js = 'totemMimeTypes.js'

custom_target(
  totem_mime_js,
  output: totem_mime_js,
  capture: true,
  command: [mime_type_imports_sh, mime_type_list]
)

totem_uri_header = 'totem-uri-schemes.h'

gen_sources += custom_target(
  totem_uri_header,
  output: totem_uri_header,
  capture: true,
  command: [uri_scheme_include_sh, uri_schemes_list],
)

schema_conf = configuration_data()
schema_conf.set('GETTEXT_PACKAGE', meson.project_name())

schema = 'org.gnome.totem.gschema.xml'

configure_file(
  input: schema + '.in',
  output: schema,
  install: true,
  install_dir: totem_schemadir,
  configuration: schema_conf
)
# Validata GSchema
glib_compile_schemas = find_program('glib-compile-schemas', required: false)
if glib_compile_schemas.found()
  test(
      'validate-gschema', glib_compile_schemas,
      args: ['--strict', '--dry-run', meson.current_source_dir()]
      )
endif

install_data(
  'totem.convert',
  install_dir: join_paths(totem_datadir, 'GConf', 'gsettings')
)

subdir('metainfo')
subdir('icons')
