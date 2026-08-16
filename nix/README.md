# OpenTrackIR for Linux

This directory contains the GTK 4 and libadwaita application for Linux. During host development, it links to a staged installation of the shared OpenTrackIR C library through pkg-config.

From the repository root:

```sh
opentrackir_prefix="$PWD/build/linux-prefix"

cmake -S . -B build \
  -DOPENTRACKIR_BUILD_PREVIEW=OFF \
  -DCMAKE_INSTALL_PREFIX="$opentrackir_prefix"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build

meson setup nix/builddir nix \
  -Dpkg_config_path="$opentrackir_prefix/lib/pkgconfig"
meson compile -C nix/builddir
meson test -C nix/builddir --print-errorlogs
meson devenv -C nix/builddir ./src/opentrackir
```

After changing Meson build definitions, reconfigure with the same pkg-config path:

```sh
meson setup nix/builddir nix --reconfigure \
  -Dpkg_config_path="$opentrackir_prefix/lib/pkgconfig"
```

Configure a GNOME Builder host build with that pkg-config path as well.

## TrackIR USB access

The application should run as your normal desktop user. Do not run it with `sudo`.
Install the repository's narrowly scoped udev rule once, then reload the rules and
reconnect the TrackIR:

```sh
sudo install -Dm644 \
  nix/udev/70-opentrackir-trackir.rules \
  /etc/udev/rules.d/70-opentrackir-trackir.rules
sudo udevadm control --reload-rules
```

Unplug and reconnect the TrackIR after reloading. If the `usbutils` package is
installed, confirm that it is visible with:

```sh
lsusb -d 131d:0159
```

The matching `/dev/bus/usb/BBB/DDD` node should have an ACL for the active desktop
user. Substitute the bus and device numbers printed by `lsusb` when checking it:

```sh
getfacl /dev/bus/usb/BBB/DDD
```

For a quick transport check before launching the GTK app, run:

```sh
./build/c/opentrackir_stream_dump
```

Press Ctrl+C to stop the stream. If access is denied, reconnect the TrackIR after
installing the rule and confirm that the current login session is active before
troubleshooting the application.

## Virtual pointer access

Mouse movement uses libevdev and the kernel's `/dev/uinput` interface. It works
under X11 and Wayland without compositor-specific plugins, but access must be
granted to the active desktop user. Never run OpenTrackIR as root.

For a development or unpackaged install, install the supplied udev rule and
modules-load configuration once:

```sh
sudo install -Dm644 \
  nix/udev/70-opentrackir-uinput.rules \
  /etc/udev/rules.d/70-opentrackir-uinput.rules
sudo install -Dm644 \
  nix/modules-load/opentrackir-uinput.conf \
  /etc/modules-load.d/opentrackir-uinput.conf
sudo modprobe uinput
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=misc --sysname-match=uinput
```

Verify access from the same desktop session that will run OpenTrackIR:

```sh
test -w /dev/uinput && echo "uinput is writable"
getfacl /dev/uinput
```

If `/dev/uinput` is still missing or not writable, reboot so the module and rule
are applied from startup. Alternatively, unload and reload `uinput` after the
rule is installed, provided no other application is using it. Native packages
should install these same files under `/usr/lib/udev/rules.d/` and
`/usr/lib/modules-load.d/`.

On a non-systemd distribution without `uaccess`, use a dedicated `uinput` group
and a narrowly scoped rule granting that group mode `0660`, then add only the
OpenTrackIR user to that group. Do not add users to the broad `input` group;
that can expose physical keyboard and pointer events. Keep this permission
separate from the TrackIR USB rule because camera access and virtual pointer
output are independent.

The Flatpak manifest is not yet wired to build the shared C library. Host builds are the supported development path while the initial Linux port is being implemented.
