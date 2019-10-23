doc_module = meson.project_name()

private_headers = [
  'bacon-resize.h',
  'bacon-time-label.h',
  'bacon-video-controls-actor.h',
  'bacon-video-spinner-actor.h',
  'bacon-video-widget-common.h',
  'bacon-video-widget-gst-missing-plugins.h',
  'bacon-video-widget-properties.h',
  'clock.h',
  'debug.h',
  'gd-fullscreen-filter.h',
  'gd-tagged-entry.h',
  'gnome-screenshot-widget.h',
  'gsd-media-keys-window.h',
  'gstscreenshot.h',
  'icon-helpers.h',
  'screenshot-filename-builder.h',
  'totem-aspect-frame.h',
  'totem-dnd-menu.h',
  'totem-dvb-setup.h',
  'totem-edit-chapter.h',
  'totem-fullscreen.h',
  'totem-gallery-progress.h',
  'totem-gallery.h',
  'totem-grilo.h',
  'totem-gst-helpers.h',
  'totem-gst-pixbuf-helpers.h',
  'totem-main-toolbar.h',
  'totem-menu.h',
  'totem-module.h',
  'totem-open-location.h',
  'totem-options.h',
  'totem-playlist.h',
  'totem-plugins-engine.h',
  'totem-plugin-manager.h',
  'totem-preferences.h',
  'totem-private.h',
  'totem-profile.h',
  'totem-python-plugin.h',
  'totem-resources.h',
  'totem-screenshot-plugin.h',
  'totem-screenshot.h',
  'totem-search-entry.h',
  'totem-selection-toolbar.h',
  'totem-session.h',
  'totem-skipto-plugin.h',
  'totem-skipto.h',
  'totem-statusbar.h',
  'totem-subtitle-encoding.h',
  'totem-time-entry.h',
  'totem-time-helpers.h',
  'totem-time-label.h',
  'totem-tracker-widget.h',
  'totem-uri.h',
  'video-utils.h'
]

version_conf = configuration_data()
version_conf.set('TOTEM_VERSION_MAJOR', totem_major_version)
version_conf.set('TOTEM_VERSION_MINOR', totem_minor_version)
version_conf.set('TOTEM_VERSION_MICRO', totem_micro_version)

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
