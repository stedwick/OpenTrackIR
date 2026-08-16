#include "opentrackir-session-state.h"

static OpentrackirSessionPhase
normalize_phase (otir_trackir_session_phase phase)
{
	switch (phase)
	{
	case OTIR_TRACKIR_SESSION_PHASE_IDLE:
		return OPENTRACKIR_SESSION_PHASE_IDLE;
	case OTIR_TRACKIR_SESSION_PHASE_STARTING:
		return OPENTRACKIR_SESSION_PHASE_STARTING;
	case OTIR_TRACKIR_SESSION_PHASE_STREAMING:
		return OPENTRACKIR_SESSION_PHASE_STREAMING;
	case OTIR_TRACKIR_SESSION_PHASE_UNAVAILABLE:
		return OPENTRACKIR_SESSION_PHASE_UNAVAILABLE;
	case OTIR_TRACKIR_SESSION_PHASE_FAILED:
	default:
		return OPENTRACKIR_SESSION_PHASE_FAILED;
	}
}

void
opentrackir_session_state_from_snapshot (const otir_trackir_session_snapshot *snapshot,
                                         OpentrackirSessionState             *state)
{
	g_return_if_fail (snapshot != NULL);
	g_return_if_fail (state != NULL);

	*state = (OpentrackirSessionState) {
		.phase = normalize_phase (snapshot->phase),
		.status = snapshot->status,
		.frame_index = snapshot->frame_index,
		.has_frame_rate = snapshot->has_frame_rate,
		.frame_rate = snapshot->frame_rate,
		.has_centroid = snapshot->has_centroid,
		.centroid_x = snapshot->centroid_x,
		.centroid_y = snapshot->centroid_y,
		.has_packet_type = snapshot->has_packet_type,
		.packet_type = snapshot->packet_type,
		.has_error_message = snapshot->has_error_message,
	};

	if (snapshot->has_error_message)
		g_strlcpy (state->error_message,
		           snapshot->error_message,
		           sizeof state->error_message);
}

gboolean
opentrackir_session_state_equal (const OpentrackirSessionState *left,
                                 const OpentrackirSessionState *right)
{
	g_return_val_if_fail (left != NULL, FALSE);
	g_return_val_if_fail (right != NULL, FALSE);

	return left->phase == right->phase &&
	       left->status == right->status &&
	       left->frame_index == right->frame_index &&
	       left->has_frame_rate == right->has_frame_rate &&
	       (!left->has_frame_rate || left->frame_rate == right->frame_rate) &&
	       left->has_centroid == right->has_centroid &&
	       (!left->has_centroid ||
	        (left->centroid_x == right->centroid_x &&
	         left->centroid_y == right->centroid_y)) &&
	       left->has_packet_type == right->has_packet_type &&
	       (!left->has_packet_type || left->packet_type == right->packet_type) &&
	       left->has_error_message == right->has_error_message &&
	       (!left->has_error_message ||
	        g_strcmp0 (left->error_message, right->error_message) == 0);
}

gboolean
opentrackir_session_state_can_start (const OpentrackirSessionState *state)
{
	g_return_val_if_fail (state != NULL, FALSE);

	return state->phase == OPENTRACKIR_SESSION_PHASE_IDLE ||
	       state->phase == OPENTRACKIR_SESSION_PHASE_UNAVAILABLE ||
	       state->phase == OPENTRACKIR_SESSION_PHASE_FAILED;
}

gboolean
opentrackir_session_state_can_stop (const OpentrackirSessionState *state)
{
	g_return_val_if_fail (state != NULL, FALSE);

	return state->phase == OPENTRACKIR_SESSION_PHASE_STARTING ||
	       state->phase == OPENTRACKIR_SESSION_PHASE_STREAMING;
}
