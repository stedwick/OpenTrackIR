#pragma once

#include <glib.h>

G_BEGIN_DECLS

GVariant *opentrackir_status_notifier_icon_pixmaps_new (const guint8 *argb32,
                                                         gint          width,
                                                         gint          height,
                                                         gsize         stride);

G_END_DECLS
