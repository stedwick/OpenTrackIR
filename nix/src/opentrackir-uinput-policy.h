#pragma once

#include <glib.h>
#include <opentrackir/tir5_mouse.h>

G_BEGIN_DECLS

typedef enum
{
	OPENTRACKIR_UINPUT_PHASE_DISABLED,
	OPENTRACKIR_UINPUT_PHASE_STARTING,
	OPENTRACKIR_UINPUT_PHASE_READY,
	OPENTRACKIR_UINPUT_PHASE_UNAVAILABLE,
	OPENTRACKIR_UINPUT_PHASE_PERMISSION_DENIED,
	OPENTRACKIR_UINPUT_PHASE_UNSUPPORTED,
	OPENTRACKIR_UINPUT_PHASE_WRITE_FAILED,
	OPENTRACKIR_UINPUT_PHASE_FAILED,
} OpentrackirUinputPhase;

typedef struct
{
	OpentrackirUinputPhase phase;
	int error_number;
	guint64 event_frame_count;
} OpentrackirUinputState;

typedef struct
{
	unsigned int type;
	unsigned int code;
	int value;
} OpentrackirUinputEvent;

typedef struct
{
	int delta_x;
	int delta_y;
	double remaining_x;
	double remaining_y;
} OpentrackirRelativeDispatch;

typedef struct
{
	int delta_x;
	int delta_y;
} OpentrackirRelativeDelta;

OpentrackirUinputPhase       opentrackir_uinput_phase_for_error     (int                              error_number);
const char                 *opentrackir_uinput_phase_message       (OpentrackirUinputPhase            phase);
gboolean                    opentrackir_uinput_state_equal         (const OpentrackirUinputState      *left,
                                                                    const OpentrackirUinputState      *right);
OpentrackirRelativeDispatch opentrackir_relative_dispatch_consume  (double                           pending_x,
                                                                    double                           pending_y);
otir_trackir_mouse_tracker_config
                            opentrackir_build_mouse_tracker_config  (double                           control_speed,
                                                                    double                           smoothing,
                                                                    double                           deadzone,
                                                                    gboolean                         avoid_mouse_jumps,
                                                                    double                           jump_threshold_pixels,
                                                                    gboolean                         horizontal_flip,
                                                                    gboolean                         vertical_flip,
                                                                    double                           rotation_degrees);
gboolean                    opentrackir_keep_awake_should_run       (gboolean                         camera_enabled,
                                                                    gboolean                         mouse_movement_enabled,
                                                                    guint                            keep_awake_seconds);
OpentrackirRelativeDelta    opentrackir_keep_awake_delta           (guint                            direction_index);
gsize                       opentrackir_uinput_build_event_frame   (int                              delta_x,
                                                                    int                              delta_y,
                                                                    OpentrackirUinputEvent           *events,
                                                                    gsize                            capacity);

G_END_DECLS
