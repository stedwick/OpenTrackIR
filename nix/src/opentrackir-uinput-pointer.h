#pragma once

#include "opentrackir-uinput-policy.h"

G_BEGIN_DECLS

typedef struct _OpentrackirUinputPointer OpentrackirUinputPointer;

OpentrackirUinputPointer *opentrackir_uinput_pointer_new       (void);
void                      opentrackir_uinput_pointer_free      (OpentrackirUinputPointer *self);
OpentrackirUinputState     opentrackir_uinput_pointer_open      (OpentrackirUinputPointer *self);
void                       opentrackir_uinput_pointer_close     (OpentrackirUinputPointer *self);
OpentrackirUinputState     opentrackir_uinput_pointer_post      (OpentrackirUinputPointer *self,
                                                                int                       delta_x,
                                                                int                       delta_y);
OpentrackirUinputState     opentrackir_uinput_pointer_get_state (OpentrackirUinputPointer *self);

G_END_DECLS
