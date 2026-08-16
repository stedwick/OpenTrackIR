#include <glib.h>

#include "opentrackir-status-notifier-menu.h"

static void
test_menu_events_map_to_application_actions (void)
{
	g_assert_cmpint (
		opentrackir_status_notifier_menu_action_for_event (
			OPENTRACKIR_STATUS_NOTIFIER_MENU_SHOW_ID,
			"clicked"),
		==,
		OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_SHOW
	);
	g_assert_cmpint (
		opentrackir_status_notifier_menu_action_for_event (
			OPENTRACKIR_STATUS_NOTIFIER_MENU_QUIT_ID,
			"clicked"),
		==,
		OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_QUIT
	);
	g_assert_cmpint (
		opentrackir_status_notifier_menu_action_for_event (
			OPENTRACKIR_STATUS_NOTIFIER_MENU_SHOW_ID,
			"hovered"),
		==,
		OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_NONE
	);
}

static void
test_menu_layout_contains_show_and_quit (void)
{
	g_autoptr(GVariant) layout = NULL;
	GVariant *children;
	GVariant *child;
	GVariant *wrapped_child;
	GVariant *properties;
	GVariant *label;
	gint item_id;

	layout = g_variant_ref_sink (opentrackir_status_notifier_menu_layout (
		OPENTRACKIR_STATUS_NOTIFIER_MENU_ROOT_ID,
		-1,
		"Show",
		"Quit"
	));
	g_assert_true (g_variant_is_of_type (layout, G_VARIANT_TYPE ("(ia{sv}av)")));
	children = g_variant_get_child_value (layout, 2);
	g_assert_cmpuint (g_variant_n_children (children), ==, 2);

	wrapped_child = g_variant_get_child_value (children, 0);
	child = g_variant_get_variant (wrapped_child);
	g_variant_unref (wrapped_child);
	g_variant_get_child (child, 0, "i", &item_id);
	g_assert_cmpint (item_id, ==, OPENTRACKIR_STATUS_NOTIFIER_MENU_SHOW_ID);
	properties = g_variant_get_child_value (child, 1);
	label = g_variant_lookup_value (properties, "label", G_VARIANT_TYPE_STRING);
	g_assert_cmpstr (g_variant_get_string (label, NULL), ==, "Show");
	g_variant_unref (label);
	g_variant_unref (properties);
	g_variant_unref (child);

	wrapped_child = g_variant_get_child_value (children, 1);
	child = g_variant_get_variant (wrapped_child);
	g_variant_unref (wrapped_child);
	g_variant_get_child (child, 0, "i", &item_id);
	g_assert_cmpint (item_id, ==, OPENTRACKIR_STATUS_NOTIFIER_MENU_QUIT_ID);
	properties = g_variant_get_child_value (child, 1);
	label = g_variant_lookup_value (properties, "label", G_VARIANT_TYPE_STRING);
	g_assert_cmpstr (g_variant_get_string (label, NULL), ==, "Quit");
	g_variant_unref (label);
	g_variant_unref (properties);
	g_variant_unref (child);
	g_variant_unref (children);
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/linux/status-notifier-menu/actions", test_menu_events_map_to_application_actions);
	g_test_add_func ("/linux/status-notifier-menu/layout", test_menu_layout_contains_show_and_quit);

	return g_test_run ();
}
