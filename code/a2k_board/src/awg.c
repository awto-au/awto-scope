#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_AWG)

#include <stdbool.h>
#include <zephyr/logging/log.h>
#include "a2k_board/drv.h"
#include "a2k_board/awg.h"

LOG_MODULE_REGISTER(a2k_awg, LOG_LEVEL_INF);

/* AWG: DDS over FPGA SPI. Frequency word = freq_hz * 4294967296.0 / 25000000.0
 * (25 MHz DDS clock). Table = 1024 samples × 8-bit, uploaded via FPGA SPI burst write.
 * Separate from fpga.c so AWG commands don't need to touch capture registers. */

static a2k_drv_t s_status = { .name = "awg" };

uint32_t awg_freq_word(uint32_t freq_hz)
{
	return (uint32_t)((double)freq_hz * 4294967296.0 / 25000000.0);
}

int awg_init(void)
{
	s_status.enabled = true;
	s_status.ok = true;
	LOG_INF("awg: DDS 25MHz clock, 1024-sample table");
	return 0;
}

const a2k_drv_t *awg_status(void) { return &s_status; }

void awg_set_freq(uint32_t freq_hz)
{
	uint32_t word = awg_freq_word(freq_hz);
	LOG_INF("awg: set_freq %u Hz → freq_word=0x%08x (stub)", freq_hz, word);
}

void awg_enable(bool on)
{
	LOG_INF("awg: enable=%d (stub)", (int)on);
}

#endif /* CONFIG_AW2_AWG */
