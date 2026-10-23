#ifndef A2K_ZEPHYR_DISPLAY_BRIDGE_H
#define A2K_ZEPHYR_DISPLAY_BRIDGE_H

#include <zephyr/drivers/display.h>
#include "a2k/ui/layout.h"

int  a2k_display_bridge_init(const struct device *display_dev, a2k_layout_t *layout);
void a2k_display_flush(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *buf);

#endif /* A2K_ZEPHYR_DISPLAY_BRIDGE_H */
