#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define OPENTRACKIR_STATUS_NOTIFIER_MENU_ROOT_ID 0
#define OPENTRACKIR_STATUS_NOTIFIER_MENU_SHOW_ID 1
#define OPENTRACKIR_STATUS_NOTIFIER_MENU_QUIT_ID 2

typedef enum
{
	OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_NONE,
	OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_SHOW,
	OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_QUIT,
} OpentrackirStatusNotifierMenuAction;

OpentrackirStatusNotifierMenuAction
         opentrackir_status_notifier_menu_action_for_event (gint        item_id,
                                                             const char *event_id);
GVariant *opentrackir_status_notifier_menu_item_properties  (gint        item_id,
                                                             const char *show_label,
                                                             const char *quit_label);
GVariant *opentrackir_status_notifier_menu_layout           (gint        parent_id,
                                                             gint        recursion_depth,
                                                             const char *show_label,
                                                             const char *quit_label);

G_END_DECLS
