#include <glib.h>

#include "opentrackir-xkeys-monitor.h"

static void
test_disabled_monitor_starts_and_stops_cleanly (void)
{
	OpentrackirXKeysMonitor *monitor = opentrackir_xkeys_monitor_new ();
	OpentrackirXKeysState state;

	g_assert_nonnull (monitor);
	state = opentrackir_xkeys_monitor_get_state (monitor);
	g_assert_cmpint (state.phase, ==, OPENTRACKIR_XKEYS_PHASE_DISABLED);
	g_object_unref (monitor);
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/xkeys-monitor/disabled-lifecycle",
	                 test_disabled_monitor_starts_and_stops_cleanly);
	return g_test_run ();
}
