#ifndef A2K_BOARD_UI_LAYOUT_H
#define A2K_BOARD_UI_LAYOUT_H

#include <stdint.h>
#include <zephyr/drivers/display.h>

/* All pixel geometry derived from display capabilities at runtime.
 * No hardcoded pixel values anywhere except inside a2k_layout_init(). */
typedef struct {
    uint16_t w, h;          /* display resolution, e.g. 320, 240 */
    uint16_t status_h;      /* status bar height (~7.5% of h) */
    uint16_t softkey_h;     /* softkey bar height (~8% of h) */
    uint16_t overview_h;    /* buffer overview strip (Phase 2; 0 for now) */
    uint16_t wave_y0;       /* top of waveform/tile area */
    uint16_t wave_h;        /* h - overview_h - status_h - softkey_h */
    uint16_t hdiv;          /* w / 12 — one horizontal division */
    uint16_t vdiv;          /* wave_h / 8 — one vertical division */
} a2k_layout_t;

void a2k_layout_init(a2k_layout_t *L, const struct display_capabilities *caps);

#endif /* A2K_BOARD_UI_LAYOUT_H */
