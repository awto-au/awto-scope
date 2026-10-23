#ifndef A2K_BOARD_LCD_H
#define A2K_BOARD_LCD_H

#include "a2k_board/drv.h"

/* lcd is a Zephyr display device — init runs automatically at POST_KERNEL.
 * Use DEVICE_DT_GET(DT_NODELABEL(ili9341_fsmc)) to get the device handle. */
const a2k_drv_t *lcd_status(void);

#endif /* A2K_BOARD_LCD_H */
