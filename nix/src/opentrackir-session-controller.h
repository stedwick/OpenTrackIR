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
void                          opentrackir_session_controller_set_video_enabled
                                                                      (OpentrackirSessionController *self,
                                                                       gboolean                      enabled);
void                          opentrackir_session_controller_set_tracking_frames_per_second
                                                                      (OpentrackirSessionController *self,
                                                                       double                        frames_per_second);
void                          opentrackir_session_controller_set_minimum_blob_area_points
                                                                      (OpentrackirSessionController *self,
                                                                       int                           minimum_blob_area_points);
void                          opentrackir_session_controller_set_centroid_mode
                                                                      (OpentrackirSessionController *self,
                                                                       otir_tir5v3_centroid_mode     mode);
gboolean                      opentrackir_session_controller_copy_preview_frame
                                                                      (OpentrackirSessionController *self,
                                                                       guint8                        *frame,
                                                                       gsize                          capacity,
                                                                       guint64                       *generation);

G_END_DECLS
