#ifndef A2K_UI_CHANNEL_H
#define A2K_UI_CHANNEL_H

#include <stdint.h>
#include <stdbool.h>
#include "a2k/ui/layout.h"

typedef enum { CH_SRC_ADC=0, CH_SRC_DMM, CH_SRC_AWG, CH_SRC_MATH } a2k_ch_src_t;

typedef struct a2k_channel a2k_channel_t;
struct a2k_channel {
    a2k_ch_src_t src;
    bool         enabled;
    uint16_t     colour;       /* RGB565 */
    const char  *label;
    float        value;        /* instant reading in base unit */
    float        freq_hz;
    const char  *unit;
    void (*render_full)(a2k_channel_t *, const void *area);
    void (*render_tile)(a2k_channel_t *, const void *area);
};

void a2k_channel_init(a2k_channel_t *ch, a2k_ch_src_t src, const char *label, uint16_t colour);
void a2k_channel_render(a2k_channel_t *ch, const void *area, bool full);

#endif /* A2K_UI_CHANNEL_H */
