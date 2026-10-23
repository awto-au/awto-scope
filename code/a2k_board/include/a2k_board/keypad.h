#pragma once
#include <stdint.h>

/* Key codes — physical label on the key, not Linux keycodes */
enum aw2_key {
	KEY_AW2_UNKNOWN = 0,
	/* Row 0 */
	KEY_AW2_R0C0, KEY_AW2_R0C1, KEY_AW2_R0C2, KEY_AW2_R0C3,
	/* Row 1 */
	KEY_AW2_R1C0, KEY_AW2_R1C1, KEY_AW2_R1C2, KEY_AW2_R1C3,
	/* Row 2 */
	KEY_AW2_R2C0, KEY_AW2_R2C1, KEY_AW2_R2C2, KEY_AW2_R2C3,
	/* Row 3 */
	KEY_AW2_R3C0, KEY_AW2_R3C1, KEY_AW2_R3C2, KEY_AW2_R3C3,
	/* Row 4 */
	KEY_AW2_R4C0, KEY_AW2_R4C1, KEY_AW2_R4C2, KEY_AW2_R4C3,
	KEY_AW2_COUNT,
};

void keypad_init(void);
