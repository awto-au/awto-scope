#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_FPGA)

#include <zephyr/logging/log.h>
#include "a2k_board/drv.h"
#include "a2k_board/fpga.h"

LOG_MODULE_REGISTER(a2k_fpga, LOG_LEVEL_INF);

static a2k_drv_t s_status = { .name = "fpga" };

int fpga_init(void)
{
	s_status.enabled = true;

#if defined(CONFIG_AW2_HW_VALIDATED)
	/* SPI1: PA5=SCK, PA6=MISO, PA7=MOSI, PA4=CS (active-low).
	 * Mode 0 (CPOL=0 CPHA=0), 8-bit, up to ~25 MHz per MachXO2 spec.
	 * Confirmed from ghidra_names.tsv ptr_spi1 / fw_fpga_spi_init. */
	LOG_INF("fpga: HW_VALIDATED: SPI1 FPGA init skipped (not yet implemented)");
#endif

	s_status.ok = true;
	LOG_INF("fpga: driver initialised (MachXO2 SPI stub)");
	return 0;
}

const a2k_drv_t *fpga_status(void) { return &s_status; }

void fpga_write_reg(uint8_t addr, uint8_t data)
{
	LOG_INF("fpga: write reg 0x%02x = 0x%02x (stub)", addr, data);
}

int fpga_read_reg(uint8_t addr, uint8_t *out)
{
	LOG_DBG("fpga: read reg 0x%02x (stub)", addr);
	if (out) { *out = 0x00; }
	return 0;
}

#endif /* CONFIG_AW2_FPGA */
