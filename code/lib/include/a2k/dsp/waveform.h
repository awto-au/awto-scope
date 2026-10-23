#ifndef A2K_DSP_WAVEFORM_H
#define A2K_DSP_WAVEFORM_H

#include <stdint.h>

/* 8-bit offset-binary: 0x80=0V, 0x00=-FS, 0xFF=+FS */
float    a2k_sample_to_mv(uint8_t sample, float mv_per_lsb);
uint8_t  a2k_mv_to_sample(float mv, float mv_per_lsb);
void     a2k_waveform_render(const uint8_t *samples, uint32_t count,
                              uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                              float mv_per_lsb, uint16_t colour);

#endif /* A2K_DSP_WAVEFORM_H */
