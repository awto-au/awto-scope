#include "a2k/zephyr/lvgl_task.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_LVGL
#include <lvgl.h>
#endif

LOG_MODULE_REGISTER(a2k_lvgl, LOG_LEVEL_INF);

void a2k_lvgl_task_init(void)
{
#ifdef CONFIG_LVGL
	/* Zephyr auto-init (LV_Z_AUTO_INIT=y) calls lv_init() and binds the
	 * display device at POST_KERNEL priority — by the time main() calls us,
	 * LVGL is ready. Draw the initial boot screen. */
	lv_display_t *disp = lv_display_get_default();
	if (!disp) {
		LOG_ERR("lvgl: no display — check DTS chosen zephyr,display");
		return;
	}
	LOG_INF("lvgl: display ready, %dx%d",
		lv_display_get_horizontal_resolution(disp),
		lv_display_get_vertical_resolution(disp));
#endif
}

void a2k_lvgl_tick(uint32_t ms)
{
#ifdef CONFIG_LVGL
	lv_tick_inc(ms);
#else
	ARG_UNUSED(ms);
#endif
}

void a2k_lvgl_run(void)
{
#ifdef CONFIG_LVGL
	lv_timer_handler();
#endif
}
