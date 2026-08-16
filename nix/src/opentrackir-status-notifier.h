#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define OPENTRACKIR_TYPE_STATUS_NOTIFIER (opentrackir_status_notifier_get_type ())

G_DECLARE_FINAL_TYPE (OpentrackirStatusNotifier, opentrackir_status_notifier, OPENTRACKIR, STATUS_NOTIFIER, GObject)

OpentrackirStatusNotifier *opentrackir_status_notifier_new          (GApplication              *application);
gboolean                   opentrackir_status_notifier_is_available (OpentrackirStatusNotifier *self);
void                       opentrackir_status_notifier_update       (OpentrackirStatusNotifier *self,
                                                                    gboolean                   visible,
                                                                    gboolean                   window_visible,
                                                                    gboolean                   camera_enabled,
                                                                    gboolean                   mouse_enabled);

G_END_DECLS
