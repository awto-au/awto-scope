#include "a2k/zephyr/display_bridge.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(a2k_display_bridge, LOG_LEVEL_INF);

int a2k_display_bridge_init(const struct device *display_dev, a2k_layout_t *layout)
{ /* stub */ return 0; }

void a2k_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *buf)
{ /* stub */ }
