#ifndef A2K_BOARD_CALIBRATION_H
#define A2K_BOARD_CALIBRATION_H

#include <stdint.h>
#include <stddef.h>
#include <zephyr/sys/util.h>
#include "a2k_board/drv.h"

/*
 * Hantek 2D82 calibration data structures — reversed from vendor firmware.
 *
 * Vendor firmware flash locations (STM32F103VET6):
 *   scope_cali_block:   g_cali_flash_start/end   @ DAT_08016218/1c
 *   volt_cali section:  g_volt_cali_flash_start  @ DAT_08016220
 *   scope config blob:  g_scope_flash_start/end  @ DAT_08019100/fc
 *
 * Sources: fw_calibrate, fw_cali_apply_ch1/ch2, fw_cali_volt_data,
 *          fw_cali_verify_checksum, USB calibration protocol (ghidra issue #37).
 */

#define CALI_STEPS    10   /* V/div steps per channel per pass */
#define CALI_PASSES    2   /* 0=AC, 1=DC                       */
#define CALI_CHANNELS  2   /* 0=CH1, 1=CH2                     */

/* ── scope_cali_block_t ─────────────────────────────────────────────────────
 * 0x2f8 bytes total.  Saved to flash by the vendor calibration wizard.
 * Valid when scope_cali_valid == 1 and volt_cali_valid == 1.
 *
 * Array access formula: field[pass][ch][step]
 *   byte offset = pass*0x50 + ch*0x28 + step*4
 */
typedef struct {
	/* Calibration DAC values from fw_calibrate (FPGA offset DAC binary search). */
	uint32_t cali_zero[CALI_PASSES][CALI_CHANNELS][CALI_STEPS]; /* +0x000  target=0   ADC */
	uint32_t cali_pos [CALI_PASSES][CALI_CHANNELS][CALI_STEPS]; /* +0x0a0  target=+100 ADC */
	uint32_t cali_neg [CALI_PASSES][CALI_CHANNELS][CALI_STEPS]; /* +0x140  target=-100 ADC */

	/* Final zero-reference ADC measurement results. */
	int32_t  ch1_zero_meas; /* +0x1e0 */
	int32_t  ch2_zero_meas; /* +0x1e4 */
	uint8_t  _pad0[0x10];   /* +0x1e8  16 bytes alignment padding */

	/*
	 * Voltage calibration float table (0xe8 bytes, i.e. 58 float32s).
	 * The inner layout [ch][type][step] has not been fully decoded — 58/4
	 * does not factor into a clean 3-D shape with CALI_CHANNELS=2, CALI_STEPS=10.
	 * Stored as raw bytes until the full inner structure is confirmed.
	 */
	uint32_t volt_cali_header;   /* +0x1f8 */
	uint8_t  volt_cali[0xe8];    /* +0x1fc  58 float32s, layout TBD */

	/* Manufacturer-specific calibration block. */
	uint8_t  misc_cali[0x10];    /* +0x2e4 */

	/* Validity flags: 0=invalid, 1=valid. */
	uint8_t  volt_cali_valid;    /* +0x2f4  g_cali_otherflag */
	uint8_t  scope_cali_valid;   /* +0x2f5  g_cali_zeroflag  */
	uint8_t  misc_cali_valid;    /* +0x2f6  g_cali_htxflag   */
	uint8_t  _pad1;              /* +0x2f7 */
} scope_cali_block_t;

BUILD_ASSERT(sizeof(scope_cali_block_t) == 0x2f8,
	"scope_cali_block_t size mismatch — check struct offsets against ghidra RE");

/* ── cali_ram_buf_t ──────────────────────────────────────────────────────────
 * 0x130 bytes.  RAM + flash.  g_cali_ram_buf @ DAT_08016b90.
 * Written by USB calibration protocol (fw_cali_save_to_flash).
 *
 * Name table (7 slots), slot n layout:
 *   type_flag @ [+2+n]
 *   x_offset  @ [+10+n*2]
 *   name_str  @ [+0x18+n*0x28]
 */
typedef struct {
	uint8_t  valid;              /* +0x00: 0x01 = valid cal present */
	uint8_t  dirty;              /* +0x01: set before flash write   */
	uint8_t  _reserved[2];
	uint8_t  name_table[0x130 - 4];
} cali_ram_buf_t;

BUILD_ASSERT(sizeof(cali_ram_buf_t) == 0x130,
	"cali_ram_buf_t size mismatch");

/* ── cali_table_t ─────────────────────────────────────────────────────────
 * 0x1e0 bytes RAM only.  g_cali_table @ DAT_08032f7c.
 * Written by USB cmd[2]=3, cmd[3]=0, cmd=0x0b.
 * 6 gain bands × 2 channels × 10 steps × int32_t.
 *
 * Byte access: g_cali_table[band_offset + ch*0x28 + step*4]
 */
#define CALI_BAND_DC_1X   0x000
#define CALI_BAND_DC_10X  0x050
#define CALI_BAND_AC_LOW  0x0a0
#define CALI_BAND_AC_MID  0x0f0
#define CALI_BAND_AC_HI1  0x140
#define CALI_BAND_AC_HI2  0x190

typedef int32_t cali_table_t[6][CALI_CHANNELS][CALI_STEPS];

BUILD_ASSERT(sizeof(cali_table_t) == 0x1e0,
	"cali_table_t size mismatch");

/* ── Boot sequence ──────────────────────────────────────────────────────────
 * 1. Load scope_cali_block_t from flash.
 * 2. fw_cali_verify_checksum: check scope_cali_valid == 1 AND volt_cali_valid == 1.
 * 3. If valid: fw_cali_apply_ch1/ch2 — write cali_zero values to FPGA offset DAC via SPI.
 * 4. If invalid: factory-default (all DAC values = 0x8000 midscale).
 */

/* ── API ────────────────────────────────────────────────────────────────── */

int              calibration_init(void);
const a2k_drv_t *calibration_status(void);
int              calibration_save(const uint8_t *blob, size_t len);
int              calibration_load(uint8_t *blob, size_t len);

#endif /* A2K_BOARD_CALIBRATION_H */
