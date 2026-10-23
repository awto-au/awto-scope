#ifndef A2K_BOARD_DRV_H
#define A2K_BOARD_DRV_H

#include <stdbool.h>

typedef struct {
    const char *name;
    bool        enabled;
    bool        ok;
    int         err;
} a2k_drv_t;

#endif /* A2K_BOARD_DRV_H */
