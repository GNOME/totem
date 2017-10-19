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
  'totem-chapters-utils.h',
  'totem-cmml-parser.h',
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
  'totem-properties-view.h',
  'totem-python-plugin.h',
  'totem-resources.h',
  'totem-sample-vala-plugin.h',
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

doc_path = join_paths(totem_datadir, 'gtk-doc', 'html', meson.project_name())

gnome.gtkdoc(
  meson.project_name(),
  main_xml: meson.project_name() + '-docs.xml',
  src_dir: [
    top_inc,
    src_inc,
    plugins_inc,
    backend_inc
  ],
  dependencies: libtotem_dep,
  scan_args: [
    '--rebuild-types',
    '--ignore-headers=' + ' '.join(private_headers),
  ],
  mkdb_args: [
    '--xml-mode',
    '--output-format=xml',
    '--name-space=' + meson.project_name()
  ],
  install: true,
  install_dir: doc_path
)
