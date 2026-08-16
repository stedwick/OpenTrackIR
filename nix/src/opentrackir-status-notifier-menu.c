#include "opentrackir-status-notifier-menu.h"

OpentrackirStatusNotifierMenuAction
opentrackir_status_notifier_menu_action_for_event (gint        item_id,
                                                    const char *event_id)
{
	if (g_strcmp0 (event_id, "clicked") != 0)
		return OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_NONE;
	if (item_id == OPENTRACKIR_STATUS_NOTIFIER_MENU_SHOW_ID)
		return OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_SHOW;
	if (item_id == OPENTRACKIR_STATUS_NOTIFIER_MENU_QUIT_ID)
		return OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_QUIT;
	return OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_NONE;
}

GVariant *
opentrackir_status_notifier_menu_item_properties (gint        item_id,
                                                   const char *show_label,
                                                   const char *quit_label)
{
	GVariantBuilder properties;
	const char *label;

	g_return_val_if_fail (show_label != NULL, NULL);
	g_return_val_if_fail (quit_label != NULL, NULL);

	if (item_id == OPENTRACKIR_STATUS_NOTIFIER_MENU_ROOT_ID)
		label = NULL;
	else if (item_id == OPENTRACKIR_STATUS_NOTIFIER_MENU_SHOW_ID)
		label = show_label;
	else if (item_id == OPENTRACKIR_STATUS_NOTIFIER_MENU_QUIT_ID)
		label = quit_label;
	else
		return NULL;

	g_variant_builder_init (&properties, G_VARIANT_TYPE ("a{sv}"));
	if (label != NULL)
		g_variant_builder_add (&properties, "{sv}", "label", g_variant_new_string (label));
	return g_variant_builder_end (&properties);
}

static GVariant *
menu_item_layout (gint        item_id,
                  const char *show_label,
                  const char *quit_label)
{
	GVariantBuilder children;
	GVariant *properties;

	properties = opentrackir_status_notifier_menu_item_properties (
		item_id,
		show_label,
		quit_label
	);
	if (properties == NULL)
		return NULL;

	g_variant_builder_init (&children, G_VARIANT_TYPE ("av"));
	return g_variant_new ("(i@a{sv}@av)",
	                      item_id,
	                      properties,
	                      g_variant_builder_end (&children));
}

GVariant *
opentrackir_status_notifier_menu_layout (gint        parent_id,
                                         gint        recursion_depth,
                                         const char *show_label,
                                         const char *quit_label)
{
	GVariantBuilder children;
	GVariant *properties;

	g_return_val_if_fail (show_label != NULL, NULL);
	g_return_val_if_fail (quit_label != NULL, NULL);

	if (parent_id != OPENTRACKIR_STATUS_NOTIFIER_MENU_ROOT_ID)
		return menu_item_layout (parent_id, show_label, quit_label);

	properties = opentrackir_status_notifier_menu_item_properties (
		OPENTRACKIR_STATUS_NOTIFIER_MENU_ROOT_ID,
		show_label,
		quit_label
	);
	g_variant_builder_init (&children, G_VARIANT_TYPE ("av"));
	if (recursion_depth != 0)
	{
		g_variant_builder_add (&children,
		                       "v",
		                       menu_item_layout (OPENTRACKIR_STATUS_NOTIFIER_MENU_SHOW_ID,
		                                         show_label,
		                                         quit_label));
		g_variant_builder_add (&children,
		                       "v",
		                       menu_item_layout (OPENTRACKIR_STATUS_NOTIFIER_MENU_QUIT_ID,
		                                         show_label,
		                                         quit_label));
	}
	return g_variant_new ("(i@a{sv}@av)",
	                      OPENTRACKIR_STATUS_NOTIFIER_MENU_ROOT_ID,
	                      properties,
	                      g_variant_builder_end (&children));
}
