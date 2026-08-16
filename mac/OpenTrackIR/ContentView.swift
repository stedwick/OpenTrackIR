//
//  ContentView.swift
//  OpenTrackIR
//
//  Created by Philip Brocoum on 3/21/26.
//

import KeyboardShortcuts
import SwiftUI

struct ContentView: View {
    @Environment(\.colorScheme) private var colorScheme
    @Environment(\.controlActiveState) private var controlActiveState
    @Environment(\.scenePhase) private var scenePhase
    @State private var isAdvancedMouseExpanded = false
    @State private var isPreviewCentroidMarkerEnabled = true
    @ObservedObject private var runtimeController: TrackIRRuntimeController
    @ObservedObject private var cameraController: TrackIRCameraController

    @MainActor
    init() {
        let runtimeController = TrackIRRuntimeController()
        _runtimeController = ObservedObject(wrappedValue: runtimeController)
        _cameraController = ObservedObject(wrappedValue: runtimeController.cameraController)
    }

    init(runtimeController: TrackIRRuntimeController) {
        _runtimeController = ObservedObject(wrappedValue: runtimeController)
        _cameraController = ObservedObject(wrappedValue: runtimeController.cameraController)
    }

    var body: some View {
        GeometryReader { geometry in
            let layout = dashboardLayout(for: geometry.size.width)
            let previewWidth = videoPreviewWidth(for: layout, width: geometry.size.width)
            let controlColumns = controlColumnCount(for: layout, width: geometry.size.width)

            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    headerSection
                    videoPreview(previewWidth: previewWidth)
                        .frame(maxWidth: .infinity, alignment: .center)
                    controlSection(columnCount: controlColumns)
                    advancedMouseControlsRow
                }
                .frame(maxWidth: 1_120, alignment: .topLeading)
                .padding(24)
                .frame(maxWidth: .infinity, alignment: .top)
            }
            .background(appBackground)
        }
        .frame(minWidth: 760, minHeight: 560)
        .onAppear {
            runtimeController.setWindowVisible(true)
            runtimeController.setScenePhase(scenePhase)
            runtimeController.setControlActiveState(controlActiveState)
        }
        .onChange(of: scenePhase) { _, _ in
            runtimeController.setScenePhase(scenePhase)
        }
        .onChange(of: controlActiveState) { _, _ in
            runtimeController.setControlActiveState(controlActiveState)
        }
        .onDisappear {
            runtimeController.shutdownForWindowClose()
        }
    }

    private var controlState: TrackIRControlState {
        runtimeController.controlState
    }

    private var isVideoEnabled: Bool {
        controlState.isVideoEnabled
    }

    private var isTrackIREnabled: Bool {
        controlState.isTrackIREnabled
    }

    private var isMouseMovementEnabled: Bool {
        controlState.isMouseMovementEnabled
    }

    private var mouseMovementSpeed: Double {
        controlState.mouseMovementSpeed
    }

    private var isXKeysFastMouseEnabled: Bool {
        controlState.isXKeysFastMouseEnabled
    }

    private var mouseSmoothing: Int {
        controlState.mouseSmoothing
    }

    private var mouseDeadzone: Double {
        controlState.mouseDeadzone
    }

    private var isAvoidMouseJumpsEnabled: Bool {
        controlState.isAvoidMouseJumpsEnabled
    }

    private var mouseJumpThresholdPixels: Int {
        controlState.mouseJumpThresholdPixels
    }

    private var minimumBlobAreaPoints: Int {
        controlState.minimumBlobAreaPoints
    }

    private var keepAwakeSeconds: Int {
        controlState.keepAwakeSeconds
    }

    private var isTimeoutEnabled: Bool {
        controlState.isTimeoutEnabled
    }

    private var timeoutSeconds: Int {
        controlState.timeoutSeconds
    }

    private var isVideoFlipHorizontalEnabled: Bool {
        controlState.isVideoFlipHorizontalEnabled
    }

    private var isVideoFlipVerticalEnabled: Bool {
        controlState.isVideoFlipVerticalEnabled
    }

    private var videoRotationDegrees: Double {
        controlState.videoRotationDegrees
    }

    private var videoFramesPerSecond: Double {
        controlState.videoFramesPerSecond
    }

    private var isDarkMode: Bool {
        colorScheme == .dark
    }

    private var headerSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(alignment: .top, spacing: 16) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("OpenTrackIR")
                        .font(.system(size: 30, weight: .semibold, design: .rounded))

                    Text("macOS TrackIR preview and controls")
                        .font(.headline)
                        .foregroundStyle(.secondary)
                }

                Spacer(minLength: 16)

                Button(action: runtimeController.refreshTrackIRCamera) {
                    Label("Refresh", systemImage: "arrow.clockwise")
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.regular)
            }

            HStack(spacing: 10) {
                statusChip(
                    title: isTrackIREnabled ? "TrackIR On" : "TrackIR Off",
                    systemImage: isTrackIREnabled ? "dot.radiowaves.left.and.right" : "power"
                )
                statusChip(
                    title: isVideoEnabled ? "Video Visible" : "Video Hidden",
                    systemImage: isVideoEnabled ? "video.fill" : "video.slash.fill"
                )
                statusChip(
                    title: isMouseMovementEnabled ? "Mouse On" : "Mouse Off",
                    systemImage: isMouseMovementEnabled ? "cursorarrow.motionlines" : "pause.circle.fill"
                )
            }
        }
    }

    private func videoPreview(previewWidth: CGFloat) -> some View {
        let videoTransform = previewVideoTransform(
            flipHorizontal: isVideoFlipHorizontalEnabled,
            flipVertical: isVideoFlipVerticalEnabled,
            rotationDegrees: videoRotationDegrees
        )
        let previewHeight = videoPreviewHeight(for: previewWidth)
        let telemetryWidth = previewTelemetryWidth(for: previewWidth)

        return VStack(spacing: 12) {
            VStack(spacing: 6) {
                Text(previewTitle)
                    .font(.title3.weight(.semibold))
                    .foregroundStyle(.primary)

                Text(previewMessage)
                    .font(.body)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
            }

            ZStack {
                RoundedRectangle(cornerRadius: 24, style: .continuous)
                    .fill(
                        LinearGradient(
                            colors: [
                                Color(red: 0.10, green: 0.12, blue: 0.18),
                                Color(red: 0.04, green: 0.05, blue: 0.08),
                            ],
                            startPoint: .topLeading,
                            endPoint: .bottomTrailing
                        )
                    )

                if isVideoEnabled, let previewImage = cameraController.previewImage {
                    Image(decorative: previewImage, scale: 1.0, orientation: .up)
                        .resizable()
                        .interpolation(.none)
                        .scaledToFill()
                        .scaleEffect(x: videoTransform.scaleX, y: videoTransform.scaleY)
                        .rotationEffect(.degrees(videoTransform.rotationDegrees))
                        .clipShape(RoundedRectangle(cornerRadius: 24, style: .continuous))
                }

                if shouldDrawPreviewCentroidMarker(
                    isVideoEnabled: isVideoEnabled,
                    hasPreviewImage: cameraController.previewImage != nil,
                    isMarkerEnabled: isPreviewCentroidMarkerEnabled
                ),
                   let centroidMarkerPosition = previewCentroidMarkerPosition(
                        centroidX: cameraController.centroidX,
                        centroidY: cameraController.centroidY,
                        frameWidth: Int(OTIR_TIR5V3_FRAME_WIDTH),
                        frameHeight: Int(OTIR_TIR5V3_FRAME_HEIGHT),
                        transform: videoTransform
                   ) {
                    Circle()
                        .fill(Color.red)
                        .frame(width: 10, height: 10)
                        .shadow(color: Color.black.opacity(0.35), radius: 3, x: 0, y: 1)
                        .position(
                            x: centroidMarkerPosition.x * previewWidth,
                            y: centroidMarkerPosition.y * previewHeight
                        )
                }

                RoundedRectangle(cornerRadius: 24, style: .continuous)
                    .strokeBorder(previewBorderColor, lineWidth: 1)

                if cameraController.previewImage == nil {
                    Image(systemName: isVideoEnabled ? "video.fill" : "video.slash.fill")
                        .font(.system(size: 54, weight: .light))
                        .foregroundStyle(isVideoEnabled ? Color.green.opacity(0.9) : Color.secondary)
                }
            }
            .frame(width: previewWidth, height: previewHeight)
            .shadow(color: Color.black.opacity(0.22), radius: 18, x: 0, y: 10)

            Toggle("Show detected blob center", isOn: $isPreviewCentroidMarkerEnabled)
                .toggleStyle(.checkbox)
                .frame(width: previewWidth, alignment: .center)

            HStack(spacing: 12) {
                previewMetric(title: "Device", value: cameraController.sourceLabel)
                previewMetric(
                    title: "FPS",
                    value: trackIRRateSummaryLabel(
                        maximumFramesPerSecond: videoFramesPerSecond,
                        sourceFrameRate: cameraController.frameRate
                    )
                )
                previewMetric(title: "Position", value: cameraController.centroidPairLabel)
                previewMetric(title: "Backend", value: cameraController.backendLabel)
            }
            .frame(width: telemetryWidth)
        }
    }

    private func controlSection(columnCount: Int) -> some View {
        sectionCard {
            VStack(alignment: .leading, spacing: 18) {
                Text("Controls")
                    .font(.title2.weight(.semibold))

                Text("Live controls for TrackIR, video, and mouse movement.")
                    .font(.body)
                    .foregroundStyle(.secondary)

                LazyVGrid(columns: gridColumns(count: columnCount), alignment: .leading, spacing: 14) {
                    controlRow(
                        title: "Enable TrackIR",
                        detail: "Toggle TrackIR.",
                        systemImage: "dot.radiowaves.left.and.right",
                        isOn: isTrackIREnabledBinding
                    )

                    controlRow(
                        title: "Show Video",
                        detail: "Show the camera preview.",
                        systemImage: "video",
                        isOn: isVideoEnabledBinding
                    )

                    controlRow(
                        title: "Enable Mouse Movement",
                        detail: "Toggle mouse movement.",
                        systemImage: "cursorarrow.motionlines",
                        isOn: isMouseMovementEnabledBinding
                    )

                    mouseSpeedControlRow

                    trackIRFramesPerSecondControlRow

                    mouseHotkeyControlRow
                        .gridCellColumns(columnCount == 1 ? 1 : 2)

                    videoControlsRow
                        .gridCellColumns(columnCount == 1 ? 1 : 2)
                }
            }
        }
    }

    private var previewMessage: String {
        trackIRPreviewMessage(
            isTrackIREnabled: isTrackIREnabled,
            isVideoEnabled: isVideoEnabled,
            phase: cameraController.phase,
            errorDescription: cameraController.lastErrorDescription
        )
    }

    private var previewTitle: String {
        if !isVideoEnabled {
            return "Video Preview Hidden"
        }

        switch cameraController.phase {
            case .streaming:
                return "Live TrackIR Camera"
            case .starting:
                return "Starting TrackIR Camera"
            case .unavailable:
                return "TrackIR Not Found"
            case .failed:
                return "TrackIR Camera Error"
            case .idle:
                return isTrackIREnabled ? "Video Preview Ready" : "TrackIR Off"
        }
    }

    private var appBackground: some View {
        LinearGradient(
            colors: [
                isDarkMode
                    ? Color(red: 0.08, green: 0.10, blue: 0.14)
                    : Color(red: 0.93, green: 0.95, blue: 0.98),
                isDarkMode
                    ? Color(red: 0.12, green: 0.15, blue: 0.22)
                    : Color(red: 0.86, green: 0.90, blue: 0.95),
            ],
            startPoint: .topLeading,
            endPoint: .bottomTrailing
        )
        .ignoresSafeArea()
    }

    private var chipFillColor: Color {
        isDarkMode ? Color.white.opacity(0.08) : Color.white.opacity(0.72)
    }

    private var chipBorderColor: Color {
        isDarkMode ? Color.white.opacity(0.12) : Color.black.opacity(0.06)
    }

    private var cardBorderColor: Color {
        isDarkMode ? Color.white.opacity(0.12) : Color.black.opacity(0.08)
    }

    private var previewBorderColor: Color {
        isDarkMode ? Color.white.opacity(0.10) : Color.white.opacity(0.16)
    }

    private var rowFillColor: Color {
        isDarkMode ? Color.white.opacity(0.06) : Color.white.opacity(0.64)
    }

    private func gridColumns(count: Int) -> [GridItem] {
        Array(repeating: GridItem(.flexible(minimum: 180), spacing: 14, alignment: .top), count: count)
    }

    private var advancedMouseControlColumns: [GridItem] {
        Array(repeating: GridItem(.flexible(minimum: 220), spacing: 14, alignment: .top), count: 2)
    }

    private func statusChip(title: String, systemImage: String) -> some View {
        HStack(spacing: 8) {
            Image(systemName: systemImage)
                .font(.subheadline.weight(.medium))
                .frame(width: 16, height: 16)

            Text(title)
                .font(.subheadline.weight(.medium))
        }
            .padding(.horizontal, 12)
            .padding(.vertical, 7)
            .frame(height: 34)
            .background(
                Capsule(style: .continuous)
                    .fill(chipFillColor)
            )
            .overlay(
                Capsule(style: .continuous)
                    .strokeBorder(chipBorderColor, lineWidth: 1)
            )
    }

    private func previewMetric(title: String, value: String) -> some View {
        VStack(spacing: 4) {
            Text(title.uppercased())
                .font(.caption.weight(.semibold))
                .foregroundStyle(.secondary)
            Text(value)
                .font(.callout.weight(.medium))
                .foregroundStyle(.primary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 12)
        .padding(.horizontal, 10)
        .background(rowFillColor)
        .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
    }

    private func controlRow(
        title: String,
        detail: String,
        systemImage: String,
        isOn: Binding<Bool>
    ) -> some View {
        controlCard {
            HStack(alignment: .top, spacing: 14) {
                controlCopy(title: title, detail: detail, systemImage: systemImage)

                Spacer(minLength: 16)

                Toggle("", isOn: isOn)
                    .labelsHidden()
                    .toggleStyle(.switch)
            }
        }
    }

    private var mouseSpeedControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 14) {
                HStack(alignment: .top, spacing: 14) {
                    controlCopy(
                        title: "Mouse Speed",
                        detail: "Adjust cursor speed.",
                        systemImage: "speedometer"
                    )

                    Spacer(minLength: 16)

                    Text(mouseSpeedValueLabel(for: normalizedMouseMovementControlSpeed(mouseMovementSpeed)))
                        .font(.title3.weight(.semibold))
                        .monospacedDigit()
                }

                VStack(alignment: .leading, spacing: 8) {
                    Slider(value: mouseMovementSpeedBinding, in: 1.0 ... 5.0, step: 0.2)

                    HStack {
                        Text("1x")
                        Spacer()
                        Text("5x")
                    }
                    .font(.caption)
                    .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var mouseHotkeyControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 14) {
                controlCopy(
                    title: "Mouse Toggle Hotkey",
                    detail: "Choose the toggle shortcut.",
                    systemImage: "keyboard"
                )

                KeyboardShortcuts.Recorder(for: .toggleMouseMovement)
                    .frame(maxWidth: .infinity, alignment: .center)

                Text("The shortcut stays active even when the window is closed.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var advancedMouseControlsRow: some View {
        sectionCard {
            VStack(alignment: .leading, spacing: 18) {
                Button {
                    withAnimation(.easeInOut(duration: 0.18)) {
                        isAdvancedMouseExpanded.toggle()
                    }
                } label: {
                    HStack(alignment: .top, spacing: 14) {
                        controlCopy(
                            title: "Advanced Controls",
                            detail: "Advanced tuning for blob detection, smoothing, keep-awake, and timeout.",
                            systemImage: "slider.horizontal.3"
                        )

                        Spacer(minLength: 16)

                        Image(systemName: isAdvancedMouseExpanded ? "chevron.up.circle.fill" : "chevron.down.circle.fill")
                            .font(.title3.weight(.semibold))
                            .foregroundStyle(Color.accentColor)
                            .padding(.top, 2)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)

                if isAdvancedMouseExpanded {
                    LazyVGrid(columns: advancedMouseControlColumns, alignment: .leading, spacing: 14) {
                        mouseSmoothingControlRow
                        mouseDeadzoneControlRow
                        mouseJumpFilterControlRow
                        keepAwakeControlRow
                        timeoutControlRow
                        blobDetectionControlRow
                        xKeysFastMouseControlRow
                    }
                }
            }
        }
    }

    private var mouseSmoothingControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 16) {
                HStack(alignment: .top, spacing: 14) {
                    controlCopy(
                        title: "Smoothing",
                        detail: "A moving average that smooths pointer movements.",
                        systemImage: "waveform.path.ecg"
                    )

                    Spacer(minLength: 16)

                    Text(mouseSmoothingValueLabel(for: mouseSmoothing))
                        .font(.title3.weight(.semibold))
                        .monospacedDigit()
                }

                VStack(alignment: .leading, spacing: 8) {
                    Slider(value: mouseSmoothingBinding, in: 1 ... 10, step: 1)

                    HStack {
                        Text("1")
                        Spacer()
                        Text("10")
                    }
                    .font(.caption)
                    .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var mouseDeadzoneControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 16) {
                HStack(alignment: .top, spacing: 14) {
                    controlCopy(
                        title: "Dead Zone",
                        detail: "Suppress tiny head movements.",
                        systemImage: "dot.scope"
                    )

                    Spacer(minLength: 16)

                    Text(mouseDeadzoneValueLabel(for: mouseDeadzone))
                        .font(.title3.weight(.semibold))
                        .monospacedDigit()
                }

                VStack(alignment: .leading, spacing: 8) {
                    Slider(value: mouseDeadzoneBinding, in: 0.0 ... 1.0, step: 0.01)

                    HStack {
                        Text("0.00")
                        Spacer()
                        Text("1.00")
                    }
                    .font(.caption)
                    .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var mouseJumpFilterControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 16) {
                controlCopy(
                    title: "Avoid Mouse Jumps",
                    detail: "Prevent sudden mouse skips before they move the cursor.",
                    systemImage: "arrow.trianglehead.branch"
                )

                Toggle("Enable Jump Filter", isOn: isAvoidMouseJumpsEnabledBinding)
                    .toggleStyle(.checkbox)

                integerSettingRow(
                    title: "Jump Threshold",
                    detail: "Prevent mouse skips over \(mouseJumpThresholdPixels) pixels.",
                    value: mouseJumpThresholdPixelsBinding,
                    suffix: "px",
                    isEnabled: isAvoidMouseJumpsEnabled
                )
            }
        }
    }

    private var blobDetectionControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 16) {
                controlCopy(
                    title: "Blob Detection",
                    detail: "Filter tiny blobs and use a previous-regularized centroid to calm tiny frame-to-frame brightness flicker.",
                    systemImage: "scope"
                )

                integerSettingRow(
                    title: "Minimum Blob Area",
                    detail: "Ignore blob candidates smaller than this many stripe points.",
                    value: minimumBlobAreaPointsBinding,
                    suffix: "pts"
                )

                Text("Centroid Formula: Previous-Regularized")
                    .font(.subheadline.weight(.semibold))

                Text(trackIRBlobCentroidModeDescription(for: trackIRDefaultBlobCentroidMode()))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var keepAwakeControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 16) {
                controlCopy(
                    title: "Keep Awake",
                    detail: "Move the cursor 1 px every N seconds while TrackIR is on and mouse movement is off. Use 0 to disable.",
                    systemImage: "cursorarrow.motionlines"
                )

                integerSettingRow(
                    title: "Interval",
                    detail: "How often to send the keep-awake nudge.",
                    value: keepAwakeSecondsBinding,
                    suffix: "sec"
                )
            }
        }
    }

    private var timeoutControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 16) {
                controlCopy(
                    title: "Timeout",
                    detail: "Set the number of seconds before OpenTrackIR turns itself off.",
                    systemImage: "timer"
                )

                Toggle("Enable Timeout", isOn: isTimeoutEnabledBinding)
                    .toggleStyle(.checkbox)

                integerSettingRow(
                    title: "Duration",
                    detail: trackIRTimeoutHelperText,
                    value: timeoutSecondsBinding,
                    suffix: "sec",
                    isEnabled: isTimeoutEnabled
                )

                Group {
                    if isTrackIREnabled,
                       isTimeoutEnabled,
                       runtimeController.timeoutDeadline != nil {
                        TimelineView(.periodic(from: .now, by: 1)) { context in
                            Text(trackIRTimeoutCountdownLabel(
                                isTrackIREnabled: isTrackIREnabled,
                                isTimeoutEnabled: isTimeoutEnabled,
                                deadline: runtimeController.timeoutDeadline,
                                now: context.date
                            ))
                        }
                    } else {
                        Text(trackIRTimeoutCountdownLabel(
                            isTrackIREnabled: isTrackIREnabled,
                            isTimeoutEnabled: isTimeoutEnabled,
                            deadline: runtimeController.timeoutDeadline,
                            now: .now
                        ))
                    }
                }
                .font(.system(.body, design: .monospaced).weight(.semibold))
                .foregroundStyle(.secondary)
            }
        }
    }

    private var xKeysFastMouseControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 16) {
                HStack(alignment: .center, spacing: 10) {
                    Circle()
                        .fill(xKeysIndicatorColor(for: cameraController.xKeysMonitorSnapshot.indicatorState))
                        .frame(width: 14, height: 14)

                    Text(xKeysIndicatorLabel(for: cameraController.xKeysMonitorSnapshot.indicatorState))
                        .font(.subheadline.weight(.medium))
                        .foregroundStyle(.secondary)
                }

                controlCopy(
                    title: "X-keys Fast Mode",
                    detail: "Hold the middle X-keys foot pedal for an instant 2.5x mouse speed boost.",
                    systemImage: "figure.run"
                )

                Toggle("Enable X-keys Foot Pedal Fast Mode", isOn: isXKeysFastMouseEnabledBinding)
                    .toggleStyle(.checkbox)
            }
        }
    }

    private var videoControlsRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 16) {
                controlCopy(
                    title: "Video Controls",
                    detail: "Adjust the preview and mouse movement transform.",
                    systemImage: "camera.filters"
                )

                VStack(alignment: .leading, spacing: 12) {
                    Toggle("Flip Horizontal", isOn: isVideoFlipHorizontalEnabledBinding)
                        .toggleStyle(.checkbox)

                    Toggle("Flip Vertical", isOn: isVideoFlipVerticalEnabledBinding)
                        .toggleStyle(.checkbox)
                }

                VStack(alignment: .leading, spacing: 14) {
                    HStack(alignment: .top, spacing: 14) {
                        controlCopy(
                            title: "Rotate",
                            detail: "Rotate the preview and mouse movement.",
                            systemImage: "rotate.right"
                        )

                        Spacer(minLength: 16)

                        Text(videoRotationValueLabel(for: videoRotationDegrees))
                            .font(.title3.weight(.semibold))
                            .monospacedDigit()
                    }

                    VStack(alignment: .leading, spacing: 8) {
                        Slider(value: videoRotationDegreesBinding, in: 0 ... 360, step: 1)

                        HStack {
                            Text("0°")
                            Spacer()
                            Text("360°")
                        }
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    }
                }
            }
        }
    }

    private var trackIRFramesPerSecondControlRow: some View {
        controlCard {
            VStack(alignment: .leading, spacing: 16) {
                HStack(alignment: .top, spacing: 14) {
                    controlCopy(
                        title: "TrackIR FPS",
                        detail: "Cap TrackIR processing.",
                        systemImage: "gauge.with.dots.needle.33percent"
                    )

                    Spacer(minLength: 16)

                    Text(trackIRFramesPerSecondValueLabel(for: videoFramesPerSecond))
                        .font(.title3.weight(.semibold))
                        .monospacedDigit()
                }

                VStack(alignment: .leading, spacing: 8) {
                    Slider(value: videoFramesPerSecondBinding, in: 0 ... 125, step: 1)

                    HStack {
                        Text("0")
                        Spacer()
                        Text("125")
                    }
                    .font(.caption)
                    .foregroundStyle(.secondary)
                }
            }
        }
    }

    private func controlCopy(title: String, detail: String, systemImage: String) -> some View {
        HStack(alignment: .top, spacing: 14) {
            Image(systemName: systemImage)
                .font(.title3)
                .foregroundStyle(Color.accentColor)
                .frame(width: 28, height: 28)
                .padding(8)
                .background(Color.accentColor.opacity(0.12))
                .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))

            VStack(alignment: .leading, spacing: 4) {
                Text(title)
                    .font(.headline)
                Text(detail)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private func controlCard<Content: View>(@ViewBuilder content: () -> Content) -> some View {
        content()
            .padding(18)
            .frame(maxWidth: .infinity, alignment: .topLeading)
            .background(rowFillColor)
            .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))
            .overlay(
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .strokeBorder(cardBorderColor, lineWidth: 1)
            )
    }

    private func sectionCard<Content: View>(@ViewBuilder content: () -> Content) -> some View {
        content()
            .padding(22)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .fill(.regularMaterial)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .strokeBorder(cardBorderColor, lineWidth: 1)
            )
    }

    private func integerSettingRow(
        title: String,
        detail: String,
        value: Binding<Int>,
        suffix: String,
        isEnabled: Bool = true
    ) -> some View {
        HStack(alignment: .top, spacing: 14) {
            VStack(alignment: .leading, spacing: 4) {
                Text(title)
                    .font(.headline)
                Text(detail)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }

            Spacer(minLength: 16)

            HStack(spacing: 8) {
                TextField("", value: value, format: .number)
                    .textFieldStyle(.roundedBorder)
                    .frame(width: 96)
                    .multilineTextAlignment(.trailing)
                    .disabled(!isEnabled)
                Text(suffix)
                    .font(.subheadline.weight(.medium))
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .fixedSize(horizontal: true, vertical: false)
            }
        }
    }

    private var mouseMovementSpeedBinding: Binding<Double> {
        Binding(
            get: { normalizedMouseMovementControlSpeed(mouseMovementSpeed) },
            set: { runtimeController.setMouseMovementSpeed(normalizedMouseMovementControlSpeed($0)) }
        )
    }

    private var mouseSmoothingBinding: Binding<Double> {
        Binding(
            get: { Double(mouseSmoothing) },
            set: { runtimeController.setMouseSmoothing($0) }
        )
    }

    private var mouseDeadzoneBinding: Binding<Double> {
        Binding(
            get: { mouseDeadzone },
            set: { runtimeController.setMouseDeadzone($0) }
        )
    }

    private var isTrackIREnabledBinding: Binding<Bool> {
        Binding(
            get: { isTrackIREnabled },
            set: { runtimeController.setTrackIREnabled($0) }
        )
    }

    private var isVideoEnabledBinding: Binding<Bool> {
        Binding(
            get: { isVideoEnabled },
            set: { runtimeController.setVideoEnabled($0) }
        )
    }

    private var isMouseMovementEnabledBinding: Binding<Bool> {
        Binding(
            get: { isMouseMovementEnabled },
            set: { runtimeController.setMouseMovementEnabled($0) }
        )
    }

    private var isXKeysFastMouseEnabledBinding: Binding<Bool> {
        Binding(
            get: { isXKeysFastMouseEnabled },
            set: { runtimeController.setXKeysFastMouseEnabled($0) }
        )
    }

    private var isAvoidMouseJumpsEnabledBinding: Binding<Bool> {
        Binding(
            get: { isAvoidMouseJumpsEnabled },
            set: { runtimeController.setAvoidMouseJumpsEnabled($0) }
        )
    }

    private var mouseJumpThresholdPixelsBinding: Binding<Int> {
        Binding(
            get: { mouseJumpThresholdPixels },
            set: { runtimeController.setMouseJumpThresholdPixels($0) }
        )
    }

    private var minimumBlobAreaPointsBinding: Binding<Int> {
        Binding(
            get: { minimumBlobAreaPoints },
            set: { runtimeController.setMinimumBlobAreaPoints($0) }
        )
    }

    private var keepAwakeSecondsBinding: Binding<Int> {
        Binding(
            get: { keepAwakeSeconds },
            set: { runtimeController.setKeepAwakeSeconds($0) }
        )
    }

    private var isTimeoutEnabledBinding: Binding<Bool> {
        Binding(
            get: { isTimeoutEnabled },
            set: { runtimeController.setTimeoutEnabled($0) }
        )
    }

    private var timeoutSecondsBinding: Binding<Int> {
        Binding(
            get: { timeoutSeconds },
            set: { runtimeController.setTimeoutSeconds($0) }
        )
    }

    private var isVideoFlipHorizontalEnabledBinding: Binding<Bool> {
        Binding(
            get: { isVideoFlipHorizontalEnabled },
            set: { runtimeController.setVideoFlipHorizontalEnabled($0) }
        )
    }

    private var isVideoFlipVerticalEnabledBinding: Binding<Bool> {
        Binding(
            get: { isVideoFlipVerticalEnabled },
            set: { runtimeController.setVideoFlipVerticalEnabled($0) }
        )
    }

    private var videoRotationDegreesBinding: Binding<Double> {
        Binding(
            get: { videoRotationDegrees },
            set: { runtimeController.setVideoRotationDegrees($0) }
        )
    }

    private var videoFramesPerSecondBinding: Binding<Double> {
        Binding(
            get: { videoFramesPerSecond },
            set: { runtimeController.setVideoFramesPerSecond($0) }
        )
    }

}

enum DashboardLayoutMode: Equatable {
    case stacked
    case twoColumn
}

enum ControlPreferenceKey: String {
    case videoEnabled = "contentView.videoEnabled"
    case trackIREnabled = "contentView.trackIREnabled"
    case mouseMovementEnabled = "contentView.mouseMovementEnabled"
    case mouseMovementSpeed = "contentView.mouseMovementSpeed"
    case xKeysFastMouseEnabled = "contentView.xKeysFastMouseEnabled"
    case mouseSmoothing = "contentView.mouseSmoothing"
    case mouseDeadzone = "contentView.mouseDeadzone"
    case avoidMouseJumpsEnabled = "contentView.avoidMouseJumpsEnabled"
    case mouseJumpThresholdPixels = "contentView.mouseJumpThresholdPixels"
    case minimumBlobAreaPoints = "contentView.minimumBlobAreaPoints"
    case keepAwakeSeconds = "contentView.keepAwakeSeconds"
    case timeoutEnabled = "contentView.timeoutEnabled"
    case timeoutSeconds = "contentView.timeoutSeconds"
    case videoFlipHorizontal = "contentView.videoFlipHorizontal"
    case videoFlipVertical = "contentView.videoFlipVertical"
    case videoRotationDegrees = "contentView.videoRotationDegrees"
    case videoFramesPerSecond = "contentView.videoFramesPerSecond"
}

struct ControlDefaultValues: Equatable {
    let videoEnabled: Bool
    let trackIREnabled: Bool
    let mouseMovementEnabled: Bool
    let mouseMovementSpeed: Double
    let isXKeysFastMouseEnabled: Bool
    let mouseSmoothing: Int
    let mouseDeadzone: Double
    let avoidMouseJumpsEnabled: Bool
    let mouseJumpThresholdPixels: Int
    let minimumBlobAreaPoints: Int
    let keepAwakeSeconds: Int
    let timeoutEnabled: Bool
    let timeoutSeconds: Int
    let videoFlipHorizontalEnabled: Bool
    let videoFlipVerticalEnabled: Bool
    let videoRotationDegrees: Double
    let videoFramesPerSecond: Double
}

struct VideoPreviewTransform: Equatable {
    let scaleX: CGFloat
    let scaleY: CGFloat
    let rotationDegrees: Double
}

func defaultMouseMovementShortcut() -> KeyboardShortcuts.Shortcut {
    .init(.f7, modifiers: [.shift])
}

func trackIRDefaultBlobCentroidMode() -> TrackIRBlobCentroidMode {
    .regularizedBinary
}

func controlDefaultValues() -> ControlDefaultValues {
    ControlDefaultValues(
        videoEnabled: true,
        trackIREnabled: true,
        mouseMovementEnabled: true,
        mouseMovementSpeed: 2.0,
        isXKeysFastMouseEnabled: false,
        mouseSmoothing: 3,
        mouseDeadzone: 0.04,
        avoidMouseJumpsEnabled: true,
        mouseJumpThresholdPixels: 50,
        minimumBlobAreaPoints: 100,
        keepAwakeSeconds: 29,
        timeoutEnabled: true,
        timeoutSeconds: 28_800,
        videoFlipHorizontalEnabled: false,
        videoFlipVerticalEnabled: false,
        videoRotationDegrees: 0.0,
        videoFramesPerSecond: 60.0
    )
}

func controlDefaultPreferences(_ defaults: ControlDefaultValues) -> [String: Any] {
    [
        ControlPreferenceKey.videoEnabled.rawValue: defaults.videoEnabled,
        ControlPreferenceKey.trackIREnabled.rawValue: defaults.trackIREnabled,
        ControlPreferenceKey.mouseMovementEnabled.rawValue: defaults.mouseMovementEnabled,
        ControlPreferenceKey.mouseMovementSpeed.rawValue: defaults.mouseMovementSpeed,
        ControlPreferenceKey.xKeysFastMouseEnabled.rawValue: defaults.isXKeysFastMouseEnabled,
        ControlPreferenceKey.mouseSmoothing.rawValue: defaults.mouseSmoothing,
        ControlPreferenceKey.mouseDeadzone.rawValue: defaults.mouseDeadzone,
        ControlPreferenceKey.avoidMouseJumpsEnabled.rawValue: defaults.avoidMouseJumpsEnabled,
        ControlPreferenceKey.mouseJumpThresholdPixels.rawValue: defaults.mouseJumpThresholdPixels,
        ControlPreferenceKey.minimumBlobAreaPoints.rawValue: defaults.minimumBlobAreaPoints,
        ControlPreferenceKey.keepAwakeSeconds.rawValue: defaults.keepAwakeSeconds,
        ControlPreferenceKey.timeoutEnabled.rawValue: defaults.timeoutEnabled,
        ControlPreferenceKey.timeoutSeconds.rawValue: defaults.timeoutSeconds,
        ControlPreferenceKey.videoFlipHorizontal.rawValue: defaults.videoFlipHorizontalEnabled,
        ControlPreferenceKey.videoFlipVertical.rawValue: defaults.videoFlipVerticalEnabled,
        ControlPreferenceKey.videoRotationDegrees.rawValue: defaults.videoRotationDegrees,
        ControlPreferenceKey.videoFramesPerSecond.rawValue: defaults.videoFramesPerSecond,
    ]
}

enum TrackIRBlobCentroidMode: Int, Equatable {
    case regularizedBinary = 5
}

func trackIRBlobCentroidModeDescription(for mode: TrackIRBlobCentroidMode) -> String {
    switch mode {
        case .regularizedBinary:
            return "Blend the new binary centroid with the previous center to calm tiny frame-to-frame wobble."
    }
}

func toggledMouseMovementState(isEnabled: Bool) -> Bool {
    !isEnabled
}

func mouseSpeedValueLabel(for speed: Double) -> String {
    "\(speed.formatted(.number.precision(.fractionLength(0 ... 2))))x"
}

func xKeysIndicatorLabel(for state: XKeysIndicatorState) -> String {
    switch state {
        case .disabled:
            return "Disabled"
        case .notDetected:
            return "No Pedal"
        case .detectedIdle:
            return "Ready"
        case .pressed:
            return "Pressed"
    }
}

func xKeysIndicatorColor(for state: XKeysIndicatorState) -> Color {
    switch state {
        case .disabled:
            return Color.gray.opacity(0.7)
        case .notDetected:
            return Color.red
        case .detectedIdle:
            return Color.yellow
        case .pressed:
            return Color.green
    }
}

func mouseSmoothingValueLabel(for smoothing: Int) -> String {
    "\(smoothing)"
}

func mouseDeadzoneValueLabel(for deadzone: Double) -> String {
    deadzone.formatted(.number.precision(.fractionLength(2)))
}

func normalizedMouseMovementControlSpeed(_ storedSpeed: Double) -> Double {
    min(max(storedSpeed, 1.0), 5.0)
}

func normalizedMouseSmoothing(_ smoothing: Double) -> Int {
    Int(min(max(smoothing.rounded(), 1.0), 10.0))
}

func normalizedMouseDeadzone(_ deadzone: Double) -> Double {
    min(max(deadzone, 0.0), 1.0)
}

func normalizedMouseJumpThreshold(_ jumpThresholdPixels: Int) -> Int {
    max(jumpThresholdPixels, 1)
}

func normalizedMinimumBlobArea(_ minimumBlobAreaPoints: Int) -> Int {
    max(minimumBlobAreaPoints, 1)
}

func normalizedKeepAwakeSeconds(_ keepAwakeSeconds: Int) -> Int {
    max(keepAwakeSeconds, 0)
}

func normalizedTimeoutSeconds(_ timeoutSeconds: Int) -> Int {
    max(timeoutSeconds, 1)
}

func trackIRMouseBackendSpeed(controlSpeed: Double) -> Double {
    normalizedMouseMovementControlSpeed(controlSpeed) * 10.0
}

let trackIRTimeoutHelperText = "8 hours = 60 sec x 60 min x 8 hrs = 28800 sec"

func trackIRTimeoutRemainingSeconds(deadline: Date?, now: Date) -> Int? {
    guard let deadline else {
        return nil
    }

    return max(Int(ceil(deadline.timeIntervalSince(now))), 0)
}

func trackIRTimeoutCountdownLabel(
    isTrackIREnabled: Bool,
    isTimeoutEnabled: Bool,
    deadline: Date?,
    now: Date
) -> String {
    guard isTimeoutEnabled else {
        return "Timeout is disabled."
    }

    guard isTrackIREnabled,
          let remainingSeconds = trackIRTimeoutRemainingSeconds(deadline: deadline, now: now) else {
        return "Starts when TrackIR is enabled."
    }

    let hours = remainingSeconds / 3600
    let minutes = (remainingSeconds % 3600) / 60
    let seconds = remainingSeconds % 60
    return String(format: "Remaining: %02d:%02d:%02d", hours, minutes, seconds)
}

func previewVideoTransform(
    flipHorizontal: Bool,
    flipVertical: Bool,
    rotationDegrees: Double
) -> VideoPreviewTransform {
    VideoPreviewTransform(
        scaleX: flipHorizontal ? -1 : 1,
        scaleY: flipVertical ? -1 : 1,
        rotationDegrees: normalizedRotationDegrees(rotationDegrees)
    )
}

func previewCentroidMarkerPosition(
    centroidX: Double?,
    centroidY: Double?,
    frameWidth: Int,
    frameHeight: Int,
    transform: VideoPreviewTransform
) -> CGPoint? {
    let normalizedX: Double
    let normalizedY: Double
    let centeredX: Double
    let centeredY: Double
    let flippedX: Double
    let flippedY: Double
    let radians: Double
    let rotatedX: Double
    let rotatedY: Double

    guard let centroidX, let centroidY, frameWidth > 0, frameHeight > 0 else {
        return nil
    }

    normalizedX = min(max(centroidX / Double(frameWidth), 0.0), 1.0)
    normalizedY = min(max(centroidY / Double(frameHeight), 0.0), 1.0)
    centeredX = normalizedX - 0.5
    centeredY = normalizedY - 0.5
    flippedX = centeredX * Double(transform.scaleX)
    flippedY = centeredY * Double(transform.scaleY)
    radians = transform.rotationDegrees * .pi / 180.0
    rotatedX = (flippedX * cos(radians)) - (flippedY * sin(radians))
    rotatedY = (flippedX * sin(radians)) + (flippedY * cos(radians))

    return CGPoint(x: rotatedX + 0.5, y: rotatedY + 0.5)
}

func shouldDrawPreviewCentroidMarker(
    isVideoEnabled: Bool,
    hasPreviewImage: Bool,
    isMarkerEnabled: Bool
) -> Bool {
    isVideoEnabled && hasPreviewImage && isMarkerEnabled
}

func normalizedRotationDegrees(_ rotationDegrees: Double) -> Double {
    let normalized = rotationDegrees.truncatingRemainder(dividingBy: 360)
    return normalized >= 0 ? normalized : normalized + 360
}

func videoRotationValueLabel(for rotationDegrees: Double) -> String {
    "\(Int(normalizedRotationDegrees(rotationDegrees).rounded()))°"
}

func trackIRFramesPerSecondValueLabel(for framesPerSecond: Double) -> String {
    guard framesPerSecond > 0 else {
        return "Uncapped"
    }

    return "\(Int(framesPerSecond.rounded())) fps"
}

func trackIRRateSummaryLabel(maximumFramesPerSecond: Double, sourceFrameRate: Double?) -> String {
    let capLabel = maximumFramesPerSecond > 0 ? String(Int(maximumFramesPerSecond.rounded())) : "Uncapped"
    let sourceLabel = sourceFrameRate.map { String(Int($0.rounded())) } ?? "-"
    return "\(capLabel) / max \(sourceLabel)"
}

extension KeyboardShortcuts.Name {
    static let toggleMouseMovement = Self("toggleMouseMovement", default: defaultMouseMovementShortcut())
}

enum TrackIRRuntimeLifecycleEvent {
    case windowClosed
    case appWillTerminate
}

func shouldShutdownTrackIRRuntime(for event: TrackIRRuntimeLifecycleEvent) -> Bool {
    switch event {
        case .windowClosed:
            return false
        case .appWillTerminate:
            return true
    }
}

func trackIRPresentationIsActive(
    scenePhase: ScenePhase,
    controlActiveState: ControlActiveState,
    isWindowVisible: Bool
) -> Bool {
    isWindowVisible && scenePhase == .active && controlActiveState == .key
}

func dashboardLayout(for width: CGFloat) -> DashboardLayoutMode {
    width >= 960 ? .twoColumn : .stacked
}

func videoPreviewWidth(for layout: DashboardLayoutMode, width: CGFloat) -> CGFloat {
    switch layout {
        case .twoColumn:
            return min(320, max(260, width * 0.24))
        case .stacked:
            return min(340, max(240, width * 0.38))
    }
}

func videoPreviewHeight(for width: CGFloat) -> CGFloat {
    width * 0.75
}

func previewTelemetryWidth(for previewWidth: CGFloat) -> CGFloat {
    max(previewWidth, 620)
}

func controlColumnCount(for layout: DashboardLayoutMode, width: CGFloat) -> Int {
    switch layout {
        case .twoColumn:
            return width >= 900 ? 2 : 1
        case .stacked:
            return width >= 720 ? 2 : 1
    }
}

#Preview {
    ContentView()
}
