#include "screens/screen_waveform.h"
#include "a2k/ui/screen_mgr.h"
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(a2k_screen_waveform, LOG_LEVEL_INF);

/* ── Layout (320×240) ─────────────────────────────────────────────────────
 *  Y  0.. 19  top bar:  [CH1 badge][CH2 badge][  timebase  ][trig info]
 *  Y 20.. 21  separator line
 *  Y 22..209  graticule  12 div × 8 div  (188px tall, 300px wide, X=10..309)
 *  Y 210..211 separator line
 *  Y 212..239 measurement bar: meas A | meas B | status
 * ────────────────────────────────────────────────────────────────────────*/

#define SCR_W        320
#define SCR_H        240

#define BAR_H        20
#define SEP_H         2
#define MEAS_H       28

#define GRAT_X        10
#define GRAT_Y       (BAR_H + SEP_H)
#define GRAT_W       300
#define GRAT_H       (SCR_H - BAR_H - SEP_H - SEP_H - MEAS_H)   /* 188 */

/* Fluke palette (RGB565 via lv_color_make) */
#define COL_BG       lv_color_make(0x10,0x10,0x10)
#define COL_TOPBAR   lv_color_make(0x18,0x18,0x18)
#define COL_MEASBAR  lv_color_make(0x18,0x18,0x18)
#define COL_GRAT_BG  lv_color_make(0x08,0x08,0x08)
#define COL_GRID     lv_color_make(0x30,0x30,0x30)
#define COL_BORDER   lv_color_make(0xC0,0xC0,0xC0)
#define COL_CH1      lv_color_make(0xFF,0xE0,0x00)   /* yellow */
#define COL_CH2      lv_color_make(0x00,0xE0,0x40)   /* green  */
#define COL_TRIG     lv_color_make(0xFF,0x40,0xFF)   /* magenta */
#define COL_WHITE    lv_color_white()
#define COL_DIM      lv_color_make(0x60,0x60,0x60)
#define COL_SEP      lv_color_make(0x50,0x50,0x50)

static lv_obj_t *s_scr;

/* ── helper: thin horizontal rule ───────────────────────────────────────*/
static void make_sep(lv_obj_t *parent, int y)
{
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_pos(sep, 0, y);
    lv_obj_set_size(sep, SCR_W, SEP_H);
    lv_obj_set_style_bg_color(sep, COL_SEP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sep, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sep, 0, LV_PART_MAIN);
}

/* ── helper: flat label ─────────────────────────────────────────────────*/
static lv_obj_t *make_label(lv_obj_t *parent, const char *txt,
                             lv_color_t col, lv_align_t align,
                             int ox, int oy)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, col, LV_PART_MAIN);
    lv_obj_align(l, align, ox, oy);
    return l;
}

/* ── helper: channel badge ──────────────────────────────────────────────*/
static void make_ch_badge(lv_obj_t *parent, const char *ch, const char *vdiv,
                           const char *coup, lv_color_t col, int x)
{
    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_set_pos(badge, x, 1);
    lv_obj_set_size(badge, 72, BAR_H - 2);
    lv_obj_set_style_bg_color(badge, lv_color_make(0x22,0x22,0x22), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(badge, col, LV_PART_MAIN);
    lv_obj_set_style_border_width(badge, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(badge, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(badge, 1, LV_PART_MAIN);

    /* "CH1" label left */
    lv_obj_t *l1 = lv_label_create(badge);
    lv_label_set_text(l1, ch);
    lv_obj_set_style_text_color(l1, col, LV_PART_MAIN);
    lv_obj_align(l1, LV_ALIGN_LEFT_MID, 2, 0);

    /* V/div centre */
    lv_obj_t *l2 = lv_label_create(badge);
    lv_label_set_text(l2, vdiv);
    lv_obj_set_style_text_color(l2, COL_WHITE, LV_PART_MAIN);
    lv_obj_align(l2, LV_ALIGN_CENTER, 4, 0);

    /* coupling right */
    lv_obj_t *l3 = lv_label_create(badge);
    lv_label_set_text(l3, coup);
    lv_obj_set_style_text_color(l3, COL_DIM, LV_PART_MAIN);
    lv_obj_align(l3, LV_ALIGN_RIGHT_MID, -2, 0);
}

/* ── helper: draw grid as individual dot/line objects ───────────────────*/
static void make_graticule(lv_obj_t *parent)
{
    /* Background */
    lv_obj_t *bg = lv_obj_create(parent);
    lv_obj_set_pos(bg, GRAT_X, GRAT_Y);
    lv_obj_set_size(bg, GRAT_W, GRAT_H);
    lv_obj_set_style_bg_color(bg, COL_GRAT_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(bg, COL_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bg, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bg, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN);

    /* 12×8 grid — major divisions, drawn as short tick marks on axes.
     * Fluke style: centre cross + short ticks on the axes, not full grid lines */
    int div_x = GRAT_W / 12;   /* 25 px */
    int div_y = GRAT_H / 8;    /* 23 px (close enough) */
    int cx = GRAT_W / 2;
    int cy = GRAT_H / 2;

    /* centre cross — horizontal line at cy, vertical line at cx */
    static lv_point_precise_t hline[2], vline[2];
    hline[0].x = 0;       hline[0].y = (lv_value_precise_t)cy;
    hline[1].x = GRAT_W;  hline[1].y = (lv_value_precise_t)cy;
    lv_obj_t *hl = lv_line_create(bg);
    lv_line_set_points(hl, hline, 2);
    lv_obj_set_style_line_color(hl, COL_GRID, LV_PART_MAIN);
    lv_obj_set_style_line_width(hl, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(hl, LV_OPA_COVER, LV_PART_MAIN);

    vline[0].x = (lv_value_precise_t)cx;  vline[0].y = 0;
    vline[1].x = (lv_value_precise_t)cx;  vline[1].y = GRAT_H;
    lv_obj_t *vl = lv_line_create(bg);
    lv_line_set_points(vl, vline, 2);
    lv_obj_set_style_line_color(vl, COL_GRID, LV_PART_MAIN);
    lv_obj_set_style_line_width(vl, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(vl, LV_OPA_COVER, LV_PART_MAIN);

    /* stub sine wave CH1 — 2 cycles via lv_line (single widget, static pts) */
    static const int8_t sin_lut[64] = {
         0,  5, 11, 16, 21, 26, 31, 35, 39, 42, 45, 48, 50, 52, 54, 54,
        55, 54, 54, 52, 50, 48, 45, 42, 39, 35, 31, 26, 21, 16, 11,  5,
         0, -5,-11,-16,-21,-26,-31,-35,-39,-42,-45,-48,-50,-52,-54,-54,
       -55,-54,-54,-52,-50,-48,-45,-42,-39,-35,-31,-26,-21,-16,-11, -5,
    };
    /* 64 points spaced across GRAT_W, static so it lives in BSS not stack */
    static lv_point_precise_t wave_pts[64];
    for (int i = 0; i < 64; i++) {
        wave_pts[i].x = (lv_value_precise_t)(i * GRAT_W / 64);
        int yi = cy - 30 - sin_lut[i];
        if (yi < 2) { yi = 2; }
        if (yi > GRAT_H - 4) { yi = GRAT_H - 4; }
        wave_pts[i].y = (lv_value_precise_t)yi;
    }
    lv_obj_t *wave = lv_line_create(bg);
    lv_line_set_points(wave, wave_pts, 64);
    lv_obj_set_style_line_color(wave, COL_CH1, LV_PART_MAIN);
    lv_obj_set_style_line_width(wave, 1, LV_PART_MAIN);
    lv_obj_set_style_line_opa(wave, LV_OPA_COVER, LV_PART_MAIN);

    /* trigger level marker — magenta T on left border */
    lv_obj_t *trig = lv_label_create(bg);
    lv_label_set_text(trig, "T");
    lv_obj_set_style_text_color(trig, COL_TRIG, LV_PART_MAIN);
    lv_obj_align(trig, LV_ALIGN_LEFT_MID, 2, -30);

    /* CH1 ground marker on left */
    lv_obj_t *gnd1 = lv_label_create(bg);
    lv_label_set_text(gnd1, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_color(gnd1, COL_CH1, LV_PART_MAIN);
    lv_obj_align(gnd1, LV_ALIGN_LEFT_MID, 2, -30 + 10);
}

void screen_waveform_enter(void *ctx)
{
    ARG_UNUSED(ctx);

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_scr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_scr, 0, LV_PART_MAIN);

    /* ── top bar ───────────────────────────────────────────────────────*/
    lv_obj_t *topbar = lv_obj_create(s_scr);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_size(topbar, SCR_W, BAR_H);
    lv_obj_set_style_bg_color(topbar, COL_TOPBAR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(topbar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(topbar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(topbar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(topbar, 0, LV_PART_MAIN);

    make_ch_badge(topbar, "CH1", "1.00V", "DC", COL_CH1,   2);
    make_ch_badge(topbar, "CH2", "500m",  "AC", COL_CH2,  78);

    /* timebase centre */
    make_label(topbar, "500us/div", COL_WHITE,  LV_ALIGN_CENTER,  0, 0);

    /* trigger info right */
    make_label(topbar, "T DC " LV_SYMBOL_UP, COL_TRIG, LV_ALIGN_RIGHT_MID, -4, 0);

    /* ── separators ────────────────────────────────────────────────────*/
    make_sep(s_scr, BAR_H);
    make_sep(s_scr, BAR_H + SEP_H + GRAT_H);

    /* ── graticule ─────────────────────────────────────────────────────*/
    make_graticule(s_scr);

    /* ── measurement bar ───────────────────────────────────────────────*/
    int meas_y = BAR_H + SEP_H + GRAT_H + SEP_H;
    lv_obj_t *measbar = lv_obj_create(s_scr);
    lv_obj_set_pos(measbar, 0, meas_y);
    lv_obj_set_size(measbar, SCR_W, MEAS_H);
    lv_obj_set_style_bg_color(measbar, COL_MEASBAR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(measbar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(measbar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(measbar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(measbar, 2, LV_PART_MAIN);

    /* CH1 measurements left */
    lv_obj_t *m1 = lv_label_create(measbar);
    lv_label_set_text(m1, "Vpp  2.14V\nFreq 1.00k");
    lv_obj_set_style_text_color(m1, COL_CH1, LV_PART_MAIN);
    lv_obj_align(m1, LV_ALIGN_LEFT_MID, 4, 0);

    /* divider */
    lv_obj_t *div = lv_obj_create(measbar);
    lv_obj_set_pos(div, SCR_W/2 - 1, 2);
    lv_obj_set_size(div, 1, MEAS_H - 4);
    lv_obj_set_style_bg_color(div, COL_SEP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(div, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(div, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(div, 0, LV_PART_MAIN);

    /* status right — run indicator */
    lv_obj_t *m2 = lv_label_create(measbar);
    lv_label_set_text(m2, LV_SYMBOL_PLAY " RUN\nAuto");
    lv_obj_set_style_text_color(m2, lv_color_make(0x00,0xFF,0x00), LV_PART_MAIN);
    lv_obj_align(m2, LV_ALIGN_RIGHT_MID, -4, 0);

    lv_screen_load(s_scr);
    LOG_INF("screen_waveform: entered");
}

void screen_waveform_exit(void *ctx)
{
    ARG_UNUSED(ctx);
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
}

void screen_waveform_key(void *ctx, uint8_t key)
{
    ARG_UNUSED(ctx);
    ARG_UNUSED(key);
    /* any key returns to launcher for now */
    a2k_screen_pop();
}

void screen_waveform_render(void *ctx) { ARG_UNUSED(ctx); }

const a2k_screen_ops_t screen_waveform_ops = {
    .on_enter  = screen_waveform_enter,
    .on_exit   = screen_waveform_exit,
    .on_key    = screen_waveform_key,
    .on_render = screen_waveform_render,
};
