# MCU↔FPGA SPI Protocol & Register Map

> **Source:** Ghidra decompile of 2D82AUTO firmware.
> All findings from firmware analysis unless marked [?] (inferred).
> Function addresses are for the 2D82AUTO build. Cross-reference: [usb-protocol.md](usb-protocol.md).

---

## 1. Physical Interface

| Signal | STM32 pin | Direction | Notes |
|--------|-----------|-----------|-------|
| SPI1_SCK | PA5 | MCU out | SPI1 hardware peripheral |
| SPI1_MISO | PA6 | MCU in | |
| SPI1_MOSI | PA7 | MCU out | |
| FPGA_CS (runtime) | PA4 | MCU out | Active LOW via `fw_gpio_reset/set_bits(GPIOA, 0x10)` |
| FPGA_CS (SSPI config) | PB4 | MCU out | Bitstream programming only, not runtime |

**SPI mode:** CPOL=0, CPHA=0 (mode 0), 8-bit, MSB first. Hardware SPI1 at 72 MHz / divisor.

**Low-level driver functions:**

| Function | Address | Description |
|----------|---------|-------------|
| `fw_spi_sendrecv` | `0x0801de48` | Full-duplex 1-byte exchange. Poll TXE→write DR→poll RXNE→read DR. |
| `fw_spi_send_n` | `0x0801de9e` | Send N bytes, discard RX |
| `fw_spi_recv_n` | `0x0801deba` | Receive N bytes (clocks 0xFF dummy out) |
| `fw_aeabi_fcmpeq` | `0x0801de2c` | ~~Assert CS (GPIOA bit4 LOW)~~ **CORRECTED 2026-07-19: NOT a CS driver — this is a soft-float equality helper (touches no GPIO), confirmed by execution-verify. The real PA4 runtime CS is `fw_gpio_reset_bits(GPIOA,0x10)` in the `0x0801f5xx` idioms.** |
| `fw_SPI1_write` | `0x0801e2ee` | Write 1 byte via SSPI path |

---

## 2. Command Space

All runtime transactions: CS LOW → 2-byte command+register header → optional data → CS HIGH.

| Cmd byte | Direction | Name | Description |
|----------|-----------|------|-------------|
| `0xa8` | MCU→FPGA | WRITE | General register write, 1–4 data bytes |
| `0xc8` | MCU→FPGA | WRITE_CTRL | Control register write (separate bank). 1 data byte. |
| `0xa0` | MCU←FPGA | READ_REG | 3-byte preamble `[cmd][reg][cmd]`, then recv N bytes |
| `0xa4` | MCU←FPGA | READ_BURST | 4-byte + 2-byte split preamble, then recv N sample bytes |
| `0x50` | MCU→FPGA | AWG_TABLE | 2-byte header + 1024 bytes waveform data |

> **Protocol quirk:** READ transactions restate the cmd byte: `[0xa0][reg][0xa0]`. Burst reads send `[0xa4][0x01][addr_lo][addr_hi]` then `[0xa4][0x01]` again. Missing the repeated byte = no FPGA response.

```c
#define FPGA_CMD_WRITE       0xa8
#define FPGA_CMD_CTRL_WRITE  0xc8
#define FPGA_CMD_READ        0xa0
#define FPGA_CMD_BURST_READ  0xa4
#define FPGA_CMD_AWG_TABLE   0x50
#define FPGA_CS_PIN          0x10   /* GPIOA bit4 mask */
#define FPGA_SPI_TIMEOUT     0xC9   /* 201 poll iterations per byte */
#define FPGA_SAMPLE_BUF_SIZE 0x1800 /* 6144 bytes per burst */
#define FPGA_AWG_TABLE_SIZE  0x400  /* 1024 bytes */
#define FPGA_ARM_CAPTURE     0x07   /* ctrl reg 0x00: RUN|ARM_TRIGGER|ENABLE_ACQ */
```

---

## 3. Write Registers — `0xa8` bank

### REG 0x00 — CH1 Front-End Control
`fw_fpga_ch1_frontend` @ `0x0801f54a` · Shadow: `*DAT_08010eb8` · 1 byte  
**Double-write required:** write full `cfg` (energises relay coil), wait ~50 ms, write `cfg & 0xf6` (de-energises coil; relay stays latched).

| Bit | Field | Description |
|-----|-------|-------------|
| [7:5] | `RANGE[2:0]` | Voltage range → FPGA → U1 74HC595 → relays |
| [4] | `COUPLING` | 1=DC, 0=AC |
| [3] | `RELAY_COIL_A` | High-gain relay pulse (500 mV–10 V). Cleared in second write. |
| [2] | `BW_LIMIT` | 1=bandwidth-limit filter enabled |
| [1] | `CH_INACTIVE` | 1=not the active display channel |
| [0] | `RELAY_COIL_B` | Low-gain relay pulse (10–200 mV). Cleared in second write. |

**V/div table (DC coupling, BW off, active):**

| V/div | cfg | RANGE[7:5] | COIL_A | COIL_B |
|-------|-----|------------|--------|--------|
| 10 mV | 0x81 | 100 | 0 | 1 |
| 20 mV | 0xC1 | 110 | 0 | 1 |
| 50 mV | 0x21 | 001 | 0 | 1 |
| 100 mV | 0x01 | 000 | 0 | 1 |
| 200 mV | 0x61 | 011 | 0 | 1 |
| 500 mV | 0x88 | 100 | 1 | 0 |
| 1 V | 0xC8 | 110 | 1 | 0 |
| 2 V | 0x48 | 010 | 1 | 0 |
| 5 V | 0x08 | 000 | 1 | 0 |
| 10 V | 0x68 | 011 | 1 | 0 |

GND coupling override: `RANGE[7:5]` forced to `0b111` (`|0xE0`) regardless of V/div.  
FPGA drives U1 (74HC595): FPGA pin 62=DS, 63=SHCP, 64=STCP → QA-QH → CH1 relays (confirmed).

### REG 0x01 — CH2 Front-End Control
`fw_fpga_ch2_frontend` @ `0x0801f5b0` · Shadow: `*DAT_08010ebc` · 1 byte  
Identical structure to REG 0x00. FPGA drives U7 (74HC595): pins 8/9/10 (order unverified).

### REG 0x02 — Trigger Mode + AWG Enable
`1 byte`

| Bit | Field | Description |
|-----|-------|-------------|
| [6] | `AWG_ENABLE` | 1 = AWG output stage enabled. Gates the AWG output-amp enable/mute — the leading reading for pin60 (see [v07-fabric-datapath.md](v07-fabric-datapath.md) §3). |
| [5] | `FREQ_GATE` | Frequency-counter / gate enable |
| [4:3] | `TRIG_TYPE` | Trigger type select |
| [2:0] | — | trigger mode bits (see [trigger.md](trigger.md)) |

### REG 0x03 / 0x06 / 0x08 — Trigger band + levels

See [trigger.md](trigger.md) for full bit-field detail: REG 0x03 = trigger
hysteresis band (lo/ctr/hi, 3 bytes); REG 0x06 / 0x08 = CH1 / CH2 trigger levels
(16-bit each).

### REG 0x0a — CH1 Vertical Offset (DAC)
`fw_fpga_ch1_voffset` @ `0x0801f724` · 2 bytes LE · `0x8000` = 0 V  
16-bit offset word. FPGA output pin(s) unknown — CIB arc bug blocks trace (see open questions).  
**Open note:** there is **no dedicated offset-DAC chip in the BOM**, so the FPGA must generate the
voffset voltage itself (mechanism unconfirmed — possibly PWM on an undocumented pin). Unresolved.

### REG 0x0c — CH2 Vertical Offset (DAC)
`fw_fpga_ch2_voffset` @ `0x0801f766` · 2 bytes LE · same encoding as REG 0x0a

### REG 0x0e — Timebase Mode Flags
`fw_fpga_timebase_mode` @ `0x0801f7a8` · 1 byte

| Bit | Field | Description |
|-----|-------|-------------|
| [5:4] | `FAST_CH_SELECT` | `0x10`=CH1-fast, `0x20`=CH2-fast, `0x00`=dual/interleaved. Writer `fw_apply_ch_display_mode` (0x080104a0) clears bits 4-5 (`flags & 0xcf`), then sets `0x10` for CH1-only + fast-rate, `0x20` for CH2-only + fast-rate, else leaves `0x00`. Only applies when `FAST_RATE=1` and the sample rate exceeds the ~200 MHz fast-rate threshold. Matches `fpga_spi.h` (FPGA_TB_FAST_CH1=0x10 / FPGA_TB_FAST_CH2=0x20). |
| [3:2] | `ACTIVE_CH` | 0x04=CH1 only, 0x08=CH2 only, 0x0c=dual [?] |
| [1] | `FAST_DUAL` | Fast + dual-channel interleave |
| [0] | `FAST_RATE` | Fast sample rate mode (set when timebase index < 6) |

Boot default: 0x04 (CH1-only active, confirmed via Renode trace).

### REG 0x0f — Timebase Divisor
`fw_fpga_timebase_div` @ `0x0801f7e4` · 4 bytes LE (uint32)  
`effective_sample_rate = 250_000_000 / (divisor + 1)` — the firmware adds 1 before converting to double (`fw_apply_timebase_freq`). The base rate is the interleaved ADC sample rate (**250 MSPS**; see [fpga.md](fpga.md) §"ADC capture architecture").  
Boot default: 0x31=49 → 250 MSPS / 50 = 5.0 MS/s. Timebase index association unknown without Renode sweep (divisor table lives in SRAM 0x20001fc0, not flash).

**All 2000-series models share the same FPGA bitstream.** Per-model bandwidth limits (2D42=40 MHz, 2D72=70 MHz, 2D82=80 MHz) are enforced by the MCU writing a higher minimum divisor — the FPGA PLL is identical across all.

### REG 0x17 — Trigger Edge / Source

See [trigger.md](trigger.md). Summary: bit 0 = EDGE_POL (1=rising, confirmed), bit 1 = SLOPE,
bits **[7:2] = dead** (a full firmware sweep found no writer touches them — no trigger-source field here).

### REG 0x18 — AWG DDS Frequency Word
`fw_fpga_awg_freq` @ `0x0801f8fe` · 4 bytes LE (uint32)  
DDS accumulator clock = **250 MHz** (confirmed from `fw_awg_apply_freq` constants:
`freq_word = freq_hz × (2^32/100_000_000) / 2.5 = freq_hz × 2^32/250_000_000`).  
This is PLL output **CLKOP = 250 MHz** — the main fabric / AWG-DDS clock (see [fpga.md](fpga.md)
§PLL for the full PLL config: CLKOP=250, CLKOS=125, CLKOS2=62.5, CLKOS3=250 MHz).
```
freq_Hz = increment × 250_000_000 / 4_294_967_296
increment = freq_Hz × 4_294_967_296 / 250_000_000
increment = freq_Hz × 17.180  (rounded)
```
FPGA drives DAC bus (pins 36–53) + CLK (pin 54).

### REG 0x1c — AWG Amplitude
`fw_fpga_awg_param1` @ `0x0801f960` · 2 bytes LE · 0x0000=off, 0xFFFF=full scale

### REG 0x1e — AWG Duty Cycle / Symmetry
`fw_fpga_awg_param2` @ `0x0801f9a0` · 2 bytes LE, negated (firmware writes `~value`)  
Larger value = shorter duty. 0 for sine.

### Unused `0xa8` registers

A full firmware sweep found no writer for: **0x04, 0x05, 0x07, 0x09, 0x0b, 0x0d,
0x10–0x16, 0x19–0x1b, 0x1d**. These addresses are never written by the OEM MCU.

---

## 4. Control Registers — `0xc8` bank

### CTRL REG 0x00 — Capture Arm
`fw_fpga_arm_capture` @ `0x0801f846` · always `0x07` (ENABLE_ACQ | ARM_TRIGGER | RUN)

See [trigger.md](trigger.md) for arm/poll sequence detail.

> Do NOT confuse with `0xa8` REG 0x00 (CH1 front-end). Same address, different cmd bank.

---

## 5. Read Registers — `0xa0` bank

Protocol: `CS LOW → [0xa0][reg][0xa0] (3 bytes) → recv N → CS HIGH`

### READ REG 0x00 — FPGA Status (1 byte)
`fw_fpga_read_status` @ `0x0801f9e0` · stored at `g_fpga_status` (0x2000d3c3)  
Bit [1] = BUSY: poll until 0 → capture complete.

### READ REG 0x01 — Ring-Buffer Write-Pointer (2 bytes)
`fw_fpga_read_extstat` @ `0x0801fa30` · stored at `g_fa30_var1`/`g_fa30_var2` (0x2000d3c4/0x2000d3c5) · `fw_spi_recv_n(&local_14, 2)`  
Returns the ring-buffer write-pointer as a 16-bit value `CONCAT11(byte1, byte0)`, latched at
trigger-arm time. Both call sites (scope-arm / trigger-setup) use it as a "before" baseline: they
later re-read `fw_fpga_read_endaddr` (REG 0x03) and compute `(new_endaddr − snapshot) * 2 mod 0x1800`
to measure how much capture data has accumulated since arming. Not a generic status/flags register.

### READ REG 0x05 — FPGA Identification (8 bytes)
`fw_fpga_read_ident` @ `0x0801fad2` · stored at `g_fpga_ident_buf` (0x08032fc8, SRAM 0x2000d2c0)

| Byte | Content |
|------|---------|
| [0..5] | Unconfirmed/unused — no reader anywhere besides `fw_fpga_read_ident`'s own per-byte debug-log loop (a trace print, not a semantic use). |
| [6] | **FPGA bitstream version** — displayed on the info screen as `V01` etc (clamped to min 1); returned to PC via USB cal cmd 0x0c. A live USB probe against the physical board returned `7`, matching the on-disk `fpga/v7/FPGA_V07.bin` ("V07"). |
| [7] | Unconfirmed/unused — same as [0..5]. |

Live 8-byte capture (UART trace of the per-byte log, via a USB cal cmd 0x0c read):
`[0]=2 [1]=0 [2]=1 [3]=9 [4]=19 [5]=49 [6]=7 [7]=133`. Only byte [6]=7 has established meaning;
the rest are observed-but-unconfirmed. The ident value is **not** statically recoverable from the
netlist — see [fpga-pluribus-recovery.md](fpga-pluribus-recovery.md) §"REG 0x05 ident" for why
(soft config-corner SPI decoder, version-invariant constants).

Not read at boot — called only when entering the info screen (`fw_fpga_status_screen`) or via USB cal cmd 0x0c. No version check or boot-halt on mismatch.

---

## 5b. MachXO2 ISC (bitstream programming) — `0x0e/0x26/…` commands

These are sent over **GPIOB bit4 CS** (not PA4). Used only during `fw_fpga_write_config` / `fw_fpga_config_restore`. Not part of the runtime register interface.

| Cmd | Name | Wire |
|-----|------|------|
| `0x0e` | ISC_ENABLE | `[0x0e] + 3×0x00` |
| `0x26` | ISC_ERASE | `[0x26] + 3×0x00` |
| `0x46` | LSC_INIT_ADDRESS | `[0x46] + 3×0x00` |
| `0x47` | LSC_WRITE_ADDRESS | `[0x47] + 3×0x00` |
| `0xc9` | PROG_INCR_NV | `[0xc9][0x00][0x00][0x01] + 16 data bytes` |
| `0x3c` | LSC_READ_STATUS | `[0x3c] + 4×0x00` → recv 4B; bit12=BUSY (poll until 0). **Firmware uses 0x3C, not 0x70.** |
| `0x5e` | ISC_PROGRAM_DONE | `[0x5e] + 3×0x00` |
| `0x74` | LSC_REFRESH (RTI) | `[0x74][0x08] + 2×0x00` |
| `0x79` | LSC_REFRESH | `[0x79] + 2×0x00` |
| `0xca` | LSC_READ_INCR_NV | `[0xca][0x10][0x00][N] + recv(N×16 B)` |

Initialisation: `fw_fpga_sspi_init` clocks 8×`0xff` to reach idle state. Timing: erase ~500 ms, program 2688 pages × 200 µs/page ≈ 538 ms. Total ~1 s per flash update.

---

## 6. AWG Waveform Table — `0x50` bank

`fw_fpga_awg_table` @ `0x0801f8aa`  
Format: `[0x50][0x00]` + 1024 × uint8 DAC samples (0x80 ≈ 0 V).  
Stored in FPGA EBR tile R6C20 (PDPW8KC, 18-bit × 512).

---

## 7. Capture Data Burst Read — `0xa4` bank

`fw_fpga_read_samples_full` @ `0x0801e14a` · `fw_fpga_capture_process` @ `0x0801ded8`

> Two functions share the Ghidra name `fw_fpga_read_samples`: `0x0801e14a`
> (`fw_fpga_read_samples_full`, the full 0x1800/6144-byte whole-capture burst described here) and
> `0x0801fc04` (`fw_fpga_read_samples_windowed`, wraps `fw_scope_spi_read_samples` for the live-view
> display path's smaller 0x4b0-sample windowed reads). Full disambiguation:
> [vendors/hantek/2d82/docs/hardware/waveform-readback.md](../../vendors/hantek/2d82/docs/hardware/waveform-readback.md) §1.

**Wire protocol:**
```
CS LOW
→ [0xa4][0x01][start_lo][start_hi]   (4 bytes)
→ [0xa4][0x01]                        (preamble REPEATED — FPGA requires this)
← [byte_0]...[byte_N-1]               (N raw ADC samples)
CS HIGH
```

**Sample encoding:** 8-bit offset-binary. `0x80`=0 V, `0x00`=−FS, `0xFF`=+FS.  
After calibration: `corrected = clamp(raw − zero_offset − 0x80, 0, 0xFF)`

**Channel modes:**

| `mode` | Layout |
|--------|--------|
| 1 | Single-channel. Each byte = one CH1 sample. |
| 2 | Dual interleaved. Even bytes=CH1, odd bytes=CH2. |

**Buffer sizes:** CH1 @ `0x200023e4`, CH2 @ `0x20003be4` — both 0x1800 bytes (6144 samples).

**No DMA on capture path.** All 6144 bytes CPU-polled via `fw_spi_sendrecv`. SPI1 DMA (DMA1 Ch2/Ch3) would free the CPU but is not used.

> **MCU-side call chain and USB-side packet format** (how these bytes get from `g_ch1_samples`/
> `g_ch2_samples` into a USB response, plus the separate live-view display read path that uses the
> identical wire command) are traced in
> [vendors/hantek/2d82/docs/hardware/waveform-readback.md](../../vendors/hantek/2d82/docs/hardware/waveform-readback.md).

---

## 8. Complete Capture Sequence

```
1. CONFIGURE FRONT-END
   fw_fpga_ch1_frontend(cfg)    → 0xa8 0x00 [byte]  (double-write + 50 ms relay delay)
   fw_fpga_ch2_frontend(cfg)    → 0xa8 0x01 [byte]
   fw_fpga_timebase_mode(flags) → 0xa8 0x0e [byte]
   fw_fpga_timebase_div(...)    → 0xa8 0x0f [b0][b1][b2][b3]
   fw_fpga_ch1_voffset(lo,hi)   → 0xa8 0x0a [lo][hi]
   fw_fpga_ch2_voffset(lo,hi)   → 0xa8 0x0c [lo][hi]

2. CONFIGURE TRIGGER
   fw_fpga_trig_mode(flags)     → 0xa8 0x02 [byte]
   fw_fpga_trig_band(lo,c,hi)   → 0xa8 0x03 [lo][center][hi]
   fw_fpga_ch1_triglevel(lo,hi) → 0xa8 0x06 [lo][hi]
   fw_fpga_ch2_triglevel(lo,hi) → 0xa8 0x08 [lo][hi]
   fw_fpga_trigger_edge(flags)  → 0xa8 0x17 [byte]

3. ARM
   fw_fpga_arm_capture()        → 0xc8 0x00 0x07

4. POLL FOR DONE
   loop: fw_fpga_read_status()  → 0xa0 0x00 0xa0 → recv 1 byte
         if (status & 0x02) == 0: break

5. READ END ADDRESS
   fw_fpga_read_endaddr()       → 0xa0 0x03 0xa0 → recv 2 bytes
   start = (end_addr − 0x1800) & 0x1FFF

6. BURST READ
   fw_fpga_read_samples_full(start, ch, mode, 0x1800):
     → [0xa4][0x01][start_lo][start_hi][0xa4][0x01] ← 6144 bytes
   → fw_fpga_capture_process: calibrate + store to g_ch1/ch2_samples
```

---

## 9. Register → FPGA Pin Summary

| SPI Reg | Bytes | Function | FPGA Pins |
|---------|-------|----------|-----------|
| 0x00 | 1 | CH1 AFE range/coupling/relay | 62,63,64 → U1 → relays |
| 0x01 | 1 | CH2 AFE range/coupling/relay | 8,9,10 → U7 → relays |
| 0x02 | 1 | Trigger mode + AWG enable | internal only |
| 0x03 | 3 | Trigger hysteresis lo/ctr/hi | internal only |
| 0x06 | 2 | CH1 trigger level | internal only |
| 0x08 | 2 | CH2 trigger level | internal only |
| 0x0a | 2 | CH1 vertical offset DAC | **unknown — CIB arc bug** |
| 0x0c | 2 | CH2 vertical offset DAC | **unknown — CIB arc bug** |
| 0x0e | 1 | Timebase mode / channel select | internal (gates ENCA/ENCB) |
| 0x0f | 4 | ADC decimation divisor | internal counter |
| 0x17 | 1 | Trigger edge polarity + slope (bits [7:2] dead) | internal only |
| 0x18 | 4 | AWG DDS frequency word | 36–53 (DAC D0-D11), 54 (CLK) |
| 0x1c | 2 | AWG amplitude | 36–53 (DAC bus) |
| 0x1e | 2 | AWG duty cycle / symmetry | 36–53 (DAC bus) |
| CTRL 0x00 | 1 | Capture arm (0x07) | 65 (ENCA), 98 (ENCB), EBR R6Cx |
| READ 0x00 | 1 | Status BUSY bit | (readback) |
| READ 0x01 | 2 | Ring-buffer write-pointer (arm-time baseline) | EBR ring pointer |
| READ 0x03 | 2 | Ring-buffer end address | EBR ring pointer |
| READ 0x05 | 8 | FPGA ident — byte[6]=version | (readback) |
| 0x50 table | 1024 | AWG waveform | EBR R6C20 |

---

## 10. FPGA Pin Map

### MCU SPI Interface

| FPGA Pin | Signal | Connected To | Confidence |
|----------|--------|-------------|------------|
| 27 | FPGA_nCS | STM32 PA4 | inferred |
| 31 | FPGA_CCLK | STM32 PA5 SPI1_SCK | inferred |
| 32 | FPGA_MISO | STM32 PA6 SPI1_MISO | inferred |
| 48 | FPGA_SN | pulled-up = SPI slave active | inferred |
| 49 | FPGA_MOSI | STM32 PA7 SPI1_MOSI | inferred |

### Clock

| FPGA Pin | Signal | Connected To | Confidence |
|----------|--------|-------------|------------|
| 1 | XTAL_IN (L_GPLLT_IN) | 25 MHz crystal XI | confirmed |
| 2 | XTAL_FB (L_GPLLC_IN) | 25 MHz crystal XO | confirmed |

PLL outputs (internal): CLKOP=CLKOS3=250 MHz, CLKOS=125 MHz (→ DAC_CLK + ADC_ENCA/ENCB), CLKOS2=62.5 MHz. Full config in [fpga.md](fpga.md) §PLL.

### ADC (AD9288BSTZ dual 8-bit, U31) — Channel A

| FPGA Pin | Signal | ADC Pin | Confidence |
|----------|--------|---------|------------|
| 75 | ADC_D0A | pin 37 (LSB) | confirmed |
| 74–67 | ADC_D1A–D6A | pins 38–43 | inferred |
| 66 | ADC_D7A | pin 44 (MSB) | confirmed |
| 65 | ADC_ENCA | pin 47 | estimate (CIB arc bug; on the shared 125 MHz clock net, same deliberate fanout as pin 61 — see [v07-fabric-datapath.md](v07-fabric-datapath.md) §3) |

### ADC — Channel B

| FPGA Pin | Signal | ADC Pin | Confidence |
|----------|--------|---------|------------|
| 83 | ADC_D0B | pin 24 (LSB) | confirmed |
| 84–88,96,97 | ADC_D1B–D7B | pins 23–17 | inferred |
| 98 | ADC_ENCB | pin 14 | inferred (net = PLL CLKOS = 125 MHz) |

ENCA/ENCB are 180° out of phase at 125 MHz → **250 MSPS** effective.

### DAC (3PD5651E / DAC902 12-bit, U12) — AWG output

| FPGA Pin | Signal | DAC Pin | Confidence |
|----------|--------|---------|------------|
| 36,38,42,43 | DAC_D0,D2,D6,D7 | pins 12,10,6,5 | inferred (CIB arc bug) |
| 37,39,40,41 | DAC_D1,D3,D4,D5 | pins 11,9,8,7 | inferred |
| 45,51,52 | DAC_D8,D9,D10 | pins 4,3,2 | inferred |
| 53 | DAC_D11 (MSB) | pin 1 | confirmed |
| 54 | DAC_CLK | pin 28 | confirmed (net = PLL CLKOS = 125 MHz) |
| 35 | DAC_PD | pin 15 | inferred (CIB arc bug) |

### AFE Shift Registers (74HC595)

| FPGA Pin | Signal | Target | Confidence |
|----------|--------|--------|------------|
| 62 | U1_DS | U1 pin 14 DS (CH1 AFE data) | confirmed |
| 63 | U1_SHCP | U1 pin 11 (CH1 shift clk) | confirmed |
| 64 | U1_STCP | U1 pin 12 (CH1 latch clk) | confirmed |
| 8 | U7_DS? | U7 pin 14 DS (CH2 AFE data) | inferred |
| 9 | U7_SHCP? | U7 pin 11 | inferred |
| 10 | U7_STCP? | U7 pin 12 | inferred |

U1 and U7 are NOT daisy-chained.

### Unknown / Unresolved Pins

Full fabric-trace verdicts (FF drivers, clock domains, cone-size fingerprints) live in
[v07-fabric-datapath.md](v07-fabric-datapath.md) §3 — the canonical home. Summary:

| FPGA Pin | Verdict (see datapath doc) |
|----------|----------------------------|
| 60 | DAC-A-domain fabric output; leading reading = **AWG output-amp enable/mute** (REG 0x02 bit6 `AWG_ENABLE`). NOT a DAC data bit. Board trace needed. |
| 99 | DAC-B-phase fabric output — a *different* signal from pin60. Board trace needed. |
| 61 | **Clock-output tap** on the shared 125 MHz PLL clock net (fans out to DAC_CLK/DAC_PD/ADC_ENCB) — a deliberate fanout, not a short. Confirm which board net it clocks. |
| 3 | Small status/control output (tiny fan-in cone), not datapath. |
| 4 | Edge-CIB orphan (grouped with DAC_D6/D7, U7_DS) — decoder tooling gap (pluribus#29 item 3), not a design fact. |

### EBR Block RAM

| Tile | Mode | Width | Depth | Function |
|------|------|-------|-------|----------|
| R6C1,C4,C10 | DP8KC | 2-bit | 3×1024 B | ADC ring buffer |
| R6C7,C14 | DP8KC | 4-bit | 2×1024 B | ADC ring buffer |
| R6C17 | PDPW8KC | 2-bit | 1024 B | ADC ring buffer |
| **R6C20** | PDPW8KC | **18-bit** | 1152 B | **AWG waveform table** |

ADC ring total = 6144 bytes = 0x1800 (matches `g_ch1_samples`/`g_ch2_samples`).

---

## 11. Boot Defaults (Renode trace 2026-06-22)

All 28 FPGA SPI transactions during emulated boot:

| # | Register | Value | Decoded |
|---|----------|-------|---------|
| 1 | REG 0x0e | 0x04 | CH1-only active, normal rate |
| 2 | REG 0x00 | 0x00 | CH1 front-end init reset |
| 3 | REG 0x01 | 0x02 | CH2 inactive |
| 4–11 | REG 0x00/0x01 | 0x88/0x8a | 500 mV/div AC, relay double-writes ×3 each |
| 12 | REG 0x0a | 0x00 0x00 | CH1 V-offset = 0 |
| 13 | REG 0x0c | 0x00 0x00 | CH2 V-offset = 0 |
| 14–16 | REG 0x02 | 0x01 | Trigger mode (3 writes) |
| 17 | REG 0x03 | lo=0x76 ctr=0x80 hi=0x8a | DC trigger at midscale, ±10 band |
| 18 | CTRL 0x00 | 0x07 | Arm capture |
| 19 | REG 0x0f | 0x31 0x00 0x00 0x00 | Divisor=49 → 250 MSPS/50 = 5.0 MS/s |
| 20–22 | REG 0x0e | 0x04 | (2 more writes) |
| 23 | REG 0x06 | lo=0x00 hi=0x0c | CH1 trigger level = 0x0c00 |
| 24 | REG 0x08 | lo=0x00 hi=0x06 | CH2 trigger level = 0x0600 |
| 25 | REG 0x17 | 0x01 | Rising edge (EDGE_POL=1 confirmed) |
| 26 | REG 0x02 | 0x01 | Final trigger mode write |

---

## 12. Known Bugs / Oddities

**Relay double-write:** REG 0x00/0x01 must be written twice with 50 ms between. First = full config (energises coil). Second = `byte & 0xf6` (de-energises; relay stays latched). Skipping this breaks gain/coupling switching.

**READ preamble repeat:** `0xa0` reads require `[cmd][reg][cmd]` (3 bytes out). `0xa4` burst reads require 4+2 byte split. Missing the repeated byte = no response.

**`0xa8` vs `0xc8` REG 0x00:** CH1 front-end and capture-arm share the same register address but are different cmd banks. Do not conflate.

**No DMA on sample readout:** 6144 bytes read byte-by-byte on CPU. Fixable with DMA1 Ch2 (SPI1 RX) in replacement firmware.

**Mislabeled functions (corrected):**
- `fw_fpga_read_capture` @ `0x08016dc8` → actually `fw_fpga_config_restore` (SSPI bitstream programming)
- `fw_fpga_status` @ `0x08016e74` → actually `fw_fpga_status_screen` (LCD display, no FPGA comms)

Open questions are tracked as GitHub issues — filter by label [`fpga`](https://github.com/awtoau/awto-2000/issues?q=is%3Aopen+label%3Afpga).
