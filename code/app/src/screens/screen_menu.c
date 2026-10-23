#include "screens/screen_menu.h"
#include "screens/screen_placeholder.h"
#include "a2k/ui/screen_mgr.h"
#include <lvgl.h>

static placeholder_ctx_t s_ctx;

void screen_menu_enter(void *ctx)
{
    s_ctx.ignore_key = (uint8_t)(uintptr_t)ctx;
    screen_placeholder_build(&s_ctx, LV_SYMBOL_LIST "  Menu", lv_color_make(0x20, 0x20, 0x60));
}

void screen_menu_exit(void *ctx)
{
    ARG_UNUSED(ctx);
    if (s_ctx.scr) { lv_obj_delete(s_ctx.scr); s_ctx.scr = NULL; }
}

void screen_menu_key(void *ctx, uint8_t key)
{
    ARG_UNUSED(ctx);
    if (screen_placeholder_key(&s_ctx, key)) { a2k_screen_pop(); }
}

void screen_menu_render(void *ctx) { ARG_UNUSED(ctx); }

const a2k_screen_ops_t screen_menu_ops = {
    .on_enter  = screen_menu_enter,
    .on_exit   = screen_menu_exit,
    .on_key    = screen_menu_key,
    .on_render = screen_menu_render,
};
