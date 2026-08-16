#pragma once

#include <glib-object.h>

#include "opentrackir-session-state.h"
#include "opentrackir-uinput-policy.h"

G_BEGIN_DECLS

#define OPENTRACKIR_TYPE_SESSION_CONTROLLER (opentrackir_session_controller_get_type ())

G_DECLARE_FINAL_TYPE (OpentrackirSessionController, opentrackir_session_controller, OPENTRACKIR, SESSION_CONTROLLER, GObject)

OpentrackirSessionController *opentrackir_session_controller_new       (void);
const OpentrackirSessionState *opentrackir_session_controller_get_state (OpentrackirSessionController *self);
const OpentrackirUinputState *opentrackir_session_controller_get_mouse_state
                                                                      (OpentrackirSessionController *self);
gboolean                      opentrackir_session_controller_can_start (OpentrackirSessionController *self);
gboolean                      opentrackir_session_controller_can_stop  (OpentrackirSessionController *self);
void                          opentrackir_session_controller_start     (OpentrackirSessionController *self);
void                          opentrackir_session_controller_stop      (OpentrackirSessionController *self);
void                          opentrackir_session_controller_set_video_enabled
                                                                      (OpentrackirSessionController *self,
                                                                       gboolean                      enabled);
void                          opentrackir_session_controller_set_low_power_enabled
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
void                          opentrackir_session_controller_set_mouse_config
                                                                      (OpentrackirSessionController       *self,
                                                                       gboolean                            movement_enabled,
                                                                       gboolean                            camera_enabled,
                                                                       guint                               keep_awake_seconds,
                                                                       otir_trackir_mouse_tracker_config   config);
gboolean                      opentrackir_session_controller_copy_preview_frame
                                                                      (OpentrackirSessionController *self,
                                                                       guint8                        *frame,
                                                                       gsize                          capacity,
                                                                       guint64                       *generation);

G_END_DECLS
