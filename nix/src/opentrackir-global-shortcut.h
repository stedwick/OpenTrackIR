#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define OPENTRACKIR_TYPE_GLOBAL_SHORTCUT (opentrackir_global_shortcut_get_type ())

G_DECLARE_FINAL_TYPE (OpentrackirGlobalShortcut, opentrackir_global_shortcut, OPENTRACKIR, GLOBAL_SHORTCUT, GObject)

typedef enum
{
	OPENTRACKIR_GLOBAL_SHORTCUT_UNAVAILABLE,
	OPENTRACKIR_GLOBAL_SHORTCUT_REGISTERING,
	OPENTRACKIR_GLOBAL_SHORTCUT_READY,
	OPENTRACKIR_GLOBAL_SHORTCUT_FAILED,
} OpentrackirGlobalShortcutState;

const char                    *opentrackir_global_shortcut_preferred_trigger (void);
gboolean                       opentrackir_global_shortcut_activation_matches
                                                                             (const char *expected_session,
                                                                              const char *actual_session,
                                                                              const char *shortcut_id);
OpentrackirGlobalShortcut     *opentrackir_global_shortcut_new               (GActionGroup *actions);
void                           opentrackir_global_shortcut_start             (OpentrackirGlobalShortcut *self);
OpentrackirGlobalShortcutState opentrackir_global_shortcut_get_state         (OpentrackirGlobalShortcut *self);
const char                    *opentrackir_global_shortcut_get_description   (OpentrackirGlobalShortcut *self);

G_END_DECLS
