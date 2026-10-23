#ifndef A2K_UI_SCREEN_MGR_H
#define A2K_UI_SCREEN_MGR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    void (*on_enter)(void *ctx);
    void (*on_exit)(void *ctx);
    void (*on_key)(void *ctx, uint8_t key);
    void (*on_render)(void *ctx);
} a2k_screen_ops_t;

void a2k_screen_mgr_init(void);
void a2k_screen_push(const a2k_screen_ops_t *ops, void *ctx);
void a2k_screen_pop(void);
void a2k_screen_key(uint8_t key);
void a2k_screen_render(void);
bool a2k_screen_can_pop(void);

#endif /* A2K_UI_SCREEN_MGR_H */
