doc_module = meson.project_name()

private_headers = [
  'bacon-time-label.h',
  'bacon-video-widget-properties.h',
  'icon-helpers.h',
  'screenshot-filename-builder.h',
  'totem-gallery-progress.h',
  'totem-gallery.h',
  'totem-grilo.h',
  'totem-gst-helpers.h',
  'totem-gst-pixbuf-helpers.h',
  'totem-main-toolbar.h',
  'totem-menu.h',
  'totem-open-location.h',
  'totem-options.h',
  'totem-playlist.h',
  'totem-plugins-engine.h',
  'totem-preferences.h',
  'totem-private.h',
  'totem-profile.h',
  'totem-resources.h',
  'totem-screenshot-plugin.h',
  'totem-search-entry.h',
  'totem-selection-toolbar.h',
  'totem-session.h',
  'totem-subtitle-encoding.h',
  'totem-time-helpers.h',
  'totem-uri.h',
]

version_conf = configuration_data()
version_conf.set('VERSION', totem_version)
version_xml = 'version.xml'

configure_file(
  input: version_xml + '.in',
  output: version_xml,
  configuration: version_conf
)

doc_path = join_paths(totem_prefix, gnome.gtkdoc_html_dir(doc_module))

gnome.gtkdoc(
  doc_module,
  main_xml: doc_module + '-docs.xml',
  src_dir: join_paths(meson.source_root(), 'src'),
  gobject_typesfile: join_paths(meson.current_source_dir(), 'totem.types'),
  dependencies: libtotem_dep,
  scan_args: '--ignore-headers=' + ' '.join(private_headers),
  mkdb_args: '--name-space=' + doc_module,
  fixxref_args: '--html-dir=' + doc_path,
  install: true,
  install_dir: doc_path
)
