//
//  ContentViewTests.swift
//  OpenTrackIRTests
//

import AppKit
import CoreGraphics
import KeyboardShortcuts
import SwiftUI
import Testing
@testable import OpenTrackIR

@MainActor
struct ContentViewTests {

    @Test func dashboardLayoutUsesTwoColumnsAtDesktopWidths() {
        #expect(dashboardLayout(for: 959) == .stacked)
        #expect(dashboardLayout(for: 960) == .twoColumn)
        #expect(dashboardLayout(for: 1_280) == .twoColumn)
    }

    @Test func videoPreviewWidthShrinksToCompactShellSizes() {
        #expect(videoPreviewWidth(for: .twoColumn, width: 1_280) == 307.2)
        #expect(videoPreviewWidth(for: .twoColumn, width: 2_000) == 320)
        #expect(videoPreviewWidth(for: .stacked, width: 760) == 288.8)
    }

    @Test func videoPreviewHeightPreservesFourByThreeAspectRatio() {
        #expect(videoPreviewHeight(for: 320) == 240)
        #expect(abs(videoPreviewHeight(for: 288.8) - 216.6) < 0.0001)
    }

    @Test func previewTelemetryWidthCanExceedVideoWidth() {
        #expect(previewTelemetryWidth(for: 288.8) == 620)
        #expect(previewTelemetryWidth(for: 700) == 700)
    }

    @Test func controlColumnCountUsesTwoColumnsOnSmallerWindows() {
        #expect(controlColumnCount(for: .stacked, width: 719) == 1)
        #expect(controlColumnCount(for: .stacked, width: 720) == 2)
        #expect(controlColumnCount(for: .twoColumn, width: 899) == 1)
        #expect(controlColumnCount(for: .twoColumn, width: 900) == 2)
    }

    @Test func controlPreferenceKeysRemainStable() {
        #expect(ControlPreferenceKey.videoEnabled.rawValue == "contentView.videoEnabled")
        #expect(ControlPreferenceKey.trackIREnabled.rawValue == "contentView.trackIREnabled")
        #expect(ControlPreferenceKey.mouseMovementEnabled.rawValue == "contentView.mouseMovementEnabled")
        #expect(ControlPreferenceKey.mouseMovementSpeed.rawValue == "contentView.mouseMovementSpeed")
        #expect(ControlPreferenceKey.xKeysFastMouseEnabled.rawValue == "contentView.xKeysFastMouseEnabled")
        #expect(ControlPreferenceKey.mouseSmoothing.rawValue == "contentView.mouseSmoothing")
        #expect(ControlPreferenceKey.mouseDeadzone.rawValue == "contentView.mouseDeadzone")
        #expect(
            ControlPreferenceKey.avoidMouseJumpsEnabled.rawValue ==
                "contentView.avoidMouseJumpsEnabled"
        )
        #expect(
            ControlPreferenceKey.mouseJumpThresholdPixels.rawValue ==
                "contentView.mouseJumpThresholdPixels"
        )
        #expect(
            ControlPreferenceKey.minimumBlobAreaPoints.rawValue ==
                "contentView.minimumBlobAreaPoints"
        )
        #expect(ControlPreferenceKey.keepAwakeSeconds.rawValue == "contentView.keepAwakeSeconds")
        #expect(ControlPreferenceKey.timeoutEnabled.rawValue == "contentView.timeoutEnabled")
        #expect(ControlPreferenceKey.timeoutSeconds.rawValue == "contentView.timeoutSeconds")
        #expect(ControlPreferenceKey.videoFlipHorizontal.rawValue == "contentView.videoFlipHorizontal")
        #expect(ControlPreferenceKey.videoFlipVertical.rawValue == "contentView.videoFlipVertical")
        #expect(ControlPreferenceKey.videoRotationDegrees.rawValue == "contentView.videoRotationDegrees")
        #expect(ControlPreferenceKey.videoFramesPerSecond.rawValue == "contentView.videoFramesPerSecond")
    }

    @Test func controlDefaultValuesMatchExpectedStartupState() {
        #expect(controlDefaultValues() == ControlDefaultValues(
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
        ))
    }

    @Test func controlDefaultPreferencesMapsDefaultsToStoredKeys() {
        let preferences = controlDefaultPreferences(controlDefaultValues())

        #expect(preferences[ControlPreferenceKey.videoEnabled.rawValue] as? Bool == true)
        #expect(preferences[ControlPreferenceKey.trackIREnabled.rawValue] as? Bool == true)
        #expect(preferences[ControlPreferenceKey.mouseMovementEnabled.rawValue] as? Bool == true)
        #expect(preferences[ControlPreferenceKey.mouseMovementSpeed.rawValue] as? Double == 2.0)
        #expect(preferences[ControlPreferenceKey.xKeysFastMouseEnabled.rawValue] as? Bool == false)
        #expect(preferences[ControlPreferenceKey.mouseSmoothing.rawValue] as? Int == 3)
        #expect(preferences[ControlPreferenceKey.mouseDeadzone.rawValue] as? Double == 0.04)
        #expect(preferences[ControlPreferenceKey.avoidMouseJumpsEnabled.rawValue] as? Bool == true)
        #expect(preferences[ControlPreferenceKey.mouseJumpThresholdPixels.rawValue] as? Int == 50)
        #expect(preferences[ControlPreferenceKey.minimumBlobAreaPoints.rawValue] as? Int == 100)
        #expect(preferences[ControlPreferenceKey.keepAwakeSeconds.rawValue] as? Int == 29)
        #expect(preferences[ControlPreferenceKey.timeoutEnabled.rawValue] as? Bool == true)
        #expect(preferences[ControlPreferenceKey.timeoutSeconds.rawValue] as? Int == 28_800)
        #expect(preferences[ControlPreferenceKey.videoFlipHorizontal.rawValue] as? Bool == false)
        #expect(preferences[ControlPreferenceKey.videoFlipVertical.rawValue] as? Bool == false)
        #expect(preferences[ControlPreferenceKey.videoRotationDegrees.rawValue] as? Double == 0.0)
        #expect(preferences[ControlPreferenceKey.videoFramesPerSecond.rawValue] as? Double == 60.0)
    }

    @Test func trackIRControlStateRoundTripsThroughUserDefaults() {
        let suiteName = "OpenTrackIRTests.\(UUID().uuidString)"
        let userDefaults = UserDefaults(suiteName: suiteName)!
        let controlState = TrackIRControlState(
            isVideoEnabled: false,
            isTrackIREnabled: true,
            isMouseMovementEnabled: false,
            mouseMovementSpeed: 3.4,
            isXKeysFastMouseEnabled: true,
            mouseSmoothing: 7,
            mouseDeadzone: 0.12,
            isAvoidMouseJumpsEnabled: false,
            mouseJumpThresholdPixels: 80,
            minimumBlobAreaPoints: 9,
            keepAwakeSeconds: 45,
            isTimeoutEnabled: false,
            timeoutSeconds: 600,
            isVideoFlipHorizontalEnabled: true,
            isVideoFlipVerticalEnabled: true,
            videoRotationDegrees: 270,
            videoFramesPerSecond: 75
        )

        userDefaults.removePersistentDomain(forName: suiteName)
        persistControlState(controlState, userDefaults: userDefaults)

        #expect(trackIRControlState(userDefaults: userDefaults) == controlState)

        userDefaults.removePersistentDomain(forName: suiteName)
    }

    @Test func defaultMouseMovementShortcutUsesShiftF7() {
        #expect(defaultMouseMovementShortcut() == KeyboardShortcuts.Shortcut(.f7, modifiers: [.shift]))
        #expect(KeyboardShortcuts.Name.toggleMouseMovement.rawValue == "toggleMouseMovement")
        #expect(KeyboardShortcuts.Name.toggleMouseMovement.defaultShortcut == defaultMouseMovementShortcut())
    }

    @Test func toggledMouseMovementStateFlipsTheStoredValue() {
        #expect(toggledMouseMovementState(isEnabled: true) == false)
        #expect(toggledMouseMovementState(isEnabled: false) == true)
    }

    @Test func mouseSpeedValueLabelUsesCompactMultiplierText() {
        #expect(mouseSpeedValueLabel(for: 0.2) == "0.2x")
        #expect(mouseSpeedValueLabel(for: 1.0) == "1x")
        #expect(mouseSpeedValueLabel(for: 2.2) == "2.2x")
        #expect(mouseSpeedValueLabel(for: 5.0) == "5x")
    }

    @Test func mouseSpeedControlValueMapsToBackendScale() {
        #expect(normalizedMouseMovementControlSpeed(2.0) == 2.0)
        #expect(normalizedMouseMovementControlSpeed(0.5) == 0.5)
        #expect(normalizedMouseMovementControlSpeed(0.2) == 0.2)
        #expect(normalizedMouseMovementControlSpeed(0.0) == 0.2)
        #expect(normalizedMouseMovementControlSpeed(7.0) == 5.0)
        #expect(trackIRMouseBackendSpeed(controlSpeed: 2.0) == 20.0)
        #expect(trackIRMouseBackendSpeed(controlSpeed: 0.2) == 2.0)
        #expect(trackIRMouseBackendSpeed(controlSpeed: 20.0) == 50.0)
    }

    @Test func advancedMouseDefaultsAndLabelsMatchExpectedValues() {
        #expect(mouseSmoothingValueLabel(for: 3) == "3")
        #expect(mouseDeadzoneValueLabel(for: 0.04) == "0.04")
        #expect(controlDefaultValues().minimumBlobAreaPoints == 100)
        #expect(trackIRDefaultBlobCentroidMode() == .regularizedBinary)
        #expect(trackIRBlobCentroidModeDescription(for: .regularizedBinary).contains("previous center"))
        #expect(trackIRTimeoutHelperText == "8 hours = 60 sec x 60 min x 8 hrs = 28800 sec")
    }

    @Test func timeoutCountdownUsesDeadlineAndRoundsUpPartialSeconds() {
        let now = Date(timeIntervalSince1970: 1_000)
        let deadline = now.addingTimeInterval(28_800.2)

        #expect(trackIRTimeoutRemainingSeconds(deadline: deadline, now: now) == 28_801)
        #expect(trackIRTimeoutCountdownLabel(
            isTrackIREnabled: true,
            isTimeoutEnabled: true,
            deadline: now.addingTimeInterval(28_800),
            now: now
        ) == "Remaining: 08:00:00")
        #expect(trackIRTimeoutCountdownLabel(
            isTrackIREnabled: false,
            isTimeoutEnabled: true,
            deadline: nil,
            now: now
        ) == "Starts when TrackIR is enabled.")
        #expect(trackIRTimeoutCountdownLabel(
            isTrackIREnabled: true,
            isTimeoutEnabled: false,
            deadline: nil,
            now: now
        ) == "Timeout is disabled.")
    }

    @Test func nativeBlobCentroidModeMappingMatchesSharedCEnum() {
        #expect(trackIRNativeBlobCentroidMode() == OTIR_TIR5V3_CENTROID_MODE_REGULARIZED_BINARY)
    }

    @Test func advancedMouseValuesAreNormalizedBeforeStorage() {
        #expect(normalizedMouseSmoothing(0) == 1)
        #expect(normalizedMouseSmoothing(10.8) == 10)
        #expect(normalizedMouseDeadzone(-1) == 0)
        #expect(normalizedMouseDeadzone(0.3) == 0.3)
        #expect(normalizedMouseDeadzone(2.5) == 1.0)
        #expect(normalizedMouseJumpThreshold(0) == 1)
        #expect(normalizedMinimumBlobArea(0) == 1)
        #expect(normalizedKeepAwakeSeconds(-10) == 0)
        #expect(normalizedTimeoutSeconds(0) == 1)
    }

    @Test func previewVideoTransformMapsFlipsAndRotation() {
        #expect(previewVideoTransform(
            flipHorizontal: true,
            flipVertical: false,
            rotationDegrees: 450
        ) == VideoPreviewTransform(scaleX: -1, scaleY: 1, rotationDegrees: 90))
        #expect(previewVideoTransform(
            flipHorizontal: false,
            flipVertical: true,
            rotationDegrees: -90
        ) == VideoPreviewTransform(scaleX: 1, scaleY: -1, rotationDegrees: 270))
    }

    @Test func previewCentroidMarkerPositionAppliesFlipAndRotation() {
        #expect(previewCentroidMarkerPosition(
            centroidX: 320,
            centroidY: 240,
            frameWidth: 640,
            frameHeight: 480,
            transform: VideoPreviewTransform(scaleX: 1, scaleY: 1, rotationDegrees: 0)
        ) == CGPoint(x: 0.5, y: 0.5))

        #expect(previewCentroidMarkerPosition(
            centroidX: 160,
            centroidY: 120,
            frameWidth: 640,
            frameHeight: 480,
            transform: VideoPreviewTransform(scaleX: -1, scaleY: 1, rotationDegrees: 0)
        ) == CGPoint(x: 0.75, y: 0.25))

        #expect(previewCentroidMarkerPosition(
            centroidX: 480,
            centroidY: 240,
            frameWidth: 640,
            frameHeight: 480,
            transform: VideoPreviewTransform(scaleX: 1, scaleY: 1, rotationDegrees: 90)
        ) == CGPoint(x: 0.5, y: 0.75))
    }

    @Test func previewCentroidMarkerVisibilityDependsOnToggleAndPreviewState() {
        #expect(shouldDrawPreviewCentroidMarker(
            isVideoEnabled: true,
            hasPreviewImage: true,
            isMarkerEnabled: true
        ))
        #expect(!shouldDrawPreviewCentroidMarker(
            isVideoEnabled: false,
            hasPreviewImage: true,
            isMarkerEnabled: true
        ))
        #expect(!shouldDrawPreviewCentroidMarker(
            isVideoEnabled: true,
            hasPreviewImage: false,
            isMarkerEnabled: true
        ))
        #expect(!shouldDrawPreviewCentroidMarker(
            isVideoEnabled: true,
            hasPreviewImage: true,
            isMarkerEnabled: false
        ))
    }

    @Test func videoRotationValueLabelUsesNormalizedDegrees() {
        #expect(videoRotationValueLabel(for: 0) == "0°")
        #expect(videoRotationValueLabel(for: 360) == "0°")
        #expect(videoRotationValueLabel(for: 450) == "90°")
    }

    @Test func trackIRFramesPerSecondValueLabelHandlesUncappedAndWholeNumbers() {
        #expect(trackIRFramesPerSecondValueLabel(for: 0) == "Uncapped")
        #expect(trackIRFramesPerSecondValueLabel(for: 60) == "60 fps")
        #expect(trackIRFramesPerSecondValueLabel(for: 124.6) == "125 fps")
    }

    @Test func trackIRRateSummaryLabelCombinesCapAndSourceRate() {
        #expect(trackIRRateSummaryLabel(maximumFramesPerSecond: 60, sourceFrameRate: 122.4) == "60 / max 122")
        #expect(trackIRRateSummaryLabel(maximumFramesPerSecond: 0, sourceFrameRate: 122.6) == "Uncapped / max 123")
        #expect(trackIRRateSummaryLabel(maximumFramesPerSecond: 30, sourceFrameRate: nil) == "30 / max -")
    }

    @Test func trackIRStreamingDependsOnTrackIREnableOnly() {
        #expect(shouldStreamTrackIRSession(isTrackIREnabled: true, isVideoEnabled: true))
        #expect(shouldStreamTrackIRSession(isTrackIREnabled: true, isVideoEnabled: false))
        #expect(!shouldStreamTrackIRSession(isTrackIREnabled: false, isVideoEnabled: true))
    }

    @Test func trackIREffectiveVideoEnabledDependsOnWindowVisibility() {
        #expect(trackIREffectiveVideoEnabled(isVideoEnabled: true, isWindowVisible: true))
        #expect(!trackIREffectiveVideoEnabled(isVideoEnabled: true, isWindowVisible: false))
        #expect(!trackIREffectiveVideoEnabled(isVideoEnabled: false, isWindowVisible: true))
    }

    @Test func trackIRRuntimeShutdownPolicyDependsOnAppLifecycleEvent() {
        #expect(!shouldShutdownTrackIRRuntime(for: .windowClosed))
        #expect(shouldShutdownTrackIRRuntime(for: .appWillTerminate))
    }

    @Test func trackIRPresentationRequiresActiveSceneAndVisibleWindow() {
        #expect(trackIRPresentationIsActive(scenePhase: .active, controlActiveState: .key, isWindowVisible: true))
        #expect(!trackIRPresentationIsActive(scenePhase: .active, controlActiveState: .active, isWindowVisible: true))
        #expect(!trackIRPresentationIsActive(scenePhase: .inactive, controlActiveState: .key, isWindowVisible: true))
        #expect(!trackIRPresentationIsActive(scenePhase: .background, controlActiveState: .key, isWindowVisible: true))
        #expect(!trackIRPresentationIsActive(scenePhase: .active, controlActiveState: .key, isWindowVisible: false))
    }

    @Test func xcodePreviewEnvironmentDetectionMatchesKnownFlags() {
        #expect(isRunningInXcodePreview(environment: ["XCODE_RUNNING_FOR_PREVIEWS": "1"]))
        #expect(isRunningInXcodePreview(environment: ["XCODE_RUNNING_FOR_PLAYGROUNDS": "1"]))
        #expect(!isRunningInXcodePreview(environment: [:]))
    }

    @Test func trackIRHardwareAccessEnvironmentDetectionMatchesPreviewAndUITestFlags() {
        #expect(isTrackIRHardwareAccessDisabledForHostEnvironment(environment: [:]) == false)
        #expect(isTrackIRHardwareAccessDisabledForHostEnvironment(
            environment: ["XCODE_RUNNING_FOR_PREVIEWS": "1"]
        ))
        #expect(isTrackIRHardwareAccessDisabledForHostEnvironment(
            environment: ["OTIR_DISABLE_TRACKIR_HARDWARE": "1"]
        ))
    }

    @Test func trackIRHardwareAccessSkipsPreviewRuns() {
        #expect(shouldAccessTrackIRHardware(
            isTrackIREnabled: true,
            isVideoEnabled: true,
            environment: [:]
        ))
        #expect(!shouldAccessTrackIRHardware(
            isTrackIREnabled: true,
            isVideoEnabled: true,
            environment: ["XCODE_RUNNING_FOR_PREVIEWS": "1"]
        ))
        #expect(!shouldAccessTrackIRHardware(
            isTrackIREnabled: true,
            isVideoEnabled: true,
            environment: ["OTIR_DISABLE_TRACKIR_HARDWARE": "1"]
        ))
        #expect(!shouldAccessTrackIRHardware(
            isTrackIREnabled: false,
            isVideoEnabled: true,
            environment: [:]
        ))
        #expect(shouldAccessTrackIRHardware(
            isTrackIREnabled: true,
            isVideoEnabled: false,
            environment: [:]
        ))
    }

    @Test func trackIRPollingCanContinueWithoutVisibleWindowWhenMouseIsEnabled() {
        #expect(trackIRShouldPollSnapshots(
            isWindowVisible: true,
            isMouseMovementEnabled: false,
            isTrackIREnabled: false,
            keepAwakeSeconds: 0
        ))
        #expect(trackIRShouldPollSnapshots(
            isWindowVisible: false,
            isMouseMovementEnabled: true,
            isTrackIREnabled: false,
            keepAwakeSeconds: 0
        ))
        #expect(trackIRShouldPollSnapshots(
            isWindowVisible: false,
            isMouseMovementEnabled: false,
            isTrackIREnabled: true,
            keepAwakeSeconds: 0
        ))
        #expect(trackIRShouldPollSnapshots(
            isWindowVisible: false,
            isMouseMovementEnabled: false,
            isTrackIREnabled: true,
            keepAwakeSeconds: 29
        ))
        #expect(!trackIRShouldPollSnapshots(
            isWindowVisible: false,
            isMouseMovementEnabled: false,
            isTrackIREnabled: false,
            keepAwakeSeconds: 0
        ))
    }

    @Test func lowPowerTransitionWaitsForContinuousBackgroundIdleAndExitsImmediately() {
        let initial = trackIRLowPowerState(
            previousBackgroundIdleStartTime: nil,
            isWindowVisible: false,
            isMouseMovementEnabled: false,
            currentTime: 10,
            idleDelay: 60
        )
        let afterThirtySeconds = trackIRLowPowerState(
            previousBackgroundIdleStartTime: initial.backgroundIdleStartTime,
            isWindowVisible: false,
            isMouseMovementEnabled: false,
            currentTime: 40,
            idleDelay: 60
        )
        let afterSixtySeconds = trackIRLowPowerState(
            previousBackgroundIdleStartTime: initial.backgroundIdleStartTime,
            isWindowVisible: false,
            isMouseMovementEnabled: false,
            currentTime: 70,
            idleDelay: 60
        )
        let afterKeepAwakeNudge = trackIRLowPowerState(
            previousBackgroundIdleStartTime: afterSixtySeconds.backgroundIdleStartTime,
            isWindowVisible: false,
            isMouseMovementEnabled: false,
            currentTime: 71,
            idleDelay: 60
        )
        let afterForeground = trackIRLowPowerState(
            previousBackgroundIdleStartTime: afterKeepAwakeNudge.backgroundIdleStartTime,
            isWindowVisible: true,
            isMouseMovementEnabled: false,
            currentTime: 72,
            idleDelay: 60
        )

        #expect(initial.backgroundIdleStartTime == 10)
        #expect(!initial.isLowPowerModeEnabled)
        #expect(afterThirtySeconds.backgroundIdleStartTime == 10)
        #expect(!afterThirtySeconds.isLowPowerModeEnabled)
        #expect(afterSixtySeconds.isLowPowerModeEnabled)
        #expect(afterKeepAwakeNudge.backgroundIdleStartTime == 10)
        #expect(afterKeepAwakeNudge.isLowPowerModeEnabled)
        #expect(afterForeground.backgroundIdleStartTime == nil)
        #expect(!afterForeground.isLowPowerModeEnabled)
    }

    @Test func keepAwakeOnlyFiresWhileTrackIRIsEnabledAndMouseMovementIsOff() {
        #expect(trackIRShouldRequestMouseEventAccess(isMouseMovementEnabled: true, keepAwakeSeconds: 0))
        #expect(trackIRShouldRequestMouseEventAccess(isMouseMovementEnabled: false, keepAwakeSeconds: 29))
        #expect(trackIRShouldFireKeepAwake(
            isTrackIREnabled: true,
            isMouseMovementEnabled: false,
            keepAwakeSeconds: 29,
            timeSinceLastMouseMovement: 29
        ))
        #expect(!trackIRShouldFireKeepAwake(
            isTrackIREnabled: false,
            isMouseMovementEnabled: false,
            keepAwakeSeconds: 29,
            timeSinceLastMouseMovement: 60
        ))
        #expect(!trackIRShouldFireKeepAwake(
            isTrackIREnabled: true,
            isMouseMovementEnabled: false,
            keepAwakeSeconds: 29,
            timeSinceLastMouseMovement: 10
        ))
        #expect(!trackIRShouldFireKeepAwake(
            isTrackIREnabled: true,
            isMouseMovementEnabled: true,
            keepAwakeSeconds: 29,
            timeSinceLastMouseMovement: 60
        ))
    }

    @Test func timeoutSchedulingAndExpirationToggleTrackIROff() {
        let controlState = TrackIRControlState(
            isVideoEnabled: true,
            isTrackIREnabled: true,
            isMouseMovementEnabled: true,
            mouseMovementSpeed: 2.0,
            isXKeysFastMouseEnabled: false,
            mouseSmoothing: 3,
            mouseDeadzone: 0.04,
            isAvoidMouseJumpsEnabled: true,
            mouseJumpThresholdPixels: 50,
            minimumBlobAreaPoints: 100,
            keepAwakeSeconds: 29,
            isTimeoutEnabled: true,
            timeoutSeconds: 28_800,
            isVideoFlipHorizontalEnabled: false,
            isVideoFlipVerticalEnabled: false,
            videoRotationDegrees: 0,
            videoFramesPerSecond: 60
        )

        #expect(shouldScheduleTrackIRTimeout(
            isTrackIREnabled: true,
            isTimeoutEnabled: true,
            timeoutSeconds: 28_800
        ))
        #expect(!shouldScheduleTrackIRTimeout(
            isTrackIREnabled: true,
            isTimeoutEnabled: false,
            timeoutSeconds: 28_800
        ))
        #expect(trackIRTimedOutControlState(controlState) == TrackIRControlState(
            isVideoEnabled: false,
            isTrackIREnabled: false,
            isMouseMovementEnabled: false,
            mouseMovementSpeed: 2.0,
            isXKeysFastMouseEnabled: false,
            mouseSmoothing: 3,
            mouseDeadzone: 0.04,
            isAvoidMouseJumpsEnabled: true,
            mouseJumpThresholdPixels: 50,
            minimumBlobAreaPoints: 100,
            keepAwakeSeconds: 29,
            isTimeoutEnabled: true,
            timeoutSeconds: 28_800,
            isVideoFlipHorizontalEnabled: false,
            isVideoFlipVerticalEnabled: false,
            videoRotationDegrees: 0,
            videoFramesPerSecond: 60
        ))
    }

    @Test func trackIRPollingIntervalMatchesMouseMovementMode() {
        #expect(abs(trackIRPollingInterval(
            shouldPublishUI: true,
            isVideoEnabled: true,
            isMouseMovementEnabled: false,
            maximumTrackingFramesPerSecond: 125
        ) - (1.0 / 125.0)) < 0.0001)
        #expect(trackIRPollingInterval(
            shouldPublishUI: false,
            isVideoEnabled: false,
            isMouseMovementEnabled: false,
            maximumTrackingFramesPerSecond: 125
        ) == 1.0)
        #expect(abs(trackIRPollingInterval(
            shouldPublishUI: false,
            isVideoEnabled: false,
            isMouseMovementEnabled: true,
            maximumTrackingFramesPerSecond: 75
        ) - (1.0 / 75.0)) < 0.0001)
        #expect(abs(trackIRPollingInterval(
            shouldPublishUI: true,
            isVideoEnabled: true,
            isMouseMovementEnabled: false,
            maximumTrackingFramesPerSecond: 0
        ) - (1.0 / 60.0)) < 0.0001)
    }

    @Test func trackIRSnapshotReadsAreSkippedForKeepAwakeOnlyBackgroundPolling() {
        #expect(trackIRShouldReadSnapshot(
            shouldPublishUI: true,
            isVideoEnabled: false,
            isMouseMovementEnabled: false
        ))
        #expect(trackIRShouldReadSnapshot(
            shouldPublishUI: false,
            isVideoEnabled: true,
            isMouseMovementEnabled: false
        ))
        #expect(trackIRShouldReadSnapshot(
            shouldPublishUI: false,
            isVideoEnabled: false,
            isMouseMovementEnabled: true
        ))
        #expect(!trackIRShouldReadSnapshot(
            shouldPublishUI: false,
            isVideoEnabled: false,
            isMouseMovementEnabled: false
        ))
    }

    @Test func trackIRPreviewUpdatesStayCappedAtThirtyFramesPerSecond() {
        #expect(trackIRShouldUpdatePreviewFrame(
            isVideoEnabled: true,
            hasPreviewFrame: true,
            previewFrameGeneration: 2,
            lastPreviewFrameGeneration: 1,
            elapsedTimeSinceLastPreview: nil,
            maximumPreviewFramesPerSecond: 30.0
        ))
        #expect(!trackIRShouldUpdatePreviewFrame(
            isVideoEnabled: true,
            hasPreviewFrame: true,
            previewFrameGeneration: 2,
            lastPreviewFrameGeneration: 2,
            elapsedTimeSinceLastPreview: 0.1,
            maximumPreviewFramesPerSecond: 30.0
        ))
        #expect(!trackIRShouldUpdatePreviewFrame(
            isVideoEnabled: true,
            hasPreviewFrame: true,
            previewFrameGeneration: 3,
            lastPreviewFrameGeneration: 2,
            elapsedTimeSinceLastPreview: 0.02,
            maximumPreviewFramesPerSecond: 30.0
        ))
        #expect(trackIRShouldUpdatePreviewFrame(
            isVideoEnabled: true,
            hasPreviewFrame: true,
            previewFrameGeneration: 3,
            lastPreviewFrameGeneration: 2,
            elapsedTimeSinceLastPreview: 1.0 / 30.0,
            maximumPreviewFramesPerSecond: 30.0
        ))
    }

    @Test func trackIRCameraPhaseMapsNativeSessionStates() {
        #expect(trackIRCameraPhase(sessionPhase: OTIR_TRACKIR_SESSION_PHASE_IDLE) == .idle)
        #expect(trackIRCameraPhase(sessionPhase: OTIR_TRACKIR_SESSION_PHASE_STARTING) == .starting)
        #expect(trackIRCameraPhase(sessionPhase: OTIR_TRACKIR_SESSION_PHASE_STREAMING) == .streaming)
        #expect(trackIRCameraPhase(sessionPhase: OTIR_TRACKIR_SESSION_PHASE_UNAVAILABLE) == .unavailable)
        #expect(trackIRCameraPhase(sessionPhase: OTIR_TRACKIR_SESSION_PHASE_FAILED) == .failed)
    }

    @Test func trackIRFrameRateLabelUsesCompactFpsText() {
        #expect(trackIRFrameRateLabel(for: 123.44) == "123.4 fps")
        #expect(trackIRFrameRateLabel(for: nil) == "-")
    }

    @Test func trackIRCoordinateLabelTruncatesToIntegers() {
        #expect(trackIRCoordinateLabel(for: 123.9) == "123")
        #expect(trackIRCoordinateLabel(for: -45.8) == "-45")
        #expect(trackIRCoordinateLabel(for: nil) == "-")
    }

    @Test func trackIRCoordinatePairLabelUsesCommaSeparatedIntegers() {
        #expect(trackIRCoordinatePairLabel(x: 123.9, y: 45.2) == "123, 45")
        #expect(trackIRCoordinatePairLabel(x: nil, y: 45.2) == "-")
        #expect(trackIRCoordinatePairLabel(x: 123.9, y: nil) == "-")
    }

    @Test func trackIRSessionErrorDescriptionReadsUtf8Message() {
        var snapshot = otir_trackir_session_snapshot()
        let message = Array("TrackIR busy".utf8)

        snapshot.has_error_message = true
        withUnsafeMutableBytes(of: &snapshot.error_message) { buffer in
            buffer.copyBytes(from: message)
        }

        #expect(trackIRSessionErrorDescription(snapshot: snapshot) == "TrackIR busy")
    }

    @Test func trackIRDisplayStateMapsNativeSnapshot() {
        var snapshot = otir_trackir_session_snapshot()

        snapshot.phase = OTIR_TRACKIR_SESSION_PHASE_STREAMING
        snapshot.frame_index = 42
        snapshot.has_frame_rate = true
        snapshot.frame_rate = 123.4
        snapshot.has_centroid = true
        snapshot.centroid_x = 321
        snapshot.centroid_y = 123
        snapshot.has_packet_type = true
        snapshot.packet_type = 0x05
        snapshot.has_error_message = true
        withUnsafeMutableBytes(of: &snapshot.error_message) { buffer in
            buffer.copyBytes(from: Array("TrackIR busy".utf8))
        }

        #expect(trackIRDisplayState(snapshot: snapshot) == TrackIRCameraDisplayState(
            phase: .streaming,
            lastErrorDescription: "TrackIR busy",
            frameIndex: 42,
            frameRate: 123.4,
            centroidX: 321,
            centroidY: 123,
            lastPacketType: 0x05
        ))
    }

    @Test func trackIRDisplayStatePublishesOnlyWhenVisibleAndWithinCap() {
        let displayState = TrackIRCameraDisplayState(
            phase: .streaming,
            lastErrorDescription: nil,
            frameIndex: 5,
            frameRate: 120,
            centroidX: 300,
            centroidY: 200,
            lastPacketType: 0x05
        )

        #expect(!trackIRShouldPublishDisplayState(
            shouldPublishUI: false,
            currentDisplayState: displayState,
            lastPublishedDisplayState: nil,
            elapsedTimeSinceLastDisplay: nil,
            maximumDisplayFramesPerSecond: 10
        ))
        #expect(trackIRShouldPublishDisplayState(
            shouldPublishUI: true,
            currentDisplayState: displayState,
            lastPublishedDisplayState: nil,
            elapsedTimeSinceLastDisplay: nil,
            maximumDisplayFramesPerSecond: 10
        ))
        #expect(!trackIRShouldPublishDisplayState(
            shouldPublishUI: true,
            currentDisplayState: displayState,
            lastPublishedDisplayState: displayState,
            elapsedTimeSinceLastDisplay: 0.02,
            maximumDisplayFramesPerSecond: 10
        ))
        #expect(trackIRShouldPublishDisplayState(
            shouldPublishUI: true,
            currentDisplayState: displayState,
            lastPublishedDisplayState: displayState,
            elapsedTimeSinceLastDisplay: 0.1,
            maximumDisplayFramesPerSecond: 10
        ))
        #expect(trackIRShouldPublishDisplayState(
            shouldPublishUI: true,
            currentDisplayState: TrackIRCameraDisplayState(
                phase: .failed,
                lastErrorDescription: "TrackIR busy"
            ),
            lastPublishedDisplayState: displayState,
            elapsedTimeSinceLastDisplay: 0.01,
            maximumDisplayFramesPerSecond: 10
        ))
    }

    @Test func trackIRPreviewImageUsesProvidedDimensions() {
        let image = trackIRPreviewImage(frameBytes: [0, 64, 128, 255], width: 2, height: 2)

        #expect(image?.width == 2)
        #expect(image?.height == 2)
    }

    @Test func notFoundFailureMessageStaysUserFacing() {
        #expect(trackIRFailureMessage(for: OTIR_STATUS_NOT_FOUND, operation: "Open") == "TrackIR not found. Connect the device and try again.")
    }

    @Test func openIoFailureMessageSuggestsRefreshWhenDeviceIsBusy() {
        #expect(trackIRFailureMessage(for: OTIR_STATUS_IO, operation: "Open") == "Open failed: io. TrackIR may be busy in another app. Quit other TrackIR tools, then Refresh.")
    }
}
