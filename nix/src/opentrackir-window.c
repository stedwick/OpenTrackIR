/* opentrackir-window.c
 *
 * Copyright 2026 Philip Brocoum
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <glib/gi18n.h>

#include "opentrackir-application.h"
#include "opentrackir-display-logic.h"
#include "opentrackir-lifecycle-policy.h"
#include "opentrackir-window.h"

#define PREVIEW_POLL_INTERVAL_MILLISECONDS 34

struct _OpentrackirWindow
{
	AdwApplicationWindow parent_instance;

	GtkPicture *preview_picture;
	GtkLabel *preview_placeholder;
	GtkSwitch *camera_switch;
	GtkSwitch *video_switch;
	GtkSwitch *mouse_switch;
	GtkSwitch *background_switch;
	GtkSwitch *avoid_jumps_switch;
	GtkSwitch *horizontal_flip_switch;
	GtkSwitch *vertical_flip_switch;
	GtkSwitch *timeout_switch;
	GtkSwitch *xkeys_switch;
	GtkSpinButton *tracking_rate_spin;
	GtkSpinButton *minimum_blob_spin;
	GtkSpinButton *mouse_speed_spin;
	GtkSpinButton *smoothing_spin;
	GtkSpinButton *dead_zone_spin;
	GtkSpinButton *jump_threshold_spin;
	GtkSpinButton *rotation_spin;
	GtkSpinButton *keep_awake_spin;
	GtkSpinButton *timeout_spin;
	AdwActionRow *timeout_duration_row;
	AdwActionRow *mouse_shortcut_row;
	AdwActionRow *xkeys_status_row;
	AdwActionRow *phase_row;
	AdwActionRow *mouse_status_row;
	AdwActionRow *background_status_row;
	AdwActionRow *error_row;
	AdwActionRow *frame_index_row;
	AdwActionRow *frame_rate_row;
	AdwActionRow *packet_type_row;
	AdwActionRow *centroid_row;

	OpentrackirSessionController *controller;
	GSettings *settings;
	guint preview_source_id;
	guint timeout_countdown_source_id;
	guint64 last_preview_generation;
};

G_DEFINE_FINAL_TYPE (OpentrackirWindow, opentrackir_window, ADW_TYPE_APPLICATION_WINDOW)

enum
{
	WORK_VISIBILITY_CHANGED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS];

static const char *
phase_label (OpentrackirSessionPhase phase)
{
	switch (phase)
	{
	case OPENTRACKIR_SESSION_PHASE_IDLE:
		return _("Idle");
	case OPENTRACKIR_SESSION_PHASE_STARTING:
		return _("Starting…");
	case OPENTRACKIR_SESSION_PHASE_STREAMING:
		return _("Streaming");
	case OPENTRACKIR_SESSION_PHASE_UNAVAILABLE:
		return _("Unavailable");
	case OPENTRACKIR_SESSION_PHASE_FAILED:
	default:
		return _("Failed");
	}
}

static void update_preview_policy (OpentrackirWindow *self);
static void update_background_status (OpentrackirWindow *self);
static void update_timeout_countdown_policy (OpentrackirWindow *self);

static void
update_mouse_shortcut (OpentrackirWindow *self)
{
	GtkApplication *application = gtk_window_get_application (GTK_WINDOW (self));

	if (OPENTRACKIR_IS_APPLICATION (application))
	{
		adw_action_row_set_subtitle (
			self->mouse_shortcut_row,
			opentrackir_application_mouse_shortcut_description (
				OPENTRACKIR_APPLICATION (application)));
	}
}

static void
update_xkeys_status (OpentrackirWindow *self)
{
	GtkApplication *application = gtk_window_get_application (GTK_WINDOW (self));

	if (OPENTRACKIR_IS_APPLICATION (application))
	{
		adw_action_row_set_subtitle (
			self->xkeys_status_row,
			opentrackir_application_xkeys_description (
				OPENTRACKIR_APPLICATION (application)));
	}
}

gboolean
opentrackir_window_is_visible_for_work (OpentrackirWindow *self)
{
	GdkSurface *surface;
	GdkToplevelState state;

	if (!gtk_widget_get_mapped (GTK_WIDGET (self)))
		return FALSE;

	surface = gtk_native_get_surface (GTK_NATIVE (self));
	if (surface == NULL || !GDK_IS_TOPLEVEL (surface))
		return FALSE;

	state = gdk_toplevel_get_state (GDK_TOPLEVEL (surface));
	return (state & (GDK_TOPLEVEL_STATE_MINIMIZED | GDK_TOPLEVEL_STATE_SUSPENDED)) == 0;
}

static void
clear_preview (OpentrackirWindow *self)
{
	gtk_picture_set_paintable (self->preview_picture, NULL);
	self->last_preview_generation = 0;
}

static void
update_preview_placeholder (OpentrackirWindow *self)
{
	const OpentrackirSessionState *state;
	const char *message;
	gboolean has_picture;

	state = opentrackir_session_controller_get_state (self->controller);
	has_picture = gtk_picture_get_paintable (self->preview_picture) != NULL;

	if (!gtk_switch_get_active (self->camera_switch))
		message = _("TrackIR is off");
	else if (!gtk_switch_get_active (self->video_switch))
		message = _("Video preview is hidden");
	else if (state->phase == OPENTRACKIR_SESSION_PHASE_STARTING)
		message = _("Starting TrackIR…");
	else if (state->has_error_message)
		message = state->error_message;
	else if (state->phase == OPENTRACKIR_SESSION_PHASE_STREAMING)
		message = _("Waiting for a camera frame…");
	else
		message = _("Camera preview unavailable");

	gtk_label_set_text (self->preview_placeholder, message);
	gtk_widget_set_visible (GTK_WIDGET (self->preview_placeholder), !has_picture);
}

static gboolean
preview_tick (gpointer user_data)
{
	OpentrackirWindow *self = user_data;
	guint8 *frame;
	guint64 generation = 0;
	GBytes *bytes;
	GdkTexture *texture;

	frame = g_malloc (OTIR_TRACKIR_SESSION_FRAME_BYTES);
	if (!opentrackir_session_controller_copy_preview_frame (self->controller,
	                                                         frame,
	                                                         OTIR_TRACKIR_SESSION_FRAME_BYTES,
	                                                         &generation))
	{
		g_free (frame);
		return G_SOURCE_CONTINUE;
	}

	if (!opentrackir_preview_should_copy (TRUE,
	                                      generation,
	                                      self->last_preview_generation))
	{
		g_free (frame);
		return G_SOURCE_CONTINUE;
	}

	bytes = g_bytes_new_take (frame, OTIR_TRACKIR_SESSION_FRAME_BYTES);
	texture = gdk_memory_texture_new (OTIR_TIR5V3_FRAME_WIDTH,
	                                  OTIR_TIR5V3_FRAME_HEIGHT,
	                                  GDK_MEMORY_G8,
	                                  bytes,
	                                  OTIR_TIR5V3_FRAME_WIDTH);
	gtk_picture_set_paintable (self->preview_picture, GDK_PAINTABLE (texture));
	gtk_widget_set_visible (GTK_WIDGET (self->preview_placeholder), FALSE);
	self->last_preview_generation = generation;

	g_object_unref (texture);
	g_bytes_unref (bytes);
	return G_SOURCE_CONTINUE;
}

static void
update_preview_policy (OpentrackirWindow *self)
{
	const OpentrackirSessionState *state;
	gboolean should_run;

	state = opentrackir_session_controller_get_state (self->controller);
	should_run = opentrackir_preview_should_run (
		gtk_switch_get_active (self->camera_switch),
		gtk_switch_get_active (self->video_switch),
		opentrackir_window_is_visible_for_work (self),
		state->phase
	);

	opentrackir_session_controller_set_video_enabled (self->controller, should_run);
	if (should_run && self->preview_source_id == 0)
	{
		self->preview_source_id =
			g_timeout_add (PREVIEW_POLL_INTERVAL_MILLISECONDS, preview_tick, self);
	}
	else if (!should_run && self->preview_source_id != 0)
	{
		g_source_remove (self->preview_source_id);
		self->preview_source_id = 0;
		clear_preview (self);
	}

	update_preview_placeholder (self);
}

static void
update_state (OpentrackirWindow *self)
{
	const OpentrackirSessionState *state;
	g_autofree char *frame_index = NULL;
	g_autofree char *frame_rate = NULL;
	g_autofree char *packet_type = NULL;
	g_autofree char *centroid = NULL;

	state = opentrackir_session_controller_get_state (self->controller);
	frame_index = g_strdup_printf ("%" G_GUINT64_FORMAT, state->frame_index);
	frame_rate = opentrackir_format_frame_rate (state->has_frame_rate, state->frame_rate);
	packet_type = opentrackir_format_packet_type (state->has_packet_type, state->packet_type);
	centroid = opentrackir_format_centroid (state->has_centroid,
	                                       state->centroid_x,
	                                       state->centroid_y);

	adw_action_row_set_subtitle (self->phase_row, phase_label (state->phase));
	adw_action_row_set_subtitle (self->error_row,
	                             state->has_error_message ? state->error_message : _("None"));
	adw_action_row_set_subtitle (self->frame_index_row, frame_index);
	adw_action_row_set_subtitle (self->frame_rate_row, frame_rate);
	adw_action_row_set_subtitle (self->packet_type_row, packet_type);
	adw_action_row_set_subtitle (self->centroid_row, centroid);

	update_preview_policy (self);
}

static void
update_mouse_state (OpentrackirWindow *self)
{
	const OpentrackirUinputState *state;
	g_autofree char *ready_message = NULL;
	const char *message;
	gboolean keep_awake_only;

	state = opentrackir_session_controller_get_mouse_state (self->controller);
	keep_awake_only = !g_settings_get_boolean (self->settings, "mouse-enabled") &&
		g_settings_get_boolean (self->settings, "camera-enabled") &&
		g_settings_get_int (self->settings, "keep-awake-seconds") > 0;
	if (state->phase == OPENTRACKIR_UINPUT_PHASE_READY && keep_awake_only)
	{
		ready_message = g_strdup_printf (_("Keep-awake pointer ready · %" G_GUINT64_FORMAT " nudges"),
		                                 state->event_frame_count);
		message = ready_message;
	}
	else if (state->phase == OPENTRACKIR_UINPUT_PHASE_READY && state->event_frame_count > 0)
	{
		ready_message = g_strdup_printf (_("Virtual pointer ready · %" G_GUINT64_FORMAT " movement frames"),
		                                 state->event_frame_count);
		message = ready_message;
	}
	else
	{
		message = opentrackir_uinput_phase_message (state->phase);
	}

	adw_action_row_set_subtitle (self->mouse_status_row, message);
}

static gboolean
timeout_countdown_should_tick (OpentrackirWindow *self)
{
	return opentrackir_window_is_visible_for_work (self) &&
	       g_settings_get_boolean (self->settings, "camera-enabled") &&
	       g_settings_get_boolean (self->settings, "timeout-enabled");
}

static void
update_timeout_countdown_label (OpentrackirWindow *self)
{
	GtkApplication *application;
	g_autofree char *remaining_text = NULL;
	const char *text;
	guint remaining_seconds = 0;

	application = gtk_window_get_application (GTK_WINDOW (self));
	if (OPENTRACKIR_IS_APPLICATION (application))
	{
		remaining_seconds =
			opentrackir_application_timeout_remaining_seconds (
				OPENTRACKIR_APPLICATION (application));
	}

	if (!g_settings_get_boolean (self->settings, "timeout-enabled"))
		text = _("Timeout is disabled.");
	else if (!g_settings_get_boolean (self->settings, "camera-enabled"))
		text = _("Starts when TrackIR is enabled.");
	else
	{
		remaining_text = opentrackir_format_timeout_remaining (remaining_seconds);
		text = remaining_text;
	}
	adw_action_row_set_subtitle (self->timeout_duration_row, text);
}

static gboolean
timeout_countdown_tick (gpointer user_data)
{
	OpentrackirWindow *self = user_data;

	update_timeout_countdown_label (self);
	if (!timeout_countdown_should_tick (self))
	{
		self->timeout_countdown_source_id = 0;
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

static void
update_timeout_countdown_policy (OpentrackirWindow *self)
{
	gboolean should_tick;

	update_timeout_countdown_label (self);
	should_tick = timeout_countdown_should_tick (self);
	if (should_tick && self->timeout_countdown_source_id == 0)
	{
		self->timeout_countdown_source_id =
			g_timeout_add_seconds (1, timeout_countdown_tick, self);
	}
	else if (!should_tick && self->timeout_countdown_source_id != 0)
	{
		g_source_remove (self->timeout_countdown_source_id);
		self->timeout_countdown_source_id = 0;
	}
}

static void
controller_state_changed (OpentrackirSessionController *controller,
                          OpentrackirWindow            *self)
{
	if (opentrackir_window_is_visible_for_work (self))
		update_state (self);
}

static void
controller_mouse_state_changed (OpentrackirSessionController *controller,
                                OpentrackirWindow            *self)
{
	if (opentrackir_window_is_visible_for_work (self))
		update_mouse_state (self);
}

static void
settings_changed (GSettings          *settings,
                  const char         *key,
                  OpentrackirWindow  *self)
{
	if (g_str_equal (key, "background-enabled"))
		update_background_status (self);
	if (g_str_equal (key, "camera-enabled") ||
	    g_str_equal (key, "timeout-enabled") ||
	    g_str_equal (key, "timeout-seconds"))
		update_timeout_countdown_policy (self);
	if (g_str_equal (key, "camera-enabled") ||
	    g_str_equal (key, "mouse-enabled") ||
	    g_str_equal (key, "keep-awake-seconds"))
		update_mouse_state (self);
}

static void
camera_enabled_changed (GtkSwitch          *camera_switch,
                        GParamSpec         *pspec,
                        OpentrackirWindow  *self)
{
	update_preview_policy (self);
}

static void
video_enabled_changed (GtkSwitch         *video_switch,
                       GParamSpec        *pspec,
                       OpentrackirWindow *self)
{
	update_preview_policy (self);
}

static void
window_mapped_changed (OpentrackirWindow *self,
                       GParamSpec        *pspec,
                       gpointer           user_data)
{
	if (opentrackir_window_is_visible_for_work (self))
	{
		update_state (self);
		update_mouse_state (self);
	}
	else
	{
		update_preview_policy (self);
	}
	update_background_status (self);
	update_timeout_countdown_policy (self);
	g_signal_emit (self, signals[WORK_VISIBILITY_CHANGED], 0);
}

static void
toplevel_state_changed (GdkToplevel        *toplevel,
                        GParamSpec         *pspec,
                        OpentrackirWindow  *self)
{
	update_preview_policy (self);
	update_background_status (self);
	update_timeout_countdown_policy (self);
	g_signal_emit (self, signals[WORK_VISIBILITY_CHANGED], 0);
}

static void
window_realized (OpentrackirWindow *self)
{
	GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (self));

	if (surface != NULL && GDK_IS_TOPLEVEL (surface))
	{
		g_signal_connect_object (surface,
		                         "notify::state",
		                         G_CALLBACK (toplevel_state_changed),
		                         self,
		                         0);
	}
	update_preview_policy (self);
	update_background_status (self);
	g_signal_emit (self, signals[WORK_VISIBILITY_CHANGED], 0);
}

static void
update_background_status (OpentrackirWindow *self)
{
	const char *message;
	GtkApplication *application;
	gboolean notifier_available = FALSE;

	application = gtk_window_get_application (GTK_WINDOW (self));
	if (OPENTRACKIR_IS_APPLICATION (application))
	{
		notifier_available =
			opentrackir_application_status_notifier_is_available (
				OPENTRACKIR_APPLICATION (application));
	}

	if (!g_settings_get_boolean (self->settings, "background-enabled"))
		message = _("Off · closing the window quits OpenTrackIR.");
	else if (!notifier_available)
		message = _("On · no tray host detected; launch OpenTrackIR again to restore after hiding.");
	else if (opentrackir_window_is_visible_for_work (self))
		message = _("On · tray available; closing keeps tracking in the background.");
	else
		message = _("Running in the background · use the tray icon to restore.");

	adw_action_row_set_subtitle (self->background_status_row, message);
}

static void
status_notifier_availability_changed (OpentrackirApplication *application,
                                      GParamSpec              *pspec,
                                      OpentrackirWindow       *self)
{
	update_background_status (self);
}

static void
mouse_shortcut_description_changed (OpentrackirApplication *application,
                                    GParamSpec              *pspec,
                                    OpentrackirWindow       *self)
{
	update_mouse_shortcut (self);
}

static void
xkeys_description_changed (OpentrackirApplication *application,
                           GParamSpec              *pspec,
                           OpentrackirWindow       *self)
{
	update_xkeys_status (self);
}

static void
integer_spin_changed (GtkSpinButton     *spin_button,
                      OpentrackirWindow *self)
{
	const char *key = g_object_get_data (G_OBJECT (spin_button), "settings-key");

	g_settings_set_int (self->settings, key, gtk_spin_button_get_value_as_int (spin_button));
}

static void
bind_integer_spin (OpentrackirWindow *self,
                   GtkSpinButton     *spin_button,
                   const char        *key)
{
	gtk_spin_button_set_value (spin_button, g_settings_get_int (self->settings, key));
	g_object_set_data (G_OBJECT (spin_button), "settings-key", (gpointer)key);
	g_signal_connect (spin_button,
	                  "value-changed",
	                  G_CALLBACK (integer_spin_changed),
	                  self);
}

static gboolean
opentrackir_window_close_request (GtkWindow *window)
{
	OpentrackirWindow *self = OPENTRACKIR_WINDOW (window);
	OpentrackirLifecyclePolicy policy;
	GtkApplication *application;

	policy = opentrackir_lifecycle_policy (
		g_settings_get_boolean (self->settings, "background-enabled"),
		g_settings_get_boolean (self->settings, "camera-enabled"),
		g_settings_get_boolean (self->settings, "mouse-enabled"),
		opentrackir_window_is_visible_for_work (self)
	);
	if (policy.close_behavior != OPENTRACKIR_CLOSE_BEHAVIOR_HIDE)
		return FALSE;

	application = gtk_window_get_application (window);
	if (application != NULL)
		g_action_group_activate_action (G_ACTION_GROUP (application), "hide", NULL);
	return TRUE;
}

static void
opentrackir_window_dispose (GObject *object)
{
	OpentrackirWindow *self = OPENTRACKIR_WINDOW (object);

	if (self->preview_source_id != 0)
	{
		g_source_remove (self->preview_source_id);
		self->preview_source_id = 0;
	}
	if (self->timeout_countdown_source_id != 0)
	{
		g_source_remove (self->timeout_countdown_source_id);
		self->timeout_countdown_source_id = 0;
	}
	g_clear_object (&self->settings);
	g_clear_object (&self->controller);

	G_OBJECT_CLASS (opentrackir_window_parent_class)->dispose (object);
}

static void
opentrackir_window_class_init (OpentrackirWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

	object_class->dispose = opentrackir_window_dispose;
	window_class->close_request = opentrackir_window_close_request;

	signals[WORK_VISIBILITY_CHANGED] =
		g_signal_new ("work-visibility-changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL,
		              NULL,
		              NULL,
		              G_TYPE_NONE,
		              0);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/opentrackir/opentrackir-window.ui");
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, preview_picture);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, preview_placeholder);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, camera_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, video_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, mouse_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, background_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, avoid_jumps_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, horizontal_flip_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, vertical_flip_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, timeout_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, xkeys_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, tracking_rate_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, minimum_blob_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, mouse_speed_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, smoothing_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, dead_zone_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, jump_threshold_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, rotation_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, keep_awake_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, timeout_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, timeout_duration_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, mouse_shortcut_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, xkeys_status_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, phase_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, mouse_status_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, background_status_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, error_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, frame_index_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, frame_rate_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, packet_type_row);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, centroid_row);
}

static void
opentrackir_window_init (OpentrackirWindow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}

OpentrackirWindow *
opentrackir_window_new (GtkApplication               *application,
                        OpentrackirSessionController *controller,
                        GSettings                    *settings)
{
	OpentrackirWindow *self;

	g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
	g_return_val_if_fail (OPENTRACKIR_IS_SESSION_CONTROLLER (controller), NULL);
	g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);

	self = g_object_new (OPENTRACKIR_TYPE_WINDOW,
	                     "application", application,
	                     NULL);
	self->controller = g_object_ref (controller);
	self->settings = g_object_ref (settings);

	g_settings_bind (self->settings, "camera-enabled", self->camera_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "video-enabled", self->video_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "mouse-enabled", self->mouse_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "background-enabled", self->background_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "avoid-mouse-jumps", self->avoid_jumps_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "horizontal-flip", self->horizontal_flip_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "vertical-flip", self->vertical_flip_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "timeout-enabled", self->timeout_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "xkeys-fast-mode-enabled", self->xkeys_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "tracking-frames-per-second", self->tracking_rate_spin, "value", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "mouse-speed", self->mouse_speed_spin, "value", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "mouse-dead-zone", self->dead_zone_spin, "value", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "rotation-degrees", self->rotation_spin, "value", G_SETTINGS_BIND_DEFAULT);

	bind_integer_spin (self, self->minimum_blob_spin, "minimum-blob-area-points");
	bind_integer_spin (self, self->smoothing_spin, "mouse-smoothing");
	bind_integer_spin (self, self->jump_threshold_spin, "mouse-jump-threshold-pixels");
	bind_integer_spin (self, self->keep_awake_spin, "keep-awake-seconds");
	bind_integer_spin (self, self->timeout_spin, "timeout-seconds");

	g_signal_connect_object (self->controller,
	                         "state-changed",
	                         G_CALLBACK (controller_state_changed),
	                         self,
	                         0);
	g_signal_connect_object (self->controller,
	                         "mouse-state-changed",
	                         G_CALLBACK (controller_mouse_state_changed),
	                         self,
	                         0);
	g_signal_connect_object (self->settings,
	                         "changed",
	                         G_CALLBACK (settings_changed),
	                         self,
	                         0);
	g_signal_connect_object (application,
	                         "notify::status-notifier-available",
	                         G_CALLBACK (status_notifier_availability_changed),
	                         self,
	                         0);
	g_signal_connect_object (application,
	                         "notify::mouse-shortcut-description",
	                         G_CALLBACK (mouse_shortcut_description_changed),
	                         self,
	                         0);
	g_signal_connect_object (application,
	                         "notify::xkeys-description",
	                         G_CALLBACK (xkeys_description_changed),
	                         self,
	                         0);
	g_signal_connect (self->camera_switch,
	                  "notify::active",
	                  G_CALLBACK (camera_enabled_changed),
	                  self);
	g_signal_connect (self->video_switch,
	                  "notify::active",
	                  G_CALLBACK (video_enabled_changed),
	                  self);
	g_signal_connect (self,
	                  "notify::mapped",
	                  G_CALLBACK (window_mapped_changed),
	                  NULL);
	g_signal_connect_swapped (self,
	                          "realize",
	                          G_CALLBACK (window_realized),
	                          self);
	update_state (self);
	update_mouse_state (self);
	update_mouse_shortcut (self);
	update_xkeys_status (self);
	update_background_status (self);
	update_timeout_countdown_policy (self);
	return self;
}
