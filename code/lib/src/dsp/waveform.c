#include "a2k/dsp/waveform.h"

float a2k_sample_to_mv(uint8_t sample, float mv_per_lsb)
{ /* stub */ return 0; }

uint8_t a2k_mv_to_sample(float mv, float mv_per_lsb)
{ /* stub */ return 0; }

void a2k_waveform_render(const uint8_t *samples, uint32_t count,
                          uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                          float mv_per_lsb, uint16_t colour)
{ /* stub */ }
