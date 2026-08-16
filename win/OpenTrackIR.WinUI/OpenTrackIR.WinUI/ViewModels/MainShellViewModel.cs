using System.Windows.Input;
using Microsoft.UI;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using OpenTrackIR.WinUI.Models;
using OpenTrackIR.WinUI.Runtime;
using OpenTrackIR.WinUI.Services;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.WindowsRuntime;

namespace OpenTrackIR.WinUI.ViewModels
{
    public sealed class MainShellViewModel : ObservableObject, IDisposable
    {
        private static readonly IReadOnlyDictionary<string, SolidColorBrush> CachedBrushes =
            new Dictionary<string, SolidColorBrush>(StringComparer.OrdinalIgnoreCase)
            {
                ["#31C48D"] = CreateBrushCore("#31C48D"),
                ["#7A8797"] = CreateBrushCore("#7A8797"),
                ["#FFB020"] = CreateBrushCore("#FFB020"),
                ["#F05252"] = CreateBrushCore("#F05252"),
            };

        private readonly ISettingsStore _settingsStore;
        private readonly ITrackIRRuntimeController _runtimeController;
        private readonly ITrayService _trayService;
        private readonly object _previewQueueSyncRoot = new();
        private TrackIRControlState _controlState;
        private TrackIRSnapshot _snapshot;
        private ImageSource? _previewImageSource;
        private byte[] _previewGrayPixels = Array.Empty<byte>();
        private byte[] _previewBgraPixels = Array.Empty<byte>();
        private WriteableBitmap? _previewBitmap;
        private int _previewBitmapWidth;
        private int _previewBitmapHeight;
        private TrackIRPreviewFrame? _pendingPreviewFrame;
        private readonly DispatcherQueue? _dispatcherQueue;
        private bool _isAdvancedExpanded;
        private bool _showDetectedBlobCenter = true;
        private bool _isDisposed;
        private bool _isShuttingDown;
        private bool _isPreviewApplyQueued;
        private CancellationTokenSource? _timeoutCancellationSource;
        private DispatcherQueueTimer? _timeoutCountdownTimer;
        private DateTimeOffset? _timeoutDeadline;
        private bool _isWindowVisible = true;

        public MainShellViewModel()
            : this(new LocalSettingsStore(), new NativeTrackIRRuntimeController(), AppServices.TrayService)
        {
        }

        public MainShellViewModel(
            ISettingsStore settingsStore,
            ITrackIRRuntimeController runtimeController,
            ITrayService trayService
        )
        {
            _settingsStore = settingsStore;
            _runtimeController = runtimeController;
            _trayService = trayService;
            _dispatcherQueue = DispatcherQueue.GetForCurrentThread();
            _controlState = TrackIRUiLogic.Normalize(_settingsStore.Load());
            _snapshot = _runtimeController.CurrentSnapshot;
            _previewImageSource = null;

            RefreshCommand = new RelayCommand(Refresh);
            _runtimeController.SnapshotChanged += OnRuntimeSnapshotChanged;
            _runtimeController.PreviewFrameChanged += OnRuntimePreviewFrameChanged;
            ApplyControlState(_controlState, persist: false);
            _runtimeController.UpdatePresentationState(new TrackIRPresentationState(true, true));
        }

        public ICommand RefreshCommand { get; }

        public string Title => "OpenTrackIR";

        public string Subtitle => "Windows TrackIR preview and controls";

        public string ControlsDescription => "Live controls for TrackIR, video, and mouse movement.";

        public string AdvancedDescription => "Advanced tuning for blob detection, smoothing, keep-awake, and timeout.";

        public string MouseOutputModeDescription => "Use absolute cursor positioning to bypass Windows mouse acceleration. This takes more direct control of the pointer.";

        public string HotkeyHelperText => "Click the field and press a shortcut. It stays active while OpenTrackIR is running, even when the window is hidden.";

        public string BlobDetectionDescription => "Filter tiny blobs and keep the previous-regularized centroid mode for steadier results.";

        public string BlobCentroidFormulaText => "Centroid Formula: Previous-Regularized";

        public string BlobCentroidFormulaDetail => "Blend the new binary centroid with the previous center to calm tiny frame-to-frame wobble.";

        public string TimeoutHelperText => TrackIRUiLogic.TimeoutHelperText;

        public string TimeoutCountdownText => TrackIRUiLogic.TimeoutCountdownLabel(
            IsTrackIREnabled,
            IsTimeoutEnabled,
            _timeoutDeadline,
            DateTimeOffset.UtcNow
        );

        public bool IsAdvancedExpanded
        {
            get => _isAdvancedExpanded;
            set
            {
                if (SetProperty(ref _isAdvancedExpanded, value))
                {
                    SyncTimeoutCountdownTimer();
                }
            }
        }

        public bool ShowDetectedBlobCenter
        {
            get => _showDetectedBlobCenter;
            set
            {
                if (SetProperty(ref _showDetectedBlobCenter, value))
                {
                    OnPropertyChanged(nameof(PreviewMarkerVisibility));
                }
            }
        }

        public bool IsTrackIREnabled
        {
            get => _controlState.IsTrackIREnabled;
            set => UpdateControlState(_controlState with { IsTrackIREnabled = value });
        }

        public bool IsVideoEnabled
        {
            get => _controlState.IsVideoEnabled;
            set => UpdateControlState(_controlState with { IsVideoEnabled = value });
        }

        public bool IsMouseMovementEnabled
        {
            get => _controlState.IsMouseMovementEnabled;
            set => UpdateControlState(_controlState with { IsMouseMovementEnabled = value });
        }

        public double MouseMovementSpeed
        {
            get => _controlState.MouseMovementSpeed;
            set => UpdateControlState(_controlState with { MouseMovementSpeed = value });
        }

        public bool IsWindowsAbsoluteMousePositioningEnabled
        {
            get => _controlState.IsWindowsAbsoluteMousePositioningEnabled;
            set => UpdateControlState(_controlState with { IsWindowsAbsoluteMousePositioningEnabled = value });
        }

        public bool IsXKeysFastMouseEnabled
        {
            get => _controlState.IsXKeysFastMouseEnabled;
            set => UpdateControlState(_controlState with { IsXKeysFastMouseEnabled = value });
        }

        public double MouseSmoothing
        {
            get => _controlState.MouseSmoothing;
            set => UpdateControlState(_controlState with { MouseSmoothing = (int)Math.Round(value) });
        }

        public double MouseDeadzone
        {
            get => _controlState.MouseDeadzone;
            set => UpdateControlState(_controlState with { MouseDeadzone = value });
        }

        public bool IsAvoidMouseJumpsEnabled
        {
            get => _controlState.IsAvoidMouseJumpsEnabled;
            set => UpdateControlState(_controlState with { IsAvoidMouseJumpsEnabled = value });
        }

        public double MouseJumpThresholdPixels
        {
            get => _controlState.MouseJumpThresholdPixels;
            set => UpdateControlState(_controlState with { MouseJumpThresholdPixels = (int)Math.Round(value) });
        }

        public double MinimumBlobAreaPoints
        {
            get => _controlState.MinimumBlobAreaPoints;
            set => UpdateControlState(_controlState with { MinimumBlobAreaPoints = (int)Math.Round(value) });
        }

        public double KeepAwakeSeconds
        {
            get => _controlState.KeepAwakeSeconds;
            set => UpdateControlState(_controlState with { KeepAwakeSeconds = (int)Math.Round(value) });
        }

        public bool IsTimeoutEnabled
        {
            get => _controlState.IsTimeoutEnabled;
            set => UpdateControlState(_controlState with { IsTimeoutEnabled = value });
        }

        public double TimeoutSeconds
        {
            get => _controlState.TimeoutSeconds;
            set => UpdateControlState(_controlState with { TimeoutSeconds = (int)Math.Round(value) });
        }

        public bool IsVideoFlipHorizontalEnabled
        {
            get => _controlState.IsVideoFlipHorizontalEnabled;
            set => UpdateControlState(_controlState with { IsVideoFlipHorizontalEnabled = value });
        }

        public bool IsVideoFlipVerticalEnabled
        {
            get => _controlState.IsVideoFlipVerticalEnabled;
            set => UpdateControlState(_controlState with { IsVideoFlipVerticalEnabled = value });
        }

        public double VideoRotationDegrees
        {
            get => _controlState.VideoRotationDegrees;
            set => UpdateControlState(_controlState with { VideoRotationDegrees = value });
        }

        public double VideoFramesPerSecond
        {
            get => _controlState.VideoFramesPerSecond;
            set => UpdateControlState(_controlState with { VideoFramesPerSecond = value });
        }

        public string MouseToggleHotkeyText
        {
            get => _controlState.MouseToggleHotkeyText;
            set => UpdateControlState(_controlState with { MouseToggleHotkeyText = value });
        }

        public string TrackIRStatusText => IsTrackIREnabled ? "TrackIR On" : "TrackIR Off";

        public string TrackIRStatusValue => TrackIRUiLogic.ToggleStateLabel(IsTrackIREnabled, "On", "Off");

        public string VideoStatusText => IsVideoEnabled ? "Video Visible" : "Video Hidden";

        public string VideoStatusValue => TrackIRUiLogic.ToggleStateLabel(IsVideoEnabled, "Visible", "Hidden");

        public string MouseStatusText => IsMouseMovementEnabled ? "Mouse On" : "Mouse Off";

        public string MouseStatusValue => TrackIRUiLogic.ToggleStateLabel(IsMouseMovementEnabled, "On", "Off");

        public Brush TrackIRStatusBrush => CreateBrush(TrackIRUiLogic.ToggleStateColorHex(IsTrackIREnabled));

        public Brush VideoStatusBrush => CreateBrush(TrackIRUiLogic.ToggleStateColorHex(IsVideoEnabled));

        public Brush MouseStatusBrush => CreateBrush(TrackIRUiLogic.ToggleStateColorHex(IsMouseMovementEnabled));

        public string PreviewTitle => TrackIRUiLogic.PreviewTitle(_controlState, _snapshot);

        public string PreviewMessage => TrackIRUiLogic.PreviewMessage(_controlState, _snapshot);

        public string DeviceLabel => _snapshot.DeviceLabel;

        public string FramesPerSecondSummary =>
            TrackIRUiLogic.TrackIRRateSummaryLabel(VideoFramesPerSecond, _snapshot.SourceFrameRate);

        public string PositionLabel => TrackIRUiLogic.CentroidPairLabel(_snapshot.CentroidX, _snapshot.CentroidY);

        public string BackendLabel => _snapshot.BackendLabel;

        public string TrackIRFramesPerSecondLabel =>
            TrackIRUiLogic.TrackIRFramesPerSecondValueLabel(VideoFramesPerSecond);

        public string MouseSpeedLabel => TrackIRUiLogic.MouseSpeedValueLabel(MouseMovementSpeed);

        public string MouseSmoothingLabel => TrackIRUiLogic.MouseSmoothingValueLabel(MouseSmoothing);

        public string MouseDeadzoneLabel => TrackIRUiLogic.MouseDeadzoneValueLabel(MouseDeadzone);

        public string VideoRotationLabel => TrackIRUiLogic.VideoRotationValueLabel(VideoRotationDegrees);

        public string PacketTypeLabel => TrackIRUiLogic.PacketTypeLabel(_snapshot.PacketType);

        public string XKeysIndicatorText => TrackIRUiLogic.XKeysIndicatorLabel(_snapshot.XKeysIndicatorState);

        public Brush XKeysIndicatorBrush => CreateBrush(TrackIRUiLogic.XKeysIndicatorColorHex(_snapshot.XKeysIndicatorState));

        public string RuntimeModeLabel => _snapshot.IsLowPowerMode ? "Background" : "Interactive";

        public ImageSource? PreviewImageSource => _previewImageSource;

        public Visibility PreviewImageVisibility =>
            _previewImageSource is null ? Visibility.Collapsed : Visibility.Visible;

        public Visibility PreviewPlaceholderVisibility =>
            _previewImageSource is null ? Visibility.Visible : Visibility.Collapsed;

        public Visibility PreviewMarkerVisibility =>
            ShowDetectedBlobCenter && _snapshot.HasPreview && _snapshot.CentroidX.HasValue && _snapshot.CentroidY.HasValue
                ? Visibility.Visible
                : Visibility.Collapsed;

        public Visibility JumpThresholdVisibility =>
            IsAvoidMouseJumpsEnabled ? Visibility.Visible : Visibility.Collapsed;

        public Visibility TimeoutDurationVisibility =>
            IsTimeoutEnabled ? Visibility.Visible : Visibility.Collapsed;

        public double PreviewScaleX => TrackIRUiLogic.PreviewAxisScale(IsVideoFlipHorizontalEnabled);

        public double PreviewScaleY => TrackIRUiLogic.PreviewAxisScale(IsVideoFlipVerticalEnabled);

        public double PreviewRotationDegrees => TrackIRUiLogic.NormalizeRotationDegrees(VideoRotationDegrees);

        public double PreviewMarkerLeft =>
            Math.Clamp((_snapshot.CentroidX ?? 0) - 10, 0, TrackIRUiLogic.FrameWidth - 20);

        public double PreviewMarkerTop =>
            Math.Clamp((_snapshot.CentroidY ?? 0) - 10, 0, TrackIRUiLogic.FrameHeight - 20);

        public void SetPresentationState(bool isWindowVisible, bool isAppActive)
        {
            _isWindowVisible = isWindowVisible;
            SyncTimeoutCountdownTimer();
            _runtimeController.UpdatePresentationState(new TrackIRPresentationState(isWindowVisible, isAppActive));
        }

        public void ToggleMouseMovement()
        {
            IsMouseMovementEnabled = TrackIRUiLogic.ToggledMouseMovementState(IsMouseMovementEnabled);
        }

        public void Dispose()
        {
            if (_isDisposed)
            {
                return;
            }

            BeginShutdown();
            _isDisposed = true;
            ResetPreviewState(raisePropertyChanged: false);
            if (_runtimeController is IDisposable disposableRuntimeController)
            {
                disposableRuntimeController.Dispose();
            }
        }

        public void BeginShutdown()
        {
            if (_isShuttingDown)
            {
                return;
            }

            _isShuttingDown = true;
            lock (_previewQueueSyncRoot)
            {
                _pendingPreviewFrame = null;
                _isPreviewApplyQueued = false;
            }
            ResetPreviewState(raisePropertyChanged: false);
            _runtimeController.SnapshotChanged -= OnRuntimeSnapshotChanged;
            _runtimeController.PreviewFrameChanged -= OnRuntimePreviewFrameChanged;
            _timeoutCancellationSource?.Cancel();
            _timeoutCancellationSource?.Dispose();
            _timeoutCancellationSource = null;
            StopTimeoutCountdownTimer();
            _runtimeController.Stop();
        }

        private void Refresh()
        {
            ApplyControlState(_settingsStore.Load(), persist: false);
            _runtimeController.Refresh();
        }

        private void UpdateControlState(TrackIRControlState controlState)
        {
            ApplyControlState(controlState, persist: true);
        }

        private void ApplyControlState(TrackIRControlState controlState, bool persist)
        {
            TrackIRControlState previousControlState = _controlState;
            _controlState = TrackIRUiLogic.Normalize(controlState);
            if (persist)
            {
                _settingsStore.Save(_controlState);
            }

            _trayService.UpdateState(_controlState.IsTrackIREnabled, _controlState.IsMouseMovementEnabled);
            _runtimeController.UpdateControlState(_controlState);
            if ((_timeoutCancellationSource is null && TrackIRRuntimeLogic.ShouldScheduleTimeout(_controlState)) ||
                TrackIRRuntimeLogic.ShouldRescheduleTimeout(previousControlState, _controlState))
            {
                SyncTimeoutTask();
            }
            OnControlStateChanged(previousControlState);
        }

        private void OnRuntimeSnapshotChanged(object? sender, TrackIRSnapshot snapshot)
        {
            if (!TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(_isDisposed, _isShuttingDown))
            {
                return;
            }

            if (_dispatcherQueue is null)
            {
                TrackIRSnapshot previousSnapshot = _snapshot;
                _snapshot = snapshot;
                OnSnapshotChanged(previousSnapshot);
                return;
            }

            _dispatcherQueue.TryEnqueue(() =>
            {
                if (!TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(_isDisposed, _isShuttingDown))
                {
                    return;
                }

                TrackIRSnapshot previousSnapshot = _snapshot;
                _snapshot = snapshot;
                OnSnapshotChanged(previousSnapshot);
            });
        }

        private void OnRuntimePreviewFrameChanged(object? sender, TrackIRPreviewFrame? previewFrame)
        {
            if (!TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(_isDisposed, _isShuttingDown))
            {
                return;
            }

            if (_dispatcherQueue is null)
            {
                ApplyPreviewFrame(previewFrame);
                return;
            }

            bool shouldEnqueue;
            lock (_previewQueueSyncRoot)
            {
                _pendingPreviewFrame = previewFrame;
                shouldEnqueue = TrackIRRuntimeLogic.ShouldQueuePreviewApply(
                    _isDisposed,
                    _isShuttingDown,
                    _isPreviewApplyQueued
                );
                if (shouldEnqueue)
                {
                    _isPreviewApplyQueued = true;
                }
            }

            if (!shouldEnqueue)
            {
                return;
            }

            if (!_dispatcherQueue.TryEnqueue(ProcessPendingPreviewFrame))
            {
                lock (_previewQueueSyncRoot)
                {
                    _isPreviewApplyQueued = false;
                }
            }
        }

        private void ProcessPendingPreviewFrame()
        {
            while (true)
            {
                TrackIRPreviewFrame? previewFrame;
                lock (_previewQueueSyncRoot)
                {
                    _isPreviewApplyQueued = false;
                    if (!TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(_isDisposed, _isShuttingDown))
                    {
                        _pendingPreviewFrame = null;
                        return;
                    }

                    previewFrame = _pendingPreviewFrame;
                    _pendingPreviewFrame = null;
                }

                ApplyPreviewFrame(previewFrame);

                lock (_previewQueueSyncRoot)
                {
                    if (!TrackIRRuntimeLogic.ShouldQueuePreviewApply(
                        _isDisposed,
                        _isShuttingDown,
                        _isPreviewApplyQueued
                    ) ||
                        _pendingPreviewFrame is null)
                    {
                        return;
                    }

                    _isPreviewApplyQueued = true;
                }
            }
        }

        private void OnControlStateChanged(TrackIRControlState previousControlState)
        {
            NotifyIfChanged(previousControlState.IsTrackIREnabled, _controlState.IsTrackIREnabled, nameof(IsTrackIREnabled));
            NotifyIfChanged(previousControlState.IsVideoEnabled, _controlState.IsVideoEnabled, nameof(IsVideoEnabled));
            NotifyIfChanged(
                previousControlState.IsMouseMovementEnabled,
                _controlState.IsMouseMovementEnabled,
                nameof(IsMouseMovementEnabled)
            );
            NotifyIfChanged(
                previousControlState.IsWindowsAbsoluteMousePositioningEnabled,
                _controlState.IsWindowsAbsoluteMousePositioningEnabled,
                nameof(IsWindowsAbsoluteMousePositioningEnabled)
            );
            NotifyIfChanged(
                previousControlState.MouseMovementSpeed,
                _controlState.MouseMovementSpeed,
                nameof(MouseMovementSpeed)
            );
            NotifyIfChanged(
                previousControlState.IsXKeysFastMouseEnabled,
                _controlState.IsXKeysFastMouseEnabled,
                nameof(IsXKeysFastMouseEnabled)
            );
            NotifyIfChanged(previousControlState.MouseSmoothing, _controlState.MouseSmoothing, nameof(MouseSmoothing));
            NotifyIfChanged(previousControlState.MouseDeadzone, _controlState.MouseDeadzone, nameof(MouseDeadzone));
            NotifyIfChanged(
                previousControlState.IsAvoidMouseJumpsEnabled,
                _controlState.IsAvoidMouseJumpsEnabled,
                nameof(IsAvoidMouseJumpsEnabled)
            );
            NotifyIfChanged(
                previousControlState.MouseJumpThresholdPixels,
                _controlState.MouseJumpThresholdPixels,
                nameof(MouseJumpThresholdPixels)
            );
            NotifyIfChanged(
                previousControlState.MinimumBlobAreaPoints,
                _controlState.MinimumBlobAreaPoints,
                nameof(MinimumBlobAreaPoints)
            );
            NotifyIfChanged(previousControlState.KeepAwakeSeconds, _controlState.KeepAwakeSeconds, nameof(KeepAwakeSeconds));
            NotifyIfChanged(previousControlState.IsTimeoutEnabled, _controlState.IsTimeoutEnabled, nameof(IsTimeoutEnabled));
            NotifyIfChanged(previousControlState.TimeoutSeconds, _controlState.TimeoutSeconds, nameof(TimeoutSeconds));
            NotifyIfChanged(
                previousControlState.IsVideoFlipHorizontalEnabled,
                _controlState.IsVideoFlipHorizontalEnabled,
                nameof(IsVideoFlipHorizontalEnabled)
            );
            NotifyIfChanged(
                previousControlState.IsVideoFlipVerticalEnabled,
                _controlState.IsVideoFlipVerticalEnabled,
                nameof(IsVideoFlipVerticalEnabled)
            );
            NotifyIfChanged(
                previousControlState.VideoRotationDegrees,
                _controlState.VideoRotationDegrees,
                nameof(VideoRotationDegrees)
            );
            NotifyIfChanged(
                previousControlState.VideoFramesPerSecond,
                _controlState.VideoFramesPerSecond,
                nameof(VideoFramesPerSecond)
            );
            NotifyIfChanged(
                previousControlState.MouseToggleHotkeyText,
                _controlState.MouseToggleHotkeyText,
                nameof(MouseToggleHotkeyText)
            );

            if (previousControlState.IsTrackIREnabled != _controlState.IsTrackIREnabled)
            {
                OnPropertyChanged(nameof(TrackIRStatusText));
                OnPropertyChanged(nameof(TrackIRStatusValue));
                OnPropertyChanged(nameof(TrackIRStatusBrush));
                OnPropertyChanged(nameof(PreviewTitle));
                OnPropertyChanged(nameof(PreviewMessage));
            }

            if (previousControlState.IsVideoEnabled != _controlState.IsVideoEnabled)
            {
                OnPropertyChanged(nameof(VideoStatusText));
                OnPropertyChanged(nameof(VideoStatusValue));
                OnPropertyChanged(nameof(VideoStatusBrush));
                OnPropertyChanged(nameof(PreviewTitle));
                OnPropertyChanged(nameof(PreviewMessage));
            }

            if (previousControlState.IsMouseMovementEnabled != _controlState.IsMouseMovementEnabled)
            {
                OnPropertyChanged(nameof(MouseStatusText));
                OnPropertyChanged(nameof(MouseStatusValue));
                OnPropertyChanged(nameof(MouseStatusBrush));
            }

            if (previousControlState.MouseMovementSpeed != _controlState.MouseMovementSpeed)
            {
                OnPropertyChanged(nameof(MouseSpeedLabel));
            }

            if (previousControlState.MouseSmoothing != _controlState.MouseSmoothing)
            {
                OnPropertyChanged(nameof(MouseSmoothingLabel));
            }

            if (previousControlState.MouseDeadzone != _controlState.MouseDeadzone)
            {
                OnPropertyChanged(nameof(MouseDeadzoneLabel));
            }

            if (previousControlState.IsAvoidMouseJumpsEnabled != _controlState.IsAvoidMouseJumpsEnabled)
            {
                OnPropertyChanged(nameof(JumpThresholdVisibility));
            }

            if (previousControlState.IsTimeoutEnabled != _controlState.IsTimeoutEnabled)
            {
                OnPropertyChanged(nameof(TimeoutDurationVisibility));
            }
            if (previousControlState.IsTrackIREnabled != _controlState.IsTrackIREnabled ||
                previousControlState.IsTimeoutEnabled != _controlState.IsTimeoutEnabled)
            {
                OnPropertyChanged(nameof(TimeoutCountdownText));
            }

            if (previousControlState.IsVideoFlipHorizontalEnabled != _controlState.IsVideoFlipHorizontalEnabled)
            {
                OnPropertyChanged(nameof(PreviewScaleX));
            }

            if (previousControlState.IsVideoFlipVerticalEnabled != _controlState.IsVideoFlipVerticalEnabled)
            {
                OnPropertyChanged(nameof(PreviewScaleY));
            }

            if (previousControlState.VideoRotationDegrees != _controlState.VideoRotationDegrees)
            {
                OnPropertyChanged(nameof(PreviewRotationDegrees));
                OnPropertyChanged(nameof(VideoRotationLabel));
            }

            if (previousControlState.VideoFramesPerSecond != _controlState.VideoFramesPerSecond)
            {
                OnPropertyChanged(nameof(TrackIRFramesPerSecondLabel));
                OnPropertyChanged(nameof(FramesPerSecondSummary));
            }
        }

        private void OnSnapshotChanged(TrackIRSnapshot previousSnapshot)
        {
            NotifyIfChanged(
                TrackIRUiLogic.PreviewTitle(_controlState, previousSnapshot),
                TrackIRUiLogic.PreviewTitle(_controlState, _snapshot),
                nameof(PreviewTitle)
            );
            NotifyIfChanged(
                TrackIRUiLogic.PreviewMessage(_controlState, previousSnapshot),
                TrackIRUiLogic.PreviewMessage(_controlState, _snapshot),
                nameof(PreviewMessage)
            );
            NotifyIfChanged(previousSnapshot.DeviceLabel, _snapshot.DeviceLabel, nameof(DeviceLabel));
            NotifyIfChanged(
                TrackIRUiLogic.TrackIRRateSummaryLabel(_controlState.VideoFramesPerSecond, previousSnapshot.SourceFrameRate),
                TrackIRUiLogic.TrackIRRateSummaryLabel(_controlState.VideoFramesPerSecond, _snapshot.SourceFrameRate),
                nameof(FramesPerSecondSummary)
            );
            NotifyIfChanged(
                TrackIRUiLogic.CentroidPairLabel(previousSnapshot.CentroidX, previousSnapshot.CentroidY),
                TrackIRUiLogic.CentroidPairLabel(_snapshot.CentroidX, _snapshot.CentroidY),
                nameof(PositionLabel)
            );
            NotifyIfChanged(previousSnapshot.BackendLabel, _snapshot.BackendLabel, nameof(BackendLabel));
            NotifyIfChanged(
                TrackIRUiLogic.PacketTypeLabel(previousSnapshot.PacketType),
                TrackIRUiLogic.PacketTypeLabel(_snapshot.PacketType),
                nameof(PacketTypeLabel)
            );
            if (previousSnapshot.XKeysIndicatorState != _snapshot.XKeysIndicatorState)
            {
                OnPropertyChanged(nameof(XKeysIndicatorText));
                OnPropertyChanged(nameof(XKeysIndicatorBrush));
            }

            if (previousSnapshot.IsLowPowerMode != _snapshot.IsLowPowerMode)
            {
                OnPropertyChanged(nameof(RuntimeModeLabel));
            }

            bool didPreviewMarkerChange =
                previousSnapshot.HasPreview != _snapshot.HasPreview ||
                previousSnapshot.CentroidX != _snapshot.CentroidX ||
                previousSnapshot.CentroidY != _snapshot.CentroidY;
            if (didPreviewMarkerChange)
            {
                OnPropertyChanged(nameof(PreviewMarkerVisibility));
                OnPropertyChanged(nameof(PreviewMarkerLeft));
                OnPropertyChanged(nameof(PreviewMarkerTop));
            }
        }

        private void SyncTimeoutTask()
        {
            _timeoutCancellationSource?.Cancel();
            _timeoutCancellationSource?.Dispose();
            _timeoutCancellationSource = null;
            _timeoutDeadline = null;
            SyncTimeoutCountdownTimer();

            if (!TrackIRRuntimeLogic.ShouldScheduleTimeout(_controlState))
            {
                return;
            }

            int timeoutSeconds = _controlState.TimeoutSeconds;
            _timeoutDeadline = DateTimeOffset.UtcNow.AddSeconds(timeoutSeconds);
            SyncTimeoutCountdownTimer();
            CancellationTokenSource cancellationSource = new();
            _timeoutCancellationSource = cancellationSource;
            _ = Task.Run(async () =>
            {
                try
                {
                    await Task.Delay(TimeSpan.FromSeconds(timeoutSeconds), cancellationSource.Token)
                        .ConfigureAwait(false);
                }
                catch (OperationCanceledException)
                {
                    return;
                }

                if (cancellationSource.IsCancellationRequested ||
                    !TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(_isDisposed, _isShuttingDown)) {
                    return;
                }

                if (_dispatcherQueue is null)
                {
                    ApplyTimeout();
                    return;
                }

                _dispatcherQueue.TryEnqueue(() =>
                {
                    if (cancellationSource.IsCancellationRequested ||
                        !TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(_isDisposed, _isShuttingDown))
                    {
                        return;
                    }

                    ApplyTimeout();
                });
            });
        }

        private void SyncTimeoutCountdownTimer()
        {
            StopTimeoutCountdownTimer();
            OnPropertyChanged(nameof(TimeoutCountdownText));

            if (_dispatcherQueue is null ||
                !_isWindowVisible ||
                !IsAdvancedExpanded ||
                !_timeoutDeadline.HasValue)
            {
                return;
            }

            _timeoutCountdownTimer = _dispatcherQueue.CreateTimer();
            _timeoutCountdownTimer.Interval = TimeSpan.FromSeconds(1);
            _timeoutCountdownTimer.Tick += OnTimeoutCountdownTick;
            _timeoutCountdownTimer.Start();
        }

        private void StopTimeoutCountdownTimer()
        {
            if (_timeoutCountdownTimer is null)
            {
                return;
            }

            _timeoutCountdownTimer.Stop();
            _timeoutCountdownTimer.Tick -= OnTimeoutCountdownTick;
            _timeoutCountdownTimer = null;
        }

        private void OnTimeoutCountdownTick(DispatcherQueueTimer sender, object args)
        {
            OnPropertyChanged(nameof(TimeoutCountdownText));
            if (TrackIRUiLogic.TimeoutRemainingSeconds(_timeoutDeadline, DateTimeOffset.UtcNow) == 0)
            {
                StopTimeoutCountdownTimer();
            }
        }

        private void ApplyTimeout()
        {
            UpdateControlState(TrackIRRuntimeLogic.TimedOutControlState(_controlState));
        }

        private static SolidColorBrush CreateBrush(string hexColor)
        {
            return CachedBrushes.TryGetValue(hexColor, out SolidColorBrush? cachedBrush)
                ? cachedBrush
                : CreateBrushCore(hexColor);
        }

        private static SolidColorBrush CreateBrushCore(string hexColor)
        {
            string normalized = hexColor.TrimStart('#');
            byte a = 0xFF;
            int offset = 0;

            if (normalized.Length == 8)
            {
                a = Convert.ToByte(normalized[..2], 16);
                offset = 2;
            }

            byte r = Convert.ToByte(normalized.Substring(offset, 2), 16);
            byte g = Convert.ToByte(normalized.Substring(offset + 2, 2), 16);
            byte b = Convert.ToByte(normalized.Substring(offset + 4, 2), 16);
            return new SolidColorBrush(ColorHelper.FromArgb(a, r, g, b));
        }

        private void NotifyIfChanged<T>(T previousValue, T currentValue, string propertyName)
        {
            if (!EqualityComparer<T>.Default.Equals(previousValue, currentValue))
            {
                OnPropertyChanged(propertyName);
            }
        }

        private void ApplyPreviewFrame(TrackIRPreviewFrame? previewFrame)
        {
            if (!TrackIRRuntimeLogic.ShouldApplyRuntimeUpdate(_isDisposed, _isShuttingDown))
            {
                return;
            }

            if (previewFrame is null)
            {
                ResetPreviewState(raisePropertyChanged: true);
                return;
            }

            int grayLength = TrackIRPreviewBitmapLogic.Gray8BufferLength(previewFrame.Width, previewFrame.Height);
            if (_previewGrayPixels.Length != grayLength)
            {
                _previewGrayPixels = new byte[grayLength];
            }

            if (!_runtimeController.TryCopyCurrentPreviewFrame(_previewGrayPixels, out TrackIRPreviewFrame? copiedPreviewFrame) ||
                copiedPreviewFrame is null)
            {
                return;
            }

            int bgraLength = TrackIRPreviewBitmapLogic.Bgra32BufferLength(
                copiedPreviewFrame.Width,
                copiedPreviewFrame.Height
            );
            if (_previewBgraPixels.Length != bgraLength)
            {
                _previewBgraPixels = new byte[bgraLength];
            }

            TrackIRPreviewBitmapLogic.ExpandGray8ToBgra32(_previewGrayPixels, _previewBgraPixels);

            bool didCreateBitmap = TrackIRPreviewBitmapLogic.ShouldRecreateBitmap(
                hasBitmap: _previewBitmap is not null,
                currentWidth: _previewBitmapWidth,
                currentHeight: _previewBitmapHeight,
                nextWidth: copiedPreviewFrame.Width,
                nextHeight: copiedPreviewFrame.Height
            );
            if (didCreateBitmap)
            {
                _previewBitmap = new WriteableBitmap(copiedPreviewFrame.Width, copiedPreviewFrame.Height);
                _previewBitmapWidth = copiedPreviewFrame.Width;
                _previewBitmapHeight = copiedPreviewFrame.Height;
            }

            try
            {
                using Stream pixelStream = _previewBitmap!.PixelBuffer.AsStream();
                pixelStream.Position = 0;
                pixelStream.Write(_previewBgraPixels, 0, _previewBgraPixels.Length);
                _previewBitmap.Invalidate();
                _previewImageSource = _previewBitmap;
                if (didCreateBitmap)
                {
                    OnPropertyChanged(nameof(PreviewImageSource));
                }
                OnPropertyChanged(nameof(PreviewImageVisibility));
                OnPropertyChanged(nameof(PreviewPlaceholderVisibility));
                OnPropertyChanged(nameof(PreviewMarkerVisibility));
            }
            catch (COMException)
            {
                ResetPreviewState(raisePropertyChanged: false);
            }
        }

        private void ResetPreviewState(bool raisePropertyChanged)
        {
            _previewBitmap = null;
            _previewBitmapWidth = 0;
            _previewBitmapHeight = 0;
            _previewGrayPixels = Array.Empty<byte>();
            _previewBgraPixels = Array.Empty<byte>();
            _previewImageSource = null;
            if (!raisePropertyChanged)
            {
                return;
            }

            OnPropertyChanged(nameof(PreviewImageSource));
            OnPropertyChanged(nameof(PreviewImageVisibility));
            OnPropertyChanged(nameof(PreviewPlaceholderVisibility));
            OnPropertyChanged(nameof(PreviewMarkerVisibility));
        }
    }
}
