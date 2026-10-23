#ifndef SCREEN_WAVEFORM_H
#define SCREEN_WAVEFORM_H

#include <stdint.h>
#include "a2k/ui/screen_mgr.h"

void screen_waveform_enter(void *ctx);
void screen_waveform_exit(void *ctx);
void screen_waveform_key(void *ctx, uint8_t key);
void screen_waveform_render(void *ctx);

extern const a2k_screen_ops_t screen_waveform_ops;

#endif /* SCREEN_WAVEFORM_H */
