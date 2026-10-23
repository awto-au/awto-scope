#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_USB_DIAG)

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/usb/usb_device.h>
#include "a2k_board/usb_diag.h"

LOG_MODULE_REGISTER(a2k_usb_diag, LOG_LEVEL_INF);

static const struct device *s_cdc;
static bool s_ready;
static K_MUTEX_DEFINE(s_lock);

#if defined(CONFIG_AW2_USB_DIAG_OEM_DEFAULT)
static enum usb_diag_format s_format = USB_DIAG_FMT_OEM;
#else
static enum usb_diag_format s_format = USB_DIAG_FMT_NATIVE;
#endif

static int usb_diag_puts(const char *line)
{
	if (!s_ready || !s_cdc || !line) {
		return -ENODEV;
	}

	k_mutex_lock(&s_lock, K_FOREVER);
	for (const char *p = line; *p; p++) {
		uart_poll_out(s_cdc, (unsigned char)*p);
	}
	k_mutex_unlock(&s_lock);

	return 0;
}

int usb_diag_init(void)
{
	int rc;

	s_cdc = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
	if (!device_is_ready(s_cdc)) {
		LOG_WRN("usb_diag: cdc_acm_uart0 not ready");
		return -ENODEV;
	}

	rc = usb_enable(NULL);
	if (rc != 0 && rc != -EALREADY) {
		LOG_ERR("usb_diag: usb_enable failed (%d)", rc);
		return rc;
	}

	s_ready = true;
	LOG_INF("usb_diag: CDC ACM diagnostic port ready");
	usb_diag_printf("usb", "INF", "diagnostic serial ready; format=%s",
		s_format == USB_DIAG_FMT_OEM ? "oem" : "native");
	return 0;
}

void usb_diag_set_format(enum usb_diag_format fmt)
{
	if (fmt == USB_DIAG_FMT_NATIVE || fmt == USB_DIAG_FMT_OEM) {
		s_format = fmt;
	}
}

enum usb_diag_format usb_diag_get_format(void)
{
	return s_format;
}

int usb_diag_write_line(const char *module, const char *level, const char *msg)
{
	char line[256];
	uint32_t ms;
	int n;

	if (!module || !level || !msg) {
		return -EINVAL;
	}

	ms = k_uptime_get_32();
	if (s_format == USB_DIAG_FMT_OEM) {
		n = snprintf(line, sizeof(line), "OEMLOG,%lu,%s,%s,%s\r\n",
			(unsigned long)ms, level, module, msg);
	} else {
		n = snprintf(line, sizeof(line), "[%s] %s: %s\r\n", level, module, msg);
	}

	if (n < 0) {
		return n;
	}

	return usb_diag_puts(line);
}

int usb_diag_printf(const char *module, const char *level, const char *fmt, ...)
{
	char msg[192];
	va_list ap;

	if (!fmt) {
		return -EINVAL;
	}

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	return usb_diag_write_line(module, level, msg);
}

static int cmd_usbdiag_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "ready=%d format=%s",
		s_ready ? 1 : 0,
		s_format == USB_DIAG_FMT_OEM ? "oem" : "native");
	return 0;
}

static int cmd_usbdiag_fmt(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_print(sh, "usage: usbdiag fmt <native|oem>");
		return -EINVAL;
	}

	if (strcmp(argv[1], "native") == 0) {
		usb_diag_set_format(USB_DIAG_FMT_NATIVE);
	} else if (strcmp(argv[1], "oem") == 0) {
		usb_diag_set_format(USB_DIAG_FMT_OEM);
	} else {
		shell_print(sh, "unknown format: %s", argv[1]);
		return -EINVAL;
	}

	shell_print(sh, "usbdiag format set: %s",
		s_format == USB_DIAG_FMT_OEM ? "oem" : "native");
	usb_diag_printf("usb", "INF", "format switched to %s",
		s_format == USB_DIAG_FMT_OEM ? "oem" : "native");
	return 0;
}

static int cmd_usbdiag_test(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int rc = usb_diag_printf("usb", "INF", "diagnostic test message");
	shell_print(sh, "usbdiag test rc=%d", rc);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_usbdiag,
	SHELL_CMD(status, NULL, "Show USB diagnostic logger status", cmd_usbdiag_status),
	SHELL_CMD(fmt, NULL, "Set log format: native|oem", cmd_usbdiag_fmt),
	SHELL_CMD(test, NULL, "Emit one diagnostic test line", cmd_usbdiag_test),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(usbdiag, &sub_usbdiag,
	"USB diagnostic serial controls", NULL);

#endif /* CONFIG_AW2_USB_DIAG */