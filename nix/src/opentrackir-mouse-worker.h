#pragma once

#include <glib.h>
#include <opentrackir/tir5_mouse.h>
#include <opentrackir/tir5_session.h>

#include "opentrackir-uinput-policy.h"

G_BEGIN_DECLS

typedef struct _OpentrackirMouseWorker OpentrackirMouseWorker;

OpentrackirMouseWorker *opentrackir_mouse_worker_new          (otir_trackir_session              *session);
void                    opentrackir_mouse_worker_free         (OpentrackirMouseWorker            *self);
void                    opentrackir_mouse_worker_set_config   (OpentrackirMouseWorker            *self,
                                                               gboolean                           enabled,
                                                               otir_trackir_mouse_tracker_config  config);
OpentrackirUinputState   opentrackir_mouse_worker_get_state    (OpentrackirMouseWorker            *self);

G_END_DECLS
