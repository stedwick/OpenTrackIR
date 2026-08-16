#pragma once

#include <glib.h>

G_BEGIN_DECLS

#define OPENTRACKIR_XKEYS_VENDOR_ID 0x05f3
#define OPENTRACKIR_XKEYS_PRODUCT_ID_3_PEDAL 0x042c
#define OPENTRACKIR_XKEYS_PRODUCT_ID_12_PEDAL 0x0438
#define OPENTRACKIR_XKEYS_FAST_MULTIPLIER 2.5

typedef enum
{
	OPENTRACKIR_XKEYS_PHASE_DISABLED,
	OPENTRACKIR_XKEYS_PHASE_NOT_DETECTED,
	OPENTRACKIR_XKEYS_PHASE_READY,
	OPENTRACKIR_XKEYS_PHASE_PRESSED,
	OPENTRACKIR_XKEYS_PHASE_PERMISSION_DENIED,
	OPENTRACKIR_XKEYS_PHASE_FAILED,
} OpentrackirXKeysPhase;

typedef struct
{
	OpentrackirXKeysPhase phase;
	int error_number;
	guint device_count;
} OpentrackirXKeysState;

gboolean                 opentrackir_xkeys_device_is_supported (guint16 vendor_id,
                                                                guint16 product_id,
                                                                guint8  interface_number);
gboolean                 opentrackir_xkeys_report_is_middle_pressed
                                                               (const guint8 *report,
                                                                gsize         report_size);
double                   opentrackir_xkeys_effective_speed     (double   base_speed,
                                                                gboolean feature_enabled,
                                                                gboolean middle_pressed);
OpentrackirXKeysState     opentrackir_xkeys_derive_state        (gboolean enabled,
                                                                guint    detected_count,
                                                                guint    opened_count,
                                                                gboolean middle_pressed,
                                                                int      error_number);
gboolean                 opentrackir_xkeys_state_equal         (const OpentrackirXKeysState *left,
                                                                const OpentrackirXKeysState *right);
const char              *opentrackir_xkeys_phase_message       (OpentrackirXKeysPhase phase);

G_END_DECLS
