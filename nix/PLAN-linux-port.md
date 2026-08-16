# Linux Port Plan

## Current baseline

As of 2026-08-16, the portable and Linux-local foundations build successfully on this machine:

- CMake builds `build/c/libopentrackir.so`, `test_tir5`, and `opentrackir_stream_dump` with GCC and the system `libusb-1.0`.
- The native C test target passes.
- All nine Python protocol tests pass in the locked Python 3.12 environment.
- The GNOME starter app builds with Meson against GTK 4 and libadwaita.
- Its desktop file, AppStream metadata, and GSettings schema validation tests pass.
- The host GTK app links the installed shared library, streams from the physical TrackIR through an active-seat udev rule, and renders a conditional native grayscale preview.
- Linux session state, display policy, and metadata tests pass without requiring hardware.

The C library is therefore already compiling on Linux. What is not done is making it an installable dependency that the GNOME Builder/Flatpak build can discover and link. That integration boundary is the first implementation chunk.

## Architecture and performance constraints

- Shared protocol, frame, session, and mouse-tracker behavior stays in `c/`.
- The GTK main thread handles widgets and small state publications only.
- The existing `otir_trackir_session` worker continues to own USB reads and frame processing.
- Linux cursor output runs outside the GTK main thread behind a narrow adapter interface.
- The app copies a preview frame only when the window is visible and video is enabled.
- Telemetry publication is throttled independently from camera processing.
- A hidden window with mouse movement enabled keeps tracking but does no preview work.
- A hidden window with mouse movement disabled uses the shared low-power session mode or stops after the configured keep-awake interval.
- Platform failures remain independent: loss of tray support, mouse permission, or preview rendering must not destabilize TrackIR transport.
- Prefer GLib, GTK, libadwaita, libusb, and libevdev already supplied by the platform. Add another dependency only when it replaces substantial, error-prone code.

## Chunk 1: Make the shared C library consumable by the GNOME build

Goal: GNOME Builder can build the core and link the starter app to its public API without copying C sources into `nix/`.

Work:

1. Add CMake install rules for `libopentrackir`, its public headers, and a small `opentrackir.pc` pkg-config file.
2. Use build/install include interfaces so both the existing build tree and an installed prefix work.
3. Keep OpenCV disabled for the Linux app build; it remains an optional diagnostic harness only.
4. Add `dependency('opentrackir')` to the GNOME Meson target.
5. Change the Flatpak manifest to build the repository's CMake core module first and the `nix/` Meson app second. Remove the current absolute source path as part of that change.
6. Add a tiny Linux link test that calls a harmless public function; do not open hardware in this test.

Acceptance:

- The normal root CMake build and C tests remain green.
- A staged CMake install contains the library, headers, and pkg-config metadata.
- The host Meson build discovers that staged core through pkg-config.
- The GNOME Builder build links the same shared implementation.
- No TrackIR protocol source is duplicated under `nix/`.

## Chunk 2: Add the Linux session adapter and a minimal functional UI

Goal: the GTK app can start and stop an `otir_trackir_session` and display its state without cursor movement or video.

Work:

1. Add a Linux controller object that owns the shared session and exposes normalized app state.
2. Start/stop the session from explicit UI controls, not widget constructors.
3. Poll the cheap session snapshot at a bounded UI rate and publish changes on the GTK main context.
4. Show phase, error, frame index, source rate, packet type, and centroid.
5. Add GSettings keys only for controls introduced in this chunk.
6. Add pure GLib tests for state mapping and lifecycle policy.

Acceptance:

- No USB read or frame processing occurs on the GTK thread.
- The app remains responsive when the device is absent or access is denied.
- Starting and stopping repeatedly does not leak the worker, session, or GTK sources.

## Chunk 3: Establish TrackIR USB access and validate real streaming

Goal: run the shared session against the physical TrackIR on a normal host Linux session.

Work:

1. Test discovery and streaming outside Flatpak first so USB behavior is isolated from sandbox behavior.
2. Add a narrowly scoped udev rule for TrackIR vendor `131d`, product `0159`, using active-seat access rather than world-writable device permissions.
3. Teach the C error surface to distinguish USB permission denial from device absence if libusb currently collapses both into `OTIR_STATUS_IO`.
4. Validate initialize, stream, reconnect, stop, and unplug behavior with the C harness before using the GTK app.
5. For Flatpak development, start with the narrowest raw USB permission supported by the installed Flatpak version.
6. Treat the USB portal as a later hardening step because its acquired file descriptor will require an explicit libusb transport entrypoint rather than today's enumerate-and-open path.

Acceptance:

- The unprivileged host user can run `opentrackir_stream_dump` with no `sudo`.
- The UI clearly distinguishes not connected, permission denied, streaming, and transport failure.
- Disconnecting the camera cannot hang the app or GTK shutdown.

## Chunk 4: Build the GNOME UI and native preview

Goal: reach functional parity with the essential macOS/Windows controls while staying native and lightweight.

Work:

1. Replace the starter label with a compact libadwaita dashboard: status, preview, essential controls, and an advanced preferences area.
2. Render copied grayscale frames with a GTK/GDK-native texture; do not add OpenCV.
3. Reuse the shared 30 FPS preview cap and avoid texture creation when the preview is hidden.
4. Keep telemetry updates modest even if tracking runs at a higher rate.
5. Persist enablement, tracking rate, blob filter, smoothing, dead zone, jump filter, transform, timeout, and keep-awake settings with GSettings.
6. Keep formatting and state derivation in pure helpers with GLib tests.

Acceptance:

- Hiding video stops preview-frame copies and texture allocation.
- Hiding the window stops all preview work.
- Tracking and telemetry continue only when their configured behavior requires them.
- Idle CPU and memory use are measured with the window visible, hidden, and in low-power mode.

## Chunk 5: Implement Linux cursor output with libevdev and uinput

Goal: convert shared tracker deltas into efficient relative pointer motion through the Linux kernel input stack, independently of X11, Wayland, or a particular compositor.

Use libevdev's uinput API as the Linux output layer. Define a small adapter contract: probe support, open/start, post a relative delta, stop, and report a specific failure. The adapter consumes `otir_trackir_mouse_tracker_update()` results; it does not implement smoothing or transforms again.

Work:

1. Add a Linux mouse-output object that creates one relative-pointer device with `libevdev_uinput_create_from_device()`.
2. Advertise `EV_KEY`/`BTN_LEFT` so udev/libinput classify the device as a mouse, but never emit button events. Enable `EV_REL`, `REL_X`, and `REL_Y` for motion.
3. Emit only X/Y deltas followed by one `EV_SYN`/`SYN_REPORT` frame.
4. Run tracker updates and libevdev writes on a dedicated Linux worker thread. The GTK thread only changes configuration and displays output status.
5. Destroy the virtual device promptly when mouse movement is disabled or the application exits.
6. Map missing device, permission denied, unsupported kernel support, and write failure to distinct adapter states with actionable messages.
7. Add tests for event selection, delta rounding, disabled output, and error-state mapping without requiring a real uinput device.

`/dev/uinput` exists on the current machine as a root-owned mode `0600` device. Access must be granted deliberately; the app must never ask users to run OpenTrackIR as root.

### Host setup to ship and document

Provide a repository-owned udev rule. Native packages install it into `/usr/lib/udev/rules.d/70-opentrackir-uinput.rules`; manual installs copy the same file to `/etc/udev/rules.d/70-opentrackir-uinput.rules`:

```udev
KERNEL=="uinput", SUBSYSTEM=="misc", TAG+="uaccess", OPTIONS+="static_node=uinput"
```

This grants the active local desktop session access to uinput without granting access to physical `/dev/input/event*` devices and without making uinput world-writable.

The native package should also install `/usr/lib/modules-load.d/opentrackir-uinput.conf`; manual installs use `/etc/modules-load.d/opentrackir-uinput.conf`. Its contents are:

```text
uinput
```

Manual setup instructions for development and unpackaged installs must tell users to:

1. Copy the supplied rule and modules-load file into the paths above with root privileges.
2. Run `sudo modprobe uinput`.
3. Run `sudo udevadm control --reload-rules`.
4. Trigger the rule or reboot. If triggering does not update the static node, reboot or unload/reload `uinput` before troubleshooting the app.
5. Verify access as the desktop user with `test -w /dev/uinput` and inspect ACLs with `getfacl /dev/uinput`.

If `uaccess` is unavailable on a supported non-systemd distribution, document a fallback dedicated `uinput` group with mode `0660`. Do not recommend membership in the broad `input` group, because that may expose physical keyboard and pointer event devices.

Keep the TrackIR USB udev rule from Chunk 3 separate from the uinput rule. Camera access and virtual mouse output are independent permissions and their errors must remain distinguishable.

Acceptance:

- Pointer posting never runs on the GTK main thread.
- The unprivileged desktop user can create and destroy the OpenTrackIR virtual pointer after following the documented setup.
- Permission probing is explicit and produces actionable UI status.
- Losing uinput access stops output safely without stopping camera tracking.
- Mouse toggle, smoothing, transforms, dead zone, and jump filtering behave through the shared C tests and one Linux adapter integration test.
- Relative movement works on the current Hyprland/Wayland session without compositor-specific code.

## Chunk 6: Background lifecycle, tray integration, and power behavior

Goal: keep intentional head-mouse operation alive with the window hidden while consuming little CPU.

Work:

1. Separate window visibility from application/session lifetime with an explicit background-enabled state.
2. Implement hide/restore actions and hold the `GApplication` only while background operation is requested.
3. Add StatusNotifierItem integration as an optional desktop capability. Do not rely on the tray as the only way to reopen or quit because GNOME does not guarantee a tray host.
4. Keep a global mouse toggle only through an appropriate desktop permission/API; do not install a broad keylogger-style input permission.
5. Disable preview publication immediately when hidden and select shared low-power behavior when neither UI nor cursor output needs full tracking.
6. Measure sustained hidden CPU use and wakeups before calling the chunk complete.

Acceptance:

- Closing/hiding behavior is predictable and recoverable on GNOME and Hyprland.
- The user can always stop background tracking and quit even without tray support.
- Hidden operation does not allocate preview frames or repaint GTK widgets.
- Background mode has an explicit status indication and no silent permission failures.

## Chunk 7: Complete integration and distribution

Goal: produce a reproducible Linux application rather than a development-only host build.

Work:

1. Connect settings, TrackIR session, preview, mouse backend, errors, and background lifecycle end to end. (Complete on the current Hyprland system.)
2. Replace generated placeholder README/AppStream/about metadata and use the real project URLs and developer identity. (Complete.)
3. Make the Flatpak source portable and pin a released GNOME runtime instead of `master` for development builds.
4. Treat a native package as the primary Linux release until Flatpak can expose uinput without an unacceptably broad `--device=all` permission.
5. Choose and document the Flatpak TrackIR USB strategy independently: raw USB permission for compatibility or USB portal integration for least privilege.
6. Package the narrow TrackIR USB and uinput udev rules plus the uinput modules-load file; document reload, replug, reboot, and verification behavior. (Complete for the native Meson install.)
7. Add Linux build/test commands and permission setup to the root README and release process. (Root build and permission documentation complete; release automation remains.)

Acceptance:

- C, Python, and Linux GLib/Meson tests pass from clean build directories.
- The host build and native package are reproducible; the GNOME Builder Flatpak development build remains functional within its documented device limitations.
- Physical-device streaming and uinput cursor output are tested on the current Hyprland system and at least one other Linux desktop.
- Visible, hidden-tracking, hidden-low-power, unplug/reconnect, and permission-denied flows all behave intentionally.

## Immediate next step

Finish Chunk 7 by turning the verified staged native install into a distributable package, then make the GNOME Builder Flatpak source portable while keeping its device limitations explicit.
