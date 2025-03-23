# Debugging Videos

As mentioned in the [README](README.md), we only support the application as
distributed from [Flathub](https://flathub.org/apps/details/org.gnome.Totem).

Bugs should be filed in the [GNOME GitLab](https://gitlab.gnome.org/GNOME/totem/issues/) 
if the problem still happens, once you've gone through the steps below.

## Development version

If you run into a bug that could be called an "interface bug", not strictly
related to video playback, please test out the development version first.

It's available on Flathub, and can be installed by running:
```
flatpak install --user https://flathub.org/beta-repo/appstream/org.gnome.Totem.Devel.flatpakref
```

If the problem still happens in the "Videos Preview" application, then
feel free to file an issue, and mention that you tested this version of
the application.

This development version is also useful for other types of bugs, make sure
to replace instances of `org.gnome.Totem` with `org.gnome.Totem.Devel` in the
commands mentioned below.

## Thumbnailer bugs

If you run into problems with crashes or problems thumbnailing videos in
Files or in the app itself, it's likely that the `totem-video-thumbnailer`
is involved, although rarely to blame.

The most important part of reporting a problem is going to be pinpointing
the video file that failed to thumbnail. Once you've figured it out, try
running:
```
totem-video-thumbnailer -v /path/to/video-file.mp4 /tmp/foo.png
```
or if the problem happens in the Videos app, run this first to enter
the Flatpak sandbox:
```
flatpak run --command=bash org.gnome.Totem.Devel
```

If you can reproduce the problem, then please file a bug and attach the
video file to your report. If the file is too big, then try using a lossless
video cutter (there are a few on Flathub) to clip the file to a more reasonable
size (make sure it still crashes...), or make the whole file available on a
third-party site. We'll let you know when the file has been downloaded.

## Video playback bugs

Most video playback bugs happen because of bugs or limitation in [GStreamer](https://gstreamer.freedesktop.org),
although it's also possible that those aren't easily reproducible because of
the way we use GStreamer.

After figuring out which video file causes problems, we'll want to see whether
the same problem happens in the GStreamer test application:
```
flatpak run --command=gst-play-1.0 org.gnome.Totem.Devel /path/to/file.mp4
```

If the problem can be reproduced, then file a bug directly against [GStreamer](https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/).
