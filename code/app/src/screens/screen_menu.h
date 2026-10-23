#ifndef SCREEN_MENU_H
#define SCREEN_MENU_H

#include <stdint.h>
#include "a2k/ui/screen_mgr.h"

void screen_menu_enter(void *ctx);
void screen_menu_exit(void *ctx);
void screen_menu_key(void *ctx, uint8_t key);
void screen_menu_render(void *ctx);

extern const a2k_screen_ops_t screen_menu_ops;

#endif /* SCREEN_MENU_H */
