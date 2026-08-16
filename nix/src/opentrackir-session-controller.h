#pragma once

#include <glib-object.h>

#include "opentrackir-session-state.h"

G_BEGIN_DECLS

#define OPENTRACKIR_TYPE_SESSION_CONTROLLER (opentrackir_session_controller_get_type ())

G_DECLARE_FINAL_TYPE (OpentrackirSessionController, opentrackir_session_controller, OPENTRACKIR, SESSION_CONTROLLER, GObject)

OpentrackirSessionController *opentrackir_session_controller_new       (void);
const OpentrackirSessionState *opentrackir_session_controller_get_state (OpentrackirSessionController *self);
gboolean                      opentrackir_session_controller_can_start (OpentrackirSessionController *self);
gboolean                      opentrackir_session_controller_can_stop  (OpentrackirSessionController *self);
void                          opentrackir_session_controller_start     (OpentrackirSessionController *self);
void                          opentrackir_session_controller_stop      (OpentrackirSessionController *self);

G_END_DECLS
