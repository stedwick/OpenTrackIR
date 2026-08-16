/* opentrackir-application.c
 *
 * Copyright 2026 Philip Brocoum
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <glib/gi18n.h>

#include "opentrackir-application.h"
#include "opentrackir-display-logic.h"
#include "opentrackir-lifecycle-policy.h"
#include "opentrackir-session-controller.h"
#include "opentrackir-status-notifier.h"
#include "opentrackir-uinput-policy.h"
#include "opentrackir-window.h"

struct _OpentrackirApplication
{
	AdwApplication parent_instance;

	OpentrackirSessionController *controller;
	OpentrackirStatusNotifier *status_notifier;
	GSettings *settings;
	OpentrackirWindow *window;
	guint timeout_source_id;
	gboolean background_hold_active;
	gboolean window_visible;
};

G_DEFINE_FINAL_TYPE (OpentrackirApplication, opentrackir_application, ADW_TYPE_APPLICATION)

enum
{
	PROP_0,
	PROP_STATUS_NOTIFIER_AVAILABLE,
	N_PROPERTIES,
};

static GParamSpec *properties[N_PROPERTIES];

static void opentrackir_application_update_power_policy (OpentrackirApplication *self);
static void opentrackir_application_update_status_notifier (OpentrackirApplication *self);

OpentrackirApplication *
opentrackir_application_new (const char        *application_id,
                             GApplicationFlags  flags)
{
	g_return_val_if_fail (application_id != NULL, NULL);

	return g_object_new (OPENTRACKIR_TYPE_APPLICATION,
	                     "application-id", application_id,
	                     "flags", flags,
	                     "resource-base-path", "/org/gnome/opentrackir",
	                     NULL);
}

static void
opentrackir_application_apply_mouse_configuration (OpentrackirApplication *self)
{
	otir_trackir_mouse_tracker_config config;

	config = opentrackir_build_mouse_tracker_config (
		g_settings_get_double (self->settings, "mouse-speed"),
		(double)g_settings_get_int (self->settings, "mouse-smoothing"),
		g_settings_get_double (self->settings, "mouse-dead-zone"),
		g_settings_get_boolean (self->settings, "avoid-mouse-jumps"),
		(double)g_settings_get_int (self->settings, "mouse-jump-threshold-pixels"),
		g_settings_get_boolean (self->settings, "horizontal-flip"),
		g_settings_get_boolean (self->settings, "vertical-flip"),
		g_settings_get_double (self->settings, "rotation-degrees")
	);
	opentrackir_session_controller_set_mouse_config (
		self->controller,
		g_settings_get_boolean (self->settings, "mouse-enabled"),
		g_settings_get_boolean (self->settings, "camera-enabled"),
		(guint)g_settings_get_int (self->settings, "keep-awake-seconds"),
		config
	);
}

static gboolean
is_mouse_setting (const char *key)
{
	return g_str_has_prefix (key, "mouse-") ||
	       g_str_equal (key, "keep-awake-seconds") ||
	       g_str_equal (key, "avoid-mouse-jumps") ||
	       g_str_equal (key, "horizontal-flip") ||
	       g_str_equal (key, "vertical-flip") ||
	       g_str_equal (key, "rotation-degrees");
}

static gboolean
timeout_elapsed (gpointer user_data)
{
	OpentrackirApplication *self = user_data;

	self->timeout_source_id = 0;
	g_settings_set_boolean (self->settings, "camera-enabled", FALSE);
	return G_SOURCE_REMOVE;
}

static void
opentrackir_application_update_timeout (OpentrackirApplication *self)
{
	guint timeout_seconds;

	if (self->timeout_source_id != 0)
	{
		g_source_remove (self->timeout_source_id);
		self->timeout_source_id = 0;
	}

	timeout_seconds = (guint)g_settings_get_int (self->settings, "timeout-seconds");
	if (opentrackir_timeout_should_run (
		g_settings_get_boolean (self->settings, "camera-enabled"),
		g_settings_get_boolean (self->settings, "timeout-enabled"),
		timeout_seconds))
	{
		self->timeout_source_id =
			g_timeout_add_seconds (timeout_seconds, timeout_elapsed, self);
	}
}

static void
opentrackir_application_update_background_hold (OpentrackirApplication *self)
{
	OpentrackirLifecyclePolicy policy;

	policy = opentrackir_lifecycle_policy (
		g_settings_get_boolean (self->settings, "background-enabled"),
		g_settings_get_boolean (self->settings, "camera-enabled"),
		g_settings_get_boolean (self->settings, "mouse-enabled"),
		self->window_visible
	);

	if (policy.hold_application && !self->background_hold_active)
	{
		g_application_hold (G_APPLICATION (self));
		self->background_hold_active = TRUE;
	}
	else if (!policy.hold_application && self->background_hold_active)
	{
		g_application_release (G_APPLICATION (self));
		self->background_hold_active = FALSE;
	}

	g_simple_action_set_enabled (
		G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (self), "hide")),
		policy.close_behavior == OPENTRACKIR_CLOSE_BEHAVIOR_HIDE
	);
	opentrackir_application_update_status_notifier (self);
}

static void
opentrackir_application_update_status_notifier (OpentrackirApplication *self)
{
	OpentrackirLifecyclePolicy policy;

	if (self->status_notifier == NULL || self->settings == NULL)
		return;
	policy = opentrackir_lifecycle_policy (
		g_settings_get_boolean (self->settings, "background-enabled"),
		g_settings_get_boolean (self->settings, "camera-enabled"),
		g_settings_get_boolean (self->settings, "mouse-enabled"),
		self->window_visible
	);
	opentrackir_status_notifier_update (
		self->status_notifier,
		policy.show_status_notifier,
		self->window_visible,
		g_settings_get_boolean (self->settings, "camera-enabled"),
		g_settings_get_boolean (self->settings, "mouse-enabled")
	);
}

static void
send_background_recovery_notification (OpentrackirApplication *self)
{
	g_autoptr(GNotification) notification = NULL;

	if (self->status_notifier != NULL &&
	    opentrackir_status_notifier_is_available (self->status_notifier))
		return;

	notification = g_notification_new (_("OpenTrackIR is running in the background"));
	g_notification_set_body (notification,
	                         _("Launch OpenTrackIR again to restore the window, or use the Quit action after restoring."));
	g_notification_set_default_action (notification, "app.show");
	g_application_send_notification (G_APPLICATION (self),
	                                 "background-recovery",
	                                 notification);
}

static void
status_notifier_availability_changed (OpentrackirStatusNotifier *notifier,
                                      OpentrackirApplication    *self)
{
	g_object_notify_by_pspec (G_OBJECT (self),
	                          properties[PROP_STATUS_NOTIFIER_AVAILABLE]);
	if (opentrackir_status_notifier_is_available (notifier))
	{
		g_application_withdraw_notification (G_APPLICATION (self),
		                                     "background-recovery");
	}
	else if (g_settings_get_boolean (self->settings, "background-enabled") &&
	         !self->window_visible)
	{
		send_background_recovery_notification (self);
	}
}

static void
opentrackir_application_update_power_policy (OpentrackirApplication *self)
{
	OpentrackirLifecyclePolicy policy;

	if (self->controller == NULL || self->settings == NULL)
		return;

	policy = opentrackir_lifecycle_policy (
		g_settings_get_boolean (self->settings, "background-enabled"),
		g_settings_get_boolean (self->settings, "camera-enabled"),
		g_settings_get_boolean (self->settings, "mouse-enabled"),
		self->window_visible
	);
	opentrackir_session_controller_set_low_power_enabled (self->controller,
	                                                     policy.use_low_power);
}

static void
opentrackir_application_apply_initial_configuration (OpentrackirApplication *self)
{
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
	opentrackir_application_apply_mouse_configuration (self);

	if (g_settings_get_boolean (self->settings, "camera-enabled"))
		opentrackir_session_controller_start (self->controller);

	opentrackir_application_update_timeout (self);
	opentrackir_application_update_background_hold (self);
	opentrackir_application_update_power_policy (self);
}

static void
settings_changed (GSettings                *settings,
                  const char               *key,
                  OpentrackirApplication   *self)
{
	if (g_str_equal (key, "camera-enabled"))
	{
		if (g_settings_get_boolean (settings, key))
			opentrackir_session_controller_start (self->controller);
		else
			opentrackir_session_controller_stop (self->controller);
		opentrackir_application_apply_mouse_configuration (self);
		opentrackir_application_update_timeout (self);
		opentrackir_application_update_power_policy (self);
		opentrackir_application_update_status_notifier (self);
	}
	else if (g_str_equal (key, "tracking-frames-per-second"))
	{
		opentrackir_session_controller_set_tracking_frames_per_second (
			self->controller,
			g_settings_get_double (settings, key)
		);
	}
	else if (g_str_equal (key, "minimum-blob-area-points"))
	{
		opentrackir_session_controller_set_minimum_blob_area_points (
			self->controller,
			g_settings_get_int (settings, key)
		);
	}
	else if (g_str_equal (key, "centroid-mode"))
	{
		opentrackir_session_controller_set_centroid_mode (
			self->controller,
			(otir_tir5v3_centroid_mode)g_settings_get_int (settings, key)
		);
	}
	else if (is_mouse_setting (key))
	{
		opentrackir_application_apply_mouse_configuration (self);
		if (g_str_equal (key, "mouse-enabled"))
		{
			opentrackir_application_update_power_policy (self);
			opentrackir_application_update_status_notifier (self);
		}
	}
	else if (g_str_equal (key, "timeout-enabled") ||
	         g_str_equal (key, "timeout-seconds"))
	{
		opentrackir_application_update_timeout (self);
	}
	else if (g_str_equal (key, "background-enabled"))
	{
		opentrackir_application_update_background_hold (self);
		opentrackir_application_update_power_policy (self);
		if (!g_settings_get_boolean (settings, key) &&
		    self->window != NULL &&
		    !self->window_visible)
		{
			gtk_window_present (GTK_WINDOW (self->window));
		}
	}
}

static void
window_work_visibility_changed (OpentrackirWindow      *window,
                                OpentrackirApplication *self)
{
	self->window_visible = opentrackir_window_is_visible_for_work (window);
	opentrackir_application_update_power_policy (self);
	opentrackir_application_update_status_notifier (self);
}

static void
opentrackir_application_activate (GApplication *app)
{
	OpentrackirApplication *self = OPENTRACKIR_APPLICATION (app);

	if (self->window == NULL)
	{
		self->window = opentrackir_window_new (GTK_APPLICATION (self),
		                                       self->controller,
		                                       self->settings);
		g_object_add_weak_pointer (G_OBJECT (self->window),
		                           (gpointer *)&self->window);
		g_signal_connect_object (self->window,
		                         "work-visibility-changed",
		                         G_CALLBACK (window_work_visibility_changed),
		                         self,
		                         0);
	}

	gtk_window_present (GTK_WINDOW (self->window));
	g_application_withdraw_notification (app, "background-recovery");
}

static void
opentrackir_application_startup (GApplication *application)
{
	OpentrackirApplication *self = OPENTRACKIR_APPLICATION (application);

	G_APPLICATION_CLASS (opentrackir_application_parent_class)->startup (application);

	self->settings = g_settings_new ("org.gnome.opentrackir");
	self->controller = opentrackir_session_controller_new ();
	self->status_notifier = opentrackir_status_notifier_new (application);
	g_signal_connect_object (self->status_notifier,
	                         "availability-changed",
	                         G_CALLBACK (status_notifier_availability_changed),
	                         self,
	                         0);
	g_signal_connect (self->settings,
	                  "changed",
	                  G_CALLBACK (settings_changed),
	                  self);
	opentrackir_application_apply_initial_configuration (self);
}

static void
opentrackir_application_shutdown (GApplication *application)
{
	OpentrackirApplication *self = OPENTRACKIR_APPLICATION (application);

	if (self->timeout_source_id != 0)
	{
		g_source_remove (self->timeout_source_id);
		self->timeout_source_id = 0;
	}
	if (self->background_hold_active)
	{
		g_application_release (application);
		self->background_hold_active = FALSE;
	}

	if (self->status_notifier != NULL)
		g_signal_handlers_disconnect_by_data (self->status_notifier, self);
	g_clear_object (&self->status_notifier);
	g_clear_object (&self->settings);
	g_clear_object (&self->controller);

	G_APPLICATION_CLASS (opentrackir_application_parent_class)->shutdown (application);
}

static void
opentrackir_application_about_action (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
	static const char *developers[] = {"Philip Brocoum", NULL};
	OpentrackirApplication *self = user_data;
	GtkWindow *window = GTK_WINDOW (self->window);

	g_assert (OPENTRACKIR_IS_APPLICATION (self));

	adw_show_about_dialog (GTK_WIDGET (window),
	                       "application-name", "OpenTrackIR",
	                       "application-icon", "org.gnome.opentrackir",
	                       "developer-name", "Philip Brocoum",
	                       "comments", _("An infrared head mouse for NaturalPoint TrackIR cameras"),
	                       "website", "https://github.com/stedwick/OpenTrackIR",
	                       "issue-url", "https://github.com/stedwick/OpenTrackIR/issues",
	                       "translator-credits", _("translator-credits"),
	                       "version", PACKAGE_VERSION,
	                       "developers", developers,
	                       "copyright", "© 2026 Philip Brocoum",
	                       NULL);
}

static void
opentrackir_application_show_action (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
	g_application_activate (G_APPLICATION (user_data));
}

static void
opentrackir_application_hide_action (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
	OpentrackirApplication *self = user_data;

	if (self->window != NULL &&
	    g_settings_get_boolean (self->settings, "background-enabled"))
	{
		gtk_widget_set_visible (GTK_WIDGET (self->window), FALSE);
		self->window_visible = FALSE;
		opentrackir_application_update_power_policy (self);
		opentrackir_application_update_status_notifier (self);
		send_background_recovery_notification (self);
	}
}

static void
opentrackir_application_toggle_mouse_action (GSimpleAction *action,
                                             GVariant      *parameter,
                                             gpointer       user_data)
{
	OpentrackirApplication *self = user_data;
	gboolean enabled = g_settings_get_boolean (self->settings, "mouse-enabled");

	g_settings_set_boolean (self->settings, "mouse-enabled", !enabled);
}

static void
opentrackir_application_quit_action (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
	g_application_quit (G_APPLICATION (user_data));
}

static const GActionEntry app_actions[] = {
	{ "show", opentrackir_application_show_action },
	{ "hide", opentrackir_application_hide_action },
	{ "toggle-mouse", opentrackir_application_toggle_mouse_action },
	{ "quit", opentrackir_application_quit_action },
	{ "about", opentrackir_application_about_action },
};

gboolean
opentrackir_application_status_notifier_is_available (OpentrackirApplication *self)
{
	g_return_val_if_fail (OPENTRACKIR_IS_APPLICATION (self), FALSE);

	return self->status_notifier != NULL &&
	       opentrackir_status_notifier_is_available (self->status_notifier);
}

static void
opentrackir_application_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
	OpentrackirApplication *self = OPENTRACKIR_APPLICATION (object);

	switch (property_id)
	{
	case PROP_STATUS_NOTIFIER_AVAILABLE:
		g_value_set_boolean (value,
		                     opentrackir_application_status_notifier_is_available (self));
		break;
	case PROP_0:
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
opentrackir_application_class_init (OpentrackirApplicationClass *klass)
{
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = opentrackir_application_get_property;
	app_class->activate = opentrackir_application_activate;
	app_class->startup = opentrackir_application_startup;
	app_class->shutdown = opentrackir_application_shutdown;

	properties[PROP_STATUS_NOTIFIER_AVAILABLE] =
		g_param_spec_boolean ("status-notifier-available",
		                      NULL,
		                      NULL,
		                      FALSE,
		                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
opentrackir_application_init (OpentrackirApplication *self)
{
	g_action_map_add_action_entries (G_ACTION_MAP (self),
	                                 app_actions,
	                                 G_N_ELEMENTS (app_actions),
	                                 self);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self),
	                                       "app.quit",
	                                       (const char *[]) { "<control>q", NULL });
}
