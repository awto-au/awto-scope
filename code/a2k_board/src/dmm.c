#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_DMM)

#include <errno.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include "a2k_board/drv.h"
#include "a2k_board/dmm.h"

LOG_MODULE_REGISTER(a2k_dmm, LOG_LEVEL_INF);

static a2k_drv_t s_status = { .name = "dmm" };
static const struct device *s_uart2;

/* CS7721 stream: 14-byte frame, each byte is 0xNX where N=1..14 sequence and
 * X is a 4-bit LCD-segment nibble. This is transport over UART, not text UART. */
static uint8_t s_frame_nibbles[14];
static uint8_t s_expect_seq;
static bool s_frame_ready;
static struct dmm_stats s_stats;

static int decode_seg_digit(uint8_t seg_pattern)
{
	/* OEM fw_dmm_cs7721_decode_digits mapping (0x0802a06c). */
	switch (seg_pattern) {
	case 0x50: return 1;
	case 0x51: return 7;
	case 0x72: return 4;
	case 0x86: return -2; /* 'L' / overload marker */
	case 0xB5: return 2;
	case 0xD7: return 0;
	case 0xE3: return 5;
	case 0xE7: return 6;
	case 0xF1: return 3;
	case 0xF3: return 9;
	case 0xF7: return 8;
	default:   return -1;
	}
}

static int decode_frame(const uint8_t *rx, struct dmm_reading *out)
{
	int d1, d2, d3, d4;
	bool negative = (rx[1] & 0x08U) != 0U;
	int32_t raw;
	int dp = 0;

	if (!out) {
		return -EINVAL;
	}

	d1 = decode_seg_digit((rx[1] & 0x07U) | ((rx[2] & 0x0FU) << 4));
	d2 = decode_seg_digit((rx[3] & 0x07U) | ((rx[4] & 0x0FU) << 4));
	d3 = decode_seg_digit((rx[5] & 0x07U) | ((rx[6] & 0x0FU) << 4));
	d4 = decode_seg_digit((rx[7] & 0x07U) | ((rx[8] & 0x0FU) << 4));

	out->digits[0] = (uint8_t)((d1 >= 0) ? d1 : 0xFF);
	out->digits[1] = (uint8_t)((d2 >= 0) ? d2 : 0xFF);
	out->digits[2] = (uint8_t)((d3 >= 0) ? d3 : 0xFF);
	out->digits[3] = (uint8_t)((d4 >= 0) ? d4 : 0xFF);
	out->negative = negative;
	out->decimal_pos = 0;
	out->value_milli = 0;
	out->status = DMM_VALUE_INVALID;

	if (d1 == -2 || d2 == -2 || d3 == -2 || d4 == -2) {
		out->status = DMM_VALUE_OVERLOAD;
		return -EOVERFLOW;
	}

	/* Any unknown/overload segment pattern means no numeric value to return. */
	if (d1 < 0 || d2 < 0 || d3 < 0 || d4 < 0) {
		out->status = DMM_VALUE_INVALID;
		return -ENODATA;
	}

	raw = (int32_t)(d1 * 1000 + d2 * 100 + d3 * 10 + d4);

	/* OEM decimal-point priority order (last match wins): rx[3], rx[5], rx[7]. */
	if (rx[3] & 0x08U) {
		dp = 1;
	}
	if (rx[5] & 0x08U) {
		dp = 2;
	}
	if (rx[7] & 0x08U) {
		dp = 3;
	}
	out->decimal_pos = (int8_t)dp;

	/* Convert displayed value to milli-scaled units using decimal-point location.
	 * dp=1 -> d.ddd (raw already in milli-units)
	 * dp=2 -> dd.dd
	 * dp=3 -> ddd.d
	 * dp=0 -> dddd
	 */
	switch (dp) {
	case 1: raw = raw; break;
	case 2: raw = raw * 10; break;
	case 3: raw = raw * 100; break;
	default: raw = raw * 1000; break;
	}

	out->value_milli = negative ? -raw : raw;
	out->status = DMM_VALUE_OK;
	return 0;
}

static void dmm_rx_byte(uint8_t b)
{
	uint8_t seq = (b >> 4) & 0x0F;
	uint8_t nib = b & 0x0F;

	if (seq == 1U) {
		s_frame_nibbles[0] = nib;
		s_expect_seq = 2U;
		return;
	}

	if (s_expect_seq < 2U || s_expect_seq > 14U) {
		return;
	}

	if (seq != s_expect_seq) {
		/* Bad sequence: keep waiting for the next frame start (seq=1). */
		s_expect_seq = 0U;
		s_stats.frame_bad_seq++;
		return;
	}

	s_frame_nibbles[seq - 1U] = nib;
	if (seq == 14U) {
		s_frame_ready = true;
		s_stats.frame_ok++;
		s_expect_seq = 0U;
	} else {
		s_expect_seq++;
	}
}

static void dmm_uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	uart_irq_update(dev);

	while (uart_irq_rx_ready(dev)) {
		uint8_t buf[16];
		int n = uart_fifo_read(dev, buf, sizeof(buf));
		for (int i = 0; i < n; i++) {
			dmm_rx_byte(buf[i]);
		}
	}
}

int dmm_init(void)
{
	s_status.enabled = true;

#if defined(CONFIG_AW2_HW_VALIDATED)
	/* CS7721 serial output is one-way into USART2 RX (PA3), 2400 8N1.
	 * Payload is 14-byte nibble-sequenced frames per FS9721/CS7721 protocol. */
	s_uart2 = DEVICE_DT_GET(DT_NODELABEL(usart2));
	if (!device_is_ready(s_uart2)) {
		s_status.ok = false;
		s_status.err = -ENODEV;
		LOG_ERR("dmm: usart2 not ready");
		return s_status.err;
	}

	int rc = uart_irq_callback_user_data_set(s_uart2, dmm_uart_isr, NULL);
	if (rc < 0) {
		s_status.ok = false;
		s_status.err = rc;
		LOG_ERR("dmm: failed to set usart2 irq callback (%d)", rc);
		return rc;
	}

	uart_irq_rx_enable(s_uart2);
	LOG_INF("dmm: usart2 rx enabled (CS7721 framed protocol, 14-byte nibbles)");
#endif

	s_status.ok = true;
	LOG_INF("dmm: CS7721 transport initialised");
	return 0;
}

const a2k_drv_t *dmm_status(void) { return &s_status; }

int dmm_reading_get(struct dmm_reading *out)
{
	uint8_t frame[14];
	struct dmm_reading reading;
	unsigned int key;
	int rc;

	if (!out) {
		return -EINVAL;
	}

	if (!s_frame_ready) {
		return -EAGAIN;
	}

	key = irq_lock();
	for (int i = 0; i < 14; i++) {
		frame[i] = s_frame_nibbles[i];
	}
	irq_unlock(key);

	rc = decode_frame(frame, &reading);
	if (rc == 0) {
		s_stats.decode_ok++;
	} else if (rc == -EOVERFLOW) {
		s_stats.decode_overload++;
	} else {
		s_stats.decode_invalid++;
	}

	*out = reading;
	return rc;
}

int dmm_frame_get(uint8_t out_frame[14])
{
	unsigned int key;

	if (!out_frame) {
		return -EINVAL;
	}
	if (!s_frame_ready) {
		return -EAGAIN;
	}

	key = irq_lock();
	for (int i = 0; i < 14; i++) {
		out_frame[i] = s_frame_nibbles[i];
	}
	irq_unlock(key);

	return 0;
}

void dmm_stats_get(struct dmm_stats *out)
{
	if (!out) {
		return;
	}
	*out = s_stats;
}

bool dmm_reading_is_valid(const struct dmm_reading *r)
{
	if (!r) {
		return false;
	}
	if (r->status != DMM_VALUE_OK) {
		return false;
	}
	for (int i = 0; i < 4; i++) {
		if (r->digits[i] > 9U) {
			return false;
		}
	}
	if (r->decimal_pos < 0 || r->decimal_pos > 3) {
		return false;
	}
	return true;
}

int dmm_reading_format(const struct dmm_reading *r, char *buf, size_t buf_len)
{
	if (!r || !buf || buf_len == 0U) {
		return -EINVAL;
	}

	if (r->status == DMM_VALUE_OVERLOAD) {
		return snprintf(buf, buf_len, "OL");
	}
	if (!dmm_reading_is_valid(r)) {
		return snprintf(buf, buf_len, "ERR");
	}

	if (r->decimal_pos == 0) {
		return snprintf(buf, buf_len, "%c%u%u%u%u",
			r->negative ? '-' : '+',
			r->digits[0], r->digits[1], r->digits[2], r->digits[3]);
	}
 if (r->decimal_pos == 1) {
		return snprintf(buf, buf_len, "%c%u.%u%u%u",
			r->negative ? '-' : '+',
			r->digits[0], r->digits[1], r->digits[2], r->digits[3]);
	}
	if (r->decimal_pos == 2) {
		return snprintf(buf, buf_len, "%c%u%u.%u%u",
			r->negative ? '-' : '+',
			r->digits[0], r->digits[1], r->digits[2], r->digits[3]);
	}

	return snprintf(buf, buf_len, "%c%u%u%u.%u",
		r->negative ? '-' : '+',
		r->digits[0], r->digits[1], r->digits[2], r->digits[3]);
}

#endif /* CONFIG_AW2_DMM */
