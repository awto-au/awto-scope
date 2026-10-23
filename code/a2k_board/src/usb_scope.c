#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_USB)

#include <zephyr/logging/log.h>
#include "a2k_board/drv.h"
#include "a2k_board/usb_scope.h"

LOG_MODULE_REGISTER(a2k_usb, LOG_LEVEL_INF);

/* USB CDC ACM for scope control commands (late init — USB enumeration is slow).
 * VID=0x0483 PID=0x2d42, EP2 out / EP1 in, 9-byte frame format (RE'd from OEM).
 * See docs/hantek-2000-PC-USB-PROTOCOL.md */

static a2k_drv_t s_status = { .name = "usb_scope" };

int usb_scope_init(void)
{
	s_status.enabled = true;
	s_status.ok = true;
	LOG_INF("usb_scope: VID=0x0483 PID=0x2d42 EP2out/EP1in 9-byte frame (stub)");
	return 0;
}

const a2k_drv_t *usb_scope_status(void) { return &s_status; }

int usb_scope_poll(void)
{
	return 0; /* no command pending */
}

#endif /* CONFIG_AW2_USB */
