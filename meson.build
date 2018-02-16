project(
  'totem', 'c',
  version: '3.26.0',
  license: 'GPL2+ with exception',
  default_options: 'buildtype=debugoptimized',
  meson_version: '>= 0.44.0'
)

totem_version = meson.project_version()
version_array = totem_version.split('.')
totem_major_version = version_array[0].to_int()
totem_minor_version = version_array[1].to_int()
totem_micro_version = version_array[2].to_int()

totem_api_version = '1.0'

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

totem_debug = get_option('buildtype').contains('debug')

cc = meson.get_compiler('c')

config_h = configuration_data()

# debug options
config_h.set('GNOME_ENABLE_DEBUG', totem_debug)

# defines
set_defines = [
  # package
  ['PACKAGE', meson.project_name()],
  ['TOTEM_API_VERSION', totem_api_version],
  ['VERSION', totem_version],
  # i18n
  ['GETTEXT_PACKAGE', meson.project_name()]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

# Compiler flags
common_flags = [
  '-DHAVE_CONFIG_H',
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

glib_req_version = '>= 2.35.0'
gtk_req_version = '>= 3.19.4'
gst_req_version = '>= 1.6.0'
grilo_req_version = '>= 0.3.0'
peas_req_version = '>= 1.1.0'
totem_plparser_req_version = '>= 3.10.1'

clutter_gst_dep = dependency('clutter-gst-3.0', version: '>= 2.99.2')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0')
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
enable_introspection = get_option('introspection')
if enable_introspection
  gir_req_version = '>= 0.6.7'
  assert(dependency('gobject-introspection-1.0', version: gir_req_version, required: false).found(),
         'gobject-introspection-1.0 ' + gir_req_version + ' required for introspection support')
endif

# missing plugins support
enable_easy_codec_installation = get_option('easy_codec_installation')
if enable_easy_codec_installation
  gst_pbutils_dep = dependency('gstreamer-pbutils-1.0')
endif
config_h.set('ENABLE_MISSING_PLUGIN_INSTALLATION', enable_easy_codec_installation,
             description: 'Whether we can and want to do installation of missing plugins')

# python support
enable_python = get_option('python')
if enable_python
  py3 = import('python3')

  assert(py3.find_python().found(), 'python3 required for python support')
  assert(find_program('pylint-3', 'pylint3', 'pylint', required: false).found(), 'pylint required for python support')

  pygobject_dep = dependency('pygobject-3.0', version: '>= 2.90.3')

  meson.add_install_script('meson_compile_python.py')
endif

# vala support
enable_vapi = get_option('vapi')
if enable_vapi
  vala_req_version = '>= 0.14.1'

  assert(enable_introspection, 'introspection support required for the vala plugins')
  assert(add_languages('vala', required: false) and meson.get_compiler('vala').version().version_compare(vala_req_version),
         'vala ' + vala_req_version + ' required for the vala plugins')
endif

# nautilus support
enable_nautilus = get_option('nautilus')
if enable_nautilus
  libnautilus_ext_dep = dependency('libnautilus-extension', version: '>= 2.91.3', required: false)
  assert(libnautilus_ext_dep.found(), 'libnautilus-extension required for nautilus support')

  nautilusdir = get_option('nautilusdir')
  if nautilusdir == ''
    nautilusdir = libnautilus_ext_dep.get_pkgconfig_variable('extensiondir')
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

if get_option('gtk_doc')
  subdir('docs/reference')
endif

meson.add_install_script('meson_post_install.py')

message('Totem was configured with the following options:')
message('Using the GStreamer-1.0 backend')
message('Easy codec installation support: ' + enable_easy_codec_installation.to_string())
message('Python plugin support: ' + enable_python.to_string())
message('Vala plugin support: ' + enable_vapi.to_string())
message('Nautilus properties page: ' + enable_nautilus.to_string())
if enable_nautilus
  message('installing nautilus plugin in: ' + nautilusdir)
endif
foreach plugin: allowed_plugins
  message('Plugin ' + plugin + ': ' + plugins.contains(plugin).to_string())
endforeach
message('End options')
