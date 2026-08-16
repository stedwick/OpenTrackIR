#include "opentrackir-uinput-pointer.h"

#include <errno.h>
#include <linux/input.h>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

struct _OpentrackirUinputPointer
{
	struct libevdev_uinput *device;
	OpentrackirUinputState state;
};

static OpentrackirUinputState
failure_state (int error_number)
{
	return (OpentrackirUinputState) {
		.phase = opentrackir_uinput_phase_for_error (error_number),
		.error_number = error_number < 0 ? -error_number : error_number,
	};
}

static OpentrackirUinputState
write_failure_state (int error_number)
{
	return (OpentrackirUinputState) {
		.phase = OPENTRACKIR_UINPUT_PHASE_WRITE_FAILED,
		.error_number = error_number < 0 ? -error_number : error_number,
	};
}

OpentrackirUinputPointer *
opentrackir_uinput_pointer_new (void)
{
	OpentrackirUinputPointer *self = g_new0 (OpentrackirUinputPointer, 1);

	self->state.phase = OPENTRACKIR_UINPUT_PHASE_DISABLED;
	return self;
}

void
opentrackir_uinput_pointer_free (OpentrackirUinputPointer *self)
{
	if (self == NULL)
		return;

	opentrackir_uinput_pointer_close (self);
	g_free (self);
}

OpentrackirUinputState
opentrackir_uinput_pointer_open (OpentrackirUinputPointer *self)
{
	struct libevdev *definition;
	int result;

	g_return_val_if_fail (self != NULL, failure_state (EINVAL));

	if (self->device != NULL)
		return self->state;

	self->state = (OpentrackirUinputState) {
		.phase = OPENTRACKIR_UINPUT_PHASE_STARTING,
	};
	definition = libevdev_new ();
	if (definition == NULL)
	{
		self->state = failure_state (ENOMEM);
		return self->state;
	}

	libevdev_set_name (definition, "OpenTrackIR Virtual Pointer");
	libevdev_set_id_bustype (definition, BUS_VIRTUAL);
	result = libevdev_enable_property (definition, INPUT_PROP_POINTER);
	/* udev/libinput require a mouse button capability to classify relative
	 * motion as a pointer. OpenTrackIR advertises BTN_LEFT but never emits it. */
	if (result == 0)
		result = libevdev_enable_event_code (definition, EV_KEY, BTN_LEFT, NULL);
	if (result == 0)
		result = libevdev_enable_event_code (definition, EV_REL, REL_X, NULL);
	if (result == 0)
		result = libevdev_enable_event_code (definition, EV_REL, REL_Y, NULL);
	if (result != 0)
	{
		libevdev_free (definition);
		self->state = failure_state (EINVAL);
		return self->state;
	}

	result = libevdev_uinput_create_from_device (definition,
	                                             LIBEVDEV_UINPUT_OPEN_MANAGED,
	                                             &self->device);
	libevdev_free (definition);
	if (result != 0)
	{
		self->device = NULL;
		self->state = failure_state (result);
		return self->state;
	}

	self->state = (OpentrackirUinputState) {
		.phase = OPENTRACKIR_UINPUT_PHASE_READY,
	};
	return self->state;
}

void
opentrackir_uinput_pointer_close (OpentrackirUinputPointer *self)
{
	if (self == NULL)
		return;

	if (self->device != NULL)
	{
		libevdev_uinput_destroy (self->device);
		self->device = NULL;
	}
	self->state = (OpentrackirUinputState) {
		.phase = OPENTRACKIR_UINPUT_PHASE_DISABLED,
	};
}

OpentrackirUinputState
opentrackir_uinput_pointer_post (OpentrackirUinputPointer *self,
                                int                       delta_x,
                                int                       delta_y)
{
	OpentrackirUinputEvent events[3];
	gsize event_count;
	gsize event_index;

	g_return_val_if_fail (self != NULL, failure_state (EINVAL));

	if (self->device == NULL)
		return self->state;

	event_count = opentrackir_uinput_build_event_frame (delta_x,
	                                                    delta_y,
	                                                    events,
	                                                    G_N_ELEMENTS (events));
	for (event_index = 0; event_index < event_count; ++event_index)
	{
		int result = libevdev_uinput_write_event (self->device,
		                                             events[event_index].type,
		                                             events[event_index].code,
		                                             events[event_index].value);
		if (result != 0)
		{
			libevdev_uinput_destroy (self->device);
			self->device = NULL;
			self->state = write_failure_state (result);
			return self->state;
		}
	}

	if (event_count > 0)
		self->state.event_frame_count += 1;
	return self->state;
}

OpentrackirUinputState
opentrackir_uinput_pointer_get_state (OpentrackirUinputPointer *self)
{
	g_return_val_if_fail (self != NULL, failure_state (EINVAL));

	return self->state;
}
