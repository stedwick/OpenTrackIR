#include "opentrackir-global-shortcut.h"

#include <glib/gi18n.h>

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SHORTCUT_INTERFACE "org.freedesktop.portal.GlobalShortcuts"
#define PORTAL_REQUEST_INTERFACE "org.freedesktop.portal.Request"
#define PORTAL_SESSION_INTERFACE "org.freedesktop.portal.Session"
#define SHORTCUT_ID "toggle-mouse"
#define SHORTCUT_PREFERRED_TRIGGER "SHIFT+F7"

typedef enum
{
	PENDING_REQUEST_NONE,
	PENDING_REQUEST_CREATE_SESSION,
	PENDING_REQUEST_LIST_SHORTCUTS,
	PENDING_REQUEST_BIND_SHORTCUTS,
} PendingRequest;

struct _OpentrackirGlobalShortcut
{
	GObject parent_instance;

	GActionGroup *actions;
	GDBusConnection *connection;
	GCancellable *cancellable;
	char *session_handle;
	char *description;
	guint portal_watch_id;
	guint activated_subscription_id;
	guint response_subscription_id;
	guint request_serial;
	PendingRequest pending_request;
	OpentrackirGlobalShortcutState state;
};

G_DEFINE_FINAL_TYPE (OpentrackirGlobalShortcut, opentrackir_global_shortcut, G_TYPE_OBJECT)

enum
{
	CHANGED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS];

const char *
opentrackir_global_shortcut_preferred_trigger (void)
{
	return SHORTCUT_PREFERRED_TRIGGER;
}

gboolean
opentrackir_global_shortcut_activation_matches (const char *expected_session,
                                                const char *actual_session,
                                                const char *shortcut_id)
{
	return expected_session != NULL &&
	       actual_session != NULL &&
	       shortcut_id != NULL &&
	       g_str_equal (expected_session, actual_session) &&
	       g_str_equal (shortcut_id, SHORTCUT_ID);
}

static void
set_state (OpentrackirGlobalShortcut      *self,
           OpentrackirGlobalShortcutState state,
           const char                    *description)
{
	gboolean changed;

	changed = self->state != state || g_strcmp0 (self->description, description) != 0;
	self->state = state;
	g_free (self->description);
	self->description = g_strdup (description);
	if (changed)
		g_signal_emit (self, signals[CHANGED], 0);
}

static char *
request_path_for_token (GDBusConnection *connection,
                        const char      *token)
{
	const char *unique_name = g_dbus_connection_get_unique_name (connection);
	g_autofree char *sender = NULL;
	char *cursor;

	if (unique_name == NULL)
		return NULL;

	sender = g_strdup (unique_name[0] == ':' ? unique_name + 1 : unique_name);
	for (cursor = sender; *cursor != '\0'; ++cursor)
	{
		if (*cursor == '.')
			*cursor = '_';
	}
	return g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s",
	                        sender,
	                        token);
}

static char *
find_shortcut_description (GVariant *results)
{
	GVariant *shortcuts;
	GVariantIter iterator;
	const char *id;
	const char *description = NULL;
	GVariant *properties;

	shortcuts = g_variant_lookup_value (results,
	                                    "shortcuts",
	                                    G_VARIANT_TYPE ("a(sa{sv})"));
	if (shortcuts == NULL)
		return NULL;

	g_variant_iter_init (&iterator, shortcuts);
	while (g_variant_iter_next (&iterator, "(&s@a{sv})", &id, &properties))
	{
		if (g_str_equal (id, SHORTCUT_ID) &&
		    g_variant_lookup (properties, "trigger_description", "&s", &description))
		{
			char *copy = g_strdup (description);

			g_variant_unref (properties);
			g_variant_unref (shortcuts);
			return copy;
		}
		g_variant_unref (properties);
	}
	g_variant_unref (shortcuts);
	return NULL;
}

static void begin_list_shortcuts (OpentrackirGlobalShortcut *self);
static void begin_bind_shortcuts (OpentrackirGlobalShortcut *self);

static void
portal_call_finished (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
	OpentrackirGlobalShortcut *self = user_data;
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) reply = NULL;

	reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
	if (reply == NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
	{
		if (self->response_subscription_id != 0)
		{
			g_dbus_connection_signal_unsubscribe (self->connection,
			                                      self->response_subscription_id);
			self->response_subscription_id = 0;
		}
		self->pending_request = PENDING_REQUEST_NONE;
		set_state (self, OPENTRACKIR_GLOBAL_SHORTCUT_FAILED, error->message);
	}
	g_object_unref (self);
}

static void
request_response_received (GDBusConnection *connection,
                           const char      *sender_name,
                           const char      *object_path,
                           const char      *interface_name,
                           const char      *signal_name,
                           GVariant        *parameters,
                           gpointer         user_data)
{
	OpentrackirGlobalShortcut *self = user_data;
	g_autoptr(GVariant) results = NULL;
	PendingRequest completed_request;
	guint response;

	g_variant_get (parameters, "(u@a{sv})", &response, &results);
	if (self->response_subscription_id != 0)
	{
		g_dbus_connection_signal_unsubscribe (connection,
		                                      self->response_subscription_id);
		self->response_subscription_id = 0;
	}
	completed_request = self->pending_request;
	self->pending_request = PENDING_REQUEST_NONE;

	if (response != 0)
	{
		set_state (self,
		           OPENTRACKIR_GLOBAL_SHORTCUT_FAILED,
		           response == 1 ? _("Shortcut setup was cancelled.")
		                         : _("The desktop rejected the shortcut."));
		return;
	}

	if (completed_request == PENDING_REQUEST_CREATE_SESSION)
	{
		const char *session_handle = NULL;

		if (!g_variant_lookup (results, "session_handle", "&s", &session_handle))
		{
			set_state (self,
			           OPENTRACKIR_GLOBAL_SHORTCUT_FAILED,
			           _("The desktop did not create a shortcut session."));
			return;
		}
		g_free (self->session_handle);
		self->session_handle = g_strdup (session_handle);
		begin_list_shortcuts (self);
	}
	else if (completed_request == PENDING_REQUEST_LIST_SHORTCUTS)
	{
		g_autofree char *description = find_shortcut_description (results);

		if (description != NULL)
			set_state (self, OPENTRACKIR_GLOBAL_SHORTCUT_READY, description);
		else
			begin_bind_shortcuts (self);
	}
	else if (completed_request == PENDING_REQUEST_BIND_SHORTCUTS)
	{
		g_autofree char *description = find_shortcut_description (results);

		if (description != NULL)
			set_state (self, OPENTRACKIR_GLOBAL_SHORTCUT_READY, description);
		else
			set_state (self,
			           OPENTRACKIR_GLOBAL_SHORTCUT_FAILED,
			           _("Shift+F7 was not bound."));
	}
}

static void
begin_request (OpentrackirGlobalShortcut *self,
               const char                *method,
               GVariant                  *parameters,
               PendingRequest             request,
               const char                *token)
{
	g_autofree char *request_path = request_path_for_token (self->connection, token);

	if (request_path == NULL)
	{
		set_state (self,
		           OPENTRACKIR_GLOBAL_SHORTCUT_FAILED,
		           _("The session bus has no unique name."));
		return;
	}

	self->pending_request = request;
	self->response_subscription_id =
		g_dbus_connection_signal_subscribe (self->connection,
		                                    PORTAL_BUS_NAME,
		                                    PORTAL_REQUEST_INTERFACE,
		                                    "Response",
		                                    request_path,
		                                    NULL,
		                                    G_DBUS_SIGNAL_FLAGS_NONE,
		                                    request_response_received,
		                                    self,
		                                    NULL);
	g_dbus_connection_call (self->connection,
	                        PORTAL_BUS_NAME,
	                        PORTAL_OBJECT_PATH,
	                        PORTAL_SHORTCUT_INTERFACE,
	                        method,
	                        parameters,
	                        G_VARIANT_TYPE ("(o)"),
	                        G_DBUS_CALL_FLAGS_NONE,
	                        -1,
	                        self->cancellable,
	                        portal_call_finished,
	                        g_object_ref (self));
}

static char *
next_token (OpentrackirGlobalShortcut *self,
            const char                *prefix)
{
	self->request_serial += 1;
	return g_strdup_printf ("opentrackir_%s_%u", prefix, self->request_serial);
}

static void
begin_create_session (OpentrackirGlobalShortcut *self)
{
	GVariantBuilder options;
	g_autofree char *request_token = next_token (self, "create");
	g_autofree char *session_token = next_token (self, "session");

	g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add (&options, "{sv}", "handle_token",
	                       g_variant_new_string (request_token));
	g_variant_builder_add (&options, "{sv}", "session_handle_token",
	                       g_variant_new_string (session_token));
	begin_request (self,
	               "CreateSession",
	               g_variant_new ("(@a{sv})", g_variant_builder_end (&options)),
	               PENDING_REQUEST_CREATE_SESSION,
	               request_token);
}

static void
begin_list_shortcuts (OpentrackirGlobalShortcut *self)
{
	GVariantBuilder options;
	g_autofree char *request_token = next_token (self, "list");

	g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add (&options, "{sv}", "handle_token",
	                       g_variant_new_string (request_token));
	begin_request (self,
	               "ListShortcuts",
	               g_variant_new ("(o@a{sv})",
	                              self->session_handle,
	                              g_variant_builder_end (&options)),
	               PENDING_REQUEST_LIST_SHORTCUTS,
	               request_token);
}

static void
begin_bind_shortcuts (OpentrackirGlobalShortcut *self)
{
	GVariantBuilder shortcuts;
	GVariantBuilder properties;
	GVariantBuilder options;
	g_autofree char *request_token = next_token (self, "bind");

	g_variant_builder_init (&shortcuts, G_VARIANT_TYPE ("a(sa{sv})"));
	g_variant_builder_init (&properties, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add (&properties, "{sv}", "description",
	                       g_variant_new_string (_("Toggle mouse movement")));
	g_variant_builder_add (&properties, "{sv}", "preferred_trigger",
	                       g_variant_new_string (SHORTCUT_PREFERRED_TRIGGER));
	g_variant_builder_add (&shortcuts,
	                       "(s@a{sv})",
	                       SHORTCUT_ID,
	                       g_variant_builder_end (&properties));

	g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
	g_variant_builder_add (&options, "{sv}", "handle_token",
	                       g_variant_new_string (request_token));
	begin_request (self,
	               "BindShortcuts",
	               g_variant_new ("(o@a(sa{sv})s@a{sv})",
	                              self->session_handle,
	                              g_variant_builder_end (&shortcuts),
	                              "",
	                              g_variant_builder_end (&options)),
	               PENDING_REQUEST_BIND_SHORTCUTS,
	               request_token);
}

static void
shortcut_activated (GDBusConnection *connection,
                    const char      *sender_name,
                    const char      *object_path,
                    const char      *interface_name,
                    const char      *signal_name,
                    GVariant        *parameters,
                    gpointer         user_data)
{
	OpentrackirGlobalShortcut *self = user_data;
	const char *session_handle;
	const char *shortcut_id;
	guint64 timestamp;
	g_autoptr(GVariant) options = NULL;

	g_variant_get (parameters,
	               "(&o&st@a{sv})",
	               &session_handle,
	               &shortcut_id,
	               &timestamp,
	               &options);
	if (opentrackir_global_shortcut_activation_matches (self->session_handle,
	                                                    session_handle,
	                                                    shortcut_id))
	{
		g_action_group_activate_action (self->actions, "toggle-mouse", NULL);
	}
}

static void
portal_appeared (GDBusConnection *connection,
                 const char      *name,
                 const char      *name_owner,
                 gpointer         user_data)
{
	OpentrackirGlobalShortcut *self = user_data;

	if (self->session_handle != NULL || self->pending_request != PENDING_REQUEST_NONE)
		return;
	set_state (self, OPENTRACKIR_GLOBAL_SHORTCUT_REGISTERING, _("Registering Shift+F7…"));
	begin_create_session (self);
}

static void
portal_vanished (GDBusConnection *connection,
                 const char      *name,
                 gpointer         user_data)
{
	OpentrackirGlobalShortcut *self = user_data;

	if (self->response_subscription_id != 0)
	{
		g_dbus_connection_signal_unsubscribe (connection, self->response_subscription_id);
		self->response_subscription_id = 0;
	}
	self->pending_request = PENDING_REQUEST_NONE;
	g_clear_pointer (&self->session_handle, g_free);
	set_state (self,
	           OPENTRACKIR_GLOBAL_SHORTCUT_UNAVAILABLE,
	           _("Global shortcut unavailable · Shift+F7 works while this window is focused."));
}

static void
bus_ready (GObject      *source,
           GAsyncResult *result,
           gpointer      user_data)
{
	OpentrackirGlobalShortcut *self = user_data;
	g_autoptr(GError) error = NULL;

	self->connection = g_bus_get_finish (result, &error);
	if (self->connection == NULL)
	{
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			set_state (self, OPENTRACKIR_GLOBAL_SHORTCUT_UNAVAILABLE, error->message);
		g_object_unref (self);
		return;
	}

	self->activated_subscription_id =
		g_dbus_connection_signal_subscribe (self->connection,
		                                    PORTAL_BUS_NAME,
		                                    PORTAL_SHORTCUT_INTERFACE,
		                                    "Activated",
		                                    PORTAL_OBJECT_PATH,
		                                    NULL,
		                                    G_DBUS_SIGNAL_FLAGS_NONE,
		                                    shortcut_activated,
		                                    self,
		                                    NULL);
	self->portal_watch_id =
		g_bus_watch_name_on_connection (self->connection,
		                                PORTAL_BUS_NAME,
		                                G_BUS_NAME_WATCHER_FLAGS_NONE,
		                                portal_appeared,
		                                portal_vanished,
		                                self,
		                                NULL);
	g_object_unref (self);
}

OpentrackirGlobalShortcut *
opentrackir_global_shortcut_new (GActionGroup *actions)
{
	g_return_val_if_fail (G_IS_ACTION_GROUP (actions), NULL);

	return g_object_new (OPENTRACKIR_TYPE_GLOBAL_SHORTCUT,
	                     "actions", actions,
	                     NULL);
}

void
opentrackir_global_shortcut_start (OpentrackirGlobalShortcut *self)
{
	g_return_if_fail (OPENTRACKIR_IS_GLOBAL_SHORTCUT (self));

	if (self->connection != NULL || self->portal_watch_id != 0)
		return;
	set_state (self, OPENTRACKIR_GLOBAL_SHORTCUT_REGISTERING, _("Registering Shift+F7…"));
	g_bus_get (G_BUS_TYPE_SESSION,
	           self->cancellable,
	           bus_ready,
	           g_object_ref (self));
}

OpentrackirGlobalShortcutState
opentrackir_global_shortcut_get_state (OpentrackirGlobalShortcut *self)
{
	g_return_val_if_fail (OPENTRACKIR_IS_GLOBAL_SHORTCUT (self),
	                      OPENTRACKIR_GLOBAL_SHORTCUT_FAILED);
	return self->state;
}

const char *
opentrackir_global_shortcut_get_description (OpentrackirGlobalShortcut *self)
{
	g_return_val_if_fail (OPENTRACKIR_IS_GLOBAL_SHORTCUT (self), NULL);
	return self->description;
}

enum
{
	PROP_0,
	PROP_ACTIONS,
	N_PROPERTIES,
};

static GParamSpec *properties[N_PROPERTIES];

static void
opentrackir_global_shortcut_set_property (GObject      *object,
                                          guint         property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
	OpentrackirGlobalShortcut *self = OPENTRACKIR_GLOBAL_SHORTCUT (object);

	switch (property_id)
	{
	case PROP_ACTIONS:
		self->actions = g_value_dup_object (value);
		break;
	case PROP_0:
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
opentrackir_global_shortcut_dispose (GObject *object)
{
	OpentrackirGlobalShortcut *self = OPENTRACKIR_GLOBAL_SHORTCUT (object);

	g_cancellable_cancel (self->cancellable);
	if (self->portal_watch_id != 0)
	{
		g_bus_unwatch_name (self->portal_watch_id);
		self->portal_watch_id = 0;
	}
	if (self->connection != NULL && self->response_subscription_id != 0)
	{
		g_dbus_connection_signal_unsubscribe (self->connection,
		                                      self->response_subscription_id);
		self->response_subscription_id = 0;
	}
	if (self->connection != NULL && self->activated_subscription_id != 0)
	{
		g_dbus_connection_signal_unsubscribe (self->connection,
		                                      self->activated_subscription_id);
		self->activated_subscription_id = 0;
	}
	if (self->connection != NULL && self->session_handle != NULL)
	{
		g_dbus_connection_call (self->connection,
		                        PORTAL_BUS_NAME,
		                        self->session_handle,
		                        PORTAL_SESSION_INTERFACE,
		                        "Close",
		                        NULL,
		                        NULL,
		                        G_DBUS_CALL_FLAGS_NONE,
		                        -1,
		                        NULL,
		                        NULL,
		                        NULL);
	}
	g_clear_object (&self->connection);
	g_clear_object (&self->actions);
	g_clear_object (&self->cancellable);

	G_OBJECT_CLASS (opentrackir_global_shortcut_parent_class)->dispose (object);
}

static void
opentrackir_global_shortcut_finalize (GObject *object)
{
	OpentrackirGlobalShortcut *self = OPENTRACKIR_GLOBAL_SHORTCUT (object);

	g_clear_pointer (&self->session_handle, g_free);
	g_clear_pointer (&self->description, g_free);
	G_OBJECT_CLASS (opentrackir_global_shortcut_parent_class)->finalize (object);
}

static void
opentrackir_global_shortcut_class_init (OpentrackirGlobalShortcutClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = opentrackir_global_shortcut_set_property;
	object_class->dispose = opentrackir_global_shortcut_dispose;
	object_class->finalize = opentrackir_global_shortcut_finalize;

	properties[PROP_ACTIONS] =
		g_param_spec_object ("actions",
		                     NULL,
		                     NULL,
		                     G_TYPE_ACTION_GROUP,
		                     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
	g_object_class_install_properties (object_class, N_PROPERTIES, properties);

	signals[CHANGED] =
		g_signal_new ("changed",
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
opentrackir_global_shortcut_init (OpentrackirGlobalShortcut *self)
{
	self->cancellable = g_cancellable_new ();
	self->state = OPENTRACKIR_GLOBAL_SHORTCUT_UNAVAILABLE;
	self->description = g_strdup (_("Global shortcut unavailable · Shift+F7 works while this window is focused."));
}
