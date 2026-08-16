#include <glib.h>

#include "opentrackir-uinput-pointer.h"

static void
test_disabled_pointer_does_not_post (void)
{
	OpentrackirUinputPointer *pointer;
	OpentrackirUinputState state;

	pointer = opentrackir_uinput_pointer_new ();
	state = opentrackir_uinput_pointer_get_state (pointer);
	g_assert_cmpint (state.phase, ==, OPENTRACKIR_UINPUT_PHASE_DISABLED);

	state = opentrackir_uinput_pointer_post (pointer, 12, -7);
	g_assert_cmpint (state.phase, ==, OPENTRACKIR_UINPUT_PHASE_DISABLED);
	g_assert_cmpuint (state.event_frame_count, ==, 0);

	opentrackir_uinput_pointer_free (pointer);
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/uinput-pointer/disabled", test_disabled_pointer_does_not_post);

	return g_test_run ();
}
