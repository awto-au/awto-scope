#ifndef SCREEN_CAL_H
#define SCREEN_CAL_H

#include <stdint.h>
#include "a2k/ui/screen_mgr.h"

void screen_cal_enter(void *ctx);
void screen_cal_exit(void *ctx);
void screen_cal_key(void *ctx, uint8_t key);
void screen_cal_render(void *ctx);

extern const a2k_screen_ops_t screen_cal_ops;

#endif /* SCREEN_CAL_H */
