#include <glib.h>

#include "opentrackir-lifecycle-policy.h"

static void
test_background_mode_hides_holds_and_shows_notifier (void)
{
	OpentrackirLifecyclePolicy policy;

	policy = opentrackir_lifecycle_policy (TRUE, TRUE, TRUE, TRUE);
	g_assert_cmpint (policy.close_behavior, ==, OPENTRACKIR_CLOSE_BEHAVIOR_HIDE);
	g_assert_true (policy.hold_application);
	g_assert_true (policy.show_status_notifier);
	g_assert_false (policy.use_low_power);

	policy = opentrackir_lifecycle_policy (FALSE, TRUE, TRUE, TRUE);
	g_assert_cmpint (policy.close_behavior, ==, OPENTRACKIR_CLOSE_BEHAVIOR_QUIT);
	g_assert_false (policy.hold_application);
	g_assert_false (policy.show_status_notifier);
}

static void
test_hidden_camera_uses_low_power_only_without_mouse_movement (void)
{
	OpentrackirLifecyclePolicy policy;

	policy = opentrackir_lifecycle_policy (TRUE, TRUE, FALSE, FALSE);
	g_assert_true (policy.use_low_power);

	policy = opentrackir_lifecycle_policy (TRUE, TRUE, TRUE, FALSE);
	g_assert_false (policy.use_low_power);

	policy = opentrackir_lifecycle_policy (TRUE, FALSE, FALSE, FALSE);
	g_assert_false (policy.use_low_power);

	policy = opentrackir_lifecycle_policy (TRUE, TRUE, FALSE, TRUE);
	g_assert_false (policy.use_low_power);
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/lifecycle/background-mode",
	                 test_background_mode_hides_holds_and_shows_notifier);
	g_test_add_func ("/linux/lifecycle/low-power",
	                 test_hidden_camera_uses_low_power_only_without_mouse_movement);

	return g_test_run ();
}
