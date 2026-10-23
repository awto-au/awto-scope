#ifndef A2K_BOARD_USB_DIAG_H
#define A2K_BOARD_USB_DIAG_H

#include <stdint.h>

enum usb_diag_format {
	USB_DIAG_FMT_NATIVE = 0,
	USB_DIAG_FMT_OEM,
};

int usb_diag_init(void);
void usb_diag_set_format(enum usb_diag_format fmt);
enum usb_diag_format usb_diag_get_format(void);
int usb_diag_write_line(const char *module, const char *level, const char *msg);
int usb_diag_printf(const char *module, const char *level, const char *fmt, ...);

#endif /* A2K_BOARD_USB_DIAG_H */