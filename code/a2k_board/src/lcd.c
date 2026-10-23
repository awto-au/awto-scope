#include <zephyr/kernel.h>

#if defined(CONFIG_AW2_LCD)

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include "a2k_board/drv.h"
#include "a2k_board/supervisor.h"

LOG_MODULE_REGISTER(a2k_lcd, LOG_LEVEL_INF);

/* FSMC Bank1: cmd@0x60000000 (A16=0/RS=0), data@0x60020000 (A16=1/RS=1) */
#define LCD_CMD_ADDR   ((volatile uint16_t *)0x60000000U)
#define LCD_DATA_ADDR  ((volatile uint16_t *)0x60020000U)

#define LCD_W 320
#define LCD_H 240

static a2k_drv_t s_status = { .name = "lcd" };

static inline void lcd_cmd(uint8_t cmd)  { *LCD_CMD_ADDR  = cmd; }
static inline void lcd_dat(uint16_t dat) { *LCD_DATA_ADDR = dat; }

static void lcd_reset_pulse(bool skip_delays)
{
	/* PD3 = ILI9341 RESET# (active-low). DT alias lcd-reset → gpiod pin 3. */
	static const struct gpio_dt_spec rst =
		GPIO_DT_SPEC_GET(DT_ALIAS(lcd_reset), gpios);
	gpio_pin_configure_dt(&rst, GPIO_OUTPUT_INACTIVE);
	gpio_pin_set_dt(&rst, 1);   /* assert reset */
	if (!skip_delays) { k_msleep(5); }
	gpio_pin_set_dt(&rst, 0);   /* release reset */
	if (!skip_delays) { k_msleep(5); }
}

static void ili9341_init_seq(bool skip_delays)
{
	/* Mandatory per datasheet: SLPOUT → wait 120ms → DISPON.
	 * Everything else: minimal, from fw_lcd_init_seq cross-checked against datasheet. */
	lcd_cmd(0x01);              /* SWRESET */
	if (!skip_delays) { k_msleep(5); }
	lcd_cmd(0x11);              /* SLPOUT */
	if (!skip_delays) { k_msleep(120); }  /* mandatory: oscillator startup */
	lcd_cmd(0x3A);              /* COLMOD pixel format */
	lcd_dat(0x55);              /* RGB565 */
	lcd_cmd(0x36);              /* MADCTL: landscape 320x240 */
	lcd_dat(0xc8);              /* MY=1 MX=1 MV=0 BGR=1: confirmed from fw_lcd_init_seq @ 0x0800c954 */
	lcd_cmd(0x29);              /* DISPON */
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	lcd_cmd(0x2A);
	lcd_dat(x0 >> 8); lcd_dat(x0 & 0xFF);
	lcd_dat(x1 >> 8); lcd_dat(x1 & 0xFF);
	lcd_cmd(0x2B);
	lcd_dat(y0 >> 8); lcd_dat(y0 & 0xFF);
	lcd_dat(y1 >> 8); lcd_dat(y1 & 0xFF);
	lcd_cmd(0x2C);              /* RAMWR */
}

/* ---- Transfer path selection --------------------------------------------
 * The caller (lcd_fill_rect, lcd_blit) does NOT choose polled vs DMA.
 * The IO layer decides based on pixel count vs LCD_DMA_MINPX threshold.
 *
 * Pattern from awto-l8 lcdts_io_xpt2046_spi_hal.c:
 *   DMA_MINSIZE = 16 — below this the DMA setup cost exceeds the transfer time.
 *   Above it, DMA frees the CPU during the transfer.
 *
 * For FSMC (memory-mapped, no SPI DMA request line): DMA1 M2M mode writes from
 * a const-colour SRAM word to LCD_DATA_ADDR repeatedly. Same threshold applies.
 * Threshold is calibrated by lcd_transfer_bench() on first boot under LCD_DEBUG.
 */
#define LCD_DMA_MINPX  16   /* pixels: below this, polled is faster */

#if defined(CONFIG_AW2_LCD_DEBUG) || (defined(CONFIG_DMA) && defined(CONFIG_AW2_HW_VALIDATED))
static void lcd_fill_polled(uint32_t n, uint16_t colour)
{
	while (n--) { lcd_dat(colour); }
}
#endif

#if defined(CONFIG_DMA) && defined(CONFIG_AW2_HW_VALIDATED)
#include <zephyr/drivers/dma.h>
static K_SEM_DEFINE(s_dma_done, 0, 1);

static void dma_cb(const struct device *dev, void *user_data, uint32_t channel, int status)
{
	ARG_UNUSED(dev); ARG_UNUSED(user_data); ARG_UNUSED(channel);
	k_sem_give(&s_dma_done);
}

static void lcd_fill_dma(uint32_t n, uint16_t colour)
{
	/* DMA1 M2M: constant SRAM source → LCD_DATA_ADDR (FSMC data window).
	 * Source address held fixed (circular fill), dest fixed (always same FSMC reg).
	 * DMA1 Ch1 used for M2M on STM32F103 (no peripheral request needed). */
	static uint16_t s_colour;
	s_colour = colour;
	const struct device *dma = DEVICE_DT_GET(DT_NODELABEL(dma1));
	struct dma_config cfg = {
		.channel_direction   = MEMORY_TO_MEMORY,
		.source_data_size    = 2,
		.dest_data_size      = 2,
		.source_burst_length = 1,
		.dest_burst_length   = 1,
		.dma_callback        = dma_cb,
		.complete_callback_en = 1,
	};
	/* DMA NDTR max = 65535 per transfer — chunk if n > 65535 */
	while (n) {
		uint16_t chunk = (n > 65535) ? 65535 : (uint16_t)n;
		struct dma_block_config blk = {
			.source_address    = (uint32_t)&s_colour,
			.dest_address      = (uint32_t)LCD_DATA_ADDR,
			.block_size        = (uint32_t)chunk * 2,
			.source_addr_adj   = DMA_ADDR_ADJ_NO_CHANGE,
			.dest_addr_adj     = DMA_ADDR_ADJ_NO_CHANGE,
		};
		cfg.head_block = &blk;
		dma_config(dma, 0, &cfg);
		k_sem_reset(&s_dma_done);
		dma_start(dma, 0);
		k_sem_take(&s_dma_done, K_FOREVER);
		n -= chunk;
	}
}

/*
 * lcd_blit_dma — issue #125: DMA1 M2M flush of a real (varying-pixel) LVGL
 * partial render buffer, as opposed to lcd_fill_dma()'s constant-colour fill.
 * Source address INCREMENTS through the caller's buffer; dest stays fixed at
 * the FSMC data register (peripheral behaves like a FIFO — every 16-bit write
 * lands on the current window pixel and auto-advances the ILI9341's internal
 * GRAM cursor, so the MCU-side dest address never needs to move).
 *
 * DMA1 NDTR max = 65535 transfers; a full LV_Z_VDB_SIZE partial buffer
 * (~4600 px, see prj.conf comment) is always under that ceiling in one shot,
 * but chunk defensively anyway in case a caller ever passes a bigger blit.
 */
static void lcd_blit_dma(const uint16_t *src, uint32_t n)
{
	const struct device *dma = DEVICE_DT_GET(DT_NODELABEL(dma1));
	struct dma_config cfg = {
		.channel_direction   = MEMORY_TO_MEMORY,
		.source_data_size    = 2,
		.dest_data_size      = 2,
		.source_burst_length = 1,
		.dest_burst_length   = 1,
		.dma_callback        = dma_cb,
		.complete_callback_en = 1,
	};
	while (n) {
		uint16_t chunk = (n > 65535) ? 65535 : (uint16_t)n;
		struct dma_block_config blk = {
			.source_address  = (uint32_t)src,
			.dest_address    = (uint32_t)LCD_DATA_ADDR,
			.block_size      = (uint32_t)chunk * 2,
			.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
			.dest_addr_adj   = DMA_ADDR_ADJ_NO_CHANGE,
		};
		cfg.head_block = &blk;
		dma_config(dma, 0, &cfg);
		k_sem_reset(&s_dma_done);
		dma_start(dma, 0);
		k_sem_take(&s_dma_done, K_FOREVER);
		src += chunk;
		n   -= chunk;
	}
}
#endif /* CONFIG_DMA && CONFIG_AW2_HW_VALIDATED */

#if defined(CONFIG_AW2_LCD_DEBUG) || (defined(CONFIG_DMA) && defined(CONFIG_AW2_HW_VALIDATED))
static void lcd_fill_pixels(uint32_t n, uint16_t colour)
{
#if defined(CONFIG_DMA) && defined(CONFIG_AW2_HW_VALIDATED)
	if (n >= LCD_DMA_MINPX) {
		lcd_fill_dma(n, colour);
		return;
	}
#endif
	lcd_fill_polled(n, colour);
}
#endif

/* ---- Zephyr display driver API ------------------------------------------
 * Implements struct display_driver_api so LVGL can use this device directly
 * via display_write(). Registered as DT_NODELABEL(ili9341_fsmc).
 */
static int zd_write(const struct device *dev, const uint16_t x, const uint16_t y,
		    const struct display_buffer_descriptor *desc, const void *buf)
{
	ARG_UNUSED(dev);
	if (!s_status.ok) {
		/* No panel detected at init (issue #155) — headless mode. Silently
		 * discard the write instead of dribbling pixels into a disconnected
		 * FSMC bus. LVGL still thinks it has a display and keeps rendering
		 * into its VDB; we just never push it out over FSMC. */
		return 0;
	}
	const uint16_t w = desc->width;
	const uint16_t h = desc->height;
	const uint16_t pitch = desc->pitch;   /* pixels between row starts; may exceed width */
	const uint16_t *p = (const uint16_t *)buf;

	lcd_set_window(x, y, x + w - 1, y + h - 1);   /* ends with RAMWR cmd */
	if (pitch == w) {
		/* Tightly packed — single bulk transfer. This is the path LVGL's
		 * partial-render flush_cb takes (issue #125): one call per VDB
		 * buffer (~4600px/9KB, LV_Z_VDB_SIZE=6 in prj.conf), tightly
		 * packed since LVGL's own draw buffer has no row padding. */
		uint32_t n = (uint32_t)w * h;
#if defined(CONFIG_DMA) && defined(CONFIG_AW2_HW_VALIDATED)
		if (n >= LCD_DMA_MINPX) {
			lcd_blit_dma(p, n);
		} else {
			while (n--) { lcd_dat(*p++); }
		}
#else
		/* Renode / not-yet-hardware-validated: polled path only. */
		while (n--) { lcd_dat(*p++); }
#endif
	} else {
		/* Stride-aligned rows — skip padding between rows */
		for (uint16_t row = 0; row < h; row++) {
			for (uint16_t col = 0; col < w; col++) {
				lcd_dat(p[col]);
			}
			p += pitch;
		}
	}
	return 0;
}

static int zd_blanking_on(const struct device *dev)  { ARG_UNUSED(dev); return 0; }
static int zd_blanking_off(const struct device *dev) { ARG_UNUSED(dev); return 0; }

static void zd_get_capabilities(const struct device *dev,
				struct display_capabilities *caps)
{
	ARG_UNUSED(dev);
	caps->x_resolution      = LCD_W;
	caps->y_resolution      = LCD_H;
	caps->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
	caps->current_pixel_format    = PIXEL_FORMAT_RGB_565;
	caps->current_orientation     = DISPLAY_ORIENTATION_NORMAL;
}

static int zd_set_pixel_format(const struct device *dev,
				const enum display_pixel_format fmt)
{
	ARG_UNUSED(dev);
	return (fmt == PIXEL_FORMAT_RGB_565) ? 0 : -ENOTSUP;
}

static DEVICE_API(display, s_display_api) = {
	.write              = zd_write,
	.blanking_on        = zd_blanking_on,
	.blanking_off       = zd_blanking_off,
	.get_capabilities   = zd_get_capabilities,
	.set_pixel_format   = zd_set_pixel_format,
};

#if defined(CONFIG_AW2_LCD_DEBUG)
static void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour);
static void lcd_color_test(void);
static void lcd_transfer_bench(void);
#endif

/* ---- Zephyr device registration -----------------------------------------
 * Two-phase init: zd_init() at POST_KERNEL marks device ready (no hardware
 * work); lcd_hw_init() at APPLICATION/80 does the actual reset + sequence,
 * before LVGL auto-init at APPLICATION/90 makes the first display_write call.
 */
#define ILI9341_FSMC_NODE DT_NODELABEL(ili9341_fsmc)
#if DT_NODE_EXISTS(ILI9341_FSMC_NODE)
static int zd_init(const struct device *dev)
{
	/* POST_KERNEL init — k_msleep not available (preemption not yet enabled).
	 * Just mark the device ready so LVGL auto-init (APPLICATION/90) can bind it.
	 * Full hardware init happens in lcd_hw_init() at APPLICATION/80. */
	ARG_UNUSED(dev);
	return 0;
}

/* Read ID4 (0xD3) response: dummy, 0x00, 0x93, 0x41.
 * Each *LCD_DATA_ADDR read consumes one byte (8-bit bus, 16-bit FSMC).
 * OEM fw_lcd_read_id discards TWO reads before assembling the ID word. */
static uint16_t lcd_read_id(void)
{
	lcd_cmd(0xD3);
	(void)*LCD_DATA_ADDR;           /* dummy */
	(void)*LCD_DATA_ADDR;           /* 0x00 */
	uint16_t id = (uint16_t)((*LCD_DATA_ADDR & 0xFF) << 8);  /* 0x93 */
	id |= (uint16_t)(*LCD_DATA_ADDR & 0xFF);                  /* 0x41 */
	return id;
}

static int lcd_hw_init(void)
{
	s_status.enabled = true;
	bool renode = a2k_is_renode();
	lcd_reset_pulse(renode);
	ili9341_init_seq(renode);

	uint16_t id = lcd_read_id();

	/*
	 * Issue #155: on this hardware LCD_ID==0 is the confirmed, reproducible
	 * "panel not connected" signature (matches vendor firmware's own
	 * fw_lcd_read_id behaviour observed live with the FPC deliberately
	 * disconnected). This is distinct from a genuinely floating/undriven
	 * FSMC data bus, which (weak pull-up bias, no device pulling it low)
	 * is expected to read back as all-1s (0xFFFF) rather than all-0s — a
	 * different fault signature (bus/wiring fault) that should NOT be
	 * treated the same as "no panel".
	 */
	if (id == 0x0000) {
		s_status.ok  = false;
		s_status.err = -ENODEV;
		LOG_ERR("lcd: LCD_ID=0x0000 — no panel connected (confirmed signature); "
			"continuing headless, display writes will be discarded");
		return 0;   /* not a fatal boot error — keep going without a display */
	}
	if (id == 0xFFFF) {
		s_status.ok  = false;
		s_status.err = -EIO;
		LOG_ERR("lcd: LCD_ID=0xffff — floating/undriven FSMC data bus "
			"(bus or wiring fault, NOT the same as 'no panel'); "
			"continuing headless, display writes will be discarded");
		return 0;
	}
	if (id != 0x9341) {
		LOG_WRN("lcd: unexpected ID 0x%04x (expected 0x9341) — panel present "
			"but unrecognised variant, continuing anyway", id);
	}

	s_status.ok = true;

#if defined(CONFIG_AW2_LCD_DEBUG)
	lcd_color_test();
	lcd_transfer_bench();
#endif

	LOG_INF("lcd: ok, ILI9341 id=0x%04x 320x240 RGB565", id);
	return 0;
}
/* Priority 80 — before LVGL auto-init (priority 90) so display is ready when LVGL binds it. */
SYS_INIT(lcd_hw_init, APPLICATION, 80);
DEVICE_DT_DEFINE(ILI9341_FSMC_NODE, zd_init, NULL, NULL, NULL,
		 POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY, &s_display_api);
#endif

#if defined(CONFIG_AW2_LCD_DEBUG)
static void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour)
{
	lcd_set_window(x, y, x + w - 1, y + h - 1);
	lcd_fill_pixels((uint32_t)w * h, colour);
}

static void lcd_color_test(void)
{
	/* Corner blocks — confirmed orientation and RGB byte order in Renode PNG
	 * before touching real hardware. 40x40 at each corner + centre. */
	lcd_fill_rect(          0,           0, 40, 40, 0xF800); /* top-left:     RED   */
	lcd_fill_rect(LCD_W - 40,           0, 40, 40, 0x07E0); /* top-right:    GREEN */
	lcd_fill_rect(          0, LCD_H - 40, 40, 40, 0x001F); /* bottom-left:  BLUE  */
	lcd_fill_rect(LCD_W - 40, LCD_H - 40, 40, 40, 0xFFFF); /* bottom-right: WHITE */
	lcd_fill_rect(LCD_W/2 - 20, LCD_H/2 - 20, 40, 40, 0xFFE0); /* centre:  YELLOW */
	LOG_INF("lcd: color test drawn");
}

static void lcd_transfer_bench(void)
{
	/* Crossover bench — issue #54. 1000 iterations each, total cycles / 1000.
	 * First row where dma_avg < polled_avg → set CONFIG_LCD_DMA_MINPX to that N. */
#define BENCH_ITERS 1000
	static const uint16_t sizes[] = {1, 2, 3, 4, 8, 16, 32, 64, 128, 256, 320, 0};
	const uint16_t colour = 0x1234;
	uint32_t t0, t1;

	LOG_INF("lcd_bench  n    polled(cy)    dma(cy)   winner  (%u iters, cy=CPU cycles)", BENCH_ITERS);
	for (int i = 0; sizes[i]; i++) {
		uint16_t n = sizes[i];
		uint32_t cy_polled, cy_dma;

		lcd_set_window(0, LCD_H - 1, n - 1, LCD_H - 1);
		t0 = k_cycle_get_32();
		for (int r = 0; r < BENCH_ITERS; r++) { lcd_fill_polled(n, colour); }
		t1 = k_cycle_get_32();
		cy_polled = (t1 - t0) / BENCH_ITERS;

#if defined(CONFIG_DMA) && defined(CONFIG_AW2_HW_VALIDATED)
		lcd_set_window(0, LCD_H - 1, n - 1, LCD_H - 1);
		t0 = k_cycle_get_32();
		for (int r = 0; r < BENCH_ITERS; r++) { lcd_fill_dma(n, colour); }
		t1 = k_cycle_get_32();
		cy_dma = (t1 - t0) / BENCH_ITERS;
		LOG_INF("           %4u  %10u  %8u   %s",
			n, cy_polled, cy_dma,
			(cy_dma < cy_polled) ? "DMA  <--" : "polled");
#else
		LOG_INF("           %4u  %10u  (no DMA)", n, cy_polled);
#endif
	}
	LOG_INF("lcd_bench  done");
}
#endif /* CONFIG_AW2_LCD_DEBUG */

const a2k_drv_t *lcd_status(void) { return &s_status; }

#endif /* CONFIG_AW2_LCD */
