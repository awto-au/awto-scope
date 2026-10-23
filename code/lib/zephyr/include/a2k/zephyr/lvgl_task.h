#ifndef A2K_ZEPHYR_LVGL_TASK_H
#define A2K_ZEPHYR_LVGL_TASK_H

#include <stdint.h>

void a2k_lvgl_task_init(void);
void a2k_lvgl_tick(uint32_t ms);
void a2k_lvgl_run(void);

#endif /* A2K_ZEPHYR_LVGL_TASK_H */
