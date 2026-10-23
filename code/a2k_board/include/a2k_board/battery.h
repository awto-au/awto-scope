#ifndef A2K_BOARD_BATTERY_H
#define A2K_BOARD_BATTERY_H

#include <stdint.h>
#include "a2k_board/drv.h"

int              battery_init(void);
const a2k_drv_t *battery_status(void);
int              battery_read_mv(uint32_t *mv);

#endif /* A2K_BOARD_BATTERY_H */
