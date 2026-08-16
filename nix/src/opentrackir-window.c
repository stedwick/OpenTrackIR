/* opentrackir-window.c
 *
 * Copyright 2026 Unknown
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <glib/gi18n.h>

#include "opentrackir-session-controller.h"
#include "opentrackir-window.h"

struct _OpentrackirWindow
{
	AdwApplicationWindow parent_instance;

	GtkButton *start_button;
	GtkButton *stop_button;
	AdwActionRow *phase_row;
	AdwActionRow *error_row;
	AdwActionRow *frame_index_row;
	AdwActionRow *frame_rate_row;
	AdwActionRow *packet_type_row;
	AdwActionRow *centroid_row;

	OpentrackirSessionController *controller;
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

static void
opentrackir_window_update_state (OpentrackirWindow *self)
{
	const OpentrackirSessionState *state;
	g_autofree char *frame_index = NULL;
	g_autofree char *frame_rate = NULL;
	g_autofree char *packet_type = NULL;
	g_autofree char *centroid = NULL;

	state = opentrackir_session_controller_get_state (self->controller);

	frame_index = g_strdup_printf ("%" G_GUINT64_FORMAT, state->frame_index);
	frame_rate = state->has_frame_rate
		? g_strdup_printf ("%.1f fps", state->frame_rate)
		: g_strdup ("—");
	packet_type = state->has_packet_type
		? g_strdup_printf ("0x%02X", state->packet_type)
		: g_strdup ("—");
	centroid = state->has_centroid
		? g_strdup_printf ("%.1f, %.1f", state->centroid_x, state->centroid_y)
		: g_strdup ("—");

	adw_action_row_set_subtitle (self->phase_row, phase_label (state->phase));
	adw_action_row_set_subtitle (self->error_row,
	                             state->has_error_message
	                               ? state->error_message
	                               : _("None"));
	adw_action_row_set_subtitle (self->frame_index_row, frame_index);
	adw_action_row_set_subtitle (self->frame_rate_row, frame_rate);
	adw_action_row_set_subtitle (self->packet_type_row, packet_type);
	adw_action_row_set_subtitle (self->centroid_row, centroid);

	gtk_widget_set_sensitive (GTK_WIDGET (self->start_button),
	                          opentrackir_session_controller_can_start (self->controller));
	gtk_widget_set_sensitive (GTK_WIDGET (self->stop_button),
	                          opentrackir_session_controller_can_stop (self->controller));
}

static void
controller_state_changed (OpentrackirSessionController *controller,
                          OpentrackirWindow            *self)
{
	opentrackir_window_update_state (self);
}

static void
start_clicked (OpentrackirWindow *self)
{
	opentrackir_session_controller_start (self->controller);
}

static void
stop_clicked (OpentrackirWindow *self)
{
	opentrackir_session_controller_stop (self->controller);
}

static void
opentrackir_window_dispose (GObject *object)
{
	OpentrackirWindow *self = OPENTRACKIR_WINDOW (object);

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
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, start_button);
	gtk_widget_class_bind_template_child (widget_class, OpentrackirWindow, stop_button);
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
	g_signal_connect_object (self->controller,
	                         "state-changed",
	                         G_CALLBACK (controller_state_changed),
	                         self,
	                         0);
	g_signal_connect_swapped (self->start_button,
	                          "clicked",
	                          G_CALLBACK (start_clicked),
	                          self);
	g_signal_connect_swapped (self->stop_button,
	                          "clicked",
	                          G_CALLBACK (stop_clicked),
	                          self);

	opentrackir_window_update_state (self);
}
