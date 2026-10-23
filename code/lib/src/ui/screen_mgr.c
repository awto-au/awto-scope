#include "a2k/ui/screen_mgr.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(a2k_screen_mgr, LOG_LEVEL_DBG);

#define STACK_DEPTH 4

static const a2k_screen_ops_t *s_stack[STACK_DEPTH];
static void                   *s_ctx[STACK_DEPTH];
static int                     s_top = -1;

void a2k_screen_mgr_init(void) { s_top = -1; }

void a2k_screen_push(const a2k_screen_ops_t *ops, void *ctx)
{
    if (s_top + 1 >= STACK_DEPTH) {
        LOG_ERR("screen stack full");
        return;
    }
    s_top++;
    s_stack[s_top] = ops;
    s_ctx[s_top]   = ctx;
    if (ops->on_enter) { ops->on_enter(ctx); }
}

void a2k_screen_pop(void)
{
    if (s_top < 0) { return; }
    if (s_stack[s_top]->on_exit) { s_stack[s_top]->on_exit(s_ctx[s_top]); }
    s_top--;
}

void a2k_screen_key(uint8_t key)
{
    if (s_top < 0) { return; }
    if (s_stack[s_top]->on_key) { s_stack[s_top]->on_key(s_ctx[s_top], key); }
}

void a2k_screen_render(void)
{
    if (s_top < 0) { return; }
    if (s_stack[s_top]->on_render) { s_stack[s_top]->on_render(s_ctx[s_top]); }
}

bool a2k_screen_can_pop(void) { return s_top > 0; }
