#include "opentrackir-session-controller.h"

#include <errno.h>

#include "opentrackir-mouse-worker.h"

#define SNAPSHOT_POLL_INTERVAL_MILLISECONDS 100

struct _OpentrackirSessionController
{
	GObject parent_instance;

	otir_trackir_session *session;
	OpentrackirSessionState state;
	OpentrackirMouseWorker *mouse_worker;
	OpentrackirUinputState mouse_state;
	guint poll_source_id;
	guint mouse_poll_source_id;
	gboolean mouse_enabled;
};

G_DEFINE_FINAL_TYPE (OpentrackirSessionController, opentrackir_session_controller, G_TYPE_OBJECT)

enum
{
	STATE_CHANGED,
	MOUSE_STATE_CHANGED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
opentrackir_session_controller_refresh (OpentrackirSessionController *self)
{
	otir_trackir_session_snapshot snapshot = { 0 };
	OpentrackirSessionState next_state;

	otir_trackir_session_copy_snapshot (self->session, &snapshot);
	opentrackir_session_state_from_snapshot (&snapshot, &next_state);

	if (opentrackir_session_state_equal (&self->state, &next_state))
		return;

	self->state = next_state;
	g_signal_emit (self, signals[STATE_CHANGED], 0);
}

static void
opentrackir_session_controller_refresh_mouse (OpentrackirSessionController *self)
{
	OpentrackirUinputState next_state;

	if (self->mouse_worker == NULL)
		return;

	next_state = opentrackir_mouse_worker_get_state (self->mouse_worker);
	if (opentrackir_uinput_state_equal (&self->mouse_state, &next_state))
		return;

	self->mouse_state = next_state;
	g_signal_emit (self, signals[MOUSE_STATE_CHANGED], 0);
}

static gboolean
opentrackir_session_controller_poll (gpointer user_data)
{
	OpentrackirSessionController *self = user_data;

	opentrackir_session_controller_refresh (self);

	if (self->state.phase == OPENTRACKIR_SESSION_PHASE_IDLE ||
	    self->state.phase == OPENTRACKIR_SESSION_PHASE_UNAVAILABLE ||
	    self->state.phase == OPENTRACKIR_SESSION_PHASE_FAILED)
	{
		self->poll_source_id = 0;
		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

static gboolean
opentrackir_session_controller_poll_mouse (gpointer user_data)
{
	OpentrackirSessionController *self = user_data;

	opentrackir_session_controller_refresh_mouse (self);
	if (!self->mouse_enabled)
	{
		self->mouse_poll_source_id = 0;
		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

static void
opentrackir_session_controller_dispose (GObject *object)
{
	OpentrackirSessionController *self = OPENTRACKIR_SESSION_CONTROLLER (object);

	if (self->poll_source_id != 0)
	{
		g_source_remove (self->poll_source_id);
		self->poll_source_id = 0;
	}
	if (self->mouse_poll_source_id != 0)
	{
		g_source_remove (self->mouse_poll_source_id);
		self->mouse_poll_source_id = 0;
	}

	if (self->mouse_worker != NULL)
	{
		opentrackir_mouse_worker_free (self->mouse_worker);
		self->mouse_worker = NULL;
	}

	if (self->session != NULL)
	{
		otir_trackir_session_destroy (self->session);
		self->session = NULL;
	}

	G_OBJECT_CLASS (opentrackir_session_controller_parent_class)->dispose (object);
}

static void
opentrackir_session_controller_class_init (OpentrackirSessionControllerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = opentrackir_session_controller_dispose;

	signals[STATE_CHANGED] =
		g_signal_new ("state-changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL,
		              NULL,
		              NULL,
		              G_TYPE_NONE,
		              0);
	signals[MOUSE_STATE_CHANGED] =
		g_signal_new ("mouse-state-changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL,
		              NULL,
		              NULL,
		              G_TYPE_NONE,
		              0);
}

static void
opentrackir_session_controller_init (OpentrackirSessionController *self)
{
	self->session = otir_trackir_session_create ();
	self->state.phase = OPENTRACKIR_SESSION_PHASE_IDLE;
	self->state.status = OTIR_STATUS_OK;
	self->mouse_state.phase = OPENTRACKIR_UINPUT_PHASE_DISABLED;

	if (self->session == NULL)
	{
		self->state.phase = OPENTRACKIR_SESSION_PHASE_FAILED;
		self->state.status = OTIR_STATUS_IO;
		self->state.has_error_message = TRUE;
		g_strlcpy (self->state.error_message,
		           "Could not allocate the TrackIR session.",
		           sizeof self->state.error_message);
		return;
	}

	/* The window enables preview publication only while it is visible. */
	otir_trackir_session_set_video_enabled (self->session, false);
	self->mouse_worker = opentrackir_mouse_worker_new (self->session);
	if (self->mouse_worker == NULL)
	{
		self->mouse_state.phase = OPENTRACKIR_UINPUT_PHASE_FAILED;
		self->mouse_state.error_number = ENOMEM;
	}
}

const OpentrackirUinputState *
opentrackir_session_controller_get_mouse_state (OpentrackirSessionController *self)
{
	g_return_val_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self), NULL);

	return &self->mouse_state;
}

OpentrackirSessionController *
opentrackir_session_controller_new (void)
{
	return g_object_new (OPENTRACKIR_TYPE_SESSION_CONTROLLER, NULL);
}

const OpentrackirSessionState *
opentrackir_session_controller_get_state (OpentrackirSessionController *self)
{
	g_return_val_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self), NULL);

	return &self->state;
}

gboolean
opentrackir_session_controller_can_start (OpentrackirSessionController *self)
{
	g_return_val_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self), FALSE);

	return self->session != NULL && opentrackir_session_state_can_start (&self->state);
}

gboolean
opentrackir_session_controller_can_stop (OpentrackirSessionController *self)
{
	g_return_val_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self), FALSE);

	return self->session != NULL && opentrackir_session_state_can_stop (&self->state);
}

void
opentrackir_session_controller_start (OpentrackirSessionController *self)
{
	otir_status status;

	g_return_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self));

	if (!opentrackir_session_controller_can_start (self))
		return;

	status = otir_trackir_session_start (self->session);
	opentrackir_session_controller_refresh (self);

	if (status == OTIR_STATUS_OK &&
	    self->poll_source_id == 0 &&
	    opentrackir_session_controller_can_stop (self))
	{
		self->poll_source_id =
			g_timeout_add (SNAPSHOT_POLL_INTERVAL_MILLISECONDS,
			               opentrackir_session_controller_poll,
			               self);
	}
}

void
opentrackir_session_controller_stop (OpentrackirSessionController *self)
{
	g_return_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self));

	if (self->session == NULL)
		return;

	if (self->poll_source_id != 0)
	{
		g_source_remove (self->poll_source_id);
		self->poll_source_id = 0;
	}

	otir_trackir_session_stop (self->session, true);
	opentrackir_session_controller_refresh (self);
}

void
opentrackir_session_controller_set_video_enabled (OpentrackirSessionController *self,
                                                  gboolean                      enabled)
{
	g_return_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self));

	if (self->session != NULL)
		otir_trackir_session_set_video_enabled (self->session, enabled);
}

void
opentrackir_session_controller_set_tracking_frames_per_second (OpentrackirSessionController *self,
                                                               double                        frames_per_second)
{
	g_return_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self));

	if (self->session != NULL)
		otir_trackir_session_set_maximum_tracking_frames_per_second (self->session,
		                                                             frames_per_second);
}

void
opentrackir_session_controller_set_minimum_blob_area_points (OpentrackirSessionController *self,
                                                             int                           minimum_blob_area_points)
{
	g_return_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self));

	if (self->session != NULL)
		otir_trackir_session_set_minimum_blob_area_points (self->session,
		                                                      minimum_blob_area_points);
}

void
opentrackir_session_controller_set_centroid_mode (OpentrackirSessionController *self,
                                                  otir_tir5v3_centroid_mode     mode)
{
	g_return_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self));

	if (self->session != NULL)
		otir_trackir_session_set_centroid_mode (self->session, mode);
}

void
opentrackir_session_controller_set_mouse_config (OpentrackirSessionController      *self,
                                                 gboolean                           enabled,
                                                 otir_trackir_mouse_tracker_config  config)
{
	g_return_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self));

	if (self->mouse_worker == NULL)
		return;

	self->mouse_enabled = enabled;
	opentrackir_mouse_worker_set_config (self->mouse_worker, enabled, config);
	opentrackir_session_controller_refresh_mouse (self);

	if (enabled && self->mouse_poll_source_id == 0)
	{
		self->mouse_poll_source_id =
			g_timeout_add (SNAPSHOT_POLL_INTERVAL_MILLISECONDS,
			               opentrackir_session_controller_poll_mouse,
			               self);
	}
	else if (!enabled && self->mouse_poll_source_id != 0)
	{
		g_source_remove (self->mouse_poll_source_id);
		self->mouse_poll_source_id = 0;
	}
}

gboolean
opentrackir_session_controller_copy_preview_frame (OpentrackirSessionController *self,
                                                   guint8                        *frame,
                                                   gsize                          capacity,
                                                   guint64                       *generation)
{
	g_return_val_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (self), FALSE);

	return self->session != NULL &&
	       otir_trackir_session_copy_preview_frame (self->session,
	                                                  frame,
	                                                  capacity,
	                                                  generation);
}
