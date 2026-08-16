#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
	OPENTRACKIR_CLOSE_BEHAVIOR_QUIT,
	OPENTRACKIR_CLOSE_BEHAVIOR_HIDE,
} OpentrackirCloseBehavior;

typedef struct
{
	OpentrackirCloseBehavior close_behavior;
	gboolean hold_application;
	gboolean show_status_notifier;
	gboolean use_low_power;
} OpentrackirLifecyclePolicy;

OpentrackirLifecyclePolicy opentrackir_lifecycle_policy (gboolean background_enabled,
                                                         gboolean camera_enabled,
                                                         gboolean mouse_movement_enabled,
                                                         gboolean window_visible);

G_END_DECLS
