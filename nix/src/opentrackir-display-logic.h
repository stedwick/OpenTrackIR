#pragma once

#include <glib.h>

#include "opentrackir-session-state.h"

G_BEGIN_DECLS

gboolean opentrackir_preview_should_run       (gboolean                     camera_enabled,
                                               gboolean                     video_enabled,
                                               gboolean                     window_visible,
                                               OpentrackirSessionPhase       phase);
gboolean opentrackir_preview_should_copy      (gboolean                     preview_enabled,
                                               guint64                      generation,
                                               guint64                      last_generation);
gboolean opentrackir_timeout_should_run       (gboolean                     camera_enabled,
                                               gboolean                     timeout_enabled,
                                               guint                        timeout_seconds);
char    *opentrackir_format_frame_rate        (gboolean                     has_frame_rate,
                                               double                       frame_rate);
char    *opentrackir_format_packet_type       (gboolean                     has_packet_type,
                                               guint8                       packet_type);
char    *opentrackir_format_centroid          (gboolean                     has_centroid,
                                               double                       centroid_x,
                                               double                       centroid_y);

G_END_DECLS
