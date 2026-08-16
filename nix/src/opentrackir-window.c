/* opentrackir-window.c
 *
 * Copyright 2026 Unknown
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <glib/gi18n.h>

#include "opentrackir-display-logic.h"
#include "opentrackir-session-controller.h"
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
	GtkSwitch *avoid_jumps_switch;
	GtkSwitch *horizontal_flip_switch;
	GtkSwitch *vertical_flip_switch;
	GtkSwitch *timeout_switch;
	GtkSpinButton *tracking_rate_spin;
	GtkSpinButton *minimum_blob_spin;
	GtkSpinButton *mouse_speed_spin;
	GtkSpinButton *smoothing_spin;
	GtkSpinButton *dead_zone_spin;
	GtkSpinButton *jump_threshold_spin;
	GtkSpinButton *rotation_spin;
	GtkSpinButton *keep_awake_spin;
	GtkSpinButton *timeout_spin;
	AdwActionRow *phase_row;
	AdwActionRow *error_row;
	AdwActionRow *frame_index_row;
	AdwActionRow *frame_rate_row;
	AdwActionRow *packet_type_row;
	AdwActionRow *centroid_row;

	OpentrackirSessionController *controller;
	GSettings *settings;
	guint preview_source_id;
	guint timeout_source_id;
	guint64 last_preview_generation;
};

G_DEFINE_FINAL_TYPE (OpentrackirWindow, opentrackir_window, ADW_TYPE_APPLICATION_WINDOW)

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
static void update_timeout_policy (OpentrackirWindow *self);

static gboolean
window_is_visible_for_preview (OpentrackirWindow *self)
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
		window_is_visible_for_preview (self),
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
controller_state_changed (OpentrackirSessionController *controller,
                          OpentrackirWindow            *self)
{
	update_state (self);
}

static gboolean
timeout_elapsed (gpointer user_data)
{
	OpentrackirWindow *self = user_data;

	self->timeout_source_id = 0;
	g_settings_set_boolean (self->settings, "camera-enabled", FALSE);
	return G_SOURCE_REMOVE;
}

static void
update_timeout_policy (OpentrackirWindow *self)
{
	guint timeout_seconds;

	if (self->timeout_source_id != 0)
	{
		g_source_remove (self->timeout_source_id);
		self->timeout_source_id = 0;
	}

	timeout_seconds = (guint)g_settings_get_int (self->settings, "timeout-seconds");
	if (opentrackir_timeout_should_run (gtk_switch_get_active (self->camera_switch),
	                                  gtk_switch_get_active (self->timeout_switch),
	                                  timeout_seconds))
	{
		self->timeout_source_id =
			g_timeout_add_seconds (timeout_seconds,
			                       timeout_elapsed,
			                       self);
	}
}

static void
camera_enabled_changed (GtkSwitch          *camera_switch,
                        GParamSpec         *pspec,
                        OpentrackirWindow  *self)
{
	if (gtk_switch_get_active (camera_switch))
		opentrackir_session_controller_start (self->controller);
	else
		opentrackir_session_controller_stop (self->controller);

	update_timeout_policy (self);
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
	update_preview_policy (self);
}

static void
toplevel_state_changed (GdkToplevel        *toplevel,
                        GParamSpec         *pspec,
                        OpentrackirWindow  *self)
{
	update_preview_policy (self);
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
}

static void
camera_configuration_changed (GtkSpinButton     *spin_button,
                              OpentrackirWindow *self)
{
	opentrackir_session_controller_set_tracking_frames_per_second (
		self->controller,
		gtk_spin_button_get_value (self->tracking_rate_spin)
	);
	opentrackir_session_controller_set_minimum_blob_area_points (
		self->controller,
		gtk_spin_button_get_value_as_int (self->minimum_blob_spin)
	);
}

static void
timeout_setting_changed (GtkSwitch         *timeout_switch,
                         GParamSpec        *pspec,
                         OpentrackirWindow *self)
{
	update_timeout_policy (self);
}

static void
timeout_duration_changed (GtkSpinButton     *spin_button,
                          OpentrackirWindow *self)
{
	update_timeout_policy (self);
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

static void
opentrackir_window_dispose (GObject *object)
{
	OpentrackirWindow *self = OPENTRACKIR_WINDOW (object);

	if (self->preview_source_id != 0)
	{
		g_source_remove (self->preview_source_id);
		self->preview_source_id = 0;
	}
	if (self->timeout_source_id != 0)
	{
		g_source_remove (self->timeout_source_id);
		self->timeout_source_id = 0;
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

	object_class->dispose = opentrackir_window_dispose;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/opentrackir/opentrackir-window.ui");
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, preview_picture);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, preview_placeholder);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, camera_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, video_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, mouse_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, avoid_jumps_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, horizontal_flip_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, vertical_flip_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, timeout_switch);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, tracking_rate_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, minimum_blob_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, mouse_speed_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, smoothing_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, dead_zone_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, jump_threshold_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, rotation_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, keep_awake_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, timeout_spin);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, phase_row);
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

	self->controller = opentrackir_session_controller_new ();
	self->settings = g_settings_new ("org.gnome.opentrackir");

	g_settings_bind (self->settings, "camera-enabled", self->camera_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "video-enabled", self->video_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "mouse-enabled", self->mouse_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "avoid-mouse-jumps", self->avoid_jumps_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "horizontal-flip", self->horizontal_flip_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "vertical-flip", self->vertical_flip_switch, "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (self->settings, "timeout-enabled", self->timeout_switch, "active", G_SETTINGS_BIND_DEFAULT);
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
	g_signal_connect (self->tracking_rate_spin,
	                  "value-changed",
	                  G_CALLBACK (camera_configuration_changed),
	                  self);
	g_signal_connect (self->minimum_blob_spin,
	                  "value-changed",
	                  G_CALLBACK (camera_configuration_changed),
	                  self);
	g_signal_connect (self->timeout_switch,
	                  "notify::active",
	                  G_CALLBACK (timeout_setting_changed),
	                  self);
	g_signal_connect (self->timeout_spin,
	                  "value-changed",
	                  G_CALLBACK (timeout_duration_changed),
	                  self);

	opentrackir_session_controller_set_tracking_frames_per_second (
		self->controller,
		g_settings_get_double (self->settings, "tracking-frames-per-second")
	);
	opentrackir_session_controller_set_minimum_blob_area_points (
		self->controller,
		g_settings_get_int (self->settings, "minimum-blob-area-points")
	);
	opentrackir_session_controller_set_centroid_mode (
		self->controller,
		(otir_tir5v3_centroid_mode)g_settings_get_int (self->settings, "centroid-mode")
	);

	if (gtk_switch_get_active (self->camera_switch))
		opentrackir_session_controller_start (self->controller);

	update_timeout_policy (self);
	update_state (self);
}
