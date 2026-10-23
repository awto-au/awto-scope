#ifndef SCREEN_GENERIC_H
#define SCREEN_GENERIC_H

#include "a2k/ui/screen_mgr.h"
#include <lvgl.h>

typedef struct {
    const char  *title;
    lv_color_t   accent;
    uint8_t      entry_key;
} screen_generic_ctx_t;

extern const a2k_screen_ops_t screen_generic_ops;

#endif /* SCREEN_GENERIC_H */
