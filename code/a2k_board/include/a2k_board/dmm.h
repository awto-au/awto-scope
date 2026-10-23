#ifndef A2K_BOARD_DMM_H
#define A2K_BOARD_DMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "a2k_board/drv.h"

enum dmm_value_status {
	DMM_VALUE_OK = 0,
	DMM_VALUE_OVERLOAD,
	DMM_VALUE_INVALID,
};

struct dmm_reading {
	int32_t value_milli;
	int8_t decimal_pos;
	bool negative;
	enum dmm_value_status status;
	uint8_t digits[4];
};

struct dmm_stats {
	uint32_t frame_ok;
	uint32_t frame_bad_seq;
	uint32_t decode_ok;
	uint32_t decode_invalid;
	uint32_t decode_overload;
};

int dmm_init(void);
const a2k_drv_t *dmm_status(void);

/* Preferred API: returns a typed reading and decode status. */
int dmm_reading_get(struct dmm_reading *out);

/* Copy the most recent raw 14-byte CS7721 frame (nibbles), if available. */
int dmm_frame_get(uint8_t out_frame[14]);

/* Get cumulative transport/decode counters. */
void dmm_stats_get(struct dmm_stats *out);

/* True when the reading is numerically usable for control/telemetry paths. */
bool dmm_reading_is_valid(const struct dmm_reading *r);

/* Human-readable representation for logs/UI.
 * Examples: "+12.34", "-1.234", "OL", "ERR".
 */
int dmm_reading_format(const struct dmm_reading *r, char *buf, size_t buf_len);

#endif /* A2K_BOARD_DMM_H */
