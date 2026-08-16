#include "opentrackir-xkeys-monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <poll.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>

typedef struct
{
	char *path;
	int fd;
	gboolean middle_pressed;
} PedalDevice;

struct _OpentrackirXKeysMonitor
{
	GObject parent_instance;

	GMutex mutex;
	GThread *thread;
	int wake_fd;
	gboolean stop_requested;
	gboolean enabled;
	OpentrackirXKeysState state;
};

G_DEFINE_FINAL_TYPE (OpentrackirXKeysMonitor, opentrackir_xkeys_monitor, G_TYPE_OBJECT)

enum
{
	STATE_CHANGED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
pedal_device_free (gpointer data)
{
	PedalDevice *device = data;

	if (device == NULL)
		return;
	if (device->fd >= 0)
		close (device->fd);
	g_free (device->path);
	g_free (device);
}

static gboolean
emit_state_changed (gpointer user_data)
{
	OpentrackirXKeysMonitor *self = user_data;

	g_signal_emit (self, signals[STATE_CHANGED], 0);
	return G_SOURCE_REMOVE;
}

static void
publish_state (OpentrackirXKeysMonitor *self,
               OpentrackirXKeysState   state)
{
	gboolean changed;

	g_mutex_lock (&self->mutex);
	changed = !opentrackir_xkeys_state_equal (&self->state, &state);
	self->state = state;
	g_mutex_unlock (&self->mutex);
	if (changed)
	{
		g_main_context_invoke_full (NULL,
		                            G_PRIORITY_DEFAULT,
		                            emit_state_changed,
		                            g_object_ref (self),
		                            g_object_unref);
	}
}

static guint
hex_property (struct udev_device *device,
              const char         *name)
{
	const char *value = udev_device_get_property_value (device, name);

	return value != NULL ? (guint)g_ascii_strtoull (value, NULL, 16) : 0;
}

static void
publish_devices_state (OpentrackirXKeysMonitor *self,
                       GPtrArray               *devices,
                       guint                    detected_count,
                       int                      error_number)
{
	gboolean middle_pressed = FALSE;
	guint index;

	for (index = 0; index < devices->len; ++index)
	{
		PedalDevice *device = g_ptr_array_index (devices, index);

		middle_pressed = middle_pressed || device->middle_pressed;
	}
	publish_state (self,
	               opentrackir_xkeys_derive_state (TRUE,
	                                                detected_count,
	                                                devices->len,
	                                                middle_pressed,
	                                                error_number));
}

static void
rebuild_devices (OpentrackirXKeysMonitor *self,
                 struct udev            *udev,
                 GPtrArray              *devices,
                 guint                  *detected_count,
                 int                    *last_error)
{
	struct udev_enumerate *enumerate;
	struct udev_list_entry *entries;
	struct udev_list_entry *entry;

	g_ptr_array_set_size (devices, 0);
	*detected_count = 0;
	*last_error = 0;
	enumerate = udev_enumerate_new (udev);
	if (enumerate == NULL)
	{
		*last_error = ENOMEM;
		publish_devices_state (self, devices, *detected_count, *last_error);
		return;
	}
	udev_enumerate_add_match_subsystem (enumerate, "hidraw");
	udev_enumerate_scan_devices (enumerate);
	entries = udev_enumerate_get_list_entry (enumerate);
	udev_list_entry_foreach (entry, entries)
	{
		const char *syspath = udev_list_entry_get_name (entry);
		struct udev_device *udev_device = udev_device_new_from_syspath (udev, syspath);
		guint vendor_id;
		guint product_id;
		guint interface_number;
		const char *devnode;
		int fd;
		PedalDevice *device;

		if (udev_device == NULL)
			continue;
		vendor_id = hex_property (udev_device, "ID_VENDOR_ID");
		product_id = hex_property (udev_device, "ID_MODEL_ID");
		interface_number = hex_property (udev_device, "ID_USB_INTERFACE_NUM");
		if (!opentrackir_xkeys_device_is_supported ((guint16)vendor_id,
		                                              (guint16)product_id,
		                                              (guint8)interface_number))
		{
			udev_device_unref (udev_device);
			continue;
		}

		*detected_count += 1;
		devnode = udev_device_get_devnode (udev_device);
		fd = devnode != NULL ? open (devnode, O_RDONLY | O_NONBLOCK | O_CLOEXEC) : -1;
		if (fd < 0)
		{
			*last_error = devnode == NULL ? ENOENT : errno;
			udev_device_unref (udev_device);
			continue;
		}

		device = g_new0 (PedalDevice, 1);
		device->path = g_strdup (devnode);
		device->fd = fd;
		g_ptr_array_add (devices, device);
		udev_device_unref (udev_device);
	}
	udev_enumerate_unref (enumerate);
	publish_devices_state (self, devices, *detected_count, *last_error);
}

static void
drain_wake_fd (int fd)
{
	uint64_t value;

	while (read (fd, &value, sizeof value) < 0 && errno == EINTR)
		;
}

static void
wake_monitor (OpentrackirXKeysMonitor *self)
{
	uint64_t wake_value = 1;
	ssize_t result;

	do
	{
		result = write (self->wake_fd, &wake_value, sizeof wake_value);
	} while (result < 0 && errno == EINTR);
}

static gpointer
monitor_thread (gpointer user_data)
{
	OpentrackirXKeysMonitor *self = user_data;
	struct udev *udev = NULL;
	struct udev_monitor *monitor = NULL;
	GPtrArray *devices = g_ptr_array_new_with_free_func (pedal_device_free);
	guint detected_count = 0;
	int last_error = 0;
	gboolean was_enabled = FALSE;

	for (;;)
	{
		gboolean enabled;
		gboolean stop_requested;
		struct pollfd *poll_fds;
		guint poll_count;
		guint index;
		int poll_result;

		g_mutex_lock (&self->mutex);
		enabled = self->enabled;
		stop_requested = self->stop_requested;
		g_mutex_unlock (&self->mutex);
		if (stop_requested)
			break;

		if (!enabled)
		{
			if (was_enabled)
			{
				g_ptr_array_set_size (devices, 0);
				if (monitor != NULL)
					udev_monitor_unref (monitor);
				monitor = NULL;
				if (udev != NULL)
					udev_unref (udev);
				udev = NULL;
			}
			was_enabled = FALSE;
			publish_state (self,
			               opentrackir_xkeys_derive_state (FALSE, 0, 0, FALSE, 0));
			poll_fds = g_new0 (struct pollfd, 1);
			poll_fds[0].fd = self->wake_fd;
			poll_fds[0].events = POLLIN;
			poll_result = poll (poll_fds, 1, -1);
			g_free (poll_fds);
			if (poll_result > 0)
				drain_wake_fd (self->wake_fd);
			continue;
		}

		if (!was_enabled)
		{
			udev = udev_new ();
			if (udev == NULL)
			{
				publish_state (self,
				               opentrackir_xkeys_derive_state (TRUE, 1, 0, FALSE, ENOMEM));
				poll_fds = g_new0 (struct pollfd, 1);
				poll_fds[0].fd = self->wake_fd;
				poll_fds[0].events = POLLIN;
				poll (poll_fds, 1, -1);
				g_free (poll_fds);
				drain_wake_fd (self->wake_fd);
				continue;
			}
			monitor = udev_monitor_new_from_netlink (udev, "udev");
			if (monitor != NULL)
			{
				udev_monitor_filter_add_match_subsystem_devtype (monitor, "hidraw", NULL);
				if (udev_monitor_enable_receiving (monitor) < 0)
				{
					udev_monitor_unref (monitor);
					monitor = NULL;
				}
			}
			rebuild_devices (self,
			                 udev,
			                 devices,
			                 &detected_count,
			                 &last_error);
			was_enabled = TRUE;
		}

		poll_count = 1 + (monitor != NULL ? 1 : 0) + devices->len;
		poll_fds = g_new0 (struct pollfd, poll_count);
		poll_fds[0].fd = self->wake_fd;
		poll_fds[0].events = POLLIN;
		index = 1;
		if (monitor != NULL)
		{
			poll_fds[index].fd = udev_monitor_get_fd (monitor);
			poll_fds[index].events = POLLIN;
			index += 1;
		}
		for (guint device_index = 0; device_index < devices->len; ++device_index)
		{
			PedalDevice *device = g_ptr_array_index (devices, device_index);

			poll_fds[index + device_index].fd = device->fd;
			poll_fds[index + device_index].events = POLLIN;
		}

		poll_result = poll (poll_fds, poll_count, -1);
		if (poll_result < 0)
		{
			g_free (poll_fds);
			if (errno == EINTR)
				continue;
			last_error = errno;
			publish_devices_state (self, devices, detected_count, last_error);
			continue;
		}
		if ((poll_fds[0].revents & POLLIN) != 0)
			drain_wake_fd (self->wake_fd);
		if (monitor != NULL && (poll_fds[1].revents & POLLIN) != 0)
		{
			struct udev_device *changed_device;

			while ((changed_device = udev_monitor_receive_device (monitor)) != NULL)
				udev_device_unref (changed_device);
			rebuild_devices (self,
			                 udev,
			                 devices,
			                 &detected_count,
			                 &last_error);
		}
		else
		{
			gboolean rebuild = FALSE;
			guint device_offset = 1 + (monitor != NULL ? 1 : 0);

			for (guint device_index = 0; device_index < devices->len; ++device_index)
			{
				PedalDevice *device = g_ptr_array_index (devices, device_index);
				short revents = poll_fds[device_offset + device_index].revents;

				if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
				{
					rebuild = TRUE;
					continue;
				}
				if ((revents & POLLIN) != 0)
				{
					guint8 report[64];
					ssize_t report_size;

					do
					{
						report_size = read (device->fd, report, sizeof report);
					} while (report_size < 0 && errno == EINTR);
					if (report_size > 0)
					{
						device->middle_pressed =
							opentrackir_xkeys_report_is_middle_pressed (
								report,
								(gsize)report_size);
					}
					else if (report_size == 0 || errno != EAGAIN)
					{
						last_error = report_size == 0 ? EIO : errno;
						rebuild = TRUE;
					}
				}
			}
			if (rebuild)
			{
				rebuild_devices (self,
				                 udev,
				                 devices,
				                 &detected_count,
				                 &last_error);
			}
			else
			{
				publish_devices_state (self, devices, detected_count, last_error);
			}
		}
		g_free (poll_fds);
	}

	g_ptr_array_unref (devices);
	if (monitor != NULL)
		udev_monitor_unref (monitor);
	if (udev != NULL)
		udev_unref (udev);
	return NULL;
}

OpentrackirXKeysMonitor *
opentrackir_xkeys_monitor_new (void)
{
	return g_object_new (OPENTRACKIR_TYPE_XKEYS_MONITOR, NULL);
}

void
opentrackir_xkeys_monitor_set_enabled (OpentrackirXKeysMonitor *self,
                                      gboolean                  enabled)
{
	g_return_if_fail (OPENTRACKIR_IS_XKEYS_MONITOR (self));

	g_mutex_lock (&self->mutex);
	if (self->enabled == enabled)
	{
		g_mutex_unlock (&self->mutex);
		return;
	}
	self->enabled = enabled;
	g_mutex_unlock (&self->mutex);
	if (self->wake_fd >= 0)
		wake_monitor (self);
}

OpentrackirXKeysState
opentrackir_xkeys_monitor_get_state (OpentrackirXKeysMonitor *self)
{
	OpentrackirXKeysState state;

	g_return_val_if_fail (OPENTRACKIR_IS_XKEYS_MONITOR (self),
	                      ((OpentrackirXKeysState) {
	                       .phase = OPENTRACKIR_XKEYS_PHASE_FAILED,
	                      }));
	g_mutex_lock (&self->mutex);
	state = self->state;
	g_mutex_unlock (&self->mutex);
	return state;
}

static void
opentrackir_xkeys_monitor_dispose (GObject *object)
{
	OpentrackirXKeysMonitor *self = OPENTRACKIR_XKEYS_MONITOR (object);

	if (self->thread != NULL)
	{
		g_mutex_lock (&self->mutex);
		self->stop_requested = TRUE;
		g_mutex_unlock (&self->mutex);
		if (self->wake_fd >= 0)
			wake_monitor (self);
		g_thread_join (self->thread);
		self->thread = NULL;
	}
	G_OBJECT_CLASS (opentrackir_xkeys_monitor_parent_class)->dispose (object);
}

static void
opentrackir_xkeys_monitor_finalize (GObject *object)
{
	OpentrackirXKeysMonitor *self = OPENTRACKIR_XKEYS_MONITOR (object);

	if (self->wake_fd >= 0)
		close (self->wake_fd);
	g_mutex_clear (&self->mutex);
	G_OBJECT_CLASS (opentrackir_xkeys_monitor_parent_class)->finalize (object);
}

static void
opentrackir_xkeys_monitor_class_init (OpentrackirXKeysMonitorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = opentrackir_xkeys_monitor_dispose;
	object_class->finalize = opentrackir_xkeys_monitor_finalize;
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
}

static void
opentrackir_xkeys_monitor_init (OpentrackirXKeysMonitor *self)
{
	g_mutex_init (&self->mutex);
	self->wake_fd = eventfd (0, EFD_CLOEXEC | EFD_NONBLOCK);
	self->state.phase = self->wake_fd >= 0
		? OPENTRACKIR_XKEYS_PHASE_DISABLED
		: OPENTRACKIR_XKEYS_PHASE_FAILED;
	self->state.error_number = self->wake_fd >= 0 ? 0 : errno;
	if (self->wake_fd >= 0)
		self->thread = g_thread_new ("opentrackir-xkeys", monitor_thread, self);
}
