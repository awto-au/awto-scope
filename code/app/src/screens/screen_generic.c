#include "screens/screen_generic.h"
#include "screens/screen_placeholder.h"
#include "a2k/ui/screen_mgr.h"
#include <lvgl.h>

static placeholder_ctx_t s_ph;

static void screen_generic_enter(void *ctx)
{
    screen_generic_ctx_t *g = (screen_generic_ctx_t *)ctx;
    s_ph.ignore_key = g->entry_key;
    screen_placeholder_build(&s_ph, g->title, g->accent);
}

static void screen_generic_exit(void *ctx)
{
    ARG_UNUSED(ctx);
    if (s_ph.scr) { lv_obj_delete(s_ph.scr); s_ph.scr = NULL; }
}

static void screen_generic_key(void *ctx, uint8_t key)
{
    ARG_UNUSED(ctx);
    if (screen_placeholder_key(&s_ph, key)) { a2k_screen_pop(); }
}

static void screen_generic_render(void *ctx) { ARG_UNUSED(ctx); }

const a2k_screen_ops_t screen_generic_ops = {
    .on_enter  = screen_generic_enter,
    .on_exit   = screen_generic_exit,
    .on_key    = screen_generic_key,
    .on_render = screen_generic_render,
};
