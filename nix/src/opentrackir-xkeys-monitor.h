#pragma once

#include <glib-object.h>

#include "opentrackir-xkeys-policy.h"

G_BEGIN_DECLS

#define OPENTRACKIR_TYPE_XKEYS_MONITOR (opentrackir_xkeys_monitor_get_type ())

G_DECLARE_FINAL_TYPE (OpentrackirXKeysMonitor, opentrackir_xkeys_monitor, OPENTRACKIR, XKEYS_MONITOR, GObject)

OpentrackirXKeysMonitor *opentrackir_xkeys_monitor_new         (void);
void                     opentrackir_xkeys_monitor_set_enabled (OpentrackirXKeysMonitor *self,
                                                                gboolean                  enabled);
OpentrackirXKeysState     opentrackir_xkeys_monitor_get_state  (OpentrackirXKeysMonitor *self);

G_END_DECLS
