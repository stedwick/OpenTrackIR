#include "opentrackir-mouse-worker.h"

#include "opentrackir-uinput-pointer.h"

#define MOUSE_POLL_INTERVAL_MICROSECONDS 4000

struct _OpentrackirMouseWorker
{
	GMutex mutex;
	GCond condition;
	GThread *thread;
	gboolean stop_requested;
	gboolean enabled;
	guint64 config_generation;
	otir_trackir_mouse_tracker_config config;
	OpentrackirUinputState state;
	otir_trackir_session *session;
};

static void
set_state (OpentrackirMouseWorker *self,
           OpentrackirUinputState  state)
{
	g_mutex_lock (&self->mutex);
	self->state = state;
	g_mutex_unlock (&self->mutex);
}

static gboolean
wait_while_configuration_is_unchanged (OpentrackirMouseWorker *self,
                                       guint64                  config_generation)
{
	gboolean should_continue;

	g_mutex_lock (&self->mutex);
	while (!self->stop_requested &&
	       self->enabled &&
	       self->config_generation == config_generation)
	{
		g_cond_wait (&self->condition, &self->mutex);
	}
	should_continue = !self->stop_requested;
	g_mutex_unlock (&self->mutex);
	return should_continue;
}

static gpointer
mouse_worker_main (gpointer user_data)
{
	OpentrackirMouseWorker *self = user_data;
	OpentrackirUinputPointer *pointer = opentrackir_uinput_pointer_new ();
	otir_trackir_mouse_tracker_state tracker = { 0 };
	double pending_x = 0.0;
	double pending_y = 0.0;
	guint64 last_frame_index = 0;
	gboolean was_streaming = FALSE;

	for (;;)
	{
		otir_trackir_mouse_tracker_config config;
		gboolean enabled;
		gboolean stop_requested;
		guint64 config_generation;

		g_mutex_lock (&self->mutex);
		stop_requested = self->stop_requested;
		enabled = self->enabled;
		config = self->config;
		config_generation = self->config_generation;
		g_mutex_unlock (&self->mutex);

		if (stop_requested)
			break;

		if (!enabled)
		{
			opentrackir_uinput_pointer_close (pointer);
			set_state (self, opentrackir_uinput_pointer_get_state (pointer));
			otir_trackir_mouse_tracker_reset (&tracker);
			pending_x = 0.0;
			pending_y = 0.0;
			was_streaming = FALSE;

			g_mutex_lock (&self->mutex);
			while (!self->stop_requested && !self->enabled)
				g_cond_wait (&self->condition, &self->mutex);
			g_mutex_unlock (&self->mutex);
			continue;
		}

		if (opentrackir_uinput_pointer_get_state (pointer).phase != OPENTRACKIR_UINPUT_PHASE_READY)
		{
			OpentrackirUinputState pointer_state;

			set_state (self, (OpentrackirUinputState) {
				.phase = OPENTRACKIR_UINPUT_PHASE_STARTING,
			});
			pointer_state = opentrackir_uinput_pointer_open (pointer);
			set_state (self, pointer_state);
			if (pointer_state.phase != OPENTRACKIR_UINPUT_PHASE_READY)
			{
				if (!wait_while_configuration_is_unchanged (self, config_generation))
					break;
				continue;
			}
		}

		{
			otir_trackir_session_snapshot snapshot = { 0 };

			otir_trackir_session_copy_snapshot (self->session, &snapshot);
			if (snapshot.phase != OTIR_TRACKIR_SESSION_PHASE_STREAMING)
			{
				if (was_streaming)
				{
					otir_trackir_mouse_tracker_reset (&tracker);
					pending_x = 0.0;
					pending_y = 0.0;
					was_streaming = FALSE;
				}
			}
			else
			{
				if (!was_streaming)
				{
					otir_trackir_mouse_tracker_reset (&tracker);
					last_frame_index = G_MAXUINT64;
					was_streaming = TRUE;
				}

				if (snapshot.frame_index != last_frame_index)
				{
					otir_trackir_mouse_step step;

					last_frame_index = snapshot.frame_index;
					config.is_movement_enabled = true;
					step = otir_trackir_mouse_tracker_update (
						&tracker,
						snapshot.has_centroid,
						(otir_trackir_mouse_point) {
							.x = snapshot.centroid_x,
							.y = snapshot.centroid_y,
						},
						config
					);
					if (step.has_cursor_delta)
					{
						OpentrackirRelativeDispatch dispatch;
						OpentrackirUinputState pointer_state;

						pending_x += step.cursor_delta.x;
						pending_y += step.cursor_delta.y;
						dispatch = opentrackir_relative_dispatch_consume (pending_x, pending_y);
						pending_x = dispatch.remaining_x;
						pending_y = dispatch.remaining_y;
						pointer_state = opentrackir_uinput_pointer_post (pointer,
						                                                 dispatch.delta_x,
						                                                 dispatch.delta_y);
						set_state (self, pointer_state);
						if (pointer_state.phase != OPENTRACKIR_UINPUT_PHASE_READY)
						{
							if (!wait_while_configuration_is_unchanged (self, config_generation))
								break;
						}
					}
				}
			}
		}

		g_mutex_lock (&self->mutex);
		if (!self->stop_requested)
		{
			g_cond_wait_until (&self->condition,
			                   &self->mutex,
			                   g_get_monotonic_time () + MOUSE_POLL_INTERVAL_MICROSECONDS);
		}
		g_mutex_unlock (&self->mutex);
	}

	opentrackir_uinput_pointer_free (pointer);
	return NULL;
}

OpentrackirMouseWorker *
opentrackir_mouse_worker_new (otir_trackir_session *session)
{
	OpentrackirMouseWorker *self;

	g_return_val_if_fail (session != NULL, NULL);

	self = g_new0 (OpentrackirMouseWorker, 1);
	g_mutex_init (&self->mutex);
	g_cond_init (&self->condition);
	self->session = session;
	self->state.phase = OPENTRACKIR_UINPUT_PHASE_DISABLED;
	self->thread = g_thread_new ("opentrackir-mouse", mouse_worker_main, self);
	return self;
}

void
opentrackir_mouse_worker_free (OpentrackirMouseWorker *self)
{
	if (self == NULL)
		return;

	g_mutex_lock (&self->mutex);
	self->stop_requested = TRUE;
	g_cond_signal (&self->condition);
	g_mutex_unlock (&self->mutex);

	g_thread_join (self->thread);
	g_cond_clear (&self->condition);
	g_mutex_clear (&self->mutex);
	g_free (self);
}

void
opentrackir_mouse_worker_set_config (OpentrackirMouseWorker            *self,
                                    gboolean                           enabled,
                                    otir_trackir_mouse_tracker_config  config)
{
	gboolean was_enabled;

	g_return_if_fail (self != NULL);

	g_mutex_lock (&self->mutex);
	was_enabled = self->enabled;
	self->enabled = enabled;
	self->config = config;
	self->config_generation += 1;
	if (!enabled)
	{
		self->state = (OpentrackirUinputState) {
			.phase = OPENTRACKIR_UINPUT_PHASE_DISABLED,
		};
	}
	else if (!was_enabled || self->state.phase != OPENTRACKIR_UINPUT_PHASE_READY)
	{
		self->state = (OpentrackirUinputState) {
			.phase = OPENTRACKIR_UINPUT_PHASE_STARTING,
		};
	}
	g_cond_signal (&self->condition);
	g_mutex_unlock (&self->mutex);
}

OpentrackirUinputState
opentrackir_mouse_worker_get_state (OpentrackirMouseWorker *self)
{
	OpentrackirUinputState state;

	g_return_val_if_fail (self != NULL,
	                      ((OpentrackirUinputState) {
	                       .phase = OPENTRACKIR_UINPUT_PHASE_FAILED,
	                      }));

	g_mutex_lock (&self->mutex);
	state = self->state;
	g_mutex_unlock (&self->mutex);
	return state;
}
