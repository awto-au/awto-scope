#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_PROBE_COMP)

#include <zephyr/logging/log.h>
#include "a2k_board/drv.h"
#include "a2k_board/probe_comp.h"

LOG_MODULE_REGISTER(a2k_probe_comp, LOG_LEVEL_INF);

/* Probe compensation: 1kHz square wave on PA8 (TIM1_CH1, same pin as backlight).
 * In probe_comp mode, backlight PWM is overridden to output 1kHz 50% duty square wave.
 * This is the standard "connect probe tip to this output to set probe compensation cap". */

static a2k_drv_t s_status = { .name = "probe_comp" };

int probe_comp_init(void)
{
	s_status.enabled = true;
	s_status.ok = true;
	LOG_INF("probe_comp: TIM1_CH1 PA8 1kHz square wave (overrides backlight)");
	return 0;
}

const a2k_drv_t *probe_comp_status(void) { return &s_status; }

void probe_comp_start(void)
{
	LOG_INF("probe_comp: start (stub)");
}

void probe_comp_stop(void)
{
	LOG_INF("probe_comp: stop (stub)");
}

#endif /* CONFIG_AW2_PROBE_COMP */
