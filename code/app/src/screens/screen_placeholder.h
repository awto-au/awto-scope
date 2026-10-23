#ifndef SCREEN_PLACEHOLDER_H
#define SCREEN_PLACEHOLDER_H

/* Shared helper: builds a titled placeholder screen that pops on any key.
 * first_key_to_ignore: the key code that triggered entry — swallowed so the
 * entry press doesn't immediately pop the screen. */
#include <lvgl.h>
#include "a2k/ui/screen_mgr.h"

typedef struct {
    lv_obj_t *scr;
    uint8_t   ignore_key;   /* swallow this key once on first on_key call */
    bool      armed;        /* true after ignore_key has been swallowed */
} placeholder_ctx_t;

void screen_placeholder_build(placeholder_ctx_t *ctx, const char *title, lv_color_t accent);

/* Call from on_key: returns true if the screen should pop. */
bool screen_placeholder_key(placeholder_ctx_t *ctx, uint8_t key);

#endif /* SCREEN_PLACEHOLDER_H */
