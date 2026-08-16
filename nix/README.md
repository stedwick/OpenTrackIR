# OpenTrackIR for Linux

This directory contains the GTK 4 and libadwaita application for Linux. During host development, it links to a staged installation of the shared OpenTrackIR C library through pkg-config.

The native build requires a C toolchain, CMake, Meson, Ninja, pkg-config,
`libusb-1.0`, GTK 4 4.12 or newer, libadwaita 1.4 or newer, libevdev 1.10
or newer, and libudev. `desktop-file-validate`, `appstreamcli`, and
`glib-compile-schemas` enable the metadata validation tests.

## Host development build

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

## Reproducible native install tree

The shared C library and Linux app use separate build systems, but they can be
assembled into one package root without writing to the live system. Build the
core in a private prefix, configure the app for its final `/usr` prefix, and use
`DESTDIR` only while assembling the package tree:

```sh
core_build="$PWD/build/linux-core-release"
core_prefix="$PWD/build/linux-core-prefix"
app_build="$PWD/build/linux-app-release"
package_root="$PWD/build/linux-package-root"

cmake -S . -B "$core_build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$core_prefix" \
  -DOPENTRACKIR_BUILD_PREVIEW=OFF
cmake --build "$core_build"
ctest --test-dir "$core_build" --output-on-failure
cmake --install "$core_build"

meson setup "$app_build" nix \
  --prefix=/usr \
  -Dpkg_config_path="$core_prefix/lib/pkgconfig" \
  --buildtype=release
meson compile -C "$app_build"
meson test -C "$app_build" --print-errorlogs

DESTDIR="$package_root" cmake --install "$core_build" --prefix /usr
DESTDIR="$package_root" meson install -C "$app_build"
```

The resulting tree contains the runtime library, application, license, desktop
and D-Bus launchers, AppStream metadata, icons, GSettings schema, udev rules,
and modules-load configuration. Inspect it with:

```sh
find "$package_root" -type f -o -type l | sort
```

This tree is suitable as the input to a distro-native package. A local source
install may use the default `/usr/local` prefix instead; modern systemd-udev
loads rules and modules-load configuration from `/usr/local/lib` as well as
`/usr/lib`. Package recipes should use their distribution's standard hooks to
compile GSettings schemas and refresh the desktop and icon caches. Meson's
post-install step performs those updates automatically for a direct install.

## Arch Linux and Omarchy package

The repository includes an Arch `PKGBUILD` under `packaging/arch/`. It builds
the same C and Meson targets, runs their tests, and produces a package that can
be installed and removed cleanly through `pacman`. See
[`packaging/arch/README.md`](../packaging/arch/README.md) for the current local
build and installation commands.

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
install these same files under `/usr/lib/udev/rules.d/` and
`/usr/lib/modules-load.d/`. A Meson source install with the default prefix puts
them under `/usr/local/lib`. After installing or upgrading either form, load
the module and refresh device permissions once:

```sh
sudo modprobe uinput
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=misc --sysname-match=uinput
```

Unplug and reconnect the TrackIR so its USB node receives the new ACL. A reboot
performs both the module load and permission setup if either device is still
missing or inaccessible.

On a non-systemd distribution without `uaccess`, use a dedicated `uinput` group
and a narrowly scoped rule granting that group mode `0660`, then add only the
OpenTrackIR user to that group. Do not add users to the broad `input` group;
that can expose physical keyboard and pointer events. Keep this permission
separate from the TrackIR USB rule because camera access and virtual pointer
output are independent.

## Keyboard shortcut and X-keys pedal

OpenTrackIR requests `Shift+F7` through the standard desktop global-shortcut
portal. The desktop may ask you to approve or change it. The shortcut toggles
mouse movement while the window is visible or hidden. If the portal is not
available, `Shift+F7` still works while the OpenTrackIR window has focus.

Hyprland exposes application shortcut actions through the portal but leaves the
physical key assignment in the compositor configuration. OpenTrackIR detects
Omarchy's `bindings.lua`, Hyprland's main Lua config, or the classic
`hyprland.conf`. The **Global Hotkey** row explains which file it found.

Select **Install Hotkey** to make a timestamped backup, add Shift+F7, reload
Hyprland, and check for configuration errors. OpenTrackIR restores the original
file if Hyprland rejects the change. Select **Open Config** to open the detected
file in the default editor.

The Omarchy Lua binding is:

```lua
o.bind(
  "SHIFT + F7",
  "Toggle OpenTrackIR mouse movement",
  hl.dsp.global("org.gnome.opentrackir:toggle-mouse")
)
```

For a Hyprland installation that uses `hyprland.conf`, add:

```ini
bind = SHIFT, F7, global, org.gnome.opentrackir:toggle-mouse
```

The optional X-keys fast mode reads the middle pedal from its consumer-control
`hidraw` interface on a background thread. Install the narrow permission rule
for a development or unpackaged build:

```sh
sudo install -Dm644 \
  nix/udev/70-opentrackir-xkeys.rules \
  /etc/udev/rules.d/70-opentrackir-xkeys.rules
sudo udevadm control --reload-rules
```

Disconnect and reconnect the X-keys pedal. The rule covers models `05f3:042c`
and `05f3:0438`, and grants access only to interface `00`. It does not grant
access to the pedal's keyboard or mouse interfaces. Native packages install
the same rule automatically.

## Background operation

`Run in Background` is enabled by default. Closing the window hides it while the
camera runtime remains owned by the application. Use the tray icon to restore
the window; activating OpenTrackIR from the desktop or command line also restores
the existing instance. Secondary activation of the tray icon toggles mouse
movement. `Ctrl+Q` and the application's Quit menu action always stop the camera,
destroy the virtual pointer, and exit.

StatusNotifierItem support is optional. If the desktop has no tray host,
OpenTrackIR reports that in the Background status row and sends a recovery
notification when hidden. Launch OpenTrackIR again to restore it. Turn off
`Run in Background` if closing the window should quit immediately.

Preview publication and GTK telemetry updates stop while the window is hidden or
minimized. With mouse movement enabled, head tracking continues at the configured
rate. With mouse movement disabled, the shared session switches to its 2 FPS
low-power mode; the uinput worker sleeps between configured keep-awake nudges.

On the current Hyprland/Wayland development machine, 10-second samples measured
about 3.5% CPU with the preview visible, 1.7% hidden with full mouse tracking,
and 1.2% hidden in low-power/keep-awake mode. These are development measurements,
not hardware-independent guarantees.

The Flatpak manifest is not yet wired to build the shared C library. Native
builds are the supported path while the Flatpak USB and uinput permission model
is being evaluated. In particular, the native permission files installed above
do not grant a Flatpak access to host devices by themselves.
