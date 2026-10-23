#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "a2k_board/supervisor.h"
#include "a2k/ui/screen_mgr.h"
#include "a2k/zephyr/lvgl_task.h"
#include "screens/screen_boot.h"

LOG_MODULE_REGISTER(aw2_main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("aw2 — booting (Hantek 2D82)");
	supervisor_init();
	a2k_lvgl_task_init();
	a2k_screen_mgr_init();
	a2k_screen_push(&screen_boot_ops, NULL);
	LOG_INF("init complete — entering supervisor");
	supervisor_run();
	return 0;
}
