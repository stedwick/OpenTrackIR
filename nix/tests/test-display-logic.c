#include <glib.h>

#include "opentrackir-display-logic.h"

static void
test_preview_policy_requires_visible_active_video (void)
{
	g_assert_true (opentrackir_preview_should_run (TRUE,
	                                                TRUE,
	                                                TRUE,
	                                                OPENTRACKIR_SESSION_PHASE_STREAMING));
	g_assert_true (opentrackir_preview_should_run (TRUE,
	                                                TRUE,
	                                                TRUE,
	                                                OPENTRACKIR_SESSION_PHASE_STARTING));
	g_assert_false (opentrackir_preview_should_run (FALSE,
	                                                 TRUE,
	                                                 TRUE,
	                                                 OPENTRACKIR_SESSION_PHASE_STREAMING));
	g_assert_false (opentrackir_preview_should_run (TRUE,
	                                                 FALSE,
	                                                 TRUE,
	                                                 OPENTRACKIR_SESSION_PHASE_STREAMING));
	g_assert_false (opentrackir_preview_should_run (TRUE,
	                                                 TRUE,
	                                                 FALSE,
	                                                 OPENTRACKIR_SESSION_PHASE_STREAMING));
	g_assert_false (opentrackir_preview_should_run (TRUE,
	                                                 TRUE,
	                                                 TRUE,
	                                                 OPENTRACKIR_SESSION_PHASE_FAILED));
}

static void
test_preview_copy_requires_new_generation (void)
{
	g_assert_false (opentrackir_preview_should_copy (FALSE, 2, 1));
	g_assert_false (opentrackir_preview_should_copy (TRUE, 0, 0));
	g_assert_false (opentrackir_preview_should_copy (TRUE, 2, 2));
	g_assert_true (opentrackir_preview_should_copy (TRUE, 2, 1));
}

static void
test_timeout_policy_requires_enabled_camera_and_duration (void)
{
	g_assert_true (opentrackir_timeout_should_run (TRUE, TRUE, 60));
	g_assert_false (opentrackir_timeout_should_run (FALSE, TRUE, 60));
	g_assert_false (opentrackir_timeout_should_run (TRUE, FALSE, 60));
	g_assert_false (opentrackir_timeout_should_run (TRUE, TRUE, 0));
}

static void
test_timeout_remaining_rounds_up_and_formats_duration (void)
{
	g_autofree char *formatted = NULL;
	guint remaining;

	remaining = opentrackir_timeout_remaining_seconds (10 * G_USEC_PER_SEC,
	                                                   1500001);
	g_assert_cmpuint (remaining, ==, 9);
	g_assert_cmpuint (opentrackir_timeout_remaining_seconds (100, 100), ==, 0);
	g_assert_cmpuint (opentrackir_timeout_remaining_seconds (0, 0), ==, 0);

	formatted = opentrackir_format_timeout_remaining (8 * 3600 + 7 * 60 + 6);
	g_assert_cmpstr (formatted, ==, "Remaining: 08:07:06");
}

static void
test_telemetry_formatting (void)
{
	g_autofree char *frame_rate = opentrackir_format_frame_rate (TRUE, 119.54);
	g_autofree char *no_frame_rate = opentrackir_format_frame_rate (FALSE, 119.54);
	g_autofree char *packet_type = opentrackir_format_packet_type (TRUE, 0x05);
	g_autofree char *centroid = opentrackir_format_centroid (TRUE, 321.25, 123.75);

	g_assert_cmpstr (frame_rate, ==, "119.5 fps");
	g_assert_cmpstr (no_frame_rate, ==, "—");
	g_assert_cmpstr (packet_type, ==, "0x05");
	g_assert_cmpstr (centroid, ==, "321.2, 123.8");
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/display/preview-policy", test_preview_policy_requires_visible_active_video);
	g_test_add_func ("/linux/display/preview-generation", test_preview_copy_requires_new_generation);
	g_test_add_func ("/linux/display/timeout-policy", test_timeout_policy_requires_enabled_camera_and_duration);
	g_test_add_func ("/linux/display/timeout-remaining", test_timeout_remaining_rounds_up_and_formats_duration);
	g_test_add_func ("/linux/display/telemetry-formatting", test_telemetry_formatting);

	return g_test_run ();
}
