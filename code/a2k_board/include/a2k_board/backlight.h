#ifndef A2K_BOARD_BACKLIGHT_H
#define A2K_BOARD_BACKLIGHT_H

#include "a2k_board/drv.h"

int              backlight_init(void);
const a2k_drv_t *backlight_status(void);

#endif /* A2K_BOARD_BACKLIGHT_H */
