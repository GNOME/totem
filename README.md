![icon](data/icons/scalable/org.gnome.Totem.svg)

# Videos
Videos is movie player for the GNOME desktop based on [GStreamer](https://gstreamer.freedesktop.org/).

[![Download on Flathub](https://flathub.org/api/badge?svg&locale=en)](https://flathub.org/apps/details/org.gnome.Totem)

## Support

The only version of totem that's supported is the one shipped on Flathub.
**Do not file bugs that don't happen with the Flathub version**.

If the problem still occurs with the Flathub version, please test the
Development version which can be installed with this command:

```
flatpak install --user https://flathub.org/beta-repo/appstream/org.gnome.Totem.Devel.flatpakref
```

See [DEBUGGING.md](DEBUGGING.md) for details on how to file bugs.

## Dependencies

- [GStreamer 1.6](http://gstreamer.freedesktop.org)
- [GNOME 3.x development platform](http://www.gnome.org)

## Controls

Almost all the controls are documented in the keyboard shortcuts dialogue,
accessible through `Ctrl+H`, or the *Keyboard Shortcuts* menu item. Some others
are listed in the table below.

| Function | Shortcut |
| :---      | :---      |
| `Ctrl+Q` | Quit |
| `Ctrl+W` | Press back button/Quit |
| `Mouse button 1 double-click` | Toggle full screen |
| `Middle mouse button click` | Play/Pause |

## Copyright

- Nifty media player icon by [Jakub Steiner](https://gitlab.gnome.org/jimmac)
- UI help by Seth Nickell and William Jon McCann
- For code copyright, see the [COPYING file](COPYING)

## Thanks

- Paul Cooper for sponsoring the Telestrator mode
- Lyndon Drake for sponsoring the video zooming feature
- Ryan Thiessen for sponsoring the Mozilla plugin
- Fluendo for sponsoring Ronald Bultje and Tim-Philipp Müller's work on GStreamer backend
- Collabora for sponsoring Tim-Philipp Müller's work on GStreamer backend
