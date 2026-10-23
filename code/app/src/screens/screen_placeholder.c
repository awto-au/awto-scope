#include "screens/screen_placeholder.h"
#include <lvgl.h>

void screen_placeholder_build(placeholder_ctx_t *ctx, const char *title, lv_color_t accent)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_make(0x10, 0x10, 0x10), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);

    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, LV_HOR_RES, 28);
    lv_obj_set_style_bg_color(bar, accent, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);

    lv_obj_t *title_lbl = lv_label_create(bar);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(title_lbl);

    lv_obj_t *stub = lv_label_create(scr);
    lv_label_set_text(stub, "[ stub ]");
    lv_obj_set_style_text_color(stub, lv_color_make(0x60, 0x60, 0x60), LV_PART_MAIN);
    lv_obj_align(stub, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " press key again to go back");
    lv_obj_set_style_text_color(hint, lv_color_make(0x50, 0x50, 0x50), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    lv_screen_load(scr);
    ctx->scr = scr;
    ctx->armed = false;
}

bool screen_placeholder_key(placeholder_ctx_t *ctx, uint8_t key)
{
    if (!ctx->armed && key == ctx->ignore_key) {
        ctx->armed = true;
        return false;   /* swallow entry key */
    }
    return true;        /* any subsequent key → pop */
}
