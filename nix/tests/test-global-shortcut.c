#include <glib.h>

#include "opentrackir-global-shortcut.h"

static void
test_default_trigger_is_shift_f7 (void)
{
	g_assert_cmpstr (opentrackir_global_shortcut_preferred_trigger (), ==, "SHIFT+F7");
}

static void
test_activation_requires_matching_session_and_action (void)
{
	g_assert_true (opentrackir_global_shortcut_activation_matches (
		"/org/freedesktop/portal/desktop/session/1_2/open",
		"/org/freedesktop/portal/desktop/session/1_2/open",
		"toggle-mouse"));
	g_assert_false (opentrackir_global_shortcut_activation_matches (
		"/org/freedesktop/portal/desktop/session/1_2/open",
		"/org/freedesktop/portal/desktop/session/1_2/other",
		"toggle-mouse"));
	g_assert_false (opentrackir_global_shortcut_activation_matches (
		"/org/freedesktop/portal/desktop/session/1_2/open",
		"/org/freedesktop/portal/desktop/session/1_2/open",
		"other"));
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/global-shortcut/default", test_default_trigger_is_shift_f7);
	g_test_add_func ("/linux/global-shortcut/activation", test_activation_requires_matching_session_and_action);
	return g_test_run ();
}
