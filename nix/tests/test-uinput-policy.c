#include <errno.h>
#include <glib.h>
#include <linux/input-event-codes.h>

#include "opentrackir-uinput-policy.h"

static void
test_error_mapping_is_actionable (void)
{
	g_assert_cmpint (opentrackir_uinput_phase_for_error (-ENOENT), ==, OPENTRACKIR_UINPUT_PHASE_UNAVAILABLE);
	g_assert_cmpint (opentrackir_uinput_phase_for_error (-EACCES), ==, OPENTRACKIR_UINPUT_PHASE_PERMISSION_DENIED);
	g_assert_cmpint (opentrackir_uinput_phase_for_error (-EPERM), ==, OPENTRACKIR_UINPUT_PHASE_PERMISSION_DENIED);
	g_assert_cmpint (opentrackir_uinput_phase_for_error (-ENOTTY), ==, OPENTRACKIR_UINPUT_PHASE_UNSUPPORTED);
	g_assert_cmpint (opentrackir_uinput_phase_for_error (-EIO), ==, OPENTRACKIR_UINPUT_PHASE_FAILED);
	g_assert_nonnull (g_strstr_len (opentrackir_uinput_phase_message (OPENTRACKIR_UINPUT_PHASE_WRITE_FAILED),
	                                    -1,
	                                    "Writing"));
	g_assert_nonnull (g_strstr_len (opentrackir_uinput_phase_message (OPENTRACKIR_UINPUT_PHASE_PERMISSION_DENIED),
	                                    -1,
	                                    "/dev/uinput"));
}

static void
test_fractional_delta_is_retained (void)
{
	OpentrackirRelativeDispatch dispatch;

	dispatch = opentrackir_relative_dispatch_consume (2.75, -1.25);
	g_assert_cmpint (dispatch.delta_x, ==, 2);
	g_assert_cmpint (dispatch.delta_y, ==, -1);
	g_assert_cmpfloat (dispatch.remaining_x, ==, 0.75);
	g_assert_cmpfloat (dispatch.remaining_y, ==, -0.25);

	dispatch = opentrackir_relative_dispatch_consume (dispatch.remaining_x + 0.5,
	                                                  dispatch.remaining_y - 0.9);
	g_assert_cmpint (dispatch.delta_x, ==, 1);
	g_assert_cmpint (dispatch.delta_y, ==, -1);
	g_assert_cmpfloat_with_epsilon (dispatch.remaining_x, 0.25, 1e-12);
	g_assert_cmpfloat_with_epsilon (dispatch.remaining_y, -0.15, 1e-12);
}

static void
test_event_frame_contains_only_required_events (void)
{
	OpentrackirUinputEvent events[3];
	gsize count;

	count = opentrackir_uinput_build_event_frame (4, -2, events, G_N_ELEMENTS (events));
	g_assert_cmpuint (count, ==, 3);
	g_assert_cmpuint (events[0].type, ==, EV_REL);
	g_assert_cmpuint (events[0].code, ==, REL_X);
	g_assert_cmpint (events[0].value, ==, 4);
	g_assert_cmpuint (events[1].type, ==, EV_REL);
	g_assert_cmpuint (events[1].code, ==, REL_Y);
	g_assert_cmpint (events[1].value, ==, -2);
	g_assert_cmpuint (events[2].type, ==, EV_SYN);
	g_assert_cmpuint (events[2].code, ==, SYN_REPORT);

	count = opentrackir_uinput_build_event_frame (0, 3, events, G_N_ELEMENTS (events));
	g_assert_cmpuint (count, ==, 2);
	g_assert_cmpuint (events[0].code, ==, REL_Y);
	g_assert_cmpuint (events[1].type, ==, EV_SYN);

	g_assert_cmpuint (opentrackir_uinput_build_event_frame (0, 0, events, G_N_ELEMENTS (events)), ==, 0);
}

static void
test_mouse_settings_map_to_shared_tracker (void)
{
	otir_trackir_mouse_tracker_config config;

	config = opentrackir_build_mouse_tracker_config (2.5,
	                                                  4.0,
	                                                  0.08,
	                                                  TRUE,
	                                                  75.0,
	                                                  TRUE,
	                                                  FALSE,
	                                                  12.0);
	g_assert_true (config.is_movement_enabled);
	g_assert_cmpfloat (config.speed, ==, 25.0);
	g_assert_cmpfloat (config.smoothing, ==, 4.0);
	g_assert_cmpfloat (config.deadzone, ==, 0.08);
	g_assert_true (config.avoid_mouse_jumps);
	g_assert_cmpfloat (config.jump_threshold_pixels, ==, 75.0);
	g_assert_cmpfloat (config.transform.scale_x, ==, -1.0);
	g_assert_cmpfloat (config.transform.scale_y, ==, 1.0);
	g_assert_cmpfloat (config.transform.rotation_degrees, ==, 12.0);
}

static void
test_keep_awake_requires_camera_without_mouse_tracking (void)
{
	OpentrackirRelativeDelta delta;

	g_assert_true (opentrackir_keep_awake_should_run (TRUE, FALSE, 29));
	g_assert_false (opentrackir_keep_awake_should_run (FALSE, FALSE, 29));
	g_assert_false (opentrackir_keep_awake_should_run (TRUE, TRUE, 29));
	g_assert_false (opentrackir_keep_awake_should_run (TRUE, FALSE, 0));

	delta = opentrackir_keep_awake_delta (0);
	g_assert_cmpint (delta.delta_x, ==, 1);
	g_assert_cmpint (delta.delta_y, ==, 0);
	delta = opentrackir_keep_awake_delta (5);
	g_assert_cmpint (delta.delta_x, ==, -1);
	g_assert_cmpint (delta.delta_y, ==, 0);
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/uinput/error-mapping", test_error_mapping_is_actionable);
	g_test_add_func ("/linux/uinput/fractional-delta", test_fractional_delta_is_retained);
	g_test_add_func ("/linux/uinput/event-frame", test_event_frame_contains_only_required_events);
	g_test_add_func ("/linux/uinput/tracker-config", test_mouse_settings_map_to_shared_tracker);
	g_test_add_func ("/linux/uinput/keep-awake", test_keep_awake_requires_camera_without_mouse_tracking);

	return g_test_run ();
}
