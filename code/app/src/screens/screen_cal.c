#include "screens/screen_cal.h"
#include "screens/screen_placeholder.h"
#include "a2k/ui/screen_mgr.h"
#include <lvgl.h>

static placeholder_ctx_t s_ctx;

void screen_cal_enter(void *ctx)
{
    s_ctx.ignore_key = (uint8_t)(uintptr_t)ctx;
    screen_placeholder_build(&s_ctx, LV_SYMBOL_SETTINGS "  Settings", lv_color_make(0x40, 0x40, 0x00));
}

void screen_cal_exit(void *ctx)
{
    ARG_UNUSED(ctx);
    if (s_ctx.scr) { lv_obj_delete(s_ctx.scr); s_ctx.scr = NULL; }
}

void screen_cal_key(void *ctx, uint8_t key)
{
    ARG_UNUSED(ctx);
    if (screen_placeholder_key(&s_ctx, key)) { a2k_screen_pop(); }
}

void screen_cal_render(void *ctx) { ARG_UNUSED(ctx); }

const a2k_screen_ops_t screen_cal_ops = {
    .on_enter  = screen_cal_enter,
    .on_exit   = screen_cal_exit,
    .on_key    = screen_cal_key,
    .on_render = screen_cal_render,
};
