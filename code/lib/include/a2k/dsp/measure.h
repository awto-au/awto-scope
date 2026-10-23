#ifndef A2K_DSP_MEASURE_H
#define A2K_DSP_MEASURE_H

#include <stdint.h>

typedef struct {
    float vpp, vrms, vmax, vmin, vmean;
    float freq_hz, period_s, duty_pct;
    float rise_s, fall_s;
} a2k_measure_t;

void a2k_measure(const uint8_t *samples, uint32_t count, float mv_per_lsb, a2k_measure_t *out);

#endif /* A2K_DSP_MEASURE_H */
