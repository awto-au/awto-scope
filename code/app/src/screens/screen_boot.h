#ifndef SCREEN_BOOT_H
#define SCREEN_BOOT_H

#include <stdint.h>
#include "a2k/ui/screen_mgr.h"

void screen_boot_enter(void *ctx);
void screen_boot_exit(void *ctx);
void screen_boot_key(void *ctx, uint8_t key);
void screen_boot_render(void *ctx);

extern const a2k_screen_ops_t screen_boot_ops;

#endif /* SCREEN_BOOT_H */
