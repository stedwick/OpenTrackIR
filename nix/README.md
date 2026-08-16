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

PKG_CONFIG_PATH="$opentrackir_prefix/lib/pkgconfig" \
  meson setup nix/builddir nix
meson compile -C nix/builddir
meson test -C nix/builddir --print-errorlogs
```

After changing Meson build definitions, use the same `PKG_CONFIG_PATH` with `meson setup --reconfigure nix/builddir nix`. Configure a GNOME Builder host build with that pkg-config path as well.

The Flatpak manifest is not yet wired to build the shared C library. Host builds are the supported development path while the initial Linux port is being implemented.
