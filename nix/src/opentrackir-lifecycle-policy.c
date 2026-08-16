#include "opentrackir-lifecycle-policy.h"

OpentrackirLifecyclePolicy
opentrackir_lifecycle_policy (gboolean background_enabled,
                              gboolean camera_enabled,
                              gboolean mouse_movement_enabled,
                              gboolean window_visible)
{
	return (OpentrackirLifecyclePolicy) {
		.close_behavior = background_enabled
			? OPENTRACKIR_CLOSE_BEHAVIOR_HIDE
			: OPENTRACKIR_CLOSE_BEHAVIOR_QUIT,
		.hold_application = background_enabled,
		.show_status_notifier = background_enabled,
		.use_low_power = camera_enabled && !window_visible && !mouse_movement_enabled,
	};
}
