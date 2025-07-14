project(
  'totem', 'c',
  version: '43.0',
  license: 'GPL-3-or-later',
  default_options: 'buildtype=debugoptimized',
  meson_version: '>= 0.57.0'
)

totem_version = meson.project_version()
version_array = totem_version.split('.')
totem_minor_version = version_array[1]

totem_api_version = '1.0'

totem_api_path = join_paths(meson.project_name(), totem_api_version)

totem_gir_ns = 'Totem'

totem_prefix = get_option('prefix')
totem_bindir = join_paths(totem_prefix, get_option('bindir'))
totem_datadir = join_paths(totem_prefix, get_option('datadir'))
totem_libdir = join_paths(totem_prefix, get_option('libdir'))
totem_libexecdir = join_paths(totem_prefix, get_option('libexecdir'))
totem_localedir = join_paths(totem_prefix, get_option('localedir'))
totem_mandir = join_paths(totem_prefix, get_option('mandir'))

totem_pkglibdir = join_paths(totem_libdir, meson.project_name())
totem_pkgdatadir = join_paths(totem_datadir, meson.project_name())

totem_pluginsdir = join_paths(totem_pkglibdir, 'plugins')
totem_schemadir = join_paths(totem_datadir, 'glib-2.0', 'schemas')

soversion = 0
current = 0
revision = 0
libversion = '@0@.@1@.@2@'.format(soversion, current, revision)

totem_debug = get_option('buildtype').contains('debug')

cc = meson.get_compiler('c')

# The app_name needs to match src/totem.c
if get_option('profile') == 'development'
  profile = '.Devel'
  application_name = 'Videos Preview'
else
  profile = ''
  application_name = 'Videos'
endif

application_id = 'org.gnome.Totem@0@'.format(profile)

config_h = configuration_data()

# debug options
config_h.set('GNOME_ENABLE_DEBUG', totem_debug)

# defines
set_defines = [
  # package
  ['PACKAGE', meson.project_name()],
  ['TOTEM_API_VERSION', totem_api_version],
  ['APPLICATION_ID', application_id],
  ['VERSION', totem_version],
  # i18n
  ['GETTEXT_PACKAGE', meson.project_name()]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach
config_h.set10('DEVELOPMENT_VERSION', get_option('profile') == 'development')

# Compiler flags
common_flags = [
  '-DHAVE_CONFIG_H',
  '-D_REENTRANT',
  '-DGCONF_DISABLE_DEPRECATED',
  '-DGCONF_DISABLE_SINGLE_INCLUDES',
  '-DBONOBO_DISABLE_DEPRECATED',
  '-DBONOBO_DISABLE_SINGLE_INCLUDES',
  '-DBONOBO_UI_DISABLE_DEPRECATED',
  '-DBONOBO_UI_DISABLE_SINGLE_INCLUDES',
  '-DGNOME_DISABLE_DEPRECATED',
  '-DGNOME_DISABLE_SINGLE_INCLUDES',
  '-DLIBGLADE_DISABLE_DEPRECATED',
  '-DLIBGLADE_DISABLE_SINGLE_INCLUDES',
  '-DGNOME_VFS_DISABLE_DEPRECATED',
  '-DGNOME_VFS_DISABLE_SINGLE_INCLUDES',
  '-DWNCK_DISABLE_DEPRECATED',
  '-DWNCK_DISABLE_SINGLE_INCLUDES',
  '-DLIBSOUP_DISABLE_DEPRECATED',
  '-DLIBSOUP_DISABLE_SINGLE_INCLUDES'
]

warn_flags = []

if totem_debug
  test_cflags = [
    '-fno-strict-aliasing',
    '-Wcast-align',
    '-Wmissing-declarations',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wpointer-arith'
  ]

  common_flags += cc.get_supported_arguments(test_cflags)

  test_cflags = [
    '-Werror=format=2',
    '-Werror=implicit-function-declaration',
    '-Werror=init-self',
    '-Werror=missing-include-dirs',
    '-Werror=missing-prototypes',
    '-Werror=pointer-arith',
    '-Werror=return-type',
    '-Wstrict-prototypes'
  ]

  warn_flags += cc.get_supported_arguments(test_cflags)
endif

add_project_arguments(common_flags, language: 'c')

glib_req_version = '>= 2.72.0'
gtk_req_version = '>= 3.22.0'
hdy_req_version = '>= 1.5.0'
gst_req_version = '>= 1.21.1'
grilo_req_version = '>= 0.3.0'
peas_req_version = '>= 1.99.1'
gnome_desktop_req_version = '>= 43'
totem_plparser_req_version = '>= 3.26.5'
libportal_req_version = '>= 0.7'

glib_dep =  dependency('glib-2.0', version: glib_req_version)
gobject_dep =  dependency('gobject-2.0', version: glib_req_version)
gmodule_dep = dependency('gmodule-2.0', version: glib_req_version)
gio_dep = dependency('gio-2.0', version: '>= 2.43.4')
gtk_dep = dependency('gtk+-3.0', version: gtk_req_version)
targets = gtk_dep.get_variable(pkgconfig : 'targets')
if targets.split(' ').contains('x11')
  x11_dep = dependency('x11', version: '>= 1.8')
else
  x11_dep = dependency('', required : false)
endif
hdy_dep = dependency('libhandy-1', version: hdy_req_version)
gst_dep = dependency('gstreamer-1.0', version: gst_req_version)
gst_tag_dep = dependency('gstreamer-tag-1.0', version: gst_req_version)
gst_video_dep = dependency('gstreamer-video-1.0', version: gst_req_version)
gst_pbutils_dep = dependency('gstreamer-pbutils-1.0', version: gst_req_version)
peas_dep = dependency('libpeas-2', version: peas_req_version)
totem_plparser_dep = dependency('totem-plparser', version: totem_plparser_req_version)
m_dep = cc.find_library('m', required: true)

libgd = subproject(
  'libgd',
  default_options: [
    'static=true',
    'with-gtk-hacks=true',
    'with-main-view=true',
    'with-main-icon-view=true',
  ]
)
libgd_dep = libgd.get_variable('libgd_dep')

gir_dep = dependency('gobject-introspection-1.0', version: '>= 0.6.7')

# python support
have_python = false
python_deps = []

python_option = get_option('enable-python')
if python_option != 'no'
  python = import('python').find_installation('python3')
  if python.found()
    python_req_version = '>= 3.0'

    r = run_command([python, '--version'], check: true)
    python_version = r.stdout().split(' ')[1]

    pygobject_dep = dependency('pygobject-3.0', version: '>= 2.90.3', required: false)
    pylint = find_program('pylint-3', 'pylint3', 'pylint', required: false)
    nomalloc = environment({'MALLOC_PERTURB_': '0'})

    if python_version.version_compare(python_req_version) and pygobject_dep.found()
      have_python = true
      python_deps += pygobject_dep
      meson.add_install_script('meson_compile_python.py')
    endif
  endif

  if not have_python
    str = 'python ' + python_req_version + ', pygobject not found'
    if python_option == 'yes'
      error(str)
    endif
    message(str + ', disabling Python support')
  endif
endif

# libportal support
libportal_dep = dependency('libportal-gtk3', version: libportal_req_version, required: get_option('libportal'))
have_libportal = libportal_dep.found()

configure_file(
  output: 'config.h',
  configuration: config_h
)

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

po_dir = join_paths(meson.project_source_root(), 'po')

top_inc = include_directories('.')

subdir('po')
subdir('data')
if get_option('help')
  subdir('help')
endif
subdir('src')

if get_option('enable-gtk-doc')
  subdir('docs/reference')
endif

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
)

is_stable = (totem_minor_version != 'alpha' and
  totem_minor_version != 'beta' and
  totem_minor_version != 'rc')
if is_stable
  meson.add_dist_script(
    find_program('check-news.sh').full_path(),
    '@0@'.format(meson.project_version()),
    'NEWS',
    'data/metainfo/org.gnome.Totem.metainfo.xml.in.in'
  )
else
  meson.add_dist_script(
    find_program('check-news.sh').full_path(),
    '@0@'.format(meson.project_version()),
    'NEWS',
  )
endif

meson.add_dist_script(
  find_program('remove-flatpak-dist.sh').full_path()
)

message('Totem was configured with the following options:')
message('** Using the GStreamer-1.0 backend')

str = 'Development build'
if get_option('profile') == 'development'
  message('** ' + str + ' enabled')
else
  message('   ' + str + ' disabled')
endif

str = 'Python plugin support'
if have_python
  message('** ' + str + ' enabled')
else
  message('   ' + str + ' disabled')
endif

foreach plugin: allowed_plugins
  if plugins.contains(plugin)
    message('** ' + plugin + ' plugin enabled')
  else
    message('   ' + plugin + ' plugin disabled')
  endif
endforeach

message('End options')
