# 2D82AUTO — hardware reference

Hardware facts about this device. Companion to `hantek_2d82.dts`.

## BOM — key components

### Board 1 — Analogue Front End

| Ref | Part | Package | Function | Confidence |
|-----|------|---------|----------|------------|
| U31 | AD9288-40 | 48-LQFP | Dual 8-bit ADC, rated 40 MSPS, run interleaved ~250 MSPS (heavily overclocked) | marking confirmed (-40 speed grade) |
| U33 | HEF4053B1 | — | Triple 2-channel analog mux — select pins to FPGA, signal pins to ADC (U31); AFE signal path switch (AC/DC coupling or range) | marking confirmed |
| U12 | 3PD5651E (marking: AAJQ) | 28-pin | 12-bit current-output AWG DAC (DAC902-equivalent) | marking scrubbed on most units; confirmed 12-bit from netlist + Rset |
| U1  | 74HC595 | 16-pin SOIC | CH1 AFE shift register — relays/gain select | confirmed (board-traced) |
| U7  | 74HC595 | 16-pin SOIC | CH2 AFE shift register — relays/gain select | confirmed (board-traced) |
| U2  | Lattice LCMXO2-1200HC-4TG100C | TQFP100 | FPGA — ADC capture, trigger, AWG, AFE control | confirmed (IDCODE 0x012BA043) |
| U3 | 25 MHz crystal | — | FPGA (U2) clock source; C2 = load cap | confirmed |
| U600 | 74HC595 | 16-pin TSOP | AFE opto driver — multiple outputs shorted together to drive 5 optocouplers (isolation barrier between digital control side and CS7721 high-voltage analog side). 5 optos → CS7721 MEA1-4 + SELECT (measurement function + mode select). Not a normal shift-register data path. | confirmed type + output-short noted |
| — | CS7721 | — | DMM dual-slope ADC (ICL7106-class) | confirmed |
| — | EL5166 / LMH6702 | — | AWG output op-amp chain to Gen-Out BNC | community / unverified |
| P3 | 2-pin header | 2.54mm | Pin 1 = 5V; Pin 2 = unknown | designator confirmed, pin 2 TBD |
| J? | 6-pin JTAG header (2×3, 2.54mm) | 2.54mm | FPGA (MachXO2) ispJTAG programming port — U2 is on the AFE board. Pin 1=VCC, 2=TMS (R66 10kΩ pull-down), 3=TDI (R3 unpopulated), 4=TCK (R80 10kΩ pull-down), 5=TDO (R67 10kΩ pull-down), 6=GND | confirmed — pinout traced |
| (none) | 33-pin FPC | — | Inter-board connector → CPU board | no silkscreen designator on either board |

### Board 2 — CPU / Front Panel (`HT-2XXX-FrontPanel_V1.0`)

| Ref | Part | Package | Function | Confidence |
|-----|------|---------|----------|------------|
| — | Geehy APM32F103VET6 | LQFP100 | MCU — Cortex-M3, 512 KB flash, 64 KB SRAM (STM32F103VET6-compatible clone) | marking confirmed |
| — | ILI9341 | — | LCD controller, 320×240 | confirmed (returns ID 0x9341) |
| J1 | 40-pin FPC connector | — | CPU board → ILI9341 display panel | confirmed |
| J3 | microSD connector | — | microSD slot (SDIO); 2× 4-resistor packs (pull-ups/series termination on data lines) | confirmed |
| J? | USB-C connector | — | USB FS device (DFU + scope protocol) | confirmed type |
| J? | 4-pin 2.54mm header | — | SWD debug. Pin 1=5V, Pin 2=SWDCLK, Pin 3=SWDIO, Pin 4=GND — corresponding test points present | confirmed |
| J? | 4-pin 2.54mm header | — | Function TBD | designator TBD |
| J? | 1× 2-pin 2.54mm header | — | Function TBD | designator TBD |
| (none) | 33-pin FPC | — | Inter-board connector → AFE board (signals + power) | no silkscreen designator on either board |
| J? | 2× switch-mode PSU | — | Power supply — rails TBD | designator TBD |
| TP1 | Test point | — | GND | confirmed |
| TP2 | Test point | — | 5V rail | confirmed |
| Q? (×10) | Transistors/MOSFETs (unmarked/TBD) | — | SDIO switching | designators + parts unidentified |
| U5 | WiFi/BT module (footprint only) | — | WiFi/BT module footprint — unpopulated on this unit. Designator U5, 3-sided castellated, 44 pins (16 left + 12 bottom + 16 right), 1.27mm pitch. PCB trace antenna keepout comb (5-finger meander) on host board silkscreen. **Footprint matches USR-C322 exactly** (18.22×26.65mm, 16L+12B+16R, 1.27mm; KiCad `RF_WiFi.pretty/USR-C322.kicad_mod` confirmed match — including the Dwgs.User keepout hatching that appears as the host silkscreen comb). USR-C322 is a TI CC3200-based serial-to-WiFi UART bridge (802.11 b/g/n). The actual module on the WiFi variant (2D82M) may be a different part in the same physical shell. Interface: UART (PA9/PA10 USART1). No firmware WiFi code path in any of 5 firmware images — consistent with module absent (footprint-only) on this unit. **Family context:** the older 2D72 (HT2XXX_FrontPanel_V1.0 board) had an ESP8266 ESP-12 form-factor module in its equivalent slot (EEVBlog forum, 2019-01-31, user tsman, reply #38) — the 2D82AUTO upgraded to the larger 44-pin 3-sided USR-C322 footprint. No teardown of a populated 2D82M has been found; USR-C322 is confirmed discontinued per PUSR/USR-IOT website. | footprint present, module absent; USR-C322 form factor confirmed by KiCad geometry; chip/brand on 2D82M unconfirmed |
| Y1 | 8 MHz crystal | — | MCU HSE clock source. Confirmed from `fw_set_sysclock_72mhz`: HSEON set, HSERDY polled, PLLMUL=×9 → 72 MHz SYSCLK. OSC_IN/OSC_OUT are dedicated MCU pins (not GPIO). LSE explicitly disabled (`fw_rcc_bdcr_lse_cfg(0)`) — no 32.768 kHz crystal fitted. | code confirmed |
| — | Keypad matrix | PCB | 5×4 key matrix — gold PCB traces, conductive rubber dome contacts (no mechanical switches) | confirmed |

---

### U5 WiFi/BT module footprint — cross-firmware survey (2026-07-14)

**Verdict: NO.** None of the 4 previously-unchecked firmware images in this repo show SPI or UART enabled on the ESP-candidate pin group (PC8-12/PD2), and none add USART1 RX handling beyond the reference build's already-known RXNEIE-set-but-unconsumed state. GPIOD is **never referenced at all** (zero literal-pool hits) in any of the 5 firmware images now checked (main reference + these 4), and SPI2/SPI3 are likewise **never referenced** in any of them. This is a clean negative result across the whole firmware corpus currently in the repo — it does not rule out an ESP module on some other variant not present here, but nothing already in this repo supports the idea.

**Method:** the Ghidra C/asm exports for these builds don't carry raw peripheral base addresses as literal text (they're resolved through indirect literal-pool loads, `ldr rX,[0x0800xxxx]`, whose pool word isn't dumped by this export pipeline). Confirmed by testing on the reference build too — even its `disassembly.asm` has zero raw `0x400xxxxx` hits. Worked around this by grepping the raw firmware `.bin` dumps directly for the little-endian 32-bit peripheral base addresses (GPIOC `0x40011000`, GPIOD `0x40011c00`, SPI2 `0x40003800`, SPI3 `0x40003c00`, USART1 `0x40013800`), then converting each hit file-offset back to a flash address (using each firmware's documented `load_addr`) and cross-referencing that address against `disassembly.asm`'s `ldr rX,[0x0800xxxx]` literal-pool consumers to read the actual pin-config/register-access code around each site.

| Firmware | GPIOC (0x40011000) | GPIOD (0x40011c00) | SPI2 (0x40003800) | SPI3 (0x40003c00) | USART1 (0x40013800) | Verdict |
|---|---|---|---|---|---|---|
| `2d82-20210818` (MAIN REFERENCE) | 3 refs — PC0-PC7,PC15 only (USB sense, DMM clock, keymat, buzzer, boot-GPIO) | 0 refs | 0 refs | 0 refs | RXNEIE bit set at init, IRQ37 vector = default stub, no RX consumer found | clean (already known, cited as baseline) |
| `2d82-20260713-swd` (mystery live-dumped build) | 8 refs — same functional set as reference (`fw_gpio_usb_sense_init`-equivalent = PC0/PC1/PC2; `fw_fpga_spi_gpio_init`-equivalent = SPI1-on-GPIOB path, one `mov.w r0,#0x1000` inside it is an RCC APB2ENR clock-enable bit, not a GPIOC/D pin mask; PC15 boot-toggle pin) | 0 refs | 0 refs | 0 refs | `fw_usart_115200_init` @ 0x0802a1e4 — same structure as reference's `fw_usart_115200_init` @ 0x0802e5cc (identical propagated comment), RXNEIE set via `fw_reg_write_inv(reg,0x40)`, but IRQ36/IRQ37 vectors both point into the same tiny stub cluster (0x080409d1/0x080409d5, thumb-bit differs only) — consistent with unused/default handlers. Only 2 GPIOA-pointer refs for USART1 GPIO cfg (PA9/PA10 std TX/RX pin setup), no RX byte consumer anywhere in the corpus. | clean, effectively identical to reference (same build lineage, confirms this is the live unit's actual firmware, not an ESP-enabled variant) |
| `2d72-20190415` | 9 refs — same PC0-PC7/PC15 functional set; one `mov.w r0,#0x1000` initially looked like a PC12 pin-config but traced to `FUN_08014b38(mask,en)` = the shared RCC-enable primitive (single fixed pointer at 0x08014ba4, not GPIOC/D) — false lead, ruled out | 0 refs | 0 refs | 0 refs | Same TX-poll + RXNEIE-set-only pattern as reference/SWD build (baud cfg 0x1c200, `bl` init/enable/RXNEIE-set triplet); IRQ36/37 vectors adjacent, same stub-cluster signature | clean |
| `2d72-20190828` | 8 refs — traced byte-for-byte: same `fw_gpio_usb_sense_init`-equivalent (masks 0x1/0x4/0x80/0x2 = PC0/PC2/PC7/PC1) and PC15 boot-toggle pattern as the other builds | 0 refs | 0 refs | 0 refs | Same TX-poll + baud-0x1c200 + RXNEIE-set-only triplet at 0x08025cb6, traced directly | clean |
| `2d72-20210909` | 8 refs — same PC0-PC7/PC15 set. Two apparent leads (`fw_gpio_set_bits(DAT_0800c590,0x200)` in a font/text-overlay function, `fw_gpio_reset_bits(DAT_080376d4,0x100)` in `fw_lcd_gpio_init`) both traced via direct binary read of the literal pool word: `DAT_0800c590`=`0x40010c00`=**GPIOB**, `DAT_080376d4`=`0x40010c00`=**GPIOB** — both false leads, these are PB8/PB9 (LCD/graphics control lines), not PC8/PC9 | 0 refs | 0 refs | 0 refs | `fw_usart_115200_init` present with the same propagated "RXNEIE" comment as the other builds; `fw_usart1_send_byte`/`fw_usart_send_buf` confirmed TX-only via `fw_usart1_handler` wrapper; no RX consumer | clean |

**On the USART1 RXNEIE question specifically:** every build checked (reference + all 4 new) calls the same `fw_reg_write_inv(usart1_cr1_ptr, 0x40)` pattern during USART1 init, which the existing reference-build TSV comment glosses as "enables RX interrupt (RXNEIE)". This is architecturally consistent across the whole firmware family — it is not something unique to a WiFi/BT-capable variant. In every build, the corresponding IRQ37 (USART1) vector table entry points into what looks like a shared default/stub handler cluster (immediately adjacent to IRQ36/SPI2's vector, which the reference build's TSV already independently confirms is `b.w <self>` i.e. an un-implemented default stub) — i.e. RXNEIE is set at init time across the whole product family, but no build here actually services USART1 RX via interrupt or polling. This matches the task's framing of the reference build as "TX-only in practice" and extends that finding to all 4 additional images.

**2D72 family caveat:** per `vendors/hantek/2d82/docs/hardware/series.md`, 2D72/2D42/2C72/2C42 are a physically different (smaller-flash handheld) product line — a clean result on their firmware says the 2D72 hardware design also doesn't wire SPI2/3 or GPIOD, and shares the reference build's USART1 TX-only/RXNEIE-inert pattern, which is a useful data point for shared firmware/BOM lineage but is not itself proof about the 2D82AUTO's specific PCB.

**Follow-up flagged for a future session:** none of the 4 surveyed firmwares justify a new `ghidra_names_2d72_*.tsv` entry — no genuinely new functionality was found (all leads traced to either RCC-enable idiom collisions or GPIOB/LCD control lines, not GPIOC/D or SPI2/3). The existing `vendors/hantek/2d72/ghidra/ghidra_names_2d72.tsv` (auto-gen + hand-curated, appears keyed most closely to `2d72-20210909`'s address layout based on matching addresses) already covers the relevant functions (`fw_usart_115200_init`, `fw_lcd_gpio_init`, etc.) via VT-match propagation from the main reference TSV; no changes were needed there. If a future session wants to fully close the USART1-RXNEIE question, the useful next step would be to `createFunction` at each build's IRQ36/IRQ37 vector targets (currently unnamed stub addresses, not yet run through the Ghidra pipeline for these 4 builds) to confirm byte-for-byte they're identical no-op handlers rather than assuming from address adjacency alone.

---

## SoC + clocks [V]
- **Geehy APM32F103VET6** (STM32F103VET6-compatible clone) -- Cortex-M3, 512 KB flash, 64 KB SRAM, 100-pin, has FSMC.
  Library: **ST SPL** (Standard Peripheral Library) + CMSIS -- confirmed from clock init
  and GPIO_Init struct idiom.
- **Clock tree:** 8 MHz HSE → PLL ×9 → **72 MHz SYSCLK**, APB1=36 MHz, APB2=72 MHz, USBCLK=48 MHz. See `fw_set_sysclock_72mhz` in named.c.

### APM32F103 clock options (USB is the constraint)

USB FS requires exactly 48 MHz. The APM32F103 supports more USB clock divider choices than STM32F103, making higher core clocks viable. Crystal = 8 MHz, so SYSCLK = 8 × PLLMUL.

**USB-safe options:**

| SYSCLK | PLLMUL | USB divider | USBCLK | Notes |
|--------|--------|-------------|--------|-------|
| 72 MHz | ×9 | /1.5 | 48 MHz | Stock (vendor firmware) |
| 96 MHz | ×12 | /2 | 48 MHz | **Recommended max** — Geehy-stated max, clean divider |
| 120 MHz | ×15 | /2.5 | 48 MHz | Some Geehy datasheets support this; board test required |

**Not USB-safe** (no clean 48 MHz divider): 108, 128, 144 MHz — USB breaks.

**At 96 MHz:** increase FLASH wait states; APB1 divide by 2 or 4; APB2 can stay high (check peripheral limits).

**Benefit here:** MCU is not in the capture path — the FPGA + AD9288 do all sampling. Overclocking speeds UI, menus, and USB readout only; it does not affect sample rate or capture depth.
- **Flash map:** 0x08000000 = 20 KB **vendor DFU bootloader** (keep it -- `F1+Power`
  enters DFU); 0x08005000 = application (~492 KB). Also a STM32 serial bootloader via
  the UART (the "Program Arm1" path, 3.3 V, RX/TX crossed).

## The capture chain (the core of a scope -- must be built new)
```
2 analog inputs -> AD9288 (dual 8-bit, ~40-100 MSPS) -> Lattice MachXO2 FPGA
   (clocks the ADC ENC, captures samples to its block-RAM, trigger + timebase)
   -> MCU reads captured records over GPIO/SPI1
```
- **AD9288 is read by the FPGA, NOT the MCU.** Parallel 8-bit buses (DA0-7/DB0-7) +
  ENCA/ENCB clocks go to the FPGA. The MCU never sees raw samples.
- **FPGA capture depth ~ 8 KB** (MachXO2-1200 block-RAM) -- shallow; no external DRAM.
- **MCU<->FPGA link = GPIO bit-bang + SPI1** (NOT memory-mapped -- FSMC is the LCD).
  The MCU writes config (sample-rate / trigger / AWG-table) and reads back captured
  data. The exact register/command protocol is the FPGA's interface (see issue #19);
  for clean-room firmware you define your own once you control both sides.
- MCU's own **ADC1 + DMA1** are used for **battery voltage monitoring only** (internal
  VREFINT + temp channels; not in the scope signal path -- see ADC1 section below).

## FPGA (Lattice MachXO2-1200, IDCODE 0x012BA043)
- Self-boots from its **internal config flash**. Reprogrammable via JTAG/SSPI from
  the MCU (the "Force update FPGA" path) -- TCK/TMS/TDI/TDO on GPIO (pins TBD by trace).
- For new gateware: write in Amaranth -> yosys -> nextpnr-machxo2 -> bitstream; flash via
  JTAG (openFPGALoader) or the DFU path. A CAN decoder (~500 LUTs) fits the 1280-LUT
  part with room to spare.

## LCD -- ILI9341 over FSMC [V] (see lcd-connection.md)
- **Controller = ILI9341** (confirmed: the panel returns ID 0x9341 to the 0xD3
  "Read ID4" command). 240x320, 16-bit parallel.
- **Bus = FSMC Bank1** (8080-style), memory-mapped: cmd @0x60000000 (RS=0),
  data @0x60020000 (RS=1, selected by A16 = **PD11**). No DMA framebuffer -- CPU
  `strh`s every pixel; panel holds its own GRAM.
- FSMC pin assignments (verified from firmware GPIO_Init):

| FSMC signal | Pin | | FSMC signal | Pin |
|---|---|---|---|---|
| D0  | PD14 | | D8  | PE11 |
| D1  | PD15 | | D9  | PE12 |
| D2  | PD0  | | D10 | PE13 |
| D3  | PD1  | | D11 | PE14 |
| D4  | PE7  | | D12 | PE15 |
| D5  | PE8  | | D13 | PD8  |
| D6  | PE9  | | D14 | PD9  |
| D7  | PE10 | | D15 | PD10 |
| NOE (RD) | PD4 | | NWE (WR) | PD5 |
| NE1 (CS) | PD7 | | **A16 (RS/DC)** | **PD11** |

- New firmware: Zephyr's `ilitek,ili9341` over an FMC NOR/SRAM child + the GPIOs.
  Fill FSMC timings from a board trace.

## DMM -- CS7721 (ICL7106-class dual-slope ADC)
- A self-contained slow integrating ADC (a few readings/sec). Read its result over a
  low-speed serial/GPIO line; range/function selected by GPIOs (drive the analog
  ranging). It is *why the DMM is slow* -- a hardware limit, not firmware.

## AWG path (D-models only; depopulated on C-models)

**AWG specs (published):** waveforms sine/square/triangle/ramp/trapezoidal/EXP + 4
arbitrary; **sine 1 Hz-25 MHz**, square 1 Hz-10 MHz, ramp 1 Hz-1 MHz, EXP 1 Hz-5 MHz;
**amplitude 2.5 Vpp @50 ohm / 5 Vpp hi-Z**; DDS, **12-bit** DAC at 250 MSa/s, 512-sample
arb depth; freq resolution 0.001 Hz; **freq stability <30 ppm**; 50 ohm out; single
channel. (Matches the 3PD5651E 12-bit DAC + `dds_cali_data`.)

**AWG output-stage analysis (UNVERIFIED -- community calc + cross-check).** A forum
estimate of the actual analog output chain (3PD5651E DAC -> op-amp -> BNC):
- Rset ~ 2.2 kohm -> DAC full-scale current ~ **18 mA** (DAC902 formula `Iout = 32*Vref/Rset`
  gives 14.5 mA @Vref 1.0 V ... 17.5 mA @1.2 V -- ~18 mA implies Vref ~ 1.24 V [V]).
- 18 mA across the 50 ohm output load -> **0.9 V** drop [V].
- Output driver gain = 560/150 = **3.733** -> BNC full-scale (unloaded) =
  0.9 x 3.733 ~ **+/-3.36 V (~6.7 Vpp)**.
- **Cross-check: the math is sound** (each step verified). It implies the **DAC range
  is NOT fully exploited** vs the published 5 Vpp hi-Z / 2.5 Vpp @50 ohm -- i.e. they leave
  headroom / picked convenient resistor values (typical cheap-design). So the real
  unloaded swing may be ~+/-3.36 V, the loaded ~+/-0.9 V.
- [!] **UNVERIFIED:** Rset, the 560/150 gain resistors, and Vref are forum guesses (R65
  itself is debated 560 ohm vs 50 ohm). Needs confirmation on a real board. Relevance: this
  sets the **actual AWG amplitude ceiling for the self-cal source** (cal-against-DMM
  range is bounded by the true unloaded swing, not the datasheet number).
- [!] <30 ppm is **frequency** stability (DDS clock) -- the **amplitude/DC has no Vref**
  and drifts with the rail like the scope. So for cal, use the AWG as a precise *swept
  source* but take amplitude truth from the DMM (see the loopback self-cal below).
- **A reference signal is built in:** the probe-comp output = **2 V / 1 kHz square** --
  the device is *designed* to feed its own output back to a channel ([Auto] probe
  check). So the loopback cal path is a designed-in workflow, not a hack.

- **3PD5651E** = 12-bit parallel current-output DAC (DAC902-equivalent): D11..D0 + CLK,
  streamed by the **FPGA** from a waveform table (not the MCU). IOUT differential.
- **EL5166 / LMH6702** = high-speed op-amp, buffers IOUT to the Gen-Out BNC. Analog
  only (maybe one enable GPIO).
- C-models (2C42/2C72) likely have the DAC + amp **unpopulated** -- check the board
  before assuming AWG output exists.

## Keypad -- 5x4 scanned matrix [V] (see key-matrix.md)
- **Columns (read inputs, pulled-up):** PB6, PB5, PC6, PB12
- **Rows (driven low one at a time):** PD13, PD12, PC3, PB14, PB13
- 20 keys total. `F1 + Power` at boot = DFU entry.
- NOTE: PB5/6/12 + PC6 are key columns; PD12/13, PC3, PB13/14 are key rows --
  these are NOT SPI2 (as the raw pinmap might suggest).

## Serial / buses [V]
- **USART1:** PA9 (TX), PA10 (RX) -- "Program Arm1" 3.3 V debug/DMM header.
  Used at **115200 baud** for debug. NOT used for DMM result read (see USART2).
- **SPI1:** PA5 (SCK), PA6 (MISO), PA7 (MOSI) -- MCU<->FPGA runtime link.
  PA4 = FPGA_CS (runtime, bit-banged active-LOW). PB4 = FPGA_SSPI_CS (bitstream programming only).
- **USART2:** PA3 (RX) -- CS7721 DMM result read at **2400 baud** (see `fw_dmm_usart_init` in named.c).
- **SPI2:** not present -- PB13/14 are keypad rows, PB15 unconfirmed. [?]

## ADC1 -- battery voltage monitor [V]
- **Not in the scope signal path.** The AD9288 handles scope samples; ADC1 is auxiliary only.
- Channels: **VREFINT (CH17)** + **temperature sensor (CH16)** -- internal, no external pin.
  Enabled via `TSVREFE` bit in ADC1_CR2. Ratiometric measurement via VREFINT.
- See `fw_battery_voltage_update` and `g_adc1_dma_buf` in named.c.

## Timers / PWM [V]
- **PA8 = TIM1_CH1** -- backlight PWM (PB15 = TIM1_CH3N complementary). See `fw_tim1_pwm_init` in named.c.
- **PC7 = TIM8_CH2** -- piezo buzzer. See `fw_tim8_buzzer_init` in named.c.
- **PA8 = TIM3_CH1** -- alternate PWM path. [?] which takes priority. See `fw_tim3_pwm_init` in named.c.
- **TIM2** -- 52 firmware refs, scope timebase / sample timing.

## LCD reset + DMM serial [V]
- **PD3 = ILI9341 RESET#** -- see `fw_lcd_gpio_pulse` in named.c.
- **DMM serial (CS7721 result):** USART2 at 2400 baud, PA3=RX (see Serial section). The CS7721
  result packet arrives here; range/function selection is via separate GPIO control lines
  (exact pins in `fw_dmm_gpio_init` / `fw_gpio_dmm_gpio_init` -- not yet pin-traced). [?]

## Front panel / power
- **Keys:** see Keypad section above. `F1 + Power` at boot = DFU entry.
- **Power-hold:** GPIO latch the MCU must assert to stay powered after button release
  (trace it -- needed early in init or unit powers off).
- **USB:** PA11/PA12 = USB DM/DP (FS device). VID 0x0483 PID 0x2d42.
- **Untraced outputs:** PB8, PB9 — never referenced in firmware, purpose unknown, needs board trace.
- **Other inputs:** PA0/1/2, PC1 (PUPD) -- never referenced in firmware. Purpose unknown; needs board trace.

## USB
- **USB FS device** (STM32 native). Used for DFU + the data/control link. (The vendor
  USB command protocol is documented externally -- hkoosha/hanteker -- but the new
  firmware can define its own host protocol.)

## Deep-capture mod assessment

**Goal:** beat the ~8 KB MachXO2 EBR capture limit.

**SPI/QSPI PSRAM:** tops ~50–133 MB/s. The AD9288 streams 8 bits @ up to 250 MS/s = 250 MB/s — **cannot keep up at full rate**. Would work at slow/medium timebases where the effective rate is decimated below the SPI ceiling. Not at full bandwidth.

**The right memory for full-rate deep capture:** parallel SRAM / SDRAM / HyperRAM (octal HyperRAM ~400 MB/s), written by the FPGA at sample rate.

**Real blocker:** requires rewriting the FPGA bitstream (add memory controller + rework capture datapath) — and we don't have the FPGA source, only compiled "Force update FPGA" binaries. Reversing/replacing a MachXO2 bitstream ≈ a from-scratch FPGA design. Plus: MCU firmware readback changes, free FPGA pins, board space.

**Verdict:** retrofitting external memory into the vendor device is high-effort/limited gain. Pragmatic path: if building clean-room Zephyr firmware, design in **HyperRAM or parallel SRAM up front**.

---

## J1962 / OBD relevance (for the CAN-analyzer goal)
- The scope's 2 channels can sit on CAN-H / CAN-L (differential). For real CAN, the
  FPGA decodes from the digitised bus; decoded frames (~30-100 KB/s) stream over USB
  FS (~1 MB/s) with ample headroom. Note: the STM32's built-in **bxCAN is NOT wired**
  on the stock board (the bus goes via the analog/FPGA path).

## Open hardware questions (need a board trace / scope)
**Resolved:** keypad pins [V], LCD FSMC pins [V], USART1 pins [V], SPI1 pins [V],
ADC1 purpose [V], HSE = 8 MHz [V].

**Still open:**
- FPGA JTAG pin assignments (TCK/TMS/TDI/TDO -> GPIO, loaded via pointer in firmware).
- LCD reset/backlight/RS control GPIOs (on GPIOB -- specific pins not yet traced).
- DMM serial + range-select GPIO lines (CS7721 control side).
- Power-hold GPIO (must be asserted early in init or unit powers off on button release).
- AWG enable GPIO (EL5166 output amp enable, if present).
- FSMC timings (LCD). Whether C-models populate the AWG DAC/amp.
- RDP/SWD readability of the MCU.

## Accuracy reality (measured) + calibrate-by-observation
**Don't expect metrology. This is a toy-grade scope front-end** (real-world observation):
- **No voltage reference** on the scope path -> readings track the supply rail. **Traces
  visibly jump when switching battery <-> USB power** (the rail moves, nothing compensates).
- **The AD9288 is run ~6x over its rated clock** (40 MS/s part driven at ~250 MS/s) --
  cheap-scope practice (Tekway and others have done it for years; add a heatsink), but it
  costs accuracy/linearity.
- **The DMM (CS7721) is the more trustworthy path** -- a dedicated dual-slope multimeter
  chip, slow but self-referencing (integrating ADC), so its absolute accuracy beats the
  scope channels. Use the DMM for *values*, the scope for *shape*.

### What the vendor firmware does (and why it's not enough)
A **static, one-shot factory DC calibration** stored in flash:
- `volt_cali_data[ch][range][N]` -- per-channel/range **gain** factors;
- `bias_up/bias_dn/bias_zero[ch]` -- per-channel **offset** at high/low/zero;
- `A_Cal_Factor`/`ACMA_Cal_Factor` -- global gain; `Serial_Number`; a manual "Cal mode
  for DC A" wizard run once at the factory.
This is **per-unit but static** -- calibrated at one supply condition with **no live
Vref**, so it can't track the rail moving (the battery<->USB jump). Hence the wander.

### Calibrate-by-observation (the real fix for clean-room firmware)
Since there's no stable reference, **make the firmware observe a known signal and
compensate live**, instead of trusting a static table:
1. **On-board reference channel:** route a known stable voltage (a precision Vref chip --
   a $0.30 part the design omitted -- or even the MCU's internal 1.2 V VREFINT) into a
   spare ADC/scope input. Read it every frame -> derive a **live gain/offset correction**
   that tracks supply/temperature drift. This directly kills the battery<->USB jump.
2. **Self-zero / auto-zero:** periodically ground an input (or use the known-0 state)
   to track offset live (the firmware's `bias_zero` but continuous, not one-shot).
3. **Cross-check against the DMM:** the CS7721 is the better reference on-board -- use a
   DMM reading of the same node to correct the scope channel's gain in software.
4. **Per-supply cal tables:** if a live Vref isn't wired, at least store **separate cal
   for battery vs USB** (detect the source -- USB-present is a GPIO) and switch tables.
5. **Keep the per-unit factory `volt_cali_data` as the baseline**, then apply the live
   correction on top.

Net: the hardware is cheap and unreferenced, but a clean-room firmware can **observe a
known reference continuously and correct in software** -- turning a wandering toy into
something usefully repeatable (the DMM-grade path already exists for absolute values).

### Self-cal via AWG loopback (no external reference needed!)
**The device has all three on one box: AWG out + scope inputs + DMM** -- separate
accessible connectors (CH1/CH2 BNC, Gen-Out, DMM banana jacks, per the manual). So a
**jumper wire from Gen-Out -> CH1 (and -> the DMM)** turns it into a **self-calibrating
system**, using only on-board parts.

**The loopback (the clever bit -- the DMM breaks the circularity):**
```
AWG Gen-Out --+--> DMM input    (CS7721 dual-slope = self-referencing -> TRUE voltage)
              +--> scope CH1     (the channel being calibrated)
live scope correction = DMM_reading / scope_reading   (per range, per level)
```
- The **AWG is a known, digitally-commanded source** (you set "output 1.000 V") AND its
  output is already characterized by the firmware's `dds_cali_data`/`dds_horiz_cali_data`.
- **BUT the AWG has no live reference either** (same DAC + supply-rail drift), so
  AWG->scope alone = two unknowns. **The DMM fixes this:** the dual-slope CS7721 is
  supply-independent (integrates against its own clock), so it reads the *true* AWG
  output. Anchor on the DMM -> correct the scope against it.
- **Sweep it:** step the AWG through each scope range/level -> build a **live**
  gain+offset table that tracks the current supply/temperature -- directly killing the
  battery<->USB jump, with no precision-Vref chip added.

**Clean-room firmware procedure (periodic or on-demand auto-cal):**
1. Internally (or via the loopback jumper) drive Gen-Out to a sequence of known levels.
2. Read each on the DMM (= truth) and on the scope channel (= raw).
3. Fit gain+offset per channel/range from (DMM, scope) pairs -> live cal table.
4. Apply on top of the factory `volt_cali_data` baseline; refresh when the supply
   source changes (USB-detect GPIO) or on a timer.
- **Requires EXTERNAL jumpers** -- the four front-end connectors (CH1/CH2 BNC, Gen-Out,
  DMM jacks) are **physically SEPARATE with no internal cross-connect** (confirmed: the
  manual's connectors list + its own probe-check tells you to *cable* Gen-Out->CH1). So
  the loopback cal needs **two wires: Gen-Out->CH1 and Gen-Out->DMM** (or a tee). It is
  NOT automatic. (An earlier draft speculated an internal mux -- the manual shows none;
  if a future board added one, auto-cal would follow, but don't assume it.)

### AWG range limits (min/max) + the cal precision hierarchy
- **Frequency:** min **1 Hz** (or effectively **DC** via a flat arb level -- good for DC
  cal); max 25 MHz sine / 10 MHz square / 5 MHz EXP / 1 MHz ramp.
- **Amplitude:** max **5 Vpp** (hi-Z) / 2.5 Vpp (50 ohm). **Min** isn't a published hard
  floor -- bounded by the **12-bit DAC = ~1.22 mV LSB** (5 V/4096); practical floor is
  the EL5166 output amp's clean level (tens of mV before noise). Set by **Vpp + DC
  offset** (firmware `Vpp=`, `UNInter.offset`).
- The firmware already exposes **"User selectable fast offset calibration"** (manual) --
  a designed-in cal hook.
- **Precision hierarchy makes the loopback clean** (user: "the DMM is more than enough
  for a scope"): **DMM (~0.1 mV, the truth) >> AWG step (~1.2 mV, the swept source) >
  scope (8-bit ~ 4 mV/code at 1 V, the thing corrected).** The DMM out-resolves both the
  AWG steps and the scope by ~40x, so it anchors the cal with margin. AWG can sweep DC
  and AC in fine steps -> cal every scope V/div (10 mV/div ... 10 V/div) against the DMM.

This is the best on-board calibration available: **the AWG is the swept source, the DMM
is the reference, the scope is the thing corrected** -- a complete self-cal loop from
parts already in the box.
