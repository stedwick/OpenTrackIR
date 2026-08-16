using System.Globalization;

namespace OpenTrackIR.WinUI.Models
{
    public static class TrackIRUiLogic
    {
        public const int FrameWidth = 640;
        public const int FrameHeight = 480;
        public const string TimeoutHelperText = "8 hours = 60 sec x 60 min x 8 hrs = 28800 sec";

        public static TrackIRControlState CreateDefaultControlState()
        {
            return new TrackIRControlState(
                IsVideoEnabled: true,
                IsTrackIREnabled: true,
                IsMouseMovementEnabled: true,
                IsWindowsAbsoluteMousePositioningEnabled: false,
                MouseMovementSpeed: 2.0,
                IsXKeysFastMouseEnabled: false,
                MouseSmoothing: 3,
                MouseDeadzone: 0.04,
                IsAvoidMouseJumpsEnabled: true,
                MouseJumpThresholdPixels: 50,
                MinimumBlobAreaPoints: 100,
                KeepAwakeSeconds: 29,
                IsTimeoutEnabled: true,
                TimeoutSeconds: 28800,
                IsVideoFlipHorizontalEnabled: false,
                IsVideoFlipVerticalEnabled: false,
                VideoRotationDegrees: 0.0,
                VideoFramesPerSecond: 60.0,
                MouseToggleHotkeyText: "Shift+F7"
            );
        }

        public static TrackIRControlState Normalize(TrackIRControlState state)
        {
            string hotkeyText = string.IsNullOrWhiteSpace(state.MouseToggleHotkeyText)
                ? "Shift+F7"
                : state.MouseToggleHotkeyText.Trim();

            return state with
            {
                MouseMovementSpeed = Math.Clamp(state.MouseMovementSpeed, 0.1, 5.0),
                MouseSmoothing = Math.Clamp(state.MouseSmoothing, 1, 10),
                MouseDeadzone = Math.Clamp(state.MouseDeadzone, 0.0, 1.0),
                MouseJumpThresholdPixels = Math.Max(state.MouseJumpThresholdPixels, 1),
                MinimumBlobAreaPoints = Math.Max(state.MinimumBlobAreaPoints, 1),
                KeepAwakeSeconds = Math.Max(state.KeepAwakeSeconds, 0),
                TimeoutSeconds = Math.Max(state.TimeoutSeconds, 1),
                VideoRotationDegrees = NormalizeRotationDegrees(state.VideoRotationDegrees),
                VideoFramesPerSecond = Math.Clamp(state.VideoFramesPerSecond, 0.0, 125.0),
                MouseToggleHotkeyText = hotkeyText
            };
        }

        public static TrackIRSnapshot BuildMockSnapshot(
            TrackIRControlState controlState,
            TrackIRPresentationState presentationState,
            ulong revision
        )
        {
            XKeysIndicatorState xKeysIndicatorState = controlState.IsXKeysFastMouseEnabled
                ? XKeysIndicatorState.NotDetected
                : XKeysIndicatorState.Disabled;

            if (!controlState.IsTrackIREnabled)
            {
                return new TrackIRSnapshot(
                    Phase: TrackIRRuntimePhase.Idle,
                    ErrorDescription: null,
                    FrameIndex: 0,
                    SourceFrameRate: null,
                    CentroidX: null,
                    CentroidY: null,
                    PacketType: null,
                    DeviceLabel: "Idle",
                    BackendLabel: "Mock Runtime",
                    XKeysIndicatorState: xKeysIndicatorState,
                    HasPreview: false,
                    IsLowPowerMode: false
                );
            }

            bool isLowPowerMode = !presentationState.IsWindowVisible || !presentationState.IsAppActive;
            double sourceFrameRate = isLowPowerMode ? 12.0 : 75.0;
            bool hasPreview = controlState.IsVideoEnabled && !isLowPowerMode;
            double centroidX = 220 + (revision % 140);
            double centroidY = 160 + ((revision * 2) % 120);

            return new TrackIRSnapshot(
                Phase: TrackIRRuntimePhase.Streaming,
                ErrorDescription: null,
                FrameIndex: 240 + (revision * 12),
                SourceFrameRate: sourceFrameRate,
                CentroidX: centroidX,
                CentroidY: centroidY,
                PacketType: 0x05,
                DeviceLabel: "TrackIR 5 (Mock)",
                BackendLabel: isLowPowerMode ? "Mock Runtime / Low Power" : "Mock Runtime",
                XKeysIndicatorState: xKeysIndicatorState,
                HasPreview: hasPreview,
                IsLowPowerMode: isLowPowerMode
            );
        }

        public static string PreviewTitle(TrackIRControlState controlState, TrackIRSnapshot snapshot)
        {
            if (!controlState.IsVideoEnabled)
            {
                return "Video Preview Hidden";
            }

            return snapshot.Phase switch
            {
                TrackIRRuntimePhase.Streaming => snapshot.IsLowPowerMode
                    ? "TrackIR Running In Background"
                    : "Live TrackIR Camera",
                TrackIRRuntimePhase.Starting => "Starting TrackIR Camera",
                TrackIRRuntimePhase.Unavailable => "TrackIR Not Found",
                TrackIRRuntimePhase.Failed => "TrackIR Camera Error",
                _ => controlState.IsTrackIREnabled ? "Video Preview Ready" : "TrackIR Off",
            };
        }

        public static string PreviewMessage(TrackIRControlState controlState, TrackIRSnapshot snapshot)
        {
            if (!controlState.IsVideoEnabled)
            {
                return "Turn video on to show the camera.";
            }

            if (!controlState.IsTrackIREnabled)
            {
                return "Turn TrackIR on to start the camera.";
            }

            return snapshot.Phase switch
            {
                TrackIRRuntimePhase.Idle or TrackIRRuntimePhase.Starting =>
                    "Opening the TrackIR camera.",
                TrackIRRuntimePhase.Streaming when snapshot.IsLowPowerMode =>
                    "TrackIR stays active while the window is hidden, but preview work is paused for low power mode.",
                TrackIRRuntimePhase.Streaming =>
                    "Live camera feed from the shared C library.",
                TrackIRRuntimePhase.Unavailable or TrackIRRuntimePhase.Failed =>
                    snapshot.ErrorDescription ?? "TrackIR camera unavailable.",
                _ =>
                    "TrackIR camera unavailable.",
            };
        }

        public static string TrackIRFramesPerSecondValueLabel(double framesPerSecond)
        {
            return framesPerSecond > 0
                ? $"{Math.Round(framesPerSecond):0} fps"
                : "Uncapped";
        }

        public static string TrackIRRateSummaryLabel(double maximumFramesPerSecond, double? sourceFrameRate)
        {
            string capLabel = maximumFramesPerSecond > 0
                ? Math.Round(maximumFramesPerSecond).ToString("0", CultureInfo.InvariantCulture)
                : "Uncapped";
            string sourceLabel = sourceFrameRate.HasValue
                ? Math.Round(sourceFrameRate.Value).ToString("0", CultureInfo.InvariantCulture)
                : "-";
            return $"{capLabel} / max {sourceLabel}";
        }

        public static string MouseSpeedValueLabel(double speed)
        {
            return speed.ToString("0.##", CultureInfo.InvariantCulture) + "x";
        }

        public static string MouseSmoothingValueLabel(double smoothing)
        {
            return Math.Round(smoothing).ToString("0", CultureInfo.InvariantCulture);
        }

        public static string MouseDeadzoneValueLabel(double deadzone)
        {
            return deadzone.ToString("0.00", CultureInfo.InvariantCulture);
        }

        public static string VideoRotationValueLabel(double rotationDegrees)
        {
            return Math.Round(NormalizeRotationDegrees(rotationDegrees)).ToString("0", CultureInfo.InvariantCulture) + "\u00B0";
        }

        public static string CentroidPairLabel(double? centroidX, double? centroidY)
        {
            if (!centroidX.HasValue || !centroidY.HasValue)
            {
                return "-";
            }

            return string.Format(
                CultureInfo.InvariantCulture,
                "{0:0}, {1:0}",
                centroidX.Value,
                centroidY.Value
            );
        }

        public static string PacketTypeLabel(byte? packetType)
        {
            return packetType.HasValue ? $"0x{packetType.Value:X2}" : "-";
        }

        public static string XKeysIndicatorLabel(XKeysIndicatorState state)
        {
            return state switch
            {
                XKeysIndicatorState.Disabled => "Disabled",
                XKeysIndicatorState.NotDetected => "No Pedal",
                XKeysIndicatorState.Ready => "Ready",
                XKeysIndicatorState.Pressed => "Pressed",
                _ => "Disabled",
            };
        }

        public static string XKeysIndicatorColorHex(XKeysIndicatorState state)
        {
            return state switch
            {
                XKeysIndicatorState.Disabled => "#7A8797",
                XKeysIndicatorState.NotDetected => "#FFB020",
                XKeysIndicatorState.Ready => "#31C48D",
                XKeysIndicatorState.Pressed => "#F05252",
                _ => "#7A8797",
            };
        }

        public static string ToggleStateLabel(bool isEnabled, string enabledLabel, string disabledLabel)
        {
            return isEnabled ? enabledLabel : disabledLabel;
        }

        public static string ToggleStateColorHex(bool isEnabled)
        {
            return isEnabled ? "#31C48D" : "#7A8797";
        }

        public static int? TimeoutRemainingSeconds(DateTimeOffset? deadline, DateTimeOffset now)
        {
            if (!deadline.HasValue)
            {
                return null;
            }

            return Math.Max((int)Math.Ceiling((deadline.Value - now).TotalSeconds), 0);
        }

        public static string TimeoutCountdownLabel(
            bool isTrackIREnabled,
            bool isTimeoutEnabled,
            DateTimeOffset? deadline,
            DateTimeOffset now
        )
        {
            if (!isTimeoutEnabled)
            {
                return "Timeout is disabled.";
            }

            int? remainingSeconds = TimeoutRemainingSeconds(deadline, now);
            if (!isTrackIREnabled || !remainingSeconds.HasValue)
            {
                return "Starts when TrackIR is enabled.";
            }

            int hours = remainingSeconds.Value / 3600;
            int minutes = (remainingSeconds.Value % 3600) / 60;
            int seconds = remainingSeconds.Value % 60;
            return $"Remaining: {hours:00}:{minutes:00}:{seconds:00}";
        }

        public static bool ToggledMouseMovementState(bool isEnabled)
        {
            return !isEnabled;
        }

        public static double PreviewAxisScale(bool isFlipped)
        {
            return isFlipped ? -1.0 : 1.0;
        }

        public static DashboardLayoutMode DashboardLayoutForWidth(double width)
        {
            if (width >= 960)
            {
                return DashboardLayoutMode.Wide;
            }

            return width >= 720 ? DashboardLayoutMode.Medium : DashboardLayoutMode.Narrow;
        }

        public static double NormalizeRotationDegrees(double rotationDegrees)
        {
            double normalized = rotationDegrees % 360.0;
            return normalized >= 0 ? normalized : normalized + 360.0;
        }
    }
}
