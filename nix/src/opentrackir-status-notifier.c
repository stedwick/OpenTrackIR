#include "config.h"

#include <glib/gi18n.h>

#include "opentrackir-status-notifier.h"
#include "opentrackir-status-notifier-menu.h"

#define STATUS_NOTIFIER_PATH "/StatusNotifierItem"
#define STATUS_NOTIFIER_MENU_PATH "/StatusNotifierItem/Menu"
#define STATUS_NOTIFIER_INTERFACE "org.kde.StatusNotifierItem"
#define STATUS_NOTIFIER_WATCHER "org.kde.StatusNotifierWatcher"
#define STATUS_NOTIFIER_WATCHER_PATH "/StatusNotifierWatcher"

struct _OpentrackirStatusNotifier
{
	GObject parent_instance;

	GWeakRef application;
	GDBusConnection *connection;
	GDBusNodeInfo *node_info;
	GDBusNodeInfo *menu_node_info;
	guint object_registration_id;
	guint menu_object_registration_id;
	guint watcher_watch_id;
	gboolean watcher_present;
	gboolean available;
	gboolean visible;
	gboolean window_visible;
	gboolean camera_enabled;
	gboolean mouse_enabled;
};

G_DEFINE_FINAL_TYPE (OpentrackirStatusNotifier, opentrackir_status_notifier, G_TYPE_OBJECT)

enum
{
	AVAILABILITY_CHANGED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS];

static const char introspection_xml[] =
	"<node>"
	" <interface name='org.kde.StatusNotifierItem'>"
	"  <method name='ContextMenu'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
	"  <method name='Activate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
	"  <method name='SecondaryActivate'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>"
	"  <method name='Scroll'><arg type='i' direction='in'/><arg type='s' direction='in'/></method>"
	"  <property name='Category' type='s' access='read'/>"
	"  <property name='Id' type='s' access='read'/>"
	"  <property name='Title' type='s' access='read'/>"
	"  <property name='Status' type='s' access='read'/>"
	"  <property name='WindowId' type='i' access='read'/>"
	"  <property name='IconName' type='s' access='read'/>"
	"  <property name='IconPixmap' type='a(iiay)' access='read'/>"
	"  <property name='OverlayIconName' type='s' access='read'/>"
	"  <property name='OverlayIconPixmap' type='a(iiay)' access='read'/>"
	"  <property name='AttentionIconName' type='s' access='read'/>"
	"  <property name='AttentionIconPixmap' type='a(iiay)' access='read'/>"
	"  <property name='AttentionMovieName' type='s' access='read'/>"
	"  <property name='ToolTip' type='(sa(iiay)ss)' access='read'/>"
	"  <property name='ItemIsMenu' type='b' access='read'/>"
	"  <property name='Menu' type='o' access='read'/>"
	"  <signal name='NewTitle'/>"
	"  <signal name='NewIcon'/>"
	"  <signal name='NewAttentionIcon'/>"
	"  <signal name='NewOverlayIcon'/>"
	"  <signal name='NewToolTip'/>"
	"  <signal name='NewStatus'><arg type='s'/></signal>"
	" </interface>"
	"</node>";

static const char menu_introspection_xml[] =
	"<node>"
	" <interface name='com.canonical.dbusmenu'>"
	"  <method name='GetLayout'>"
	"   <arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='as' direction='in'/>"
	"   <arg type='u' direction='out'/><arg type='(ia{sv}av)' direction='out'/>"
	"  </method>"
	"  <method name='GetGroupProperties'>"
	"   <arg type='ai' direction='in'/><arg type='as' direction='in'/>"
	"   <arg type='a(ia{sv})' direction='out'/>"
	"  </method>"
	"  <method name='GetProperty'>"
	"   <arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='v' direction='out'/>"
	"  </method>"
	"  <method name='Event'>"
	"   <arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='v' direction='in'/><arg type='u' direction='in'/>"
	"  </method>"
	"  <method name='AboutToShow'>"
	"   <arg type='i' direction='in'/><arg type='b' direction='out'/>"
	"  </method>"
	"  <property name='Version' type='u' access='read'/>"
	"  <property name='Status' type='s' access='read'/>"
	"  <signal name='ItemsPropertiesUpdated'><arg type='a(ia{sv})'/><arg type='a(ias)'/></signal>"
	"  <signal name='LayoutUpdated'><arg type='u'/><arg type='i'/></signal>"
	"  <signal name='ItemActivationRequested'><arg type='i'/><arg type='u'/></signal>"
	" </interface>"
	"</node>";

static void
set_available (OpentrackirStatusNotifier *self,
               gboolean                   available)
{
	if (self->available == available)
		return;

	self->available = available;
	g_signal_emit (self, signals[AVAILABILITY_CHANGED], 0);
}

static void
activate_application_action (OpentrackirStatusNotifier *self,
                             const char                *action_name)
{
	GApplication *application = g_weak_ref_get (&self->application);

	if (application == NULL)
		return;
	g_action_group_activate_action (G_ACTION_GROUP (application), action_name, NULL);
	g_object_unref (application);
}

static void
handle_method_call (GDBusConnection       *connection,
                    const char            *sender,
                    const char            *object_path,
                    const char            *interface_name,
                    const char            *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
	OpentrackirStatusNotifier *self = user_data;

	if (g_str_equal (method_name, "Activate"))
	{
		activate_application_action (self, "show");
	}
	else if (g_str_equal (method_name, "SecondaryActivate"))
	{
		activate_application_action (self, "toggle-mouse");
	}
	else if (!g_str_equal (method_name, "ContextMenu") &&
	         !g_str_equal (method_name, "Scroll"))
	{
		g_dbus_method_invocation_return_error (invocation,
		                                       G_DBUS_ERROR,
		                                       G_DBUS_ERROR_UNKNOWN_METHOD,
		                                       "Unknown StatusNotifierItem method %s",
		                                       method_name);
		return;
	}

	g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
add_menu_item_properties (GVariantBuilder *builder,
                          gint             item_id)
{
	GVariant *properties;

	properties = opentrackir_status_notifier_menu_item_properties (
		item_id,
		_("Show"),
		_("Quit")
	);
	if (properties != NULL)
		g_variant_builder_add (builder, "(i@a{sv})", item_id, properties);
}

static void
handle_menu_method_call (GDBusConnection       *connection,
                         const char            *sender,
                         const char            *object_path,
                         const char            *interface_name,
                         const char            *method_name,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation,
                         gpointer               user_data)
{
	OpentrackirStatusNotifier *self = user_data;

	if (g_str_equal (method_name, "GetLayout"))
	{
		g_autoptr(GVariant) property_names = NULL;
		GVariant *layout;
		gint parent_id;
		gint recursion_depth;

		g_variant_get (parameters, "(ii@as)", &parent_id, &recursion_depth, &property_names);
		layout = opentrackir_status_notifier_menu_layout (
			parent_id,
			recursion_depth,
			_("Show"),
			_("Quit")
		);
		if (layout == NULL)
		{
			g_dbus_method_invocation_return_error (invocation,
			                                       G_DBUS_ERROR,
			                                       G_DBUS_ERROR_INVALID_ARGS,
			                                       "Unknown menu item %d",
			                                       parent_id);
			return;
		}
		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new ("(u@(ia{sv}av))", 1u, layout)
		);
		return;
	}

	if (g_str_equal (method_name, "GetGroupProperties"))
	{
		g_autoptr(GVariant) ids = NULL;
		g_autoptr(GVariant) property_names = NULL;
		GVariantBuilder properties;
		GVariantIter iter;
		gint item_id;

		g_variant_get (parameters, "(@ai@as)", &ids, &property_names);
		g_variant_builder_init (&properties, G_VARIANT_TYPE ("a(ia{sv})"));
		if (g_variant_n_children (ids) == 0)
		{
			add_menu_item_properties (&properties, OPENTRACKIR_STATUS_NOTIFIER_MENU_SHOW_ID);
			add_menu_item_properties (&properties, OPENTRACKIR_STATUS_NOTIFIER_MENU_QUIT_ID);
		}
		else
		{
			g_variant_iter_init (&iter, ids);
			while (g_variant_iter_next (&iter, "i", &item_id))
				add_menu_item_properties (&properties, item_id);
		}
		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new ("(@a(ia{sv}))", g_variant_builder_end (&properties))
		);
		return;
	}

	if (g_str_equal (method_name, "GetProperty"))
	{
		GVariant *properties;
		GVariant *value;
		const char *property_name;
		gint item_id;

		g_variant_get (parameters, "(i&s)", &item_id, &property_name);
		properties = opentrackir_status_notifier_menu_item_properties (
			item_id,
			_("Show"),
			_("Quit")
		);
		value = properties != NULL
			? g_variant_lookup_value (properties, property_name, NULL)
			: NULL;
		g_clear_pointer (&properties, g_variant_unref);
		if (value == NULL)
		{
			g_dbus_method_invocation_return_error (invocation,
			                                       G_DBUS_ERROR,
			                                       G_DBUS_ERROR_INVALID_ARGS,
			                                       "Unknown menu property %s for item %d",
			                                       property_name,
			                                       item_id);
			return;
		}
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(v)", value));
		g_variant_unref (value);
		return;
	}

	if (g_str_equal (method_name, "Event"))
	{
		g_autoptr(GVariant) data = NULL;
		OpentrackirStatusNotifierMenuAction action;
		const char *event_id;
		guint timestamp;
		gint item_id;

		g_variant_get (parameters, "(i&s@vu)", &item_id, &event_id, &data, &timestamp);
		action = opentrackir_status_notifier_menu_action_for_event (item_id, event_id);
		if (action == OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_SHOW)
			activate_application_action (self, "show");
		else if (action == OPENTRACKIR_STATUS_NOTIFIER_MENU_ACTION_QUIT)
			activate_application_action (self, "quit");
		g_dbus_method_invocation_return_value (invocation, NULL);
		return;
	}

	if (g_str_equal (method_name, "AboutToShow"))
	{
		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", FALSE));
		return;
	}

	g_dbus_method_invocation_return_error (invocation,
	                                       G_DBUS_ERROR,
	                                       G_DBUS_ERROR_UNKNOWN_METHOD,
	                                       "Unknown DBusMenu method %s",
	                                       method_name);
}

static GVariant *
empty_pixmap_array (void)
{
	GVariantBuilder builder;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(iiay)"));
	return g_variant_builder_end (&builder);
}

static GVariant *
get_dbus_property (GDBusConnection  *connection,
                   const char       *sender,
                   const char       *object_path,
                   const char       *interface_name,
                   const char       *property_name,
                   GError          **error,
                   gpointer          user_data)
{
	OpentrackirStatusNotifier *self = user_data;

	if (g_str_equal (property_name, "Category"))
		return g_variant_new_string ("ApplicationStatus");
	if (g_str_equal (property_name, "Id"))
		return g_variant_new_string ("opentrackir");
	if (g_str_equal (property_name, "Title"))
		return g_variant_new_string ("OpenTrackIR");
	if (g_str_equal (property_name, "Status"))
		return g_variant_new_string (self->visible ? "Active" : "Passive");
	if (g_str_equal (property_name, "WindowId"))
		return g_variant_new_int32 (0);
	if (g_str_equal (property_name, "IconName"))
		return g_variant_new_string ("org.gnome.opentrackir");
	if (g_str_equal (property_name, "IconPixmap") ||
	    g_str_equal (property_name, "OverlayIconPixmap") ||
	    g_str_equal (property_name, "AttentionIconPixmap"))
		return empty_pixmap_array ();
	if (g_str_equal (property_name, "OverlayIconName") ||
	    g_str_equal (property_name, "AttentionIconName") ||
	    g_str_equal (property_name, "AttentionMovieName"))
		return g_variant_new_string ("");
	if (g_str_equal (property_name, "ToolTip"))
	{
		g_autofree char *description = g_strdup_printf (
			_("%s · Mouse movement %s · %s"),
			self->camera_enabled ? _("Camera on") : _("Camera off"),
			self->mouse_enabled ? _("on") : _("off"),
			self->window_visible ? _("window visible") : _("running in background")
		);
		return g_variant_new ("(s@a(iiay)ss)",
		                      "org.gnome.opentrackir",
		                      empty_pixmap_array (),
		                      "OpenTrackIR",
		                      description);
	}
	if (g_str_equal (property_name, "ItemIsMenu"))
		return g_variant_new_boolean (FALSE);
	if (g_str_equal (property_name, "Menu"))
		return g_variant_new_object_path (STATUS_NOTIFIER_MENU_PATH);

	g_set_error (error,
	             G_DBUS_ERROR,
	             G_DBUS_ERROR_UNKNOWN_PROPERTY,
	             "Unknown StatusNotifierItem property %s",
	             property_name);
	return NULL;
}

static const GDBusInterfaceVTable interface_vtable = {
	.method_call = handle_method_call,
	.get_property = get_dbus_property,
};

static GVariant *
get_menu_dbus_property (GDBusConnection  *connection,
                        const char       *sender,
                        const char       *object_path,
                        const char       *interface_name,
                        const char       *property_name,
                        GError          **error,
                        gpointer          user_data)
{
	if (g_str_equal (property_name, "Version"))
		return g_variant_new_uint32 (3);
	if (g_str_equal (property_name, "Status"))
		return g_variant_new_string ("normal");

	g_set_error (error,
	             G_DBUS_ERROR,
	             G_DBUS_ERROR_UNKNOWN_PROPERTY,
	             "Unknown DBusMenu property %s",
	             property_name);
	return NULL;
}

static const GDBusInterfaceVTable menu_interface_vtable = {
	.method_call = handle_menu_method_call,
	.get_property = get_menu_dbus_property,
};

static void
registration_finished (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
	OpentrackirStatusNotifier *self = user_data;
	g_autoptr(GError) error = NULL;
	GVariant *reply;

	reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), result, &error);
	if (reply != NULL)
		g_variant_unref (reply);
	set_available (self, reply != NULL && self->watcher_present);
	g_object_unref (self);
}

static void
watcher_appeared (GDBusConnection *connection,
                  const char      *name,
                  const char      *name_owner,
                  gpointer         user_data)
{
	OpentrackirStatusNotifier *self = user_data;

	self->watcher_present = TRUE;
	g_dbus_connection_call (connection,
	                        STATUS_NOTIFIER_WATCHER,
	                        STATUS_NOTIFIER_WATCHER_PATH,
	                        STATUS_NOTIFIER_WATCHER,
	                        "RegisterStatusNotifierItem",
	                        g_variant_new ("(s)", STATUS_NOTIFIER_PATH),
	                        NULL,
	                        G_DBUS_CALL_FLAGS_NONE,
	                        -1,
	                        NULL,
	                        registration_finished,
	                        g_object_ref (self));
}

static void
watcher_vanished (GDBusConnection *connection,
                  const char      *name,
                  gpointer         user_data)
{
	OpentrackirStatusNotifier *self = user_data;

	self->watcher_present = FALSE;
	set_available (self, FALSE);
}

static void
emit_signal (OpentrackirStatusNotifier *self,
             const char                *signal_name,
             GVariant                  *parameters)
{
	if (self->connection == NULL || self->object_registration_id == 0)
		return;
	g_dbus_connection_emit_signal (self->connection,
	                               NULL,
	                               STATUS_NOTIFIER_PATH,
	                               STATUS_NOTIFIER_INTERFACE,
	                               signal_name,
	                               parameters,
	                               NULL);
}

static void
opentrackir_status_notifier_dispose (GObject *object)
{
	OpentrackirStatusNotifier *self = OPENTRACKIR_STATUS_NOTIFIER (object);

	if (self->watcher_watch_id != 0)
	{
		g_bus_unwatch_name (self->watcher_watch_id);
		self->watcher_watch_id = 0;
	}
	if (self->connection != NULL && self->object_registration_id != 0)
	{
		g_dbus_connection_unregister_object (self->connection,
		                                     self->object_registration_id);
		self->object_registration_id = 0;
	}
	if (self->connection != NULL && self->menu_object_registration_id != 0)
	{
		g_dbus_connection_unregister_object (self->connection,
		                                     self->menu_object_registration_id);
		self->menu_object_registration_id = 0;
	}
	g_clear_pointer (&self->node_info, g_dbus_node_info_unref);
	g_clear_pointer (&self->menu_node_info, g_dbus_node_info_unref);
	g_clear_object (&self->connection);

	G_OBJECT_CLASS (opentrackir_status_notifier_parent_class)->dispose (object);
}

static void
opentrackir_status_notifier_finalize (GObject *object)
{
	OpentrackirStatusNotifier *self = OPENTRACKIR_STATUS_NOTIFIER (object);

	g_weak_ref_clear (&self->application);
	G_OBJECT_CLASS (opentrackir_status_notifier_parent_class)->finalize (object);
}

static void
opentrackir_status_notifier_class_init (OpentrackirStatusNotifierClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = opentrackir_status_notifier_dispose;
	object_class->finalize = opentrackir_status_notifier_finalize;
	signals[AVAILABILITY_CHANGED] =
		g_signal_new ("availability-changed",
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
opentrackir_status_notifier_init (OpentrackirStatusNotifier *self)
{
	g_weak_ref_init (&self->application, NULL);
}

OpentrackirStatusNotifier *
opentrackir_status_notifier_new (GApplication *application)
{
	OpentrackirStatusNotifier *self;
	g_autoptr(GError) error = NULL;

	g_return_val_if_fail (G_IS_APPLICATION (application), NULL);

	self = g_object_new (OPENTRACKIR_TYPE_STATUS_NOTIFIER, NULL);
	g_weak_ref_set (&self->application, application);
	self->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (self->connection == NULL)
		return self;

	self->node_info = g_dbus_node_info_new_for_xml (introspection_xml, &error);
	if (self->node_info == NULL)
		return self;

	self->object_registration_id =
		g_dbus_connection_register_object (self->connection,
		                                   STATUS_NOTIFIER_PATH,
		                                   self->node_info->interfaces[0],
		                                   &interface_vtable,
		                                   self,
		                                   NULL,
		                                   &error);
	if (self->object_registration_id == 0)
		return self;

	self->menu_node_info = g_dbus_node_info_new_for_xml (menu_introspection_xml, &error);
	if (self->menu_node_info == NULL)
		return self;
	self->menu_object_registration_id =
		g_dbus_connection_register_object (self->connection,
		                                   STATUS_NOTIFIER_MENU_PATH,
		                                   self->menu_node_info->interfaces[0],
		                                   &menu_interface_vtable,
		                                   self,
		                                   NULL,
		                                   &error);
	if (self->menu_object_registration_id == 0)
		return self;

	self->watcher_watch_id =
		g_bus_watch_name_on_connection (self->connection,
		                                STATUS_NOTIFIER_WATCHER,
		                                G_BUS_NAME_WATCHER_FLAGS_NONE,
		                                watcher_appeared,
		                                watcher_vanished,
		                                self,
		                                NULL);
	return self;
}

gboolean
opentrackir_status_notifier_is_available (OpentrackirStatusNotifier *self)
{
	g_return_val_if_fail (OPENTRACKIR_IS_STATUS_NOTIFIER (self), FALSE);

	return self->available;
}

void
opentrackir_status_notifier_update (OpentrackirStatusNotifier *self,
                                    gboolean                   visible,
                                    gboolean                   window_visible,
                                    gboolean                   camera_enabled,
                                    gboolean                   mouse_enabled)
{
	gboolean status_changed;

	g_return_if_fail (OPENTRACKIR_IS_STATUS_NOTIFIER (self));

	status_changed = self->visible != visible;
	self->visible = visible;
	self->window_visible = window_visible;
	self->camera_enabled = camera_enabled;
	self->mouse_enabled = mouse_enabled;

	if (status_changed)
		emit_signal (self,
		             "NewStatus",
		             g_variant_new ("(s)", visible ? "Active" : "Passive"));
	emit_signal (self, "NewToolTip", NULL);
}
