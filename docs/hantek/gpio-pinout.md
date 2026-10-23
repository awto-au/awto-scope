# STM32 GPIO Pinout — Hantek 2D82AUTO

**MCU:** Geehy APM32F103VET6 (STM32F103VET6-compatible, 100-pin LQFP)
**Clock:** 8 MHz HSE → PLL ×9 → 72 MHz SYSCLK, APB1=36 MHz, APB2=72 MHz

All pin assignments extracted from `decompiled_named.c` firmware analysis.
Confidence: ✓ = confirmed from code trace; ? = inferred from context; no ref = never referenced in firmware; trace = referenced in code but function unknown, needs Ghidra code trace.

---

## GPIOA (base 0x40010800)

| Pin | Function | AF / Peripheral | Confidence | Source |
|-----|----------|----------------|------------|--------|
| PA0 | — | GPIO input pull-up/down | trace | `boot_gpio_init` configures as input (mask=1, CNF=0x28). `boot_main` polls LOW before DFU check — likely power button sense. `fw_gpio_dmm_gpio_init` also configures same pin. |
| PA1 | — | GPIO input pull-up/down | no ref | `fw_fpga_spi_gpio_init` sets CNF=0x48 as side-effect of SPI1 pin sweep. All 22 `fw_gpio_read_input_bit` call sites confirmed — none read GPIOA bit1. |
| PA2 | USART2_TX | USART2 AF | ? inferred | `fw_dmm_usart_init` calls `fw_gpio_pin_init({0x26,1,3,1})` which appears to configure this pin as USART2 AF; PA2 is the STM32F103 default USART2_TX. CS7721 is one-way (data out only) so TX is configured but never transmits. |
| PA3 | USART2_RX | USART2 AF | ✓ | CS7721 serial data output — 2400 baud 8N1, hardware USART2 (`fw_dmm_usart_init`) |
| PA4 | FPGA_SPI1_CS | GPIO output | ✓ | `fw_fpga_cs_low/high` — FPGA runtime CS, active-LOW |
| PA5 | FPGA_SPI1_SCK | SPI1 AF | ✓ | `fw_spi1_init` — FPGA-only bus, CPOL=0 CPHA=0 BR=÷2 (36 MHz) |
| PA6 | FPGA_SPI1_MISO | SPI1 AF | ✓ | `fw_spi1_init` |
| PA7 | FPGA_SPI1_MOSI | SPI1 AF | ✓ | `fw_spi1_init` |
| PA8 | BACKLIGHT_PWM | TIM1_CH1 AF | ✓ | `fw_tim1_pwm_init`: PA8 (mask=0x100) AF push-pull 50MHz. TIM1 CH1, CCR=600, PSC=650, ARR set dynamically. `fw_backlight_pwm_set` calls `fw_tim_oc_ch1_init(TIM1,...)` to update duty. `fw_tim4_pwm_init` does NOT touch PA8 — it only enables TIM4 clock + IRQ#30. |
| PA9 | USART1_TX | USART1 AF | ✓ | `fw_usart1_gpio_init` — debug 115200 baud |
| PA10 | USART1_RX | USART1 AF | ✓ | `fw_usart1_gpio_init` — debug 115200 baud |

> **Bench note (physical debug header):** connecting a USB-serial adapter to the physical 4-pin USART1 header only reads clean idle (no framing, no data) with the AFE/DMM board FPC cable **connected**. Disconnecting that FPC injects noise/glitches onto the captured line (non-periodic garbage bytes, no valid UART framing) — likely coupling from the shared DMM serial bus (PE0/PC4, see `DMM_SR_DATA` below) onto the USART1 lines when the FPC is unseated. Keep the AFE FPC connected during any USART1 bench capture.
>
> **Confirmed working capture command:**
> ```
> tio -b 115200 --log --log-file tmp/tio_session.log /dev/ttyUSB2
> ```
> With the AFE FPC connected, GND/TX/RX wired correctly (adapter TX→PA10/RX, adapter RX→PA9/TX, per the code trace — NOT a straight-through pin mapping), this streams a continuous repeating debug telemetry line (no RX/command handling seen — see below):
> ```
> ******BAT_Voltage = 0.000000,ADC_ConvertedValue[0] = 0, ADC_ConvertedValue[1] = 1587
> ```
> Observed **with no battery installed** (USB/bench power only) — `BAT_Voltage = 0.000000` and `ADC_ConvertedValue[0] = 0` are consistent with the battery-sense ADC channel (PC5/`BATT_SENSE`) reading zero with no cell present; `ADC_ConvertedValue[1] = 1587` is presumably the VREFINT reference channel and stayed constant.
>
> **LOCATED 2026-07-14 (issue #163).** The bench unit is running the `2d82-20260713-swd` build, not the `2d82-20210818` reference build (confirmed build-mismatch, see the SWD probe note elsewhere in this repo) — the `BAT_Voltage` string only exists in `2d82-20260713-swd` and `2d72-20210909`, not in the `2d82-20210818` reference image (grep-confirmed against all raw `.bin`s). Emitting function: **`fw_battery_voltage_update_swd`** (`0x08014B34` in the swd build), renamed from a misleading auto-generated `fw_awg_param_screen_apply` guess — it is this build's equivalent of the reference build's `fw_battery_voltage_update` (`0x0801622C`), with an added USART1 debug printf not present in the reference build. **Refutes** the earlier hypothesis: the print is not called from a separate function alongside `fw_battery_voltage_update` in the same tick — it fires *from within* the tick's battery-voltage-update function itself (sole caller: `fw_app_main`, unconditional, immediately after `fw_idle_timer_handler(...)`, byte-for-byte matching the reference build's main-loop call-site shape). Call chain: `fw_battery_voltage_update_swd` → `fw_debug_printf` (`0x08017DCC`, itself renamed from a wrong `fw_math_double_to_int32` guess — it's a vsnprintf-style wrapper) → `fw_printf_format_dispatch` (`0x08018998`) → `fw_debug_putc_callback` (`0x08040148`, new function boundary created for this) → `fw_usart_putchar_poll` (`0x08040160`) → USART1 DR (base `0x40013800`, confirmed via literal read, not USART2/DMM). **ADC channels confirmed**: `ADC_ConvertedValue[0]` = ADC1 channel 15 (`0xf`, PC5/`BATT_SENSE`), `ADC_ConvertedValue[1]` = ADC1 channel 17 (`0x11`, VREFINT) — read via `fw_adc_channel_seq_cfg(ADC1, ch, ...)` inside `fw_adc_dma_sample_read`/`fw_adc_dma_sample_average`, matching the reference build's `g_adc1_dma_buf` CH15/CH17 layout exactly. Full trace + TSV entries: `vendors/hantek/2d82/ghidra/ghidra_names_2d82_swd.tsv` (search "issue #163").
>
> **RX is one-way only:** the only two references to `mmio_USART1` base (0x40013800) in the whole decompiled firmware are inside `boot_usart_send` (TX-only, polls TXE). No USART1 RX read / ISR / command parser has been found — RXNEIE is enabled by `fw_usart_115200_init` but nothing appears to consume received bytes. Typing into the terminal is not expected to do anything to the board; unconfirmed whether a USART1 IRQ vector exists that hasn't been traced yet.
| PA11 | USB_DM (D−) | USB FS | ✓ | `fw_usb_gpio_init` |
| PA12 | USB_DP (D+) | USB FS | ✓ | `fw_usb_gpio_init` |
| PA13 | SWDIO | SWD | ✓ | AFIO_MAPR written but SWJ_CFG not changed from reset — full SWD+JTAG remains active. PA13=SWDIO confirmed by STM32 reset default + no remap in firmware. On-board 4-pin SWD header: Pin 3 (see [board.md](board.md)). |
| PA14 | SWDCK | SWD | ✓ | PA14=SWDCK confirmed by STM32 reset default + no SWJ remap in firmware. On-board 4-pin SWD header: Pin 2 (see [board.md](board.md)). |
| PA15 | — | — | no ref | never referenced in firmware |

---

## GPIOB (base 0x40010C00)

| Pin | Function | AF / Peripheral | Confidence | Source |
|-----|----------|----------------|------------|--------|
| PB0 | DMM_U600_RCLK | GPIO output | ✓ | `fw_dmm_bit_send_clk` — RCLK latch for U600 (74HC595, AFE board). U600 outputs shorted in groups → 5 optocouplers → CS7721 MEA1-4 + SELECT. Shared serial bus with U9: same PE0/PC4 data/clock, different latch pin. |
| PB1 | — | — | no ref | never referenced in firmware |
| PB2 | BOOT1 | — | ? | STM32 standard (tied LOW on this board) |
| PB3 | — | — | no ref | never referenced in firmware |
| PB4 | FPGA_SPI1_SSPI_CS | GPIO output | ✓ | `fw_fpga_sspi_init` / `fw_fpga_send_block` — FPGA bitstream programming CS (SSPI mode), shares PA5/6/7 with runtime access |
| PB5 | KEYMAT_COL_B | GPIO input | ✓ | `fw_keypad_scan_matrix` |
| PB6 | KEYMAT_COL_A | GPIO input | ✓ | `fw_keypad_scan_matrix` |
| PB7 | USB_POWER_FLAG | GPIO input | ✓ | Read by `fw_usb_power_sense_update` into `g_usb_power_flag` (SRAM 0x200023d9). HIGH=USB power ready; LOW=no USB power. Gated on PC2=USB_VBUS_SENSE. Used by `fw_battery_voltage_update` to select charging icon vs battery level display. |
| PB8 | — | GPIO output | trace | `fw_led_and_pb8_gpio_init` configures (mask=0x100) then resets LOW. Called after LCD is up — NOT LCD. Function unknown |
| PB9 | — | GPIO output | trace | `fw_lcd_gpio_periph_init` configures (mask=0x200, CNF=0x10, mode=3) — return value encodes this. `fw_gpio_lcd_cs_high` sets HIGH. Function unknown — in LCD init path |
| PB10 | — | — | no ref | never referenced in firmware |
| PB11 | — | — | no ref | never referenced in firmware |
| PB12 | KEYMAT_COL_D | GPIO input | ✓ | `fw_keypad_scan_matrix`; also DFU entry at boot (pull LOW) |
| PB13 | KEYMAT_ROW_5 | GPIO output | ✓ | `fw_keypad_scan_matrix` — driven LOW for row scan |
| PB14 | KEYMAT_ROW_4 | GPIO output | ✓ | `fw_keypad_scan_matrix` |
| PB15 | [?] | TIM1_CH3N AF | ✓ | `fw_tim1_pwm_init` configures as AF push-pull 50MHz (mask=0x8000). TIM1 OC3 CCR=651. Function unknown — probe compensation square wave or relay drive candidate. |

---

## GPIOC (base 0x40011000)

| Pin | Function | AF / Peripheral | Confidence | Source |
|-----|----------|----------------|------------|--------|
| PC0 | — | GPIO output | trace | Set/clear via BSRR (`fw_usb_pc0_output_set`). Driven HIGH when USB VBUS present (mirrors PC2). Also controlled by scope menu param_cur==3 (boolean, saved in settings). Physical destination unknown — possibly USB hub/switch enable. |
| PC1 | — | GPIO input pull-down | trace | Configured as input pull-down in `fw_gpio_usb_sense_init` — function unknown |
| PC2 | USB_VBUS_SENSE | GPIO input | ✓ | Read by `fw_usb_vbus_sense_update` into `g_usb_connected` (SRAM 0x2000d3cd). HIGH=USB cable plugged in. Drives USB charging icon path in `fw_battery_voltage_update`. Boot init drives it LOW (output push-pull) as the "no VBUS" default; external VBUS pulls HIGH through protection resistor → IDR reads HIGH. |
| PC3 | KEYMAT_ROW_3 | GPIO output | ✓ | `fw_keypad_scan_matrix` |
| PC4 | DMM_SR_CLK | GPIO output | ✓ | Shared shift clock (SRCLK) for both DMM 595s (U9 + U600, daisy-chained). Used by both `fw_dmm_usart_send_byte` and `fw_dmm_bit_send_clk`. TSV: `ptr_dmm_gpio_c` (was `flt_cf3c_2p017`, GPIOC base misidentified as float). |
| PC5 | BATT_SENSE | ADC1_IN15 | ✓ | `fw_battery_voltage_update` / `g_adc1_dma_buf[0]` — battery voltage sense (ratiometric vs VREFINT CH17) |
| PC6 | KEYMAT_COL_C | GPIO input | ✓ | `fw_keypad_scan_matrix` |
| PC7 | BUZZER | TIM8_CH2 AF | ✓ | `fw_tim8_buzzer_init` — 3030 Hz piezo, PSC=719 ARR=32 |
| PC8 | SDIO_D0 (unused) | — | no ref | Standard STM32F103 SDIO pin; never configured to AF in this firmware. See SDIO note below. |
| PC9 | SDIO_D1 (unused) | — | no ref | Standard STM32F103 SDIO pin; never configured to AF in this firmware. See SDIO note below. |
| PC10 | SDIO_D2 (unused) | — | no ref | Standard STM32F103 SDIO pin; never configured to AF in this firmware. See SDIO note below. |
| PC11 | SDIO_D3 (unused) | — | no ref | Standard STM32F103 SDIO pin; never configured to AF in this firmware. See SDIO note below. |
| PC12 | SDIO_CK (unused) | — | no ref | Standard STM32F103 SDIO pin; never configured to AF in this firmware. See SDIO note below. |
| PC13 | — (pkg pin 7, LQFP100) | — | no ref | Datasheet alt-fn is TAMPER-RTC (backup-domain cluster with PC14/PC15/VBAT), but never referenced anywhere in this firmware — no RCC/GPIO/EXTI/BKP touches it. Prior note calling this a "power button latch / tamper pin, structurally expected" was unsupported inference, softened 2026-07-13. |
| PC14 | — (pkg pin 8, LQFP100) | — | no ref | Datasheet alt-fn OSC32_IN, never referenced. LSE disabled (`fw_rcc_bdcr_lse_cfg(0)`, sole call site, 0x0801918a called from inside `fw_usart1_gpio_init` at decompiled_named.c:52828) — confirmed no `fw_rtc_init` function exists anywhere in the firmware; the LSE/OSC32 crystal theory was based on a citation to a nonexistent function and is now retired. |
| PC15 | — (pkg pin 9, LQFP100) | GPIO output | trace | Datasheet alt-fn OSC32_OUT, but genuinely repurposed as plain push-pull GPIO (mode=3 50MHz, CNF=0x10) by misnamed `fw_usart1_gpio_init` (0x0802fef8, not USART1) — same function unconditionally disables LSE, confirming no crystal is fitted on PC14/PC15 on this board. Boots HIGH (unconditional `fw_gpio_set_bits` at end of that init). Toggled in `fw_usb_cmd_parser`: LOW when `g_usb_cmd_buffer[0]==0`, HIGH when USB command present — confirmed the ONLY other runtime write site for this bit anywhere in the decompiled corpus. **Live SWD test 2026-07-13** (probe `26004600160000305748534E`): a breakpoint at the REFERENCE build's `fw_usb_cmd_parser` address (0x08032ea0) never fired — root cause found to be a **firmware build mismatch**: this specific bench unit is running the mystery/unofficial build (`vendors/hantek/2d82/firmware/2d82-20260713-swd/`), not the `2d82-20210818` reference this TSV is normally keyed to; the equivalent function on this unit lives at 0x08034184. Once re-armed at the correct live address, the breakpoint fired immediately under real USB scope traffic, and single-instruction stepping confirmed the documented HIGH-path logic exactly (buffer nonzero → `fw_gpio_set_bits` → PC15 HIGH) — no contradiction, no hidden writer, just the wrong build's address. **LED-check (2026-07-13, live, user eyes-on)**: PC15 held forced LOW then HIGH for sustained multi-second windows via direct BSRR/BRR writes while the user watched the physical board (both boards, USB connector area, SD slot area) — **no LED anywhere lit up or changed state in either window; LED theory ruled out.** Physical destination still unknown — no schematic/board trace found; remaining candidates: unpopulated test point, NC pad, or a mux/select line. Bench probe point if pursued further: **LQFP100 physical package pin 9** (Geehy APM32F103xC datasheet Table 3 — pin 6=VBAT, 7=PC13, 8=PC14, 9=PC15, 10=VSS). Full test log in `ghidra_names_2d82.tsv` mmio_GPIOC entry and PC15 backup-domain note. |

> **SDIO note (2026-07-13):** the SDIO peripheral (0x40018000) is genuinely present and touched by the firmware — `fw_sdio_status`/`fw_sdio_icr_clear`/`fw_sdio_mask_set` read/write SDIO_STA/ICR/MASK, and IRQ49 (SDIO) + IRQ59 (DMA2_Channel4_5, SDIO's DMA line) both vector into real handlers (`fw_sdio_irq_handler`, `fw_sdio_dma_irq_handler`) — see `ghidra_names_2d82.tsv` for full evidence. However, no SDIO_CLKCR/POWER/CMD/ARG/DLEN/DCTRL register programming exists anywhere in the decompiled corpus, and PC8-PC12/PD2 (the standard STM32F103 SDIO pin group) are never configured to their AF here. Owner confirms a physical SD card holder exists on this board, but this firmware build does not appear to actually drive it — most likely unused ISR/DMA-ack scaffolding shared from a BSP/reference codebase across a Hantek product family. No FAT/screenshot/waveform-export-to-SD code was found either.

---

## GPIOD (base 0x40011400)

| Pin | Function | AF / Peripheral | Confidence | Source |
|-----|----------|----------------|------------|--------|
| PD0 | FSMC_D2 | FSMC AF | ✓ | `fw_fsmc_init` — LCD data bus |
| PD1 | FSMC_D3 | FSMC AF | ✓ | `fw_fsmc_init` |
| PD2 | SDIO_CMD (unused) | — | no ref | Standard STM32F103 SDIO pin; never configured to AF in this firmware. See SDIO note below. |
| PD3 | LCD_RESET# | GPIO output | ✓ | `fw_lcd_gpio_pulse` — ILI9341 RESET# (mask=0x08, active-LOW pulse 100µs) |
| PD4 | FSMC_NOE (RD) | FSMC AF | ✓ | `fw_fsmc_init` |
| PD5 | FSMC_NWE (WR) | FSMC AF | ✓ | `fw_fsmc_init` |
| PD6 | — | — | no ref | never referenced in firmware |
| PD7 | FSMC_NE1 (LCD CS) | FSMC AF | ✓ | `fw_fsmc_init` — Bank1 chip-select |
| PD8 | FSMC_D13 | FSMC AF | ✓ | `fw_fsmc_init` |
| PD9 | FSMC_D14 | FSMC AF | ✓ | `fw_fsmc_init` |
| PD10 | FSMC_D15 | FSMC AF | ✓ | `fw_fsmc_init` |
| PD11 | FSMC_A16 (LCD RS/DC) | FSMC AF | ✓ | LCD cmd=0x60000000 (A16=0), data=0x60020000 (A16=1) |
| PD12 | KEYMAT_ROW_2 | GPIO output | ✓ | `fw_keypad_scan_matrix` |
| PD13 | KEYMAT_ROW_1 | GPIO output | ✓ | `fw_keypad_scan_matrix` |
| PD14 | FSMC_D0 | FSMC AF | ✓ | `fw_fsmc_init` |
| PD15 | FSMC_D1 | FSMC AF | ✓ | `fw_fsmc_init` |

---

## GPIOE (base 0x40011800)

| Pin | Function | AF / Peripheral | Confidence | Source |
|-----|----------|----------------|------------|--------|
| PE0 | DMM_SR_DATA | GPIO output | ✓ | Shared serial data line for both DMM 595s (U9 + U600, daisy-chained). `fw_dmm_usart_send_byte` (SR1/U9 → CS7721 HOLD/REL/SELECT/RANGE) and `fw_dmm_bit_send_clk` (SR2/U600 → 5 optos → CS7721 MEA1-4/SELECT). |
| PE1 | POWER_HOLD (candidate) | GPIO output | ? | GH#150, 2026-07-14: `fw_gpio_dmm_gpio_init` (misleadingly named — see TSV) is the FIRST substantive call in `fw_app_main`, before LCD/FPGA/USB init. Configures PE1 as output (mask=2, CNF=0x10, mode=3) and drives it HIGH once (`decompiled_named.c:37973`) — the only GPIOE mask-0x2 write in the entire firmware, never cleared. Bootloader never touches GPIOE at all (clock not even enabled), so the device must survive the whole bootloader DFU-check window on hardware margin alone before this pin is asserted. Medium-high confidence, pending live SWD confirmation (hold PE1 LOW post-boot, check for power loss — same method as the PC15 test). |
| PE2 | — | — | no ref | never referenced in firmware |
| PE3 | LED_HOLD_AWG | GPIO output | ✓ | Indicator LED. HIGH when AWG active or DMM HOLD active. LOW otherwise. Both PE3+PE5 go LOW together on screensaver dim — proves LED not analog mux (mux can't safely go undefined). ~16 call sites. |
| PE4 | — | — | no ref | never referenced in firmware |
| PE5 | LED_RUN | GPIO output | ✓ | Indicator LED. HIGH when scope acquisition running or DMM live (no hold). LOW when AWG active, DMM hold, or screensaver. Set HIGH at boot before mode dispatch. Paired with PE3: exactly one HIGH during normal operation, both LOW = screensaver. |
| PE6 | DMM_U9_RCLK | GPIO output | ✓ | `fw_dmm_usart_send_byte` — RCLK latch for U9 (74HC595, CPU board). U9 outputs → CS7721 HOLD/REL/SELECT/RANGE command pins directly. Shared serial bus with U600: same PE0/PC4 data/clock, different latch pin. |
| PE7 | FSMC_D4 | FSMC AF | ✓ | `fw_fsmc_init` — LCD data bus |
| PE8 | FSMC_D5 | FSMC AF | ✓ | `fw_fsmc_init` |
| PE9 | FSMC_D6 | FSMC AF | ✓ | `fw_fsmc_init` |
| PE10 | FSMC_D7 | FSMC AF | ✓ | `fw_fsmc_init` |
| PE11 | FSMC_D8 | FSMC AF | ✓ | `fw_fsmc_init` |
| PE12 | FSMC_D9 | FSMC AF | ✓ | `fw_fsmc_init` |
| PE13 | FSMC_D10 | FSMC AF | ✓ | `fw_fsmc_init` |
| PE14 | FSMC_D11 | FSMC AF | ✓ | `fw_fsmc_init` |
| PE15 | FSMC_D12 | FSMC AF | ✓ | `fw_fsmc_init` |

---

## Keypad matrix (full)

Rows driven LOW one at a time; columns read LOW when key pressed.

| Row \ Col | ColA / PB6 | ColB / PB5 | ColC / PC6 | ColD / PB12 |
|-----------|-----------|-----------|-----------|------------|
| Row1 / PD13 | 1 F1 | 2 F2 | 3 F3 | 4 F4 |
| Row2 / PD12 | 5 CH | 6 AWG | 7 DMM | 8 Trig |
| Row3 / PC3  | 9 Mode | 10 Up | 11 Down | 12 Enter |
| Row4 / PB14 | 13 Auto | 14 CW | 15 CCW | 16 CH2/V |
| Row5 / PB13 | 17 BW | 18 Time | 19 TrgLv | 20 Run |

---

## FPGA_SPI1 — runtime + SSPI programming (same bus)

SPI1 (PA5/PA6/PA7) is used exclusively for the FPGA. Runtime register access uses CS=PA4; SSPI bitstream programming uses CS=PB4. CPOL=0, CPHA=0, BR=÷2 (36 MHz). No other device shares this bus.

---

## DMM CS7721 shift register wiring

Two 74HC595s daisy-chained on a shared serial bus:

| Signal | MCU pin | Notes |
|--------|---------|-------|
| SER (data) | PE0 | Shared between U9 and U600 |
| SRCLK (clock) | PC4 | Shared between U9 and U600 |
| U9 RCLK (latch) | PE6 | Latches U9 → CS7721 HOLD/REL/SELECT/RANGE |
| U600 RCLK (latch) | PB0 | Latches U600 → 5 optos → CS7721 MEA1-4 + SELECT |

**U9** (74HC595, CPU board): outputs drive CS7721 command pins directly — HOLD, REL, SELECT, RANGE.

**U600** (74HC595, AFE board): outputs are **shorted in groups** to drive 5 optocouplers across the DMM isolation barrier → CS7721 MEA1-4 + SELECT. The opto isolation is required because the CS7721 analog side references high-voltage DMM inputs.

---

## Notes

**Power button** — PA0 configured as input pull-up/down at boot, polled LOW in `boot_main` before DFU check. Likely the power/on button sense. Power-hold output candidate found 2026-07-14 (GH#150): **PE1**, driven HIGH once by `fw_gpio_dmm_gpio_init` at the very start of `fw_app_main` — see PE1 row above and GH#150 for full trace. Medium-high confidence from code alone; live SWD confirmation still needed before treating this as settled.

**USB detection** — PC2=USB_VBUS_SENSE (HIGH when cable plugged in → `g_usb_connected`), PB7=USB_POWER_FLAG (HIGH when USB power ready → `g_usb_power_flag`). Both confirmed from flash literal pool: ptr_unk_pc2_flag→0x2000d3cd, ptr_unk_pb7_status→0x200023d9. `fw_battery_voltage_update` reads these to select charging icon vs battery gauge display. PC0 is driven HIGH when VBUS present — physical destination unknown (possibly USB hub enable or power switch).

**FPGA JTAG** — the MCU does NOT drive JTAG. Programming is via SSPI (SPI1 + PB4). The physical JTAG header on the AFE board is for an external programmer (openFPGALoader) only.
