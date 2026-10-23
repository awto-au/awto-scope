#ifndef A2K_BOARD_USB_SCOPE_H
#define A2K_BOARD_USB_SCOPE_H

#include "a2k_board/drv.h"

int               usb_scope_init(void);
const a2k_drv_t  *usb_scope_status(void);
int               usb_scope_poll(void);

#endif /* A2K_BOARD_USB_SCOPE_H */
