#include <errno.h>
#include <glib.h>

#include "opentrackir-xkeys-policy.h"

static void
test_supported_devices_require_consumer_interface (void)
{
	g_assert_true (opentrackir_xkeys_device_is_supported (0x05f3, 0x042c, 0));
	g_assert_true (opentrackir_xkeys_device_is_supported (0x05f3, 0x0438, 0));
	g_assert_false (opentrackir_xkeys_device_is_supported (0x05f3, 0x042c, 1));
	g_assert_false (opentrackir_xkeys_device_is_supported (0x05f3, 0x042c, 2));
	g_assert_false (opentrackir_xkeys_device_is_supported (0x131d, 0x0159, 0));
}

static void
test_middle_pedal_reports_accept_observed_layouts (void)
{
	const guint8 index_one[] = { 0x00, 0x04 };
	const guint8 index_two[] = { 0x00, 0x00, 0x04 };
	const guint8 index_three[] = { 0x00, 0x00, 0x00, 0x04 };
	const guint8 released[] = { 0x00, 0x00, 0x00, 0x00 };

	g_assert_true (opentrackir_xkeys_report_is_middle_pressed (index_one, sizeof index_one));
	g_assert_true (opentrackir_xkeys_report_is_middle_pressed (index_two, sizeof index_two));
	g_assert_true (opentrackir_xkeys_report_is_middle_pressed (index_three, sizeof index_three));
	g_assert_false (opentrackir_xkeys_report_is_middle_pressed (released, sizeof released));
	g_assert_false (opentrackir_xkeys_report_is_middle_pressed (NULL, 0));
}

static void
test_fast_speed_requires_enabled_pressed_pedal (void)
{
	g_assert_cmpfloat (opentrackir_xkeys_effective_speed (2.0, FALSE, TRUE), ==, 2.0);
	g_assert_cmpfloat (opentrackir_xkeys_effective_speed (2.0, TRUE, FALSE), ==, 2.0);
	g_assert_cmpfloat (opentrackir_xkeys_effective_speed (2.0, TRUE, TRUE), ==, 5.0);
}

static void
test_state_distinguishes_presence_permissions_and_press (void)
{
	OpentrackirXKeysState state;

	state = opentrackir_xkeys_derive_state (FALSE, 1, 1, TRUE, 0);
	g_assert_cmpint (state.phase, ==, OPENTRACKIR_XKEYS_PHASE_DISABLED);
	state = opentrackir_xkeys_derive_state (TRUE, 0, 0, FALSE, 0);
	g_assert_cmpint (state.phase, ==, OPENTRACKIR_XKEYS_PHASE_NOT_DETECTED);
	state = opentrackir_xkeys_derive_state (TRUE, 1, 0, FALSE, EACCES);
	g_assert_cmpint (state.phase, ==, OPENTRACKIR_XKEYS_PHASE_PERMISSION_DENIED);
	state = opentrackir_xkeys_derive_state (TRUE, 2, 2, FALSE, 0);
	g_assert_cmpint (state.phase, ==, OPENTRACKIR_XKEYS_PHASE_READY);
	state = opentrackir_xkeys_derive_state (TRUE, 2, 2, TRUE, 0);
	g_assert_cmpint (state.phase, ==, OPENTRACKIR_XKEYS_PHASE_PRESSED);
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/xkeys/devices", test_supported_devices_require_consumer_interface);
	g_test_add_func ("/linux/xkeys/reports", test_middle_pedal_reports_accept_observed_layouts);
	g_test_add_func ("/linux/xkeys/speed", test_fast_speed_requires_enabled_pressed_pedal);
	g_test_add_func ("/linux/xkeys/state", test_state_distinguishes_presence_permissions_and_press);
	return g_test_run ();
}
