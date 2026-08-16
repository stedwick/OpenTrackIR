using OpenTrackIR.WinUI.Models;

namespace OpenTrackIR.WinUI.Tests
{
    public sealed class TrackIRUiLogicTests
    {
        [Fact]
        public void CreateDefaultControlState_matches_macos_defaults()
        {
            TrackIRControlState state = TrackIRUiLogic.CreateDefaultControlState();

            Assert.True(state.IsVideoEnabled);
            Assert.True(state.IsTrackIREnabled);
            Assert.True(state.IsMouseMovementEnabled);
            Assert.False(state.IsWindowsAbsoluteMousePositioningEnabled);
            Assert.Equal(2.0, state.MouseMovementSpeed);
            Assert.Equal(3, state.MouseSmoothing);
            Assert.Equal(0.04, state.MouseDeadzone);
            Assert.True(state.IsAvoidMouseJumpsEnabled);
            Assert.Equal(50, state.MouseJumpThresholdPixels);
            Assert.Equal(100, state.MinimumBlobAreaPoints);
            Assert.Equal(29, state.KeepAwakeSeconds);
            Assert.True(state.IsTimeoutEnabled);
            Assert.Equal(28800, state.TimeoutSeconds);
            Assert.Equal(60.0, state.VideoFramesPerSecond);
            Assert.Equal("Shift+F7", state.MouseToggleHotkeyText);
        }

        [Fact]
        public void Normalize_clamps_and_fills_hotkey_defaults()
        {
            TrackIRControlState state = new(
                IsVideoEnabled: true,
                IsTrackIREnabled: true,
                IsMouseMovementEnabled: true,
                IsWindowsAbsoluteMousePositioningEnabled: false,
                MouseMovementSpeed: 0.05,
                IsXKeysFastMouseEnabled: false,
                MouseSmoothing: 99,
                MouseDeadzone: -1.0,
                IsAvoidMouseJumpsEnabled: true,
                MouseJumpThresholdPixels: -4,
                MinimumBlobAreaPoints: 0,
                KeepAwakeSeconds: -8,
                IsTimeoutEnabled: true,
                TimeoutSeconds: 0,
                IsVideoFlipHorizontalEnabled: false,
                IsVideoFlipVerticalEnabled: false,
                VideoRotationDegrees: -90.0,
                VideoFramesPerSecond: 999,
                MouseToggleHotkeyText: "   "
            );

            TrackIRControlState normalized = TrackIRUiLogic.Normalize(state);

            Assert.Equal(0.1, normalized.MouseMovementSpeed);
            Assert.Equal(10, normalized.MouseSmoothing);
            Assert.Equal(0.0, normalized.MouseDeadzone);
            Assert.Equal(1, normalized.MouseJumpThresholdPixels);
            Assert.Equal(1, normalized.MinimumBlobAreaPoints);
            Assert.Equal(0, normalized.KeepAwakeSeconds);
            Assert.Equal(1, normalized.TimeoutSeconds);
            Assert.Equal(270.0, normalized.VideoRotationDegrees);
            Assert.Equal(125.0, normalized.VideoFramesPerSecond);
            Assert.Equal("Shift+F7", normalized.MouseToggleHotkeyText);
        }

        [Fact]
        public void TrackIRControlStateJson_round_trips_without_reflection_serialization()
        {
            TrackIRControlState state = TrackIRUiLogic.CreateDefaultControlState() with
            {
                IsXKeysFastMouseEnabled = true,
                MouseToggleHotkeyText = "Ctrl+F8",
            };

            string json = TrackIRControlStateJson.Serialize(state);
            TrackIRControlState? deserialized = TrackIRControlStateJson.Deserialize(json);

            Assert.Equal(state, deserialized);
        }

        [Fact]
        public void BuildMockSnapshot_switches_to_low_power_when_not_visible()
        {
            TrackIRControlState state = TrackIRUiLogic.CreateDefaultControlState();

            TrackIRSnapshot snapshot = TrackIRUiLogic.BuildMockSnapshot(
                state,
                new TrackIRPresentationState(false, true),
                3
            );

            Assert.Equal(TrackIRRuntimePhase.Streaming, snapshot.Phase);
            Assert.True(snapshot.IsLowPowerMode);
            Assert.False(snapshot.HasPreview);
            Assert.Equal(12.0, snapshot.SourceFrameRate);
            Assert.Equal("Mock Runtime / Low Power", snapshot.BackendLabel);
        }

        [Fact]
        public void PreviewTitle_and_message_match_current_ui_rules()
        {
            TrackIRControlState defaults = TrackIRUiLogic.CreateDefaultControlState();
            TrackIRSnapshot idleSnapshot = TrackIRUiLogic.BuildMockSnapshot(
                defaults with { IsTrackIREnabled = false },
                new TrackIRPresentationState(true, true),
                1
            );

            Assert.Equal("TrackIR Off", TrackIRUiLogic.PreviewTitle(defaults with { IsTrackIREnabled = false }, idleSnapshot));
            Assert.Equal(
                "Turn TrackIR on to start the camera.",
                TrackIRUiLogic.PreviewMessage(defaults with { IsTrackIREnabled = false }, idleSnapshot)
            );
            Assert.Equal(
                "Video Preview Hidden",
                TrackIRUiLogic.PreviewTitle(defaults with { IsVideoEnabled = false }, idleSnapshot)
            );
        }

        [Fact]
        public void Label_helpers_render_expected_strings()
        {
            Assert.Equal("Uncapped", TrackIRUiLogic.TrackIRFramesPerSecondValueLabel(0));
            Assert.Equal("60 / max 75", TrackIRUiLogic.TrackIRRateSummaryLabel(60, 75));
            Assert.Equal("2.5x", TrackIRUiLogic.MouseSpeedValueLabel(2.5));
            Assert.Equal("3", TrackIRUiLogic.MouseSmoothingValueLabel(3.4));
            Assert.Equal("0.04", TrackIRUiLogic.MouseDeadzoneValueLabel(0.04));
            Assert.Equal("270°", TrackIRUiLogic.VideoRotationValueLabel(-90));
            Assert.Equal(-1.0, TrackIRUiLogic.PreviewAxisScale(isFlipped: true));
            Assert.Equal(1.0, TrackIRUiLogic.PreviewAxisScale(isFlipped: false));
            Assert.Equal("No Pedal", TrackIRUiLogic.XKeysIndicatorLabel(XKeysIndicatorState.NotDetected));
            Assert.Equal("#FFB020", TrackIRUiLogic.XKeysIndicatorColorHex(XKeysIndicatorState.NotDetected));
            Assert.Equal("Visible", TrackIRUiLogic.ToggleStateLabel(true, "Visible", "Hidden"));
            Assert.Equal("#7A8797", TrackIRUiLogic.ToggleStateColorHex(false));
            Assert.False(TrackIRUiLogic.ToggledMouseMovementState(true));
            Assert.True(TrackIRUiLogic.ToggledMouseMovementState(false));
        }

        [Fact]
        public void Timeout_countdown_uses_deadline_and_rounds_up_partial_seconds()
        {
            DateTimeOffset now = DateTimeOffset.FromUnixTimeSeconds(1_000);

            Assert.Equal(28_801, TrackIRUiLogic.TimeoutRemainingSeconds(now.AddSeconds(28_800.2), now));
            Assert.Equal(
                "Remaining: 08:00:00",
                TrackIRUiLogic.TimeoutCountdownLabel(true, true, now.AddSeconds(28_800), now)
            );
            Assert.Equal(
                "Starts when TrackIR is enabled.",
                TrackIRUiLogic.TimeoutCountdownLabel(false, true, null, now)
            );
            Assert.Equal(
                "Timeout is disabled.",
                TrackIRUiLogic.TimeoutCountdownLabel(true, false, null, now)
            );
        }

        [Fact]
        public void DashboardLayoutForWidth_uses_expected_breakpoints()
        {
            Assert.Equal(DashboardLayoutMode.Narrow, TrackIRUiLogic.DashboardLayoutForWidth(700));
            Assert.Equal(DashboardLayoutMode.Medium, TrackIRUiLogic.DashboardLayoutForWidth(800));
            Assert.Equal(DashboardLayoutMode.Wide, TrackIRUiLogic.DashboardLayoutForWidth(1100));
        }

        [Fact]
        public void HotkeyCaptureLogic_formats_display_text_and_ignores_modifier_only_keys()
        {
            Assert.Equal("Ctrl+Shift+F7", HotkeyCaptureLogic.FormatHotkeyText(true, false, true, false, "F7"));
            Assert.True(HotkeyCaptureLogic.IsModifierKey(16));
            Assert.Equal("A", HotkeyCaptureLogic.KeyTokenForVirtualKey(65));
            Assert.Equal("F7", HotkeyCaptureLogic.KeyTokenForVirtualKey(118));
            Assert.True(HotkeyCaptureLogic.TryParseHotkeyText("Shift+F7", out RegisteredHotkey parsedHotkey));
            Assert.True(parsedHotkey.IsShiftPressed);
            Assert.Equal(118, parsedHotkey.VirtualKeyCode);
            Assert.False(HotkeyCaptureLogic.TryParseHotkeyText("Shift", out _));
            Assert.Null(HotkeyCaptureLogic.KeyTokenForVirtualKey(17));
        }

        [Fact]
        public void TrayUiLogic_builds_short_notify_icon_tooltip_text()
        {
            Assert.Equal(
                "OpenTrackIR: TrackIR On, Mouse Off",
                TrayUiLogic.TooltipText(isTrackIREnabled: true, isMouseMovementEnabled: false)
            );
        }

        [Fact]
        public void MainWindowLogic_blocks_tray_restore_once_exit_starts()
        {
            Assert.True(MainWindowLogic.ShouldBeginExit(isDisposed: false, isExitRequested: false));
            Assert.False(MainWindowLogic.ShouldBeginExit(isDisposed: false, isExitRequested: true));
            Assert.True(
                MainWindowLogic.ShouldProcessTrayWindowAction(
                    isDisposed: false,
                    isExitRequested: false,
                    hasContent: true
                )
            );
            Assert.False(
                MainWindowLogic.ShouldProcessTrayWindowAction(
                    isDisposed: false,
                    isExitRequested: true,
                    hasContent: true
                )
            );
            Assert.False(
                MainWindowLogic.ShouldProcessTrayWindowAction(
                    isDisposed: true,
                    isExitRequested: false,
                    hasContent: true
                )
            );
        }

        [Fact]
        public void TrackIRRuntimeLogic_uses_preview_visibility_and_background_rules()
        {
            TrackIRControlState state = TrackIRUiLogic.CreateDefaultControlState();
            TrackIRSnapshot activeSnapshot = TrackIRUiLogic.BuildMockSnapshot(
                state,
                new TrackIRPresentationState(true, true),
                1
            );

            Assert.True(
                TrackIRRuntimeLogic.ShouldPublishPreview(
                    state,
                    new TrackIRPresentationState(true, true),
                    hasPreviewFrame: true
                )
            );
            Assert.False(
                TrackIRRuntimeLogic.ShouldPublishPreview(
                    state,
                    new TrackIRPresentationState(false, true),
                    hasPreviewFrame: true
                )
            );
            Assert.Equal(
                33,
                TrackIRRuntimeLogic.PollIntervalMilliseconds(
                    state with { IsMouseMovementEnabled = false },
                    new TrackIRPresentationState(true, true)
                )
            );
            Assert.True(
                TrackIRRuntimeLogic.ShouldEnableLowPowerMode(
                    state with { IsMouseMovementEnabled = false },
                    new TrackIRPresentationState(false, true)
                )
            );
            Assert.False(
                TrackIRRuntimeLogic.ShouldEnableLowPowerMode(
                    state with { IsMouseMovementEnabled = true },
                    new TrackIRPresentationState(false, true)
                )
            );
            Assert.Equal(
                17,
                TrackIRRuntimeLogic.PollIntervalMilliseconds(
                    state with { IsMouseMovementEnabled = true, VideoFramesPerSecond = 60.0 },
                    new TrackIRPresentationState(false, true)
                )
            );
            Assert.Equal(
                TrackIRRuntimePhase.Idle,
                TrackIRRuntimeLogic.IdleSnapshot(
                    state with { IsTrackIREnabled = false },
                    new TrackIRPresentationState(true, true)
                ).Phase
            );
            Assert.True(
                TrackIRRuntimeLogic.ShouldPublishSnapshot(
                    TrackIRUiLogic.BuildMockSnapshot(state, new TrackIRPresentationState(true, true), 1),
                    TrackIRUiLogic.BuildMockSnapshot(
                        state with { IsVideoEnabled = false },
                        new TrackIRPresentationState(true, true),
                        1
                    )
                )
            );
            Assert.False(
                TrackIRRuntimeLogic.ShouldPublishSnapshot(
                    activeSnapshot,
                    TrackIRUiLogic.BuildMockSnapshot(state, new TrackIRPresentationState(true, true), 1)
                )
            );
            Assert.True(
                TrackIRRuntimeLogic.ShouldReadSnapshot(
                    state with { IsMouseMovementEnabled = false },
                    new TrackIRPresentationState(true, true)
                )
            );
            Assert.False(
                TrackIRRuntimeLogic.ShouldReadSnapshot(
                    state with { IsMouseMovementEnabled = false },
                    new TrackIRPresentationState(false, false)
                )
            );
            Assert.True(
                TrackIRRuntimeLogic.ShouldReadSnapshot(
                    state with { IsMouseMovementEnabled = true },
                    new TrackIRPresentationState(false, false)
                )
            );
            Assert.True(
                TrackIRRuntimeLogic.ShouldPublishTelemetry(
                    shouldPublishUi: true,
                    currentSnapshot: activeSnapshot with { FrameIndex = activeSnapshot.FrameIndex + 1 },
                    lastPublishedSnapshot: activeSnapshot,
                    elapsedTimeSinceLastPublish: TimeSpan.FromMilliseconds(120),
                    maximumFramesPerSecond: TrackIRRuntimeLogic.VisibleTelemetryFramesPerSecond
                )
            );
            Assert.False(
                TrackIRRuntimeLogic.ShouldPublishTelemetry(
                    shouldPublishUi: true,
                    currentSnapshot: activeSnapshot with { FrameIndex = activeSnapshot.FrameIndex + 1 },
                    lastPublishedSnapshot: activeSnapshot,
                    elapsedTimeSinceLastPublish: TimeSpan.FromMilliseconds(50),
                    maximumFramesPerSecond: TrackIRRuntimeLogic.VisibleTelemetryFramesPerSecond
                )
            );
            Assert.True(
                TrackIRRuntimeLogic.ShouldPublishTelemetry(
                    shouldPublishUi: true,
                    currentSnapshot: activeSnapshot with { Phase = TrackIRRuntimePhase.Failed },
                    lastPublishedSnapshot: activeSnapshot,
                    elapsedTimeSinceLastPublish: TimeSpan.Zero,
                    maximumFramesPerSecond: TrackIRRuntimeLogic.VisibleTelemetryFramesPerSecond
                )
            );
            Assert.Equal(
                new TrackIRPresentationState(false, false),
                TrackIRRuntimeLogic.PresentationState(
                    isWindowVisible: true,
                    isWindowMinimized: true,
                    isWindowFocused: true
                )
            );
            Assert.Equal(
                new TrackIRPresentationState(true, false),
                TrackIRRuntimeLogic.PresentationState(
                    isWindowVisible: true,
                    isWindowMinimized: false,
                    isWindowFocused: false
                )
            );
        }

        [Fact]
        public void TrackIRMouseRuntimeLogic_computes_backend_speed_keep_awake_and_subpixel_dispatch()
        {
            TrackIRControlState defaults = TrackIRUiLogic.CreateDefaultControlState();

            Assert.Equal(20.0, TrackIRMouseRuntimeLogic.MouseBackendSpeed(2.0));
            Assert.Equal(1.0, TrackIRMouseRuntimeLogic.MouseBackendSpeed(0.05));
            Assert.Equal(
                5.0,
                TrackIRMouseRuntimeLogic.EffectiveMouseSpeed(
                    2.0,
                    isXKeysFastMouseEnabled: true,
                    isXKeysPedalPressed: true
                )
            );
            Assert.True(
                TrackIRMouseRuntimeLogic.ShouldFireKeepAwake(
                    defaults with { IsMouseMovementEnabled = false, KeepAwakeSeconds = 5 },
                    TimeSpan.FromSeconds(5)
                )
            );
            Assert.False(
                TrackIRMouseRuntimeLogic.ShouldFireKeepAwake(
                    defaults with { IsMouseMovementEnabled = true, KeepAwakeSeconds = 5 },
                    TimeSpan.FromSeconds(10)
                )
            );
            Assert.Equal(
                new KeepAwakeNudge(TrackIRMouseRuntimeLogic.KeepAwakeNudgePixels, 0),
                TrackIRMouseRuntimeLogic.KeepAwakeNudgeForIndex(0)
            );
            Assert.Equal(
                new KeepAwakeNudge(0, -TrackIRMouseRuntimeLogic.KeepAwakeNudgePixels),
                TrackIRMouseRuntimeLogic.KeepAwakeNudgeForIndex(3)
            );
            Assert.Equal(
                new AbsoluteCursorTarget(104, 193),
                TrackIRMouseRuntimeLogic.AbsoluteCursorTargetForDelta(100, 200, 4, -7)
            );

            RelativeMouseDispatch dispatch = TrackIRMouseRuntimeLogic.ConsumeRelativeDelta(1.75, -0.4);

            Assert.Equal(1, dispatch.DeltaX);
            Assert.Equal(0, dispatch.DeltaY);
            Assert.Equal(0.75, dispatch.RemainingX, 10);
            Assert.Equal(-0.4, dispatch.RemainingY, 10);
        }

        [Fact]
        public void XKeysReportLogic_matches_devices_reports_and_indicator_states()
        {
            Assert.True(XKeysReportLogic.IsMatchingFootPedal(0x05F3, 0x042C, 0x000C, 0x0001));
            Assert.True(XKeysReportLogic.IsMatchingFootPedal(0x05F3, 0x0438, 0x000C, 0x0001));
            Assert.False(XKeysReportLogic.IsMatchingFootPedal(0x05F3, 0x9999, 0x000C, 0x0001));
            Assert.True(XKeysReportLogic.MiddlePedalPressed(new byte[] { 0x00, 0x01, 0x01, 0x04 }));
            Assert.True(XKeysReportLogic.MiddlePedalPressed(new byte[] { 0x00, 0x04, 0x00 }));
            Assert.True(XKeysReportLogic.MiddlePedalPressed(new byte[] { 0x00, 0x00, 0x04 }));
            Assert.False(XKeysReportLogic.MiddlePedalPressed(new byte[] { 0x00, 0x00, 0x00 }));
            Assert.Equal(
                XKeysIndicatorState.Disabled,
                XKeysReportLogic.IndicatorState(isEnabled: false, didDetectPedal: false, isPressed: false)
            );
            Assert.Equal(
                XKeysIndicatorState.NotDetected,
                XKeysReportLogic.IndicatorState(isEnabled: true, didDetectPedal: false, isPressed: false)
            );
            Assert.Equal(
                XKeysIndicatorState.Ready,
                XKeysReportLogic.IndicatorState(isEnabled: true, didDetectPedal: true, isPressed: false)
            );
            Assert.Equal(
                XKeysIndicatorState.Pressed,
                XKeysReportLogic.Snapshot(isEnabled: true, didDetectPedal: true, isPressed: true).IndicatorState
            );
            Assert.True(
                XKeysReportLogic.AggregateSnapshot(
                    isEnabled: true,
                    pressedStates: new[] { false, true, false }
                ).IsPressed
            );
        }

        [Fact]
        public void TrackIRRuntimeLogic_ignores_runtime_updates_after_dispose()
        {
            Assert.True(TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(isDisposed: false, isShuttingDown: false));
            Assert.False(TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(isDisposed: true, isShuttingDown: false));
            Assert.False(TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(isDisposed: false, isShuttingDown: true));
            Assert.True(
                TrackIRRuntimeLogic.ShouldQueuePreviewApply(
                    isDisposed: false,
                    isShuttingDown: false,
                    isPreviewApplyQueued: false
                )
            );
            Assert.False(
                TrackIRRuntimeLogic.ShouldQueuePreviewApply(
                    isDisposed: false,
                    isShuttingDown: false,
                    isPreviewApplyQueued: true
                )
            );
            Assert.False(
                TrackIRRuntimeLogic.ShouldQueuePreviewApply(
                    isDisposed: true,
                    isShuttingDown: false,
                    isPreviewApplyQueued: false
                )
            );
            Assert.False(
                TrackIRRuntimeLogic.ShouldQueuePreviewApply(
                    isDisposed: false,
                    isShuttingDown: true,
                    isPreviewApplyQueued: false
                )
            );
        }

        [Fact]
        public void TrackIRRuntimeLogic_schedules_timeout_and_disables_runtime_controls_on_expiry()
        {
            TrackIRControlState defaults = TrackIRUiLogic.CreateDefaultControlState();

            Assert.True(TrackIRRuntimeLogic.ShouldScheduleTimeout(defaults));
            Assert.False(
                TrackIRRuntimeLogic.ShouldScheduleTimeout(
                    defaults with { IsTimeoutEnabled = false }
                )
            );

            TrackIRControlState timedOut = TrackIRRuntimeLogic.TimedOutControlState(defaults);

            Assert.False(timedOut.IsTrackIREnabled);
            Assert.False(timedOut.IsVideoEnabled);
            Assert.False(timedOut.IsMouseMovementEnabled);
            Assert.False(
                TrackIRRuntimeLogic.ShouldRescheduleTimeout(
                    defaults,
                    defaults with { MouseMovementSpeed = 1.5 }
                )
            );
            Assert.True(
                TrackIRRuntimeLogic.ShouldRescheduleTimeout(
                    defaults,
                    defaults with { TimeoutSeconds = defaults.TimeoutSeconds + 1 }
                )
            );
        }

        [Fact]
        public void TrackIRPreviewBitmapLogic_expands_gray8_pixels_to_bgra_in_place()
        {
            byte[] bgra = new byte[TrackIRPreviewBitmapLogic.Bgra32BufferLength(2, 1)];

            TrackIRPreviewBitmapLogic.ExpandGray8ToBgra32(new byte[] { 0x12, 0xAB }, bgra);

            Assert.True(
                TrackIRPreviewBitmapLogic.ShouldRecreateBitmap(
                    hasBitmap: false,
                    currentWidth: 0,
                    currentHeight: 0,
                    nextWidth: 2,
                    nextHeight: 1
                )
            );
            Assert.False(
                TrackIRPreviewBitmapLogic.ShouldRecreateBitmap(
                    hasBitmap: true,
                    currentWidth: 2,
                    currentHeight: 1,
                    nextWidth: 2,
                    nextHeight: 1
                )
            );
            Assert.Equal(2, TrackIRPreviewBitmapLogic.Gray8BufferLength(2, 1));
            Assert.Equal(new byte[] { 0x12, 0x12, 0x12, 0xFF, 0xAB, 0xAB, 0xAB, 0xFF }, bgra);
        }
    }
}
