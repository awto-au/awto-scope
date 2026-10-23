#include "screens/screen_boot.h"
#include "screens/screen_waveform.h"
#include "screens/screen_dmm.h"
#include "screens/screen_awg.h"
#include "screens/screen_menu.h"
#include "screens/screen_cal.h"
#include "screens/screen_generic.h"
#include "a2k/ui/screen_mgr.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(a2k_screen_boot, LOG_LEVEL_INF);

/* Physical layout: 4 keys across × 5 keys down = 20 keys.
 * phys_col (0-3) = row-gpio index, phys_row (0-4) = col-gpio index.
 * key = phys_row * KP_COLS + phys_col + 1 */
#define KP_ROWS  5   /* 5 down  — col-gpio index */
#define KP_COLS  4   /* 4 across — row-gpio index */
#define KP_KEYS  (KP_ROWS * KP_COLS)

/* Pin names for diagnostic display.
 * phys_col (0-3) selects the row-gpio (input pulled low by key contact).
 * phys_row (0-4) selects the col-gpio (scan output driven low by firmware). */
static const char * const PHYS_COL_PINS[KP_COLS] = { "PB6", "PB5", "PC6", "PB12" };   /* row-gpios */
static const char * const PHYS_ROW_PINS[KP_ROWS] = { "PD13", "PD12", "PC3", "PB14", "PB13" }; /* col-gpios */

static lv_obj_t *s_scr;
static lv_obj_t *s_cells[KP_KEYS];   /* one per key, 1-based index = cell[key-1] */
static lv_obj_t *s_info;             /* bottom status line */

/* Key groups for cell colouring:
 *   MODE  : Scope DMM AWG Menu Settings  — light blue bg, white text
 *   DPAD  : ▲▼►◄ Enter                  — black bg, white text
 *   GREY  : everything else              — grey bg, black text */
typedef enum { KG_GREY, KG_MODE, KG_DPAD } key_group_t;

/* ---- per-key navigation table ---- */
typedef struct {
    const a2k_screen_ops_t *ops;  /* NULL = use generic placeholder */
    const char             *title;
    uint8_t r, g, b;             /* accent bar colour for placeholder screen */
    key_group_t             group;
} key_nav_t;

static screen_generic_ctx_t s_gctx[KP_KEYS + 1];

/* Key actions confirmed from fw_scope_frame_tick() decompilation:
 *  1=F1(step/menu)  2=F2(menu-act)  3=F3(save)     4=F4(recall)
 *  5=CH(ch-menu)    6=AWG(awg-mode) 7=DMM(probe)   8=Trig(trig-menu)
 *  9=Mode(ch-sel)   10=▲(inc)       11=▼(dec)       12=Enter/Back
 *  13=Auto(autoset) 14=►(knob-cw)   15=◄(prob/triglvl) 16=CH2/V(sel-volt)
 *  17=BW/Hold       18=Time(sel-tb) 19=TrigLvl(sel-trig) 20=Run/Pause(no scope handler) */
static const key_nav_t s_nav[KP_KEYS + 1] = {
    [0]  = {NULL, NULL,                                        0,0,0,          KG_GREY},
    [1]  = {NULL, "F1\nStep/Menu",                             0x20,0x20,0x20, KG_GREY},
    [2]  = {NULL, "F2\nMenu Act",                              0x20,0x20,0x20, KG_GREY},
    [3]  = {NULL, "F3\nSave",                                  0x20,0x20,0x20, KG_GREY},
    [4]  = {NULL, "F4\nRecall",                                0x20,0x20,0x20, KG_GREY},
    [5]  = {&screen_waveform_ops, NULL,                        0x40,0x80,0xC0, KG_MODE},
    [6]  = {&screen_awg_ops,      NULL,                        0x40,0x80,0xC0, KG_MODE},
    [7]  = {&screen_dmm_ops,      NULL,                        0x40,0x80,0xC0, KG_MODE},
    [8]  = {&screen_menu_ops,     NULL,                        0x40,0x80,0xC0, KG_MODE},
    [9]  = {&screen_cal_ops,      NULL,                        0x40,0x80,0xC0, KG_MODE},
    [10] = {NULL, LV_SYMBOL_UP,                                0x10,0x10,0x10, KG_DPAD},
    [11] = {NULL, LV_SYMBOL_DOWN,                              0x10,0x10,0x10, KG_DPAD},
    [12] = {NULL, "Enter",                                     0x10,0x10,0x10, KG_DPAD},
    [13] = {NULL, "Auto\nAutoset",                             0x20,0x20,0x20, KG_GREY},
    [14] = {NULL, LV_SYMBOL_RIGHT,                             0x10,0x10,0x10, KG_DPAD},
    [15] = {NULL, LV_SYMBOL_LEFT,                              0x10,0x10,0x10, KG_DPAD},
    [16] = {NULL, "Auto\nCH2/V",                               0x20,0x20,0x20, KG_GREY},
    [17] = {NULL, "BW\nHold",                                  0x20,0x20,0x20, KG_GREY},
    [18] = {NULL, "Time\nTimebase",                            0x20,0x20,0x20, KG_GREY},
    [19] = {NULL, "TrigLvl\nTrig Sel",                         0x20,0x20,0x20, KG_GREY},
    [20] = {NULL, LV_SYMBOL_PLAY " " LV_SYMBOL_PAUSE,          0x20,0x20,0x20, KG_GREY},
};

static void nav_key(uint8_t key)
{
    if (key < 1 || key > KP_KEYS) { return; }
    const key_nav_t *n = &s_nav[key];
    if (n->ops) {
        /* dedicated screen — passes entry key as ctx */
        a2k_screen_push(n->ops, (void *)(uintptr_t)key);
    } else if (n->title) {
        /* generic placeholder */
        s_gctx[key].title     = n->title;
        s_gctx[key].accent    = lv_color_make(n->r, n->g, n->b);
        s_gctx[key].entry_key = key;
        a2k_screen_push(&screen_generic_ops, &s_gctx[key]);
    }
}

void screen_boot_enter(void *ctx)
{
    ARG_UNUSED(ctx);

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_make(0x10, 0x10, 0x10), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_scr, 0, LV_PART_MAIN);

    /* Title */
    lv_obj_t *title = lv_label_create(s_scr);
    lv_label_set_text(title, "KEY TEST");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    /* Key grid — 4 rows × 5 cols */
    int top    = 22;
    int bot    = 24;
    int cell_w = LV_HOR_RES / KP_COLS;
    int cell_h = (LV_VER_RES - top - bot) / KP_ROWS;

    /* Labels confirmed from fw_scope_frame_tick() decompilation */
    static const char * const key_labels[KP_KEYS + 1] = {
        "",
        "F1","F2","F3","F4",
        "Scope","AWG","DMM","Trig",
        "Mode",
        LV_SYMBOL_UP, LV_SYMBOL_DOWN, "Enter",
        "Auto", LV_SYMBOL_RIGHT, LV_SYMBOL_LEFT, "CH2/V",
        "BW/Hold", "Time", "TrigLvl",
        LV_SYMBOL_PLAY " " LV_SYMBOL_PAUSE,
    };

    /* D-pad cross zone: keys 10(▲) 11(▼) 14(►) 15(◄) occupy a 2×2 block
     * at grid cols 1-2, rows 2-3.  We draw a single merged zone with 4
     * directional sub-cells arranged as a cross, plus a dark centre. */
    int dpad_x  = 1 * cell_w;                    /* left edge of col 1 */
    int dpad_y  = top + 2 * cell_h;              /* top edge of row 2 */
    int dpad_w  = 2 * cell_w;
    int dpad_h  = 2 * cell_h;
    int arm_w   = dpad_w / 3;
    int arm_h   = dpad_h / 3;
    lv_color_t dpad_bg  = lv_color_make(0x08, 0x08, 0x08);
    lv_color_t dpad_arm = lv_color_make(0x28, 0x28, 0x28);
    lv_color_t dpad_txt = lv_color_white();

    /* background square */
    lv_obj_t *dpad_zone = lv_obj_create(s_scr);
    lv_obj_set_pos(dpad_zone, dpad_x + 1, dpad_y + 1);
    lv_obj_set_size(dpad_zone, dpad_w - 2, dpad_h - 2);
    lv_obj_set_style_bg_color(dpad_zone, dpad_bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dpad_zone, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(dpad_zone, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(dpad_zone, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dpad_zone, 0, LV_PART_MAIN);

    /* Helper: create one cross-arm cell relative to dpad_zone */
    struct { int key; int x; int y; int w; int h; } arms[4] = {
        {10, (dpad_w-arm_w)/2-1, 1,           arm_w, arm_h},  /* ▲ top */
        {11, (dpad_w-arm_w)/2-1, dpad_h-arm_h-3, arm_w, arm_h},  /* ▼ bottom */
        {15, 1,                  (dpad_h-arm_h)/2-1, arm_w, arm_h},  /* ◄ left */
        {14, dpad_w-arm_w-3,     (dpad_h-arm_h)/2-1, arm_w, arm_h},  /* ► right */
    };
    for (int i = 0; i < 4; i++) {
        int k = arms[i].key;
        lv_obj_t *arm = lv_obj_create(dpad_zone);
        lv_obj_set_pos(arm, arms[i].x, arms[i].y);
        lv_obj_set_size(arm, arms[i].w - 2, arms[i].h - 2);
        lv_obj_set_style_bg_color(arm, dpad_arm, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(arm, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(arm, 2, LV_PART_MAIN);
        lv_obj_set_style_border_width(arm, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(arm, 0, LV_PART_MAIN);
        lv_obj_t *al = lv_label_create(arm);
        lv_label_set_text(al, key_labels[k]);
        lv_obj_set_style_text_color(al, dpad_txt, LV_PART_MAIN);
        lv_obj_center(al);
        s_cells[k - 1] = arm;
    }
    /* centre dot — no key */
    lv_obj_t *ctr = lv_obj_create(dpad_zone);
    lv_obj_set_pos(ctr, (dpad_w-arm_w)/2-1, (dpad_h-arm_h)/2-1);
    lv_obj_set_size(ctr, arm_w-2, arm_h-2);
    lv_obj_set_style_bg_color(ctr, lv_color_make(0x18,0x18,0x18), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ctr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ctr, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(ctr, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ctr, 0, LV_PART_MAIN);

    /* All other keys as normal cells — skip the d-pad zone (rows 2-3, cols 1-2) */
    for (int r = 0; r < KP_ROWS; r++) {
        for (int c = 0; c < KP_COLS; c++) {
            int key = r * KP_COLS + c + 1;
            /* skip cells covered by the d-pad cross */
            if ((r == 2 || r == 3) && (c == 1 || c == 2)) { continue; }

            lv_obj_t *cell = lv_obj_create(s_scr);
            lv_obj_set_pos(cell, c * cell_w + 1, top + r * cell_h + 1);
            lv_obj_set_size(cell, cell_w - 2, cell_h - 2);

            key_group_t grp = (key <= KP_KEYS) ? s_nav[key].group : KG_GREY;
            lv_color_t bg_col, txt_col;
            if (grp == KG_MODE) {
                bg_col  = lv_color_make(0x60, 0xA0, 0xD0);
                txt_col = lv_color_white();
            } else if (grp == KG_DPAD) {
                bg_col  = lv_color_make(0x08, 0x08, 0x08);
                txt_col = lv_color_white();
            } else {
                bg_col  = lv_color_make(0xB0, 0xB0, 0xB0);
                txt_col = lv_color_black();
            }
            lv_obj_set_style_bg_color(cell, bg_col, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(cell, 3, LV_PART_MAIN);
            lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN);

            lv_obj_t *lbl = lv_label_create(cell);
            lv_label_set_text(lbl, key_labels[key]);
            lv_obj_set_style_text_color(lbl, txt_col, LV_PART_MAIN);
            lv_obj_center(lbl);

            s_cells[key - 1] = cell;
        }
    }

    /* Info line at bottom */
    s_info = lv_label_create(s_scr);
    lv_label_set_text(s_info, "press a key");
    lv_obj_set_style_text_color(s_info, lv_color_make(0xA0, 0xA0, 0xA0), LV_PART_MAIN);
    lv_obj_align(s_info, LV_ALIGN_BOTTOM_MID, 0, -4);

    lv_screen_load(s_scr);
    lv_refr_now(NULL);
    LOG_INF("screen_boot: key test screen ready");
}

void screen_boot_exit(void *ctx)
{
    ARG_UNUSED(ctx);
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
}

static void cell_reset_cb(lv_timer_t *t)
{
    lv_obj_t *cell = (lv_obj_t *)lv_timer_get_user_data(t);
    lv_obj_set_style_bg_color(cell, lv_color_make(0x30, 0x30, 0x30), LV_PART_MAIN);
}

void screen_boot_key(void *ctx, uint8_t key)
{
    ARG_UNUSED(ctx);
    if (key < 1 || key > KP_KEYS) { return; }

    int phys_col = (key - 1) % KP_COLS;
    int phys_row = (key - 1) / KP_COLS;

    LOG_INF("KEY %-2u  scan=%s(col-gpio)  input=%s(row-gpio)",
        key, PHYS_ROW_PINS[phys_row], PHYS_COL_PINS[phys_col]);

    /* Navigate first — before touching LVGL widgets so on_exit/on_enter
     * can safely delete/rebuild the screen tree. */
    nav_key(key);
}

void screen_boot_render(void *ctx)
{
    ARG_UNUSED(ctx);
}

const a2k_screen_ops_t screen_boot_ops = {
    .on_enter  = screen_boot_enter,
    .on_exit   = screen_boot_exit,
    .on_key    = screen_boot_key,
    .on_render = screen_boot_render,
};
