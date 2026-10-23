#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_BACKLIGHT) && DT_NODE_EXISTS(DT_ALIAS(backlight))

#include <zephyr/shell/shell.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>
#include "a2k_board/drv.h"

LOG_MODULE_REGISTER(a2k_backlight, LOG_LEVEL_INF);

static const struct pwm_dt_spec s_bl = PWM_DT_SPEC_GET(DT_ALIAS(backlight));
static a2k_drv_t s_status = { .name = "backlight" };

static void set_percent(uint8_t pct)
{
	uint32_t period = s_bl.period;
	uint32_t pulse  = (pct == 0) ? 0 : (period / 100 * pct);
	pwm_set_dt(&s_bl, period, pulse);
}

int backlight_init(void)
{
	s_status.enabled = true;
	if (!pwm_is_ready_dt(&s_bl)) {
		s_status.err = -ENODEV;
		LOG_ERR("backlight: PWM not ready");
		return s_status.err;
	}
	set_percent(80);
	s_status.ok = true;
	LOG_INF("backlight: ok, TIM1_CH1 PA8 @ 80%%");
	return 0;
}

const a2k_drv_t *backlight_status(void) { return &s_status; }

static int cmd_backlight(const struct shell *sh, size_t argc, char **argv)
{
	if (!s_status.ok) {
		shell_error(sh, "backlight: not initialised");
		return -ENODEV;
	}
	if (argc >= 2 && strcmp(argv[1], "off") == 0) {
		set_percent(0);
		shell_print(sh, "backlight: off");
		return 0;
	}
	uint8_t pct = (argc >= 2) ? (uint8_t)strtoul(argv[1], NULL, 0) : 80;
	if (pct > 100) { pct = 100; }
	set_percent(pct);
	shell_print(sh, "backlight: %u%%", pct);
	return 0;
}
SHELL_CMD_REGISTER(backlight, NULL, "backlight <0-100> | backlight off", cmd_backlight);

#endif /* CONFIG_AW2_BACKLIGHT && backlight alias */
