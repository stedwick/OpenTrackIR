import AppKit
import Combine
import Foundation
import SwiftUI

struct TrackIRControlState: Equatable {
    var isVideoEnabled: Bool
    var isTrackIREnabled: Bool
    var isMouseMovementEnabled: Bool
    var mouseMovementSpeed: Double
    var isXKeysFastMouseEnabled: Bool
    var mouseSmoothing: Int
    var mouseDeadzone: Double
    var isAvoidMouseJumpsEnabled: Bool
    var mouseJumpThresholdPixels: Int
    var minimumBlobAreaPoints: Int
    var keepAwakeSeconds: Int
    var isTimeoutEnabled: Bool
    var timeoutSeconds: Int
    var isVideoFlipHorizontalEnabled: Bool
    var isVideoFlipVerticalEnabled: Bool
    var videoRotationDegrees: Double
    var videoFramesPerSecond: Double
}

@MainActor
final class TrackIRRuntimeController: ObservableObject {
    @Published private(set) var controlState: TrackIRControlState
    @Published private(set) var timeoutDeadline: Date?

    let cameraController: TrackIRCameraController

    private let userDefaults: UserDefaults
    private var scenePhase: ScenePhase = .background
    private var controlActiveState: ControlActiveState = .inactive
    private var isWindowVisible = false
    private var timeoutTask: Task<Void, Never>?

    init(
        userDefaults: UserDefaults = .standard,
        cameraController: TrackIRCameraController? = nil
    ) {
        self.userDefaults = userDefaults
        self.cameraController = cameraController ?? TrackIRCameraController()
        self.controlState = trackIRControlState(userDefaults: userDefaults)
        self.timeoutDeadline = nil
        self.cameraController.xKeysFailureHandler = { [weak self] in
            self?.setXKeysFastMouseEnabled(false)
        }
        syncTrackIRCamera()
        syncTimeoutTask()
    }

    func setWindowVisible(_ isWindowVisible: Bool) {
        guard self.isWindowVisible != isWindowVisible else {
            return
        }

        self.isWindowVisible = isWindowVisible
        syncTrackIRCamera()
    }

    func setScenePhase(_ scenePhase: ScenePhase) {
        guard self.scenePhase != scenePhase else {
            return
        }

        self.scenePhase = scenePhase
        syncTrackIRCamera()
    }

    func setControlActiveState(_ controlActiveState: ControlActiveState) {
        guard self.controlActiveState != controlActiveState else {
            return
        }

        self.controlActiveState = controlActiveState
        syncTrackIRCamera()
    }

    func refreshTrackIRCamera() {
        cameraController.refresh(
            isTrackIREnabled: controlState.isTrackIREnabled,
            isVideoEnabled: controlState.isVideoEnabled,
            maximumTrackingFramesPerSecond: controlState.videoFramesPerSecond,
            isWindowVisible: trackIRPresentationIsActive(
                scenePhase: scenePhase,
                controlActiveState: controlActiveState,
                isWindowVisible: isWindowVisible
            ),
            isXKeysFastMouseEnabled: controlState.isXKeysFastMouseEnabled,
            isMouseMovementEnabled: controlState.isMouseMovementEnabled,
            mouseMovementSpeed: trackIRMouseBackendSpeed(controlSpeed: controlState.mouseMovementSpeed),
            mouseSmoothing: controlState.mouseSmoothing,
            mouseDeadzone: controlState.mouseDeadzone,
            isAvoidMouseJumpsEnabled: controlState.isAvoidMouseJumpsEnabled,
            mouseJumpThresholdPixels: controlState.mouseJumpThresholdPixels,
            minimumBlobAreaPoints: controlState.minimumBlobAreaPoints,
            keepAwakeSeconds: controlState.keepAwakeSeconds,
            mouseTransform: currentMouseTransform()
        )
    }

    func setTrackIREnabled(_ isTrackIREnabled: Bool) {
        updateControlState {
            $0.isTrackIREnabled = isTrackIREnabled
        }
    }

    func setVideoEnabled(_ isVideoEnabled: Bool) {
        updateControlState {
            $0.isVideoEnabled = isVideoEnabled
        }
    }

    func setMouseMovementEnabled(_ isMouseMovementEnabled: Bool) {
        updateControlState {
            $0.isMouseMovementEnabled = isMouseMovementEnabled
        }
    }

    func toggleMouseMovement() {
        setMouseMovementEnabled(
            toggledMouseMovementState(isEnabled: controlState.isMouseMovementEnabled)
        )
    }

    func setMouseMovementSpeed(_ mouseMovementSpeed: Double) {
        updateControlState {
            $0.mouseMovementSpeed = normalizedMouseMovementControlSpeed(mouseMovementSpeed)
        }
    }

    func setXKeysFastMouseEnabled(_ isXKeysFastMouseEnabled: Bool) {
        updateControlState {
            $0.isXKeysFastMouseEnabled = isXKeysFastMouseEnabled
        }
    }

    func setMouseSmoothing(_ mouseSmoothing: Double) {
        updateControlState {
            $0.mouseSmoothing = normalizedMouseSmoothing(mouseSmoothing)
        }
    }

    func setMouseDeadzone(_ mouseDeadzone: Double) {
        updateControlState {
            $0.mouseDeadzone = normalizedMouseDeadzone(mouseDeadzone)
        }
    }

    func setAvoidMouseJumpsEnabled(_ isAvoidMouseJumpsEnabled: Bool) {
        updateControlState {
            $0.isAvoidMouseJumpsEnabled = isAvoidMouseJumpsEnabled
        }
    }

    func setMouseJumpThresholdPixels(_ mouseJumpThresholdPixels: Int) {
        updateControlState {
            $0.mouseJumpThresholdPixels = normalizedMouseJumpThreshold(mouseJumpThresholdPixels)
        }
    }

    func setMinimumBlobAreaPoints(_ minimumBlobAreaPoints: Int) {
        updateControlState {
            $0.minimumBlobAreaPoints = normalizedMinimumBlobArea(minimumBlobAreaPoints)
        }
    }

    func setKeepAwakeSeconds(_ keepAwakeSeconds: Int) {
        updateControlState {
            $0.keepAwakeSeconds = normalizedKeepAwakeSeconds(keepAwakeSeconds)
        }
    }

    func setTimeoutEnabled(_ isTimeoutEnabled: Bool) {
        updateControlState {
            $0.isTimeoutEnabled = isTimeoutEnabled
        }
    }

    func setTimeoutSeconds(_ timeoutSeconds: Int) {
        updateControlState {
            $0.timeoutSeconds = normalizedTimeoutSeconds(timeoutSeconds)
        }
    }

    func setVideoFlipHorizontalEnabled(_ isVideoFlipHorizontalEnabled: Bool) {
        updateControlState {
            $0.isVideoFlipHorizontalEnabled = isVideoFlipHorizontalEnabled
        }
    }

    func setVideoFlipVerticalEnabled(_ isVideoFlipVerticalEnabled: Bool) {
        updateControlState {
            $0.isVideoFlipVerticalEnabled = isVideoFlipVerticalEnabled
        }
    }

    func setVideoRotationDegrees(_ videoRotationDegrees: Double) {
        updateControlState {
            $0.videoRotationDegrees = videoRotationDegrees
        }
    }

    func setVideoFramesPerSecond(_ videoFramesPerSecond: Double) {
        updateControlState {
            $0.videoFramesPerSecond = videoFramesPerSecond
        }
    }

    func shutdownForWindowClose() {
        if shouldShutdownTrackIRRuntime(for: .windowClosed) {
            cameraController.shutdown()
        } else {
            setWindowVisible(false)
        }
    }

    func shutdownAndWait() {
        timeoutTask?.cancel()
        cameraController.shutdownAndWait()
    }

    private func updateControlState(_ mutate: (inout TrackIRControlState) -> Void) {
        var nextState = controlState
        mutate(&nextState)

        guard nextState != controlState else {
            return
        }

        controlState = nextState
        persistControlState(nextState, userDefaults: userDefaults)
        syncTrackIRCamera()
        syncTimeoutTask()
    }

    private func syncTrackIRCamera() {
        cameraController.syncStreaming(
            isTrackIREnabled: controlState.isTrackIREnabled,
            isVideoEnabled: controlState.isVideoEnabled,
            maximumTrackingFramesPerSecond: controlState.videoFramesPerSecond,
            isWindowVisible: trackIRPresentationIsActive(
                scenePhase: scenePhase,
                controlActiveState: controlActiveState,
                isWindowVisible: isWindowVisible
            ),
            isXKeysFastMouseEnabled: controlState.isXKeysFastMouseEnabled,
            isMouseMovementEnabled: controlState.isMouseMovementEnabled,
            mouseMovementSpeed: trackIRMouseBackendSpeed(controlSpeed: controlState.mouseMovementSpeed),
            mouseSmoothing: controlState.mouseSmoothing,
            mouseDeadzone: controlState.mouseDeadzone,
            isAvoidMouseJumpsEnabled: controlState.isAvoidMouseJumpsEnabled,
            mouseJumpThresholdPixels: controlState.mouseJumpThresholdPixels,
            minimumBlobAreaPoints: controlState.minimumBlobAreaPoints,
            keepAwakeSeconds: controlState.keepAwakeSeconds,
            mouseTransform: currentMouseTransform()
        )
    }

    private func currentMouseTransform() -> VideoPreviewTransform {
        previewVideoTransform(
            flipHorizontal: controlState.isVideoFlipHorizontalEnabled,
            flipVertical: controlState.isVideoFlipVerticalEnabled,
            rotationDegrees: controlState.videoRotationDegrees
        )
    }

    private func syncTimeoutTask() {
        timeoutTask?.cancel()
        timeoutDeadline = nil

        guard shouldScheduleTrackIRTimeout(
            isTrackIREnabled: controlState.isTrackIREnabled,
            isTimeoutEnabled: controlState.isTimeoutEnabled,
            timeoutSeconds: controlState.timeoutSeconds
        ) else {
            timeoutTask = nil
            return
        }

        let timeoutSeconds = controlState.timeoutSeconds
        timeoutDeadline = Date().addingTimeInterval(TimeInterval(timeoutSeconds))
        timeoutTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(timeoutSeconds) * 1_000_000_000)
            guard !Task.isCancelled else {
                return
            }

            await MainActor.run {
                self?.applyTrackIRTimeout()
            }
        }
    }

    private func applyTrackIRTimeout() {
        updateControlState {
            $0 = trackIRTimedOutControlState($0)
        }
    }

    deinit {
        timeoutTask?.cancel()
    }
}

func trackIRControlState(userDefaults: UserDefaults) -> TrackIRControlState {
    let defaults = controlDefaultValues()

    return TrackIRControlState(
        isVideoEnabled: userDefaults.object(forKey: ControlPreferenceKey.videoEnabled.rawValue) as? Bool
            ?? defaults.videoEnabled,
        isTrackIREnabled: userDefaults.object(forKey: ControlPreferenceKey.trackIREnabled.rawValue) as? Bool
            ?? defaults.trackIREnabled,
        isMouseMovementEnabled: userDefaults.object(forKey: ControlPreferenceKey.mouseMovementEnabled.rawValue) as? Bool
            ?? defaults.mouseMovementEnabled,
        mouseMovementSpeed: userDefaults.object(forKey: ControlPreferenceKey.mouseMovementSpeed.rawValue) as? Double
            ?? defaults.mouseMovementSpeed,
        isXKeysFastMouseEnabled: userDefaults.object(
            forKey: ControlPreferenceKey.xKeysFastMouseEnabled.rawValue
        ) as? Bool ?? defaults.isXKeysFastMouseEnabled,
        mouseSmoothing: userDefaults.object(forKey: ControlPreferenceKey.mouseSmoothing.rawValue) as? Int
            ?? defaults.mouseSmoothing,
        mouseDeadzone: userDefaults.object(forKey: ControlPreferenceKey.mouseDeadzone.rawValue) as? Double
            ?? defaults.mouseDeadzone,
        isAvoidMouseJumpsEnabled: userDefaults.object(
            forKey: ControlPreferenceKey.avoidMouseJumpsEnabled.rawValue
        ) as? Bool ?? defaults.avoidMouseJumpsEnabled,
        mouseJumpThresholdPixels: userDefaults.object(
            forKey: ControlPreferenceKey.mouseJumpThresholdPixels.rawValue
        ) as? Int ?? defaults.mouseJumpThresholdPixels,
        minimumBlobAreaPoints: userDefaults.object(
            forKey: ControlPreferenceKey.minimumBlobAreaPoints.rawValue
        ) as? Int ?? defaults.minimumBlobAreaPoints,
        keepAwakeSeconds: userDefaults.object(forKey: ControlPreferenceKey.keepAwakeSeconds.rawValue)
            as? Int ?? defaults.keepAwakeSeconds,
        isTimeoutEnabled: userDefaults.object(forKey: ControlPreferenceKey.timeoutEnabled.rawValue)
            as? Bool ?? defaults.timeoutEnabled,
        timeoutSeconds: userDefaults.object(forKey: ControlPreferenceKey.timeoutSeconds.rawValue)
            as? Int ?? defaults.timeoutSeconds,
        isVideoFlipHorizontalEnabled: userDefaults.object(forKey: ControlPreferenceKey.videoFlipHorizontal.rawValue) as? Bool
            ?? defaults.videoFlipHorizontalEnabled,
        isVideoFlipVerticalEnabled: userDefaults.object(forKey: ControlPreferenceKey.videoFlipVertical.rawValue) as? Bool
            ?? defaults.videoFlipVerticalEnabled,
        videoRotationDegrees: userDefaults.object(forKey: ControlPreferenceKey.videoRotationDegrees.rawValue) as? Double
            ?? defaults.videoRotationDegrees,
        videoFramesPerSecond: userDefaults.object(forKey: ControlPreferenceKey.videoFramesPerSecond.rawValue) as? Double
            ?? defaults.videoFramesPerSecond
    )
}

func persistControlState(_ controlState: TrackIRControlState, userDefaults: UserDefaults) {
    userDefaults.set(controlState.isVideoEnabled, forKey: ControlPreferenceKey.videoEnabled.rawValue)
    userDefaults.set(controlState.isTrackIREnabled, forKey: ControlPreferenceKey.trackIREnabled.rawValue)
    userDefaults.set(
        controlState.isMouseMovementEnabled,
        forKey: ControlPreferenceKey.mouseMovementEnabled.rawValue
    )
    userDefaults.set(controlState.mouseMovementSpeed, forKey: ControlPreferenceKey.mouseMovementSpeed.rawValue)
    userDefaults.set(
        controlState.isXKeysFastMouseEnabled,
        forKey: ControlPreferenceKey.xKeysFastMouseEnabled.rawValue
    )
    userDefaults.set(controlState.mouseSmoothing, forKey: ControlPreferenceKey.mouseSmoothing.rawValue)
    userDefaults.set(controlState.mouseDeadzone, forKey: ControlPreferenceKey.mouseDeadzone.rawValue)
    userDefaults.set(
        controlState.isAvoidMouseJumpsEnabled,
        forKey: ControlPreferenceKey.avoidMouseJumpsEnabled.rawValue
    )
    userDefaults.set(
        controlState.mouseJumpThresholdPixels,
        forKey: ControlPreferenceKey.mouseJumpThresholdPixels.rawValue
    )
    userDefaults.set(
        controlState.minimumBlobAreaPoints,
        forKey: ControlPreferenceKey.minimumBlobAreaPoints.rawValue
    )
    userDefaults.set(controlState.keepAwakeSeconds, forKey: ControlPreferenceKey.keepAwakeSeconds.rawValue)
    userDefaults.set(controlState.isTimeoutEnabled, forKey: ControlPreferenceKey.timeoutEnabled.rawValue)
    userDefaults.set(controlState.timeoutSeconds, forKey: ControlPreferenceKey.timeoutSeconds.rawValue)
    userDefaults.set(
        controlState.isVideoFlipHorizontalEnabled,
        forKey: ControlPreferenceKey.videoFlipHorizontal.rawValue
    )
    userDefaults.set(
        controlState.isVideoFlipVerticalEnabled,
        forKey: ControlPreferenceKey.videoFlipVertical.rawValue
    )
    userDefaults.set(
        controlState.videoRotationDegrees,
        forKey: ControlPreferenceKey.videoRotationDegrees.rawValue
    )
    userDefaults.set(
        controlState.videoFramesPerSecond,
        forKey: ControlPreferenceKey.videoFramesPerSecond.rawValue
    )
}

func shouldScheduleTrackIRTimeout(
    isTrackIREnabled: Bool,
    isTimeoutEnabled: Bool,
    timeoutSeconds: Int
) -> Bool {
    isTrackIREnabled && isTimeoutEnabled && timeoutSeconds > 0
}

func trackIRTimedOutControlState(_ controlState: TrackIRControlState) -> TrackIRControlState {
    var nextState = controlState
    nextState.isTrackIREnabled = false
    nextState.isVideoEnabled = false
    nextState.isMouseMovementEnabled = false
    return nextState
}
