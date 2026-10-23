#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_BATTERY)

#include <zephyr/logging/log.h>
#include "a2k_board/drv.h"
#include "a2k_board/battery.h"

LOG_MODULE_REGISTER(a2k_battery, LOG_LEVEL_INF);

static a2k_drv_t s_status = { .name = "battery" };

int battery_init(void)
{
	s_status.enabled = true;

#if defined(CONFIG_AW2_HW_VALIDATED)
	/* Battery voltage read via ADC1 — channel TBD (not yet traced).
	 * OEM fw reads two ADC channels: one for battery, one for supply rail.
	 * Auto-zero / calibration offset applied before reporting mV. */
	LOG_INF("battery: HW_VALIDATED: ADC1 battery channel not yet traced");
#endif

	s_status.ok = true;
	LOG_INF("battery: ADC1 placeholder");
	return 0;
}

const a2k_drv_t *battery_status(void) { return &s_status; }

int battery_read_mv(uint32_t *mv)
{
	*mv = 3700; /* nominal LiPo — replace when ADC channel traced */
	return 0;
}

#endif /* CONFIG_AW2_BATTERY */
