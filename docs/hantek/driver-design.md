# 2D82 — Zephyr firmware driver design

**Principle:** OEM firmware is reference material only — it tells us what hardware
exists and how it's wired. Every driver is written from scratch using standard Zephyr
APIs. No OEM register sequences, no OEM logic is copied.

Informed by:
- `vendors/hantek/2d82/reference/dso2000/vendors/hantek/2d82/ghidra/ghidra_names_2d82.tsv` (source of truth for all RE)
- `vendors/hantek/2d82/firmware/2d82-20210818/analysis/decompiled_named.c` (OEM RE)
- `vendors/hantek/2d82/docs/hardware/devices/hantek-2000-MCU-FPGA-SPI-PROTOCOL.md` (FPGA register map)
- `~/git/esp32-zephyr/shared/awto_board/` (reference patterns: buzzer, encoder, watchdog)

---

## Module layout

```
code/
  first_stage/                <- current: minimal bring-up shell
    src/main.c                <- sequential init + supervisor_run()
    boards/hantek_2d82/       <- board DTS + defconfig (real copy, not symlink)
    prj.conf

  hantek_board/               <- shared Zephyr module (to be created)
    CMakeLists.txt
    Kconfig
    include/hantek_board/
      drv.h                   <- hantek_drv_t status struct + getters
    src/
      supervisor.c            <- k_msgq + message types + supervisor_run()
      lcd.c                   <- ILI9341 via FSMC, Zephyr display API
      fpga.c                  <- MachXO2 SPI1 runtime protocol
      awg.c                   <- DDS AWG via FPGA registers (split from fpga.c)
      dmm.c                   <- CS7721 USART1@2400 + bit-bang control
      usb_scope.c             <- USB bulk device USB protocol
      backlight.c             <- TIM1_CH1 PA8 PWM
      battery.c               <- ADC1 -> voltage divider -> level 0-4
      calibration.c           <- NVS load/save 0x130 bytes
      probe_comp.c            <- probe compensation GPIO output + sense
```

**Two EXTRA_ZEPHYR_MODULES in first_stage CMakeLists.txt:**
```cmake
list(APPEND EXTRA_ZEPHYR_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/../hantek_board
    ~/git/esp32-zephyr/shared/awto_board)
```

`awto_board` activates where DT nodes exist — it provides buzzer (via `buzzer` alias),
rotary encoder (via `awto,gpio-encoder` compatible), watchdog (`wdt0` + `CONFIG_AWTO_WATCHDOG=y`),
and heartbeat LED. No code duplication.

**DT gate rule:** every `.c` file in `hantek_board/src/` compiles to nothing unless its
DT alias or node exists. Missing pin = missing DT node = code compiles out cleanly.
`CONFIG_HANTEK_HW_VALIDATED` is the only `#ifdef` needed (see below).

---

## Driver status struct

Every `hantek_board` driver exposes:

```c
/* include/hantek_board/drv.h */
typedef struct {
    const char *name;
    bool        enabled;   /* compiled + DT node present */
    bool        ok;        /* init succeeded */
    int         err;       /* last error (0 = none) */
} hantek_drv_t;

/* Each driver: */
const hantek_drv_t *lcd_status(void);
const hantek_drv_t *fpga_status(void);
/* etc. */
```

Dependents gate on `->ok` before using a driver. LVGL init checks `lcd_status()->ok`.
`awg_init()` checks `fpga_status()->ok` and skips if FPGA failed.

---

## Boot sequence — main.c

Sequential bring-up. Logged to UART (fast, no enumeration wait). USB CDC ACM added
late — it's slow to enumerate and should not block init.

```
main()
  uart_log_init()       <- FIRST: UART1 log sink, instant
  supervisor_init()     <- msgq setup
  backlight_init()      <- PA8 on — screen is about to light up
  lcd_init()            <- SECOND: ILI9341 FSMC, show boot screen immediately
  [IF CONFIG_HANTEK_LCD_DEBUG]: lcd_color_test()   <- corner blocks, Renode-verified first
  cfb_init()            <- pre-LVGL text, shows boot progress on screen
  [LATER: lvgl_init()]  <- replaces cfb when LVGL is ready
  buzzer_init()         <- awto_board buzzer activates from DT alias
  fpga_init()
  awg_init()            <- gates: fpga_status()->ok
  keypad_init()         <- gpio-kbd-matrix DT node
  battery_init()        <- DT gate: compiles out if ADC channel unknown
  dmm_init()            <- DT gate: compiles out pending issue #45
  calibration_load()    <- NVS read, apply to FPGA regs
  usb_cdc_init()        <- LATE: non-blocking, USB CDC ACM console
  usb_scope_init()      <- USB bulk endpoints, device USB protocol
  supervisor_run()      <- never returns
```

Each step logs its result: `LOG_INF("lcd: ok, ILI9341 320x240")` or
`LOG_ERR("fpga: no response, SPI timeout")`.

### Log backend strategy

- **Dev:** `CONFIG_LOG_BACKEND_UART=y` — UART1 at 115200, immediate
- **Prod:** `CONFIG_LOG_BACKEND_RING_BUF=y` — circular buffer in RAM, flushed to
  USB CDC ACM shell on demand via `log backend` shell command
- USB CDC ACM is the shell transport, not the log sink. Enumeration costs 200–500ms;
  init must not block on it.

---

## Supervisor — post-init event loop

After all `_init()` calls complete, `supervisor_run()` is the permanent main loop.

```c
enum hantek_msg_type {
    MSG_ERROR = 0,       /* highest priority */
    MSG_KEYPAD,
    MSG_USB_CMD,
    MSG_ESP_CMD,
    MSG_CAPTURE_DONE,
    MSG_DMM_READING,
    MSG_BATTERY_LOW,
};
```

Queue is drained each iteration, scanned highest-priority-first. FPGA capture DMA
manages its own fast path internally (wakes a render thread directly). Only control
messages (capture done, FPGA error, battery threshold crossed) enter the supervisor queue.

---

## Renode-first development strategy

All drivers are written targeting Renode first. The existing Renode infrastructure is
already comprehensive:

- `smart_lcd.repl` — Python peripheral decoding ILI9341 FSMC commands into a 240×320
  framebuffer, written to PNG. `hantek_emu.py render` shows the result.
- `ILI9341FSMC.cs` — C# Renode peripheral for `hantek_emu.py gui` live window.
- `hantek_emu.py key <code>` — inject keypad events.
- GPIO, timers (TIM1, TIM8), UART modeled in `stm32f103_lcd.repl`.

**Renode full-speed fixes needed** (Python peripherals, same pattern as `rcc_fix.repl`):

| File | Peripheral | Why needed |
|------|-----------|------------|
| `spi1_fix.repl` | SPI1 @ 0x40013000 | Zephyr STM32 SPI polls `SR_BSY`/`SR_RXNE` — SVD returns 0 → hang |
| `adc_fix.repl` | ADC1 @ 0x40012400 | Zephyr ADC polls `SR_EOC` — never set → hang |
| `renode_detect.repl` | sentinel @ 0x50000000 | returns `0x524E4F44` ('RNOD') — unmapped on STM32F103 |

**`CONFIG_HANTEK_HW_VALIDATED=y`** — the only `#ifdef` in driver code. Gates paths
that are genuinely wrong or meaningless in Renode:
- Real FPGA capture burst (stub returns synthetic sine; `HW_VALIDATED` enables real acquisition)
- DMM USART real readings
- Battery ADC auto-zeroing against real supply
- Calibration flash writes (don't overwrite NVS with synthetic ADC values in Renode)

Everything else (LCD, LVGL, buzzer, keypad, supervisor, NVS) runs identically in
Renode and hardware. No other `#ifdef` needed.

**Renode detection at runtime** (`CONFIG_HANTEK_RENODE_DETECT=y`):
```c
/* supervisor.c — called once at boot, result stored in g_is_renode */
bool hantek_is_renode(void) {
    /* 0x50000000: unmapped on STM32F103 (bus error), returns 0x524E4F44 in Renode.
     * SCB->CCR BFHFNMIGN lets us probe it without taking a fault. */
    volatile uint32_t *sentinel = (volatile uint32_t *)0x50000000;
    uint32_t cfsr_before = SCB->CFSR;
    SCB->CCR |= SCB_CCR_BFHFNMIGN_Msk;
    __DMB();
    uint32_t val = *sentinel;
    __DMB();
    SCB->CCR &= ~SCB_CCR_BFHFNMIGN_Msk;
    bool fault = (SCB->CFSR != cfsr_before);
    SCB->CFSR = SCB->CFSR;
    return !fault && (val == 0x524E4F44U);
}
```

---

## Implementation table

| Step | Module | Target | Blocker |
|------|--------|--------|---------|
| 1 | hantek_board | Scaffold: `CMakeLists.txt`, `Kconfig`, `drv.h` | none |
| 2 | hantek_board | `supervisor.c` — msgq, types, init + run skeleton | none |
| 2a | renode | `spi1_fix.repl` + `adc_fix.repl` + `renode_detect.repl` | none |
| 3 | first_stage | Add both modules to CMakeLists.txt; UART log backend | none |
| 4 | DTS | TIM1 PWM node + `backlight` alias | none |
| 5 | hantek_board | `backlight.c` — TIM1_CH1 PA8, `hantek_drv_t` | step 4 |
| 6 | DTS | TIM8 PWM node + `buzzer` alias → awto_board buzzer activates | none |
| 7 | DTS | ILI9341 FSMC node | none |
| 8 | hantek_board | `lcd.c` — ILI9341 via FSMC, Zephyr `display` API — **see delay note** | step 7 |
| 8a | hantek_board | `CONFIG_HANTEK_LCD_DEBUG` — `lcd_color_test()` corner blocks, Renode PNG | step 8 |
| 8b | first_stage | CFB `CONFIG_CHARACTER_FRAMEBUFFER=y`, boot progress text on screen | step 8 |
| 9 | DTS | NVS flash partition (calibration slot, 0x130 bytes) | none |
| 10 | DTS | `gpio-kbd-matrix` node (all row/col pins confirmed) | none |
| 11 | hantek_board | `fpga.c` — SPI1, 5 cmd types, `hantek_drv_t` | SPI1 DTS |
| 12 | hantek_board | `awg.c` — DDS freq word, amplitude, waveform via FPGA regs | step 11 |
| 13 | first_stage | `main.c` — full sequential init, boot progress bar via CFB | steps 1–12 |
| 14 | first_stage | LVGL replaces CFB: `CONFIG_LVGL=y`, `lv_conf`, `lv_indev` | step 8 + step 10 |
| 15 | hantek_board | `battery.c` — ADC1 → mV → level → `MSG_BATTERY_LOW` | DT gate (pin unknown) |
| 16 | hantek_board | `dmm.c` — CS7721 USART1@2400 + bit-bang | issue [#45](https://github.com/awtoau/awto-2000/issues/45) → DT gate |
| 17 | hantek_board | `calibration.c` — NVS load/save 0x130 bytes, apply on load | step 9 |
| 18 | hantek_board | `usb_scope.c` — USB bulk, device USB protocol, `MSG_USB_CMD` | USB config |
| 19 | hantek_board | FPGA fast path — DMA capture, render thread, `MSG_CAPTURE_DONE` | steps 11 + 8 |
| 20 | awto_board | Rotary encoder — `awto,gpio-encoder` DT node (polled, not IRQ) | encoder pins → DT gate |
| 21 | hantek_board | `probe_comp.c` — GPIO square-wave + sense | pin → DT gate |
| 22 | hantek_board | Power-hold GPIO — assert at boot or device powers off | pin → DT gate |
| 23 | awto_board | `CONFIG_AWTO_WATCHDOG=y` → watchdog on `wdt0` | `wdt0` DT node |

---

## Per-driver design

### lcd.c — ILI9341 via FSMC

**DT gate:** `DT_NODE_HAS_STATUS(DT_NODELABEL(ili9341), okay)`

**Zephyr subsystem:** `display` API (`display_write`, `display_blanking_on/off`, `display_get_capabilities`)

**What it covers:**
- FSMC Bank1: cmd @ `0x60000000` (RS=0/A16=0), data @ `0x60020000` (RS=1/A16=1)
- ILI9341 init sequence — see delay note below
- Reset pulse: PD3 active-low, 5ms after deassertion before first command
- Pixel format: RGB565 16-bit
- MADCTL: set for landscape 320×240 directly (do not rely on software rotation)
- `display_write()` → CASET/PASET/RAMWR pixel burst
- Returns `0x9341` for chip ID read — works in Renode via `smart_lcd.repl` (Python
  peripheral, used by `render`/`start`) and via the C# `ILI9341FSMC.cs` peripheral
  (used by `gui`/`headless`)
- **Missing-panel detection (issue #155, implemented):** `lcd_hw_init()` reads ID4
  (cmd `0xD3`) at init. `LCD_ID==0x0000` is the confirmed "no panel connected"
  signature on this hardware (matches vendor firmware's own observed behaviour with
  the LCD FPC disconnected) — driver sets `lcd_status()->ok = false` and returns
  without erroring the boot; `zd_write()` then silently discards all display writes
  so LVGL keeps running headless instead of dribbling pixels into a disconnected
  FSMC bus. `LCD_ID==0xFFFF` is treated as a *different* fault (floating/undriven
  bus — weak pull-up bias reads all-1s with nothing pulling the bus low) and logged
  distinctly, also headless. Test hook: `hantek_emu.py gui|headless --no-panel`
  sets the C# peripheral's `PanelConnected=false` before `start` to exercise this
  boot path in Renode; `hantek_emu.py lcd-panel {connect|disconnect}` toggles it on
  an already-running session (affects any later ID re-read only).

**No Zephyr FSMC driver for STM32F1** — use direct `*((volatile uint16_t*)0x60000000)`.

**LCD init sequence — delay note:**
> The canonical ILI9341 init sequences from Adafruit, Sipext, and Arduino libs are
> doubly corrupted: (1) bogus delays cargo-culted from 2011 early-silicon workarounds,
> and (2) shuffled command order from copies-of-copies that worked by luck.
>
> **Only two delays are mandatory per the ILI9341 datasheet:**
> - 5ms after RESET# deassertion before first command
> - 120ms after `SLPOUT` (0x11) before `DISPON` (0x29)
>
> Start with these two only. Add back only what a scope confirms is needed on this
> specific panel. Expect <50ms total init on modern silicon.
>
> **Also check command ordering:** `SLPOUT` must precede `DISPON`; `COLMOD` (pixel
> format) must be set before any `RAMWR`. Don't assume any reference sequence has the
> right order — derive from the datasheet. Cross-check `fw_lcd_init_seq` @
> `0x0800c70a` for what the OEM firmware sends, but verify each command against the datasheet.
>
> `fw_lcd_init_seq` sends: sleep-out → wait → display-on → MADCTL → COLMOD.
> That order matches the datasheet. COLMOD = `0x55` for RGB565 (confirmed from
> FSMC emulation). MADCTL byte from OEM firmware: check `fw_lcd_init_seq` disassembly.

**LCD debug mode (`CONFIG_HANTEK_LCD_DEBUG=y`):**
```c
void lcd_color_test(void) {
    /* 40x40 corner blocks — confirms MADCTL orientation + RGB byte order in Renode PNG */
    lcd_fill_rect(0,   0,   40, 40, 0xF800); /* top-left:     RED   */
    lcd_fill_rect(280, 0,   40, 40, 0x07E0); /* top-right:    GREEN */
    lcd_fill_rect(0,   200, 40, 40, 0x001F); /* bottom-left:  BLUE  */
    lcd_fill_rect(280, 200, 40, 40, 0xFFFF); /* bottom-right: WHITE */
    lcd_fill_rect(140, 100, 40, 40, 0xFFE0); /* centre:       YELLOW */
}
```
Run in Renode first: `hantek_emu.py render` → PNG shows five blocks → confirms orientation
before hardware. Same binary runs on hardware.

---

### supervisor.c — event loop

**No DT gate** — always compiled.

**Message queue:** `K_MSGQ_DEFINE(hantek_msgq, sizeof(struct hantek_msg), 32, 4)`

**`supervisor_run()`:** drains queue each tick, scans `MSG_ERROR` first, then in
priority order. High-speed paths (FPGA DMA capture → render thread) bypass the queue.

---

### fpga.c — MachXO2 SPI runtime interface

**DT gate:** `DT_NODE_HAS_STATUS(DT_NODELABEL(fpga), okay)`

**Zephyr subsystem:** `spi` API (SPI1, master)

**5 command types** (from SPI protocol doc):
- `0xa8` WRITE_REG, `0xc8` WRITE_CTRL, `0xa0` READ_REG (double preamble),
  `0xa4` BURST_READ (1026 bytes), `0x50` AWG_TABLE (1026 bytes)

**Key facts from RE:**
- SPI1: PA5/PA6/PA7, CS=PA4 active-low (runtime), CS=PB4 (SSPI config only)
- Init SPI clock: BR=7 → fPCLK2/256 = 281 kHz (from SPI1 CR1 = 0x037C)
- Relay double-write: write byte, wait 50ms, write `byte & 0xf6`
- Capture state machine: ARM → POLL bit1 → read end-addr → BURST_READ 6144 bytes

**In Renode:** `spi1_fix.repl` makes SPI1 appear ready. `fpga_spi_stub.repl` logs
all transactions and returns synthetic sine data for BURST_READ.

**`CONFIG_HANTEK_HW_VALIDATED`** gates real acquisition mode (synthetic data in Renode).

---

### awg.c — DDS AWG via FPGA

**DT gate:** same as fpga.c (gates on `fpga_status()->ok`)

**From OEM RE:** DDS frequency word = `freq_hz * 4294967296.0 / 250000000.0` (250 MHz DDS
clock, 32-bit accumulator; 25 MHz crystal × FPGA PLL; confirmed from `dbl_dds_freq_scale`/`dbl_dds_div_2p5` constants in `fw_awg_apply_freq`). 6 waveform types: sine, square, triangle, sawtooth, pulse
(narrow/wide), noise. Amplitude and offset set via FPGA registers.

---

### backlight.c — TIM1_CH1 PA8

**DT gate:** `DT_NODE_EXISTS(DT_ALIAS(backlight))` + `CONFIG_PWM`

**Pattern:** identical to `awto_board/src/buzzer.c` — `PWM_DT_SPEC_GET(DT_ALIAS(backlight))`
+ `pwm_set_dt()`. Shell: `backlight <0-100>`.

**DTS:** `pwm-leds` child with TIM1 CH1 PA8, pinctrl `tim1_ch1_pwm_out_pa8` (confirmed
label in `stm32f103v(c-d-e)tx-pinctrl.dtsi`). Alias `backlight`.

**Confirmed:** PA8 = TIM1_CH1 from `fw_tim1_pwm_init` @ 0x0802fa50.

---

### buzzer.c — from awto_board, TIM8_CH2 PC7

**Provided by:** `awto_board/src/buzzer.c` — activates when `buzzer` DT alias exists.

**DTS:** `pwm-buzzers` child with TIM8 CH2 PC7, pinctrl `tim8_ch2_pwm_out_pc7` (confirmed
in `stm32f103v(c-d-e)tx-pinctrl.dtsi`). Alias `buzzer`.

**Confirmed:** PC7 = TIM8_CH2, 3030 Hz native (PSC=719 ARR=32) from `fw_tim8_buzzer_init`
@ 0x0802fb72.

---

### battery.c — ADC1 battery sense

**DT gate:** `DT_NODE_EXISTS(DT_ALIAS(battery))` (compiles out until ADC channel pin known)

Reads ADC1 channel N → voltage divider → mV → level 0–4 (matching OEM
`fw_gfx_draw_battery` 0=empty/red, 1–4=green bars) → sends `MSG_BATTERY_LOW` to
supervisor queue when below threshold.

**In Renode:** `adc_fix.repl` returns fixed mid-scale ADC value. `CONFIG_HANTEK_HW_VALIDATED`
gates auto-zeroing calibration against real supply.

**ADC channel pin:** not in OEM code — needs PCB trace.

---

### dmm.c — CS7721CN-1

**DT gate:** `DT_NODE_HAS_STATUS(DT_NODELABEL(dmm), okay)`

USART1 @ 2400 baud for RX. Bit-bang control via GPIO.

**Pin uncertainty:** tracked in issue [#45](https://github.com/awtoau/awto-2000/issues/45):
- DMM data: `ptr_dmm_gpio_data` → `0x40011800` = GPIOE → likely **PE0**, not PB0
- DMM clock: `ptr_dmm_gpio_clk` → GPIOB bit4 = PB4 (same pin as FPGA_SSPI_CS, dual-use)

DT gate handles this: code compiles out until the DTS aliases are confirmed and added.

---

### calibration.c — NVS 0x130 bytes

**DT gate:** NVS flash partition in DTS

Loads calibration record on boot, applies to FPGA registers. Saves on user request or
after auto-calibration sequence. `CONFIG_HANTEK_HW_VALIDATED` gates write-to-flash
(Renode: load only, do not write synthetic cal data).

---

### usb_scope.c — device USB protocol

**DT gate:** `CONFIG_USB_DEVICE_STACK` + USB node

VID 0x0483 PID 0x2d42. EP 0x81 IN bulk (scope data), EP 0x02 OUT bulk (commands).
9-byte command frame → response selector 0x00–0x17. Waveform readback: 6144 bytes via
`fpga.c` capture. Routes `MSG_USB_CMD` to supervisor queue for control commands.

---

## Rotary encoder — from awto_board

**From `awto_board/src/encoder.c`:** pure-polling quadrature, 2ms poll interval, no IRQ.

**Why polled:** the awto_board encoder.c documents why (and this applies here too):
GPIO IRQ on a shared port crashed the S3 when the encoder interrupt fired during display
activity. Check if the encoder pins share a GPIO port with FSMC data lines before
deciding interrupt vs poll. If they do, use the polling thread.

**DT gate:** `awto,gpio-encoder` compatible node — activates when encoder pins are known.

---

## What is NOT a driver

| Thing | Why |
|-------|-----|
| Key matrix scan | `gpio-kbd-matrix` in-tree Zephyr driver, fires `input` events |
| LVGL UI | App layer above `display` API |
| System clock 72MHz | DTS + stm32f1 SoC layer |
| DMA | Used internally by lcd.c / fpga.c via Zephyr DMA API |
| FPGA bitstream programming | One-shot tool, not a runtime driver |
| Scope waveform math | Application layer |

---

## Key function map — derived from OEM firmware

`fw_ch_menu_key_dispatch` and `fw_awg_menu_key_dispatch`. Physical silkscreen labels
need hardware; *functions* are known from code.

| key_code | row/col | Inferred function |
|----------|---------|-------------------|
| 1–4 | row 0 | not yet traced |
| 5 | 1,0 | Channel param apply |
| 6 | 1,1 | AWG mode enable (`fw_awg_mode_enable`) |
| 7 | 1,2 | Probe factor select |
| 8 | 1,3 | Trigger menu (`fw_ui_switch_trigger_menu`) |
| 9 | 2,0 | Mode select (`fw_ui_handle_mode_select`) |
| 10 | 2,1 | Trigger panel refresh |
| 11 | 2,2 | Coupling toggle |
| 12 | 2,3 | Trigger menu switch |
| 13 | 3,0 | not yet traced |
| 14 | 3,1 | Mode select / coupling |
| 15 | 3,2 | Trigger level apply |
| 16–20 | row 3–4 | not yet traced |

Keypad rows: PD13, PD12, PC3, PB14, PB13. Cols: PB6, PB5, PC6, PB12 (all confirmed RE).

---

## Confirmed hardware — no traces needed

| Item | Value | Source |
|------|-------|--------|
| Buzzer | PC7 = TIM8_CH2, 3030 Hz | `fw_tim8_buzzer_init` @ 0x0802fb72 |
| Backlight | PA8 = TIM1_CH1 | `fw_tim1_pwm_init` @ 0x0802fa50 |
| Keypad rows | PD13, PD12, PC3, PB14, PB13 | `fw_keypad_scan_matrix` |
| Keypad cols | PB6, PB5, PC6, PB12 | `fw_keypad_scan_matrix` |
| SPI1 pins | PA5/PA6/PA7 + PA4 CS (runtime), PB4 CS (SSPI) | `fw_periph_gpio_spi_init` |
| SPI1 clock | BR=7 → 281 kHz init | SPI1 CR1 = 0x037C |
| FPGA protocol | 5 cmd types, regs 0x00–0x1e | SPI protocol doc |
| FSMC timing | BCR1=0x1099 | `fw_fsmc_timing_write` |
| USB | PA11/PA12, VID 0x0483 PID 0x2d42 | `fw_usb_gpio_init` |
| USART1 debug | PA9 TX, PA10 RX, 115200 baud | `fw_usart_115200_init` |
| USART1 DMM RX | PA10, 2400 baud | `fw_dmm_usart_init` |
| LCD reset | PD3 active-low, 100µs pulse | `fw_lcd_gpio_pulse` |
| LCD format | RGB565, cmd@0x60000000 data@0x60020000 | FSMC-confirmed |
| AWG DDS clock | 250 MHz, 32-bit accumulator | `fw_awg_apply_freq` |
| Calibration | 0x130 bytes in internal flash | `fw_cali_save_to_flash` |

## Still needs hardware

| Item | Status |
|------|--------|
| ADC1 channel pins (battery) | Not in OEM code — PCB trace needed |
| DMM data pin | Likely PE0 (see issue #45) |
| Rotary encoder pins | Not traced yet |
| Power-hold GPIO | Not traced yet — critical, device powers off without it |
| Probe comp GPIO | Not traced yet |
| Keypad silkscreen labels | Nice-to-have; functions known from firmware |
