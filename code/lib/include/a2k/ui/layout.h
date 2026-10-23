#ifndef A2K_UI_LAYOUT_H
#define A2K_UI_LAYOUT_H

#include <stdint.h>

typedef struct {
    uint16_t w, h;
    uint16_t status_h;
    uint16_t softkey_h;
    uint16_t overview_h;
    uint16_t wave_y0;
    uint16_t wave_h;
    uint16_t hdiv;
    uint16_t vdiv;
} a2k_layout_t;

void a2k_layout_init(a2k_layout_t *L, uint16_t w, uint16_t h);
void a2k_layout_tiles(const a2k_layout_t *L, int n_active, /* lv_area_t *rects_out — add lvgl dep later */ void *rects_out);

#endif /* A2K_UI_LAYOUT_H */
