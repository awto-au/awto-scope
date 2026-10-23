#ifndef SCREEN_DMM_H
#define SCREEN_DMM_H

#include <stdint.h>
#include "a2k/ui/screen_mgr.h"

void screen_dmm_enter(void *ctx);
void screen_dmm_exit(void *ctx);
void screen_dmm_key(void *ctx, uint8_t key);
void screen_dmm_render(void *ctx);

extern const a2k_screen_ops_t screen_dmm_ops;

#endif /* SCREEN_DMM_H */
