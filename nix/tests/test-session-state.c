#include <glib.h>
#include <string.h>

#include "opentrackir-session-state.h"

static void
test_state_maps_snapshot (void)
{
	otir_trackir_session_snapshot snapshot = {
		.phase = OTIR_TRACKIR_SESSION_PHASE_STREAMING,
		.status = OTIR_STATUS_OK,
		.frame_index = 42,
		.has_frame_rate = true,
		.frame_rate = 119.5,
		.has_centroid = true,
		.centroid_x = 321.25,
		.centroid_y = 123.75,
		.has_packet_type = true,
		.packet_type = 0x05,
		.has_error_message = true,
	};
	OpentrackirSessionState state;

	g_strlcpy (snapshot.error_message,
	           "Observed transport error",
	           sizeof snapshot.error_message);
	opentrackir_session_state_from_snapshot (&snapshot, &state);

	g_assert_cmpint (state.phase, ==, OPENTRACKIR_SESSION_PHASE_STREAMING);
	g_assert_cmpint (state.status, ==, OTIR_STATUS_OK);
	g_assert_cmpuint (state.frame_index, ==, 42);
	g_assert_true (state.has_frame_rate);
	g_assert_cmpfloat (state.frame_rate, ==, 119.5);
	g_assert_true (state.has_centroid);
	g_assert_cmpfloat (state.centroid_x, ==, 321.25);
	g_assert_cmpfloat (state.centroid_y, ==, 123.75);
	g_assert_true (state.has_packet_type);
	g_assert_cmpuint (state.packet_type, ==, 0x05);
	g_assert_true (state.has_error_message);
	g_assert_cmpstr (state.error_message, ==, "Observed transport error");
}

static void
test_state_ignores_absent_values_when_comparing (void)
{
	OpentrackirSessionState left = { 0 };
	OpentrackirSessionState right = { 0 };

	left.frame_rate = 100.0;
	right.frame_rate = 200.0;
	left.centroid_x = 10.0;
	right.centroid_x = 20.0;
	left.packet_type = 0x05;
	right.packet_type = 0x00;
	g_strlcpy (left.error_message, "old", sizeof left.error_message);
	g_strlcpy (right.error_message, "new", sizeof right.error_message);

	g_assert_true (opentrackir_session_state_equal (&left, &right));

	right.frame_index = 1;
	g_assert_false (opentrackir_session_state_equal (&left, &right));
}

static void
test_state_lifecycle_policy (void)
{
	OpentrackirSessionState state = { 0 };

	state.phase = OPENTRACKIR_SESSION_PHASE_IDLE;
	g_assert_true (opentrackir_session_state_can_start (&state));
	g_assert_false (opentrackir_session_state_can_stop (&state));

	state.phase = OPENTRACKIR_SESSION_PHASE_STARTING;
	g_assert_false (opentrackir_session_state_can_start (&state));
	g_assert_true (opentrackir_session_state_can_stop (&state));

	state.phase = OPENTRACKIR_SESSION_PHASE_STREAMING;
	g_assert_false (opentrackir_session_state_can_start (&state));
	g_assert_true (opentrackir_session_state_can_stop (&state));

	state.phase = OPENTRACKIR_SESSION_PHASE_UNAVAILABLE;
	g_assert_true (opentrackir_session_state_can_start (&state));
	g_assert_false (opentrackir_session_state_can_stop (&state));

	state.phase = OPENTRACKIR_SESSION_PHASE_FAILED;
	g_assert_true (opentrackir_session_state_can_start (&state));
	g_assert_false (opentrackir_session_state_can_stop (&state));
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/session-state/maps-snapshot", test_state_maps_snapshot);
	g_test_add_func ("/linux/session-state/ignores-absent-values", test_state_ignores_absent_values_when_comparing);
	g_test_add_func ("/linux/session-state/lifecycle-policy", test_state_lifecycle_policy);

	return g_test_run ();
}
