#ifndef SCREEN_AWG_H
#define SCREEN_AWG_H

#include <stdint.h>
#include "a2k/ui/screen_mgr.h"

void screen_awg_enter(void *ctx);
void screen_awg_exit(void *ctx);
void screen_awg_key(void *ctx, uint8_t key);
void screen_awg_render(void *ctx);

extern const a2k_screen_ops_t screen_awg_ops;

#endif /* SCREEN_AWG_H */
