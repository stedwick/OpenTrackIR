#pragma once

#include <glib.h>
#include <opentrackir/tir5_session.h>

G_BEGIN_DECLS

typedef enum
{
	OPENTRACKIR_SESSION_PHASE_IDLE,
	OPENTRACKIR_SESSION_PHASE_STARTING,
	OPENTRACKIR_SESSION_PHASE_STREAMING,
	OPENTRACKIR_SESSION_PHASE_UNAVAILABLE,
	OPENTRACKIR_SESSION_PHASE_FAILED,
} OpentrackirSessionPhase;

typedef struct
{
	OpentrackirSessionPhase phase;
	otir_status status;
	guint64 frame_index;
	gboolean has_frame_rate;
	double frame_rate;
	gboolean has_centroid;
	double centroid_x;
	double centroid_y;
	gboolean has_packet_type;
	guint8 packet_type;
	gboolean has_error_message;
	char error_message[OTIR_TRACKIR_SESSION_ERROR_MESSAGE_LENGTH];
} OpentrackirSessionState;

void     opentrackir_session_state_from_snapshot (const otir_trackir_session_snapshot *snapshot,
                                                   OpentrackirSessionState             *state);
gboolean opentrackir_session_state_equal         (const OpentrackirSessionState        *left,
                                                   const OpentrackirSessionState        *right);
gboolean opentrackir_session_state_can_start     (const OpentrackirSessionState        *state);
gboolean opentrackir_session_state_can_stop      (const OpentrackirSessionState        *state);

G_END_DECLS
