#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/device.h>
#include "a2k_board/keypad.h"
#include "a2k_board/supervisor.h"

LOG_MODULE_REGISTER(a2k_keypad, LOG_LEVEL_INF);

/* Physical layout: 4 keys across × 5 keys down = 20 keys.
 *
 * gpio-kbd-matrix driver:
 *   ABS_X = col-gpio index (0-4): scan output = physical ROW  (PD13 PD12 PC3 PB14 PB13)
 *   ABS_Y = row-gpio index (0-3): read input  = physical COL  (PB6  PB5  PC6  PB12)
 *
 * key = phys_row * PHYS_COLS + phys_col + 1
 *     = s_col    * 4          + s_row   + 1
 */
#define PHYS_COLS 4   /* keys across — row-gpio count */
#define PHYS_ROWS 5   /* keys down   — col-gpio count */

static const char * const SCAN_PINS[PHYS_ROWS] = { "PD13", "PD12", "PC3", "PB14", "PB13" };
static const char * const READ_PINS[PHYS_COLS] = { "PB6",  "PB5",  "PC6", "PB12" };

static uint8_t s_col;   /* latched from ABS_X = col-gpio = physical row */
static uint8_t s_row;   /* latched from ABS_Y = row-gpio = physical col */

static void keypad_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	switch (evt->code) {
	case INPUT_ABS_X:
		s_col = (uint8_t)evt->value;
		break;
	case INPUT_ABS_Y:
		s_row = (uint8_t)evt->value;
		break;
	case INPUT_BTN_TOUCH:
		if (evt->value) {
			uint8_t key = s_col * PHYS_COLS + s_row + 1;
			const char *scan = s_col < PHYS_ROWS ? SCAN_PINS[s_col] : "?";
			const char *read = s_row < PHYS_COLS ? READ_PINS[s_row] : "?";
			LOG_INF("key %2u  scan=%s(col-gpio low)  read=%s(row-gpio low)",
				key, scan, read);
			supervisor_post(MSG_KEYPAD, key);
		} else {
			LOG_DBG("key release col-gpio=%u row-gpio=%u", s_col, s_row);
		}
		break;
	default:
		break;
	}
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(kscan0)), keypad_cb, NULL);

void keypad_init(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(kscan0));

	if (!device_is_ready(dev)) {
		LOG_ERR("keypad: kscan0 not ready");
		return;
	}
	LOG_INF("keypad: %d across × %d down = 20 keys", PHYS_COLS, PHYS_ROWS);
}
