#include "opentrackir-xkeys-policy.h"

#include <errno.h>
#include <glib/gi18n.h>

#define MIDDLE_PEDAL_MASK 0x04

gboolean
opentrackir_xkeys_device_is_supported (guint16 vendor_id,
                                       guint16 product_id,
                                       guint8  interface_number)
{
	return vendor_id == OPENTRACKIR_XKEYS_VENDOR_ID &&
	       (product_id == OPENTRACKIR_XKEYS_PRODUCT_ID_3_PEDAL ||
	        product_id == OPENTRACKIR_XKEYS_PRODUCT_ID_12_PEDAL) &&
	       interface_number == 0;
}

gboolean
opentrackir_xkeys_report_is_middle_pressed (const guint8 *report,
                                            gsize         report_size)
{
	static const gsize observed_offsets[] = { 3, 1, 2 };
	gsize offset_index;

	if (report == NULL)
		return FALSE;
	for (offset_index = 0; offset_index < G_N_ELEMENTS (observed_offsets); ++offset_index)
	{
		gsize report_index = observed_offsets[offset_index];

		if (report_index < report_size &&
		    (report[report_index] & MIDDLE_PEDAL_MASK) != 0)
			return TRUE;
	}
	return FALSE;
}

double
opentrackir_xkeys_effective_speed (double   base_speed,
                                  gboolean feature_enabled,
                                  gboolean middle_pressed)
{
	return feature_enabled && middle_pressed
		? base_speed * OPENTRACKIR_XKEYS_FAST_MULTIPLIER
		: base_speed;
}

OpentrackirXKeysState
opentrackir_xkeys_derive_state (gboolean enabled,
                                guint    detected_count,
                                guint    opened_count,
                                gboolean middle_pressed,
                                int      error_number)
{
	OpentrackirXKeysPhase phase;

	if (!enabled)
		phase = OPENTRACKIR_XKEYS_PHASE_DISABLED;
	else if (opened_count > 0 && middle_pressed)
		phase = OPENTRACKIR_XKEYS_PHASE_PRESSED;
	else if (opened_count > 0)
		phase = OPENTRACKIR_XKEYS_PHASE_READY;
	else if (detected_count > 0 && (error_number == EACCES || error_number == EPERM))
		phase = OPENTRACKIR_XKEYS_PHASE_PERMISSION_DENIED;
	else if (detected_count == 0)
		phase = OPENTRACKIR_XKEYS_PHASE_NOT_DETECTED;
	else
		phase = OPENTRACKIR_XKEYS_PHASE_FAILED;

	return (OpentrackirXKeysState) {
		.phase = phase,
		.error_number = error_number,
		.device_count = opened_count,
	};
}

gboolean
opentrackir_xkeys_state_equal (const OpentrackirXKeysState *left,
                               const OpentrackirXKeysState *right)
{
	return left != NULL && right != NULL &&
	       left->phase == right->phase &&
	       left->error_number == right->error_number &&
	       left->device_count == right->device_count;
}

const char *
opentrackir_xkeys_phase_message (OpentrackirXKeysPhase phase)
{
	switch (phase)
	{
	case OPENTRACKIR_XKEYS_PHASE_DISABLED:
		return _("Disabled");
	case OPENTRACKIR_XKEYS_PHASE_NOT_DETECTED:
		return _("No supported X-keys pedal detected.");
	case OPENTRACKIR_XKEYS_PHASE_READY:
		return _("Ready · hold the middle pedal for 2.5× speed.");
	case OPENTRACKIR_XKEYS_PHASE_PRESSED:
		return _("Fast mode · 2.5× speed");
	case OPENTRACKIR_XKEYS_PHASE_PERMISSION_DENIED:
		return _("Access denied · install the OpenTrackIR X-keys udev rule and reconnect the pedal.");
	case OPENTRACKIR_XKEYS_PHASE_FAILED:
	default:
		return _("The X-keys pedal could not be read.");
	}
}
