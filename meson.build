project(
  'totem', 'c',
  version: '3.25.90',
  license: 'GPL2+',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=gnu99',
    'warning_level=1'
  ],
  meson_version: '>= 0.41.0'
)

totem_version = meson.project_version()
version_array = totem_version.split('.')
totem_major_version = version_array[0].to_int()
totem_minor_version = version_array[1].to_int()
totem_micro_version = version_array[2].to_int()

totem_api_version = '1.0'
api_version_array = totem_version.split('.')
totem_api_major_version = api_version_array[0].to_int()
totem_api_minor_version = api_version_array[1].to_int()

totem_api_path = join_paths(meson.project_name(), totem_api_version)

totem_gir_ns = 'Totem'

totem_prefix = get_option('prefix')
totem_bindir = join_paths(totem_prefix, get_option('bindir'))
totem_datadir = join_paths(totem_prefix, get_option('datadir'))
totem_includedir = join_paths(totem_prefix, get_option('includedir'))
totem_libdir = join_paths(totem_prefix, get_option('libdir'))
totem_libexecdir = join_paths(totem_prefix, get_option('libexecdir'))
totem_localedir = join_paths(totem_prefix, get_option('localedir'))
totem_mandir = join_paths(totem_prefix, get_option('mandir'))

totem_pkglibdir = join_paths(totem_libdir, meson.project_name())
totem_pkgdatadir = join_paths(totem_datadir, meson.project_name())
totem_pkgincludedir = join_paths(totem_includedir, meson.project_name())

totem_pluginsdir = join_paths(totem_pkglibdir, 'plugins')
totem_schemadir = join_paths(totem_datadir, 'glib-2.0', 'schemas')

soversion = 0
current = 0
revision = 0
libversion = '@0@.@1@.@2@'.format(soversion, current, revision)

totem_buildtype = get_option('buildtype')

cc = meson.get_compiler('c')

config_h = configuration_data()

config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name())

# debug options
config_h.set('GNOME_ENABLE_DEBUG', totem_buildtype != 'release',
             description: 'Define if debugging is enabled')
config_h.set('NDEBUG', totem_buildtype == 'release',
             description: 'Define if debugging is disabled')

# package
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_BUGREPORT', 'http://bugzilla.gnome.org/enter_bug.cgi?product=totem')
config_h.set_quoted('PACKAGE_NAME', meson.project_name())
config_h.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), totem_version))
config_h.set_quoted('PACKAGE_TARNAME', meson.project_name())
config_h.set_quoted('PACKAGE_URL', 'https://wiki.gnome.org/Apps/Videos')
config_h.set_quoted('PACKAGE_VERSION', totem_version)
config_h.set_quoted('TOTEM_API_VERSION', totem_api_version)
config_h.set_quoted('VERSION', totem_version)

# headers
config_h.set('HAVE_DLFCN_H', cc.has_header('dlfcn.h'))
config_h.set('HAVE_INTTYPES_H', cc.has_header('inttypes.h'))
config_h.set('HAVE_MEMORY_H', cc.has_header('memory.h'))
config_h.set('HAVE_STDINT_H', cc.has_header('stdint.h'))
config_h.set('HAVE_STDLIB_H', cc.has_header('stdlib.h'))
config_h.set('HAVE_STRINGS_H', cc.has_header('strings.h'))
config_h.set('HAVE_STRING_H', cc.has_header('string.h'))
config_h.set('HAVE_SYS_STAT_H', cc.has_header('sys/stat.h'))
config_h.set('HAVE_SYS_TYPES_H', cc.has_header('sys/types.h'))
config_h.set('HAVE_UNISTD_H', cc.has_header('unistd.h'))

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

if totem_buildtype == 'debug' or totem_buildtype == 'debugoptimized'
  test_cflags = [
    '-fno-strict-aliasing',
    '-Wcast-align',
    '-Wmissing-declarations',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wpointer-arith'
  ]

  foreach cflag: test_cflags
    if cc.has_argument(cflag)
      common_flags += [cflag]
    endif
  endforeach

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

  foreach cflag: test_cflags
    if cc.has_argument(cflag)
      warn_flags += [cflag]
    endif
  endforeach

  add_project_arguments(common_flags, language: 'c')
endif

glib_req_version = '>= 2.35.0'
gtk_req_version = '>= 3.19.4'
gst_req_version = '>= 1.6.0'
grilo_req_version = '>= 0.3.0'
peas_req_version = '>= 1.1.0'
totem_plparser_req_version = '>= 3.10.1'

glib_dep =  dependency('glib-2.0', version: glib_req_version)
gobject_dep =  dependency('gobject-2.0', version: glib_req_version)
gio_dep = dependency('gio-2.0', version: '>= 2.43.4')
gtk_dep = dependency('gtk+-3.0', version: gtk_req_version)
gdk_x11_dep = dependency('gdk-x11-3.0', version: gtk_req_version)
gst_dep = dependency('gstreamer-1.0', version: gst_req_version)
gst_tag_dep = dependency('gstreamer-tag-1.0', version: '>= 0.11.93')
gst_video_dep = dependency('gstreamer-video-1.0')
peas_dep = dependency('libpeas-1.0', version: peas_req_version)
peas_gtk_dep = dependency('libpeas-gtk-1.0', version: peas_req_version)
totem_plparser_dep = dependency('totem-plparser', version: totem_plparser_req_version)
clutter_gtk_dep = dependency('clutter-gtk-1.0', version: '>= 1.8.1')
m_dep = cc.find_library('m', required: true)

libgd = subproject(
  'libgd',
  default_options: [
    'static=true',
    'with-gtk-hacks=true',
    'with-main-view=true',
    'with-tagged-entry=true',
    'with-view-common=true'
  ]
)
libgd_dep = libgd.get_variable('libgd_dep')

# introspection support
have_gir = false

introspection_option = get_option('enable-introspection')
if introspection_option != 'no'
  gir_dep = dependency('gobject-introspection-1.0', version: '>= 0.6.7', required: (introspection_option == 'yes'))

  if gir_dep.found()
    have_gir = true
  endif
endif

# missing plugins support
missing_plugins_deps = []

easy_codec_option = get_option('enable-easy-codec-installation')
if easy_codec_option != 'no'
  have_easy_codec = false

  gst_pbutils_dep = dependency('gstreamer-pbutils-1.0', required: (easy_codec_option == 'yes'))
  if gst_pbutils_dep.found()
    have_easy_codec = true
  endif

  missing_plugins_deps += gst_pbutils_dep
  config_h.set('ENABLE_MISSING_PLUGIN_INSTALLATION', have_easy_codec,
               description: 'Whether we can and want to do installation of missing plugins')
endif

# python support
have_python = false
python_deps = []

python_option = get_option('enable-python')
if python_option != 'no'
  python = import('python3').find_python()
  if python.found()
    python_req_version = '>= 3.0'

    r = run_command([python, '--version'])
    python_version = r.stdout().split(' ')[1]

    pygobject_dep = dependency('pygobject-3.0', version: '>= 2.90.3', required: false)
    pylint = find_program('pylint-3', 'pylint3', 'pylint', required: false)

    if python_version.version_compare(python_req_version) and pygobject_dep.found() and pylint.found()
      have_python = true
      python_deps += pygobject_dep
      meson.add_install_script('meson_compile_python.py')
    endif
  endif

  if not have_python
    str = 'python ' + python_req_version + ', pygobject or pylint not found'
    if python_option == 'yes'
      error(str)
    endif
    message(str + ', disabling Python support')
  endif
endif

# vala support
have_vala = false

vala_option = get_option('enable-vala')
if vala_option != 'no'
  if have_gir
    if add_languages('vala', required: false)
      vala_req_version = '>= 0.14.1'
      if meson.get_compiler('vala').version().version_compare(vala_req_version)
        have_vala = true
      endif
    endif

    if not have_vala
      str = 'you need vala ' + vala_req_version + ' installed to use vala plugins'
    endif
  else
    str = 'you need introspection support for the vala plugins'
  endif

  if not have_vala
    if vala_options == 'yes' or introspection_option == 'yes'
      error(str)
    endif
    message(str)
  endif
endif

# nautilus support
have_nautilus = false

if get_option('enable-nautilus') != 'no'
  libnautilus_ext_dep = dependency('libnautilus-extension', version: '>= 2.91.3', required: false)
  if libnautilus_ext_dep.found()
    nautilusdir = get_option('with-nautilusdir')
    if nautilusdir == ''
      nautilusdir = libnautilus_ext_dep.get_pkgconfig_variable('extensiondir')
    endif

    config_h.set('HAVE_NAUTILUS', true)
    have_nautilus = true
    message('installing nautilus plugin in ' + nautilusdir)
  else
    if get_option('enable-nautilus') == 'yes'
      error('nautilus support enabled but libnautilus-extension missing')
    else
      message('libnautilus-extension missing so disabling nautilus supportt')
    endif
  endif
endif

configure_file(
  output: 'config.h',
  configuration: config_h
)

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

po_dir = join_paths(meson.source_root(), 'po')

intltool_merge = find_program('intltool-merge')
intltool_cache = join_paths(po_dir, '.intltool-merge-cache')
intltool_desktop_cmd = [intltool_merge, '-d', '-u', '-c', intltool_cache, po_dir, '@INPUT@', '@OUTPUT@']
intltool_xml_cmd = [intltool_merge, '-x', '-u', '-c', intltool_cache, po_dir, '@INPUT@', '@OUTPUT@']

top_inc = include_directories('.')

subdir('po')
subdir('data')
subdir('help')
subdir('src')

if get_option('enable-gtk-doc')
  subdir('docs')
endif

meson.add_install_script('meson_post_install.py')

message('Totem was configured with the following options:')
message('** Using the GStreamer-1.0 backend')

str = 'Easy codec installation support'
if have_easy_codec
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

str = 'Vala plugin support'
if have_vala
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

str = 'Nautilus properties page'
if have_nautilus
  message('** ' + str + ' enabled')
else
  message('   ' + str + ' disabled')
endif

message('End options')
