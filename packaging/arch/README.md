# Arch Linux package

This directory builds the native OpenTrackIR package for Arch Linux and
Omarchy. `makepkg` compiles the shared C library and GTK application, runs both
test suites, and assembles a package tracked by `pacman`.

The committed package source is pinned to an exact Git commit. Until that
commit is available from `origin`, build from the current local repository:

```sh
cd packaging/arch
package_source_cache="${XDG_CACHE_HOME:-$HOME/.cache}/makepkg/opentrackir"
mkdir -p "$package_source_cache"
OPENTRACKIR_SOURCE_URL="file://$(git -C ../.. rev-parse --show-toplevel)" \
  SRCDEST="$package_source_cache" \
  makepkg --cleanbuild
```

Keeping `SRCDEST` outside the checkout prevents makepkg's bare Git source cache
from appearing to editors as a nested repository. The generated package remains
in `packaging/arch/`.

Inspect the resulting package before installing it:

```sh
pacman -Qip ./opentrackir-*.pkg.tar.zst
pacman -Qlp ./opentrackir-*.pkg.tar.zst
bsdtar -tvf ./opentrackir-*.pkg.tar.zst | head
```

The archive listing must show `root root` ownership. Build release packages as
a normal user in a normal host session; running fakeroot inside an additional
user namespace can incorrectly record package files as `nobody:nobody`.

Install it with:

```sh
sudo pacman -U ./opentrackir-*.pkg.tar.zst
```

Arch's package hooks reload udev rules, compile the GSettings schemas, and
refresh the desktop and icon caches. To use pointer movement without rebooting,
load `uinput`, then reconnect the TrackIR so the new USB rule is applied:

```sh
sudo modprobe uinput
```

Remove the package with:

```sh
sudo pacman -Rns opentrackir
```

After installation, verify that every packaged file is present with the
expected metadata:

```sh
pacman -Qkk opentrackir
```

Run this check from a normal host shell. A nested user namespace may display
host UID 0 as `nobody` and produce false UID/GID mismatch warnings.
