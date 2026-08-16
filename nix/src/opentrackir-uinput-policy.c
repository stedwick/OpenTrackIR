#include "opentrackir-uinput-policy.h"

#include <errno.h>
#include <limits.h>
#include <linux/input-event-codes.h>

OpentrackirUinputPhase
opentrackir_uinput_phase_for_error (int error_number)
{
	int normalized_error = error_number < 0 ? -error_number : error_number;

	switch (normalized_error)
	{
	case ENOENT:
	case ENODEV:
	case ENXIO:
		return OPENTRACKIR_UINPUT_PHASE_UNAVAILABLE;
	case EACCES:
	case EPERM:
		return OPENTRACKIR_UINPUT_PHASE_PERMISSION_DENIED;
	case ENOSYS:
	case ENOTTY:
#if EOPNOTSUPP != ENOTSUP
	case EOPNOTSUPP:
#endif
	case ENOTSUP:
		return OPENTRACKIR_UINPUT_PHASE_UNSUPPORTED;
	default:
		return OPENTRACKIR_UINPUT_PHASE_FAILED;
	}
}

const char *
opentrackir_uinput_phase_message (OpentrackirUinputPhase phase)
{
	switch (phase)
	{
	case OPENTRACKIR_UINPUT_PHASE_DISABLED:
		return "Mouse movement is off.";
	case OPENTRACKIR_UINPUT_PHASE_STARTING:
		return "Creating the OpenTrackIR virtual pointer…";
	case OPENTRACKIR_UINPUT_PHASE_READY:
		return "Virtual pointer ready.";
	case OPENTRACKIR_UINPUT_PHASE_UNAVAILABLE:
		return "/dev/uinput is unavailable. Load the uinput kernel module and try again.";
	case OPENTRACKIR_UINPUT_PHASE_PERMISSION_DENIED:
		return "Access to /dev/uinput was denied. Install the OpenTrackIR udev rule and reconnect or reboot.";
	case OPENTRACKIR_UINPUT_PHASE_UNSUPPORTED:
		return "This kernel does not support the required uinput interface.";
	case OPENTRACKIR_UINPUT_PHASE_WRITE_FAILED:
		return "Writing to the virtual pointer failed. Disable and re-enable mouse movement to retry.";
	case OPENTRACKIR_UINPUT_PHASE_FAILED:
	default:
		return "The virtual pointer failed. Disable and re-enable mouse movement to retry.";
	}
}

gboolean
opentrackir_uinput_state_equal (const OpentrackirUinputState *left,
                                const OpentrackirUinputState *right)
{
	g_return_val_if_fail (left != NULL, FALSE);
	g_return_val_if_fail (right != NULL, FALSE);

	return left->phase == right->phase &&
	       left->error_number == right->error_number &&
	       left->event_frame_count == right->event_frame_count;
}

OpentrackirRelativeDispatch
opentrackir_relative_dispatch_consume (double pending_x,
                                       double pending_y)
{
	OpentrackirRelativeDispatch dispatch = { 0 };

	if (pending_x >= INT_MAX)
		dispatch.delta_x = INT_MAX;
	else if (pending_x <= INT_MIN)
		dispatch.delta_x = INT_MIN;
	else
		dispatch.delta_x = (int)pending_x;

	if (pending_y >= INT_MAX)
		dispatch.delta_y = INT_MAX;
	else if (pending_y <= INT_MIN)
		dispatch.delta_y = INT_MIN;
	else
		dispatch.delta_y = (int)pending_y;

	dispatch.remaining_x = pending_x - dispatch.delta_x;
	dispatch.remaining_y = pending_y - dispatch.delta_y;
	return dispatch;
}

otir_trackir_mouse_tracker_config
opentrackir_build_mouse_tracker_config (double   control_speed,
                                        double   smoothing,
                                        double   deadzone,
                                        gboolean avoid_mouse_jumps,
                                        double   jump_threshold_pixels,
                                        gboolean horizontal_flip,
                                        gboolean vertical_flip,
                                        double   rotation_degrees)
{
	return (otir_trackir_mouse_tracker_config) {
		.is_movement_enabled = true,
		/* Match the control-to-tracker scale used by the macOS and Windows apps. */
		.speed = control_speed * 10.0,
		.smoothing = smoothing,
		.deadzone = deadzone,
		.avoid_mouse_jumps = avoid_mouse_jumps,
		.jump_threshold_pixels = jump_threshold_pixels,
		.transform = {
			.scale_x = horizontal_flip ? -1.0 : 1.0,
			.scale_y = vertical_flip ? -1.0 : 1.0,
			.rotation_degrees = rotation_degrees,
		},
	};
}

gsize
opentrackir_uinput_build_event_frame (int                    delta_x,
                                     int                    delta_y,
                                     OpentrackirUinputEvent *events,
                                     gsize                  capacity)
{
	gsize required = (delta_x != 0 ? 1 : 0) + (delta_y != 0 ? 1 : 0);
	gsize count = 0;

	if (required == 0)
		return 0;
	required += 1;
	if (events == NULL || capacity < required)
		return required;

	if (delta_x != 0)
		events[count++] = (OpentrackirUinputEvent){ EV_REL, REL_X, delta_x };
	if (delta_y != 0)
		events[count++] = (OpentrackirUinputEvent){ EV_REL, REL_Y, delta_y };
	events[count++] = (OpentrackirUinputEvent){ EV_SYN, SYN_REPORT, 0 };
	return count;
}
