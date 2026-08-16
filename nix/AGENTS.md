# Linux Agent Notes

The `nix/` directory contains the native GNOME application for Linux. Treat it as a GTK/libadwaita app layer that consumes the shared C library, not as another implementation of the TrackIR protocol.

## Current structure

- `meson.build`: Meson project definition, generated `config.h`, compiler warnings, and data/source/translation subdirectories.
- `org.gnome.opentrackir.json`: GNOME Builder Flatpak manifest using the GNOME SDK and runtime.
- `src/main.c`: gettext setup and `AdwApplication` entrypoint.
- `src/opentrackir-application.c`: application-owned settings/runtime, background holds, timeout/power policy, actions, shortcuts, and about dialog.
- `src/opentrackir-session-controller.c`: shared-session ownership, bounded telemetry polling, and runtime camera configuration.
- `src/opentrackir-session-state.c`: pure normalization of shared C session snapshots for the Linux UI.
- `src/opentrackir-display-logic.c`: pure preview, timeout, and telemetry formatting policy.
- `src/opentrackir-lifecycle-policy.c`: pure close, application-hold, tray visibility, and low-power policy.
- `src/opentrackir-uinput-policy.c`: pure error mapping, fractional delta dispatch, event selection, and mouse configuration mapping.
- `src/opentrackir-uinput-pointer.c`: Linux libevdev/uinput relative-pointer adapter.
- `src/opentrackir-mouse-worker.c`: dedicated tracker and pointer-output worker, isolated from GTK.
- `src/opentrackir-global-shortcut.c`: XDG Global Shortcuts portal adapter with a focused-window fallback.
- `src/opentrackir-xkeys-monitor.c`: blocking libudev/hidraw X-keys worker; report and speed policy stays in `opentrackir-xkeys-policy.c`.
- `src/opentrackir-status-notifier.c`: optional standard D-Bus StatusNotifierItem integration without a GTK 3 dependency.
- `src/opentrackir-window.c`: `AdwApplicationWindow` implementation and GTK template bindings.
- `src/opentrackir-window.ui`: main window template and primary menu.
- `src/shortcuts-dialog.ui`: keyboard-shortcut resource.
- `src/opentrackir.gresource.xml`: compiled GTK resource manifest.
- `data/`: desktop entry, AppStream metadata, GSettings schema, D-Bus service, application icons, and Meson installation of Linux system-integration files.
- `udev/`, `modules-load/`: narrowly scoped TrackIR USB, X-keys interface, and uinput permission rules plus boot-time uinput module loading for native installs.
- `../packaging/arch/`: Arch Linux/Omarchy `PKGBUILD`, install notice, and package build instructions.
- `po/`: gettext source list and language catalog configuration.

The host application links the shared C library and provides camera enablement, native grayscale preview, telemetry, persisted settings, advanced controls, libevdev/uinput mouse output, background operation, and optional StatusNotifierItem tray integration. TrackIR USB and uinput access use separate rules that are included in the native Meson install.

## Linux-specific rules

- Keep protocol parsing, frame reconstruction, centroid math, session orchestration, and reusable mouse tracking in the shared `c/` library.
- Consume the public headers under `c/include/opentrackir/`; do not copy or fork shared C behavior into `nix/`.
- Keep GTK, libadwaita, GDK textures, GLib main-loop integration, and application lifecycle code in `nix/`.
- Keep Linux cursor injection, permissions, compositor integration, and any X11- or Wayland-specific behavior in clearly named Linux adapters. Do not add those concerns to shared C.
- Do not assume that an X11 cursor-posting implementation also works under Wayland. Keep backend selection explicit and surface unsupported or denied functionality to the user.
- Keep blocking USB/session work off the GTK main thread. Publish immutable snapshots or copied frame buffers back to the UI without allowing GTK objects to cross worker-thread boundaries.
- Do not introduce OpenCV into the GNOME application. Use GTK/GDK-native image presentation for the preview.
- Treat Flatpak device access and cursor-control permissions as explicit product constraints. Do not silently broaden `finish-args`; document and validate each permission added.
- Preserve the application ID `org.gnome.opentrackir` consistently across C code, GResources, the Flatpak manifest, desktop file, AppStream metadata, GSettings schema, D-Bus service, and icon names unless an intentional migration changes all of them together.

## GTK and GNOME conventions

- Keep `OpentrackirApplication` responsible for application-wide actions and activation. Keep `OpentrackirWindow` responsible for window presentation and binding app state to widgets.
- Prefer GTK composite templates and resources for stable UI structure. Add each new resource to `src/opentrackir.gresource.xml`.
- Bind template children explicitly and clear owned objects or signal connections during disposal when lifecycle work is introduced.
- Keep user-visible strings translatable with `translatable="yes"` in UI/XML files or `_()` in C.
- Keep `po/POTFILES.in` sorted and update it when adding a source or resource containing translatable strings.
- Put persistent user preferences in the GSettings schema rather than scattering ad hoc config files through the app.
- Register actions before exposing them through menus or shortcut resources.
- Keep the desktop entry, AppStream metadata, icons, version, developer information, and about dialog aligned before release.
- Treat `org.gnome.opentrackir.json` as the authoritative Flatpak manifest. `org.gnome.opentrackir.json~` is an editor backup, not a second manifest to maintain.
- The current Flatpak source URL is an absolute path to this local checkout. Replace it with a portable source arrangement before expecting the manifest to work on another machine or in CI.

## Testing expectations

- Add one focused unit test for each new piece of Linux-side business logic that can be expressed without GTK or hardware.
- Keep protocol and shared mouse/session tests in `c/tests/`; Linux tests should cover adapter policy, state derivation, formatting, and other platform behavior.
- Prefer deterministic GLib tests over broad UI automation.
- Keep hardware integration checks separate from the default unit-test suite so normal validation does not require a TrackIR device.
- When metadata changes, retain the Meson validation tests for the desktop file, AppStream metadata, and GSettings schema.

## Build and validation

GNOME Builder should load `org.gnome.opentrackir.json` and build the application in its Flatpak environment. For a host build with the required GTK 4 and libadwaita development packages installed, use:

```sh
cd nix
meson setup builddir
meson compile -C builddir
meson test -C builddir --print-errorlogs
meson devenv -C builddir ./src/opentrackir
```

Use a fresh build directory or `meson setup --reconfigure builddir` after changing build definitions. Validate both the host build and GNOME Builder Flatpak build when changing dependencies, sandbox permissions, packaging, or hardware access.

Build Arch release packages outside nested user namespaces so fakeroot records `root:root`; confirm archive ownership with `bsdtar -tvf` and installed integrity with `pacman -Qkk opentrackir`. Run the installed integrity check in the host namespace because a sandbox may map host UID 0 to `nobody` and report false UID/GID mismatches.

## Change strategy

- Preserve the separation between shared behavior in `c/` and GNOME/Linux integration in `nix/`.
- Introduce the port incrementally: first link the public C API, then expose session state, then render preview frames, and finally add a Linux cursor backend and its permission/error UX.
- Make the smallest defensible UI or build-system change, and keep the starter project buildable while replacing placeholder content.
