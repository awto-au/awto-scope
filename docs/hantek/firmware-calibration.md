
# Calibration Data Format — 2D82AUTO Firmware

> Reversed from `fw_calibrate` (0x08018c18) and related functions.  
> All offsets in the main block are confirmed from code; gaps are marked [?].

---

## Overview

There are four distinct calibration data regions. Three live in STM32 internal flash,
one exists only in RAM (written from PC via USB).

| Name | Size | Flash (runtime ptr) | SRAM | Save function |
|------|------|---------------------|------|---------------|
| Scope cali block | 0x2f8 | `g_scope_flash_start` (ptr@`0x08019100`) | `0x080190f4` | `fw_scope_config_save_flash` |
| Cal RAM block | 0x130 | `g_cali_flash_start` (`DAT_08016218`) | `g_cali_ram_buf` (`DAT_08016b90`) | `fw_cali_save_to_flash` |
| Volt cal floats | 0x0ec | `g_volt_cali_flash_start` (`DAT_08016220`) | `g_volt_cali_ram_buf` (`DAT_08016228`) | (written by USB/FPGA path) |
| USB cali table | 0x1e0 | none (RAM only) | `g_cali_table` (`DAT_08032f7c`) | none (PC recalibrates on reconnect) |

Flash calibration sector: `0x0805c000` (factory slot, product-specific — usually 0xFF/erased on consumer units).

---

## 1. Scope Calibration Block (`g_scope_cali_block`) — 0x2f8 bytes

This is the primary calibration output. Written by `fw_calibrate` (on-device interactive calibration)
and by `fw_scope_config_save_flash` which saves scope config + calibration together.

### 1a. Voffset DAC calibration table — offsets 0x000..0x1df

`fw_calibrate` binary-searches the FPGA vertical offset DAC (`fw_fpga_ch1/ch2_voffset`) for each
voltage range step. It finds three DAC values per step: the value that produces ADC readings
of 0, +100, and −100 counts. These three points define a linear transfer function for the ADC path.

**Storage formula:** `g_scope_cali_block + ref_base + pass*0x50 + ch*0x28 + step*4`

| Symbol | Range | Meaning |
|--------|-------|---------|
| `ref_base` | 0x000 / 0x0a0 / 0x140 | Zero-ref / Pos-ref (+100) / Neg-ref (−100) DAC values |
| `pass` | 0 / 1 | Path=0: `fw_usb_gpio_init` path (AC-path cal); Path=1: `fw_scope_timebase_config_init` (DC-path cal) |
| `ch` | 0 / 1 | CH1 / CH2 |
| `step` | 0..9 | Gain step (10 steps = 10 V/div ranges: step 0 = lowest gain, step 9 = highest) |

Each value is a **uint32** FPGA DAC word. The DAC word is split when sent over SPI:
```c
fw_fpga_ch1_voffset(dac & 0xff, dac >> 8 & 0xff);  // 16-bit LE pair
```

Full layout of the 0x1e0-byte table:

```
+0x000..+0x027  pass=0, ch=0 (CH1), zero-ref DAC:  uint32[10]  (step 0..9)
+0x028..+0x04f  pass=0, ch=1 (CH2), zero-ref DAC:  uint32[10]
+0x050..+0x077  pass=1, ch=0 (CH1), zero-ref DAC:  uint32[10]
+0x078..+0x09f  pass=1, ch=1 (CH2), zero-ref DAC:  uint32[10]

+0x0a0..+0x0c7  pass=0, ch=0 (CH1), pos-ref  DAC:  uint32[10]   (+100 ADC counts target)
+0x0c8..+0x0ef  pass=0, ch=1 (CH2), pos-ref  DAC:  uint32[10]
+0x0f0..+0x117  pass=1, ch=0 (CH1), pos-ref  DAC:  uint32[10]
+0x118..+0x13f  pass=1, ch=1 (CH2), pos-ref  DAC:  uint32[10]

+0x140..+0x167  pass=0, ch=0 (CH1), neg-ref  DAC:  uint32[10]   (−100 ADC counts target)
+0x168..+0x18f  pass=0, ch=1 (CH2), neg-ref  DAC:  uint32[10]
+0x190..+0x1b7  pass=1, ch=0 (CH1), neg-ref  DAC:  uint32[10]
+0x1b8..+0x1df  pass=1, ch=1 (CH2), neg-ref  DAC:  uint32[10]
```

**Convergence:** binary-search stops when `|measured - target| <= threshold`, where threshold=2
for steps 0–2 (low-gain, noisiest), threshold=0 (exact) for steps 3–9. Anti-oscillation: if
the midpoint repeats 5 consecutive times, the search is forced to converge.

**Step ordering:** `fw_calibrate` iterates steps 9 down to 0 (high gain first), but array indexing
is step 0 = index 0, so step 9 is written to index 9 regardless.

### 1b. Final zero measurements — offsets 0x1e0..0x1e7

Post-calibration verification: `fw_scope_wave_process` is called at the end to measure residual
DC offset. These are integer ADC-count readings.

```
+0x1e0  int32   CH1 final zero measurement  (ADC counts after calibration)
+0x1e4  int32   CH2 final zero measurement
```

### 1c. Padding — offsets 0x1e8..0x1f7

24 bytes. **Confirmed: no firmware code reads or writes these bytes. They are actively zeroed on every flash write cycle.**

Proof from `fw_fpga_write_config` (23251):
```c
fw_memset_word(buf, 0x2f8, 0);   // zeros entire 0x2f8 buffer
// sel=0: fills buf+0x000..+0x1f7  (0x1f8 bytes)
// sel=1: fills buf+0x1f8..+0x2e3  (0xec bytes)
// sel=2: fills buf+0x2e4..+0x2f3  (0x10 bytes)
// +0x1e8..+0x1f7 is always left as 0x00 before write to FPGA flash
```

Proof from `fw_fpga_config_restore` (23347):
```c
fw_memcpy_fast(g_cali_default_buf_ptr, buf, 0x1f8);       // reads +0x000..+0x1f7
fw_memcpy_fast(g_cali_zero_sub_buf_ptr, buf + 0x1f8, 0xec); // reads +0x1f8..+0x2e3
fw_memcpy_fast(g_cali_gpio_nibble_ptr, buf + 0x2e4, 0x10);  // reads +0x2e4..+0x2f3
// +0x1e8..+0x1f7 is read into buf but never distributed — silently discarded
```

**This range is not a candidate for a firmware-read capability field.** The firmware erases it to zero on every cal flash write and ignores it on read. A factory-written value here would be destroyed the first time the scope runs `fw_calibrate` or any USB cali save command that triggers `fw_fpga_write_config`.

### 1d. Voltage calibration float table — offsets 0x1f8..0x2e3 (0xec bytes)

Also stored separately as `g_volt_cali_ram_buf` (see §3 below). This copy at +0x1f8 is written
by `fw_fpga_write_config(sel=1)` (memcpy from `DAT_080170f0`) and read back by `fw_fpga_config_restore`.

```c
// Storage formula (from fw_cali_volt_data_dump):
//   base = &g_scope_cali_block[0x1f8]
//   &entry = base + mode*0x50 + adc_ch*0x28 + step*4 + 4

+0x1f8          uint32    header   (version/type tag)  [?]
+0x1fc..+0x29b  float[2][2][10]
                  [mode=0..1][adc_ch=0..1][step=0..9]
+0x29c..+0x2e3  0x48 bytes  unknown / unused  [?]
```

**Dimensions confirmed from `fw_cali_capture_samples`:**

| Dimension | Value | Meaning |
|-----------|-------|---------|
| `mode=0` | outer=0 | Single-channel interleaved (250 MSPS) — both ADC slices sampling one input |
| `mode=1` | outer=1 | Dual-channel (125 MSPS per channel) — ADC-A=CH1, ADC-B=CH2 |
| `adc_ch=0` | inner=0 | ADC physical channel A gain factor |
| `adc_ch=1` | inner=1 | ADC physical channel B gain factor |
| `step` | 0..9 | Gain step (0=lowest gain/highest V/div, 9=highest gain) |

Access pattern (confirmed from code):
- Single-ch, count=1, arg=0 (ADC-A): `volt_cal[0][0][step]` at offset 4
- Single-ch, count=1, arg=1 (ADC-B): `volt_cal[0][1][step]` at offset 0x2c
- Dual-ch, even sample (ADC-A = CH1): `volt_cal[1][0][step]` at offset 0x54
- Dual-ch, odd sample  (ADC-B = CH2): `volt_cal[1][1][step]` at offset 0x7c

**Physical meaning:** The AD9288 dual ADC has two independently-characterised 8-bit slices (A and B).
In 250 MSPS interleaved mode they sample the same input on alternate clocks; their gain mismatch
creates interleave spurs without correction. In 125 MSPS dual mode, A handles CH1 and B handles CH2.
These float factors are per-step gain multipliers applied to raw ADC counts before display.

### 1e. DMM calibration block — offsets 0x2e4..0x2f3 (0x10 bytes)

Written by `fw_fpga_write_config(sel=2)` (memcpy from `g_cali_gpio_nibble_ptr` → SRAM `0x2000D268`)
and restored by `fw_fpga_config_restore` → `fw_memcpy_fast(g_cali_gpio_nibble_ptr, buf+0x2e4, 0x10)`.
Applied by `fw_cali_apply_ch2()` which copies 0x10 bytes from flash into the DMM CaliParam struct.

**This is the DMM CaliParam calibration block** — DC/AC milliamp scaling factors for the CS7721 DMM
chip. Previously mislabelled "AWG CH2 calibration". Written by USB cali commands:
- sub `0x0e`: DC milliamp cal factor → `CaliParam+4` (float), sets `CaliParam[0]=1`, then `fw_fpga_write_config(2)`
- sub `0x0f`: AC milliamp cal factor → `CaliParam+0xc` (float), sets `CaliParam[8]=1`, then `fw_fpga_write_config(2)`

Debug format strings confirm: `ptr_usb_fmt_dcma_cal` (sub 0x0e), `ptr_usb_fmt_acma_cal` (sub 0x0f).

```
+0x2e4  uint32   dcma_enable    (set to 1 by USB cali cmd 0x0e)
+0x2e8  float    DCMA_Cal_Factor  (DC milliamp calibration — uint16 pkt value / 4.5)
+0x2ec  uint32   acma_enable    (set to 1 by USB cali cmd 0x0f)
+0x2f0  float    ACMA_Cal_Factor  (AC milliamp calibration — uint16 pkt value / 4.5)
```

The pointer `g_awg2_param_ptr` (`DAT_08032fa0`) in the TSV was **misnamed** — it points to this same
DMM CaliParam struct at `0x2000D268`, not to any AWG parameter block.

Scaling constants (confirmed from binary):
- `flt_usb_cali_1p0` @ `0x080329E0` = 1.0
- `flt_usb_cali_4p5` @ `0x080329E4` = 4.5
- Formula: `factor = uint16_from_pc / 4.5`

### 1f. Validity flags — offsets 0x2f4..0x2f7

```
+0x2f4  uint8   volt_cali_valid   (set by fw_fpga_write_config(sel=1); checked by fw_cali_other_flag_get)
+0x2f5  uint8   scope_cali_valid  (set by fw_fpga_write_config(sel=0); checked by fw_cali_zero_flag_get)
+0x2f6  uint8   dmm_cali_valid    (set by fw_fpga_write_config(sel=2); checked by fw_cali_htx1_write_flag_get)
+0x2f7  uint8   padding
```

`fw_fpga_write_config` sel→flag mapping (confirmed from code:23264):
- sel=0 (DAC table): sets `+0x2f5` (scope_cali_valid)
- sel=1 (volt floats): sets `+0x2f4` (volt_cali_valid)
- sel=2 (AWG/misc):   sets `+0x2f6` (misc_cali_valid)

**Boot sequence:** On power-up, firmware checks each flag and restores the calibration region
if the flag is **not yet set** (`!= 1`). A unit with all flags = 1 skips restoration (cal already
applied from flash). A unit with flags = 0 or 0xFF (erased) runs the full restore sequence from
defaults.

---

## 2. Cal RAM Block (`g_cali_ram_buf`) — 0x130 bytes

Loaded by `fw_cali_apply_ch1()` from flash at `g_cali_flash_start`.  
Saved by `fw_cali_save_to_flash()` (USB cali cmd 0x0c sub=6).

```
+0x000  uint8   valid_flag   (0x01 = block valid; checked by fw_cali_verify_checksum at buf[0])
+0x001  uint8   written_flag (0x01 = data written; set by fw_cali_save_to_flash before write;
                             checked by fw_cali_apply_ch1 at buf[1] — NOTE: apply_ch1 checks
                             buf[1], not buf[0]; verify_checksum checks buf[0])
                             Also first byte of GPIO nibble data (overlaps):
                             nibble_0 = buf[1] & 0x07 | (buf[2] << 4)
                             nibble_1 = buf[3] & 0x07 | (buf[4] << 4)
                             nibble_2 = buf[5] & 0x07 | (buf[6] << 4)
                             nibble_3 = buf[7] & 0x07 | (buf[8] << 4)
                             Validated by fw_cali_gpio_nibble_validate() against 12 allowed
                             MachXO2 FPGA status codes: 0xd7 0x50 0xb5 0xf1 0x72 0xe3 0xe7
                             0x51 0xf7 0xf3 0x86 0x00
+0x001..+0x008  uint8[8]   FPGA GPIO nibble bytes (packed MachXO2 status codes)
+0x009..+0x12f  scope config state (0x127 bytes)
```

**Scope config serialisation** (written by `fw_cali_load_channel`, restored by `fw_cali_verify_checksum`):

| Buf offset | Size | Source RAM | Content |
|-----------|------|-----------|---------|
| +0x001 | 1 | scope param cur | `fw_scope_param_get_cur()` |
| +0x002 | 1 | scope param cur1b | `fw_scope_param_get_cur1b()` |
| +0x004 | 0x10 | `DAT_080161e4` | scope param table A |
| +0x014 | 0xb8 | `DAT_080161e0` | main scope param struct |
| +0x0cc | 0x18 | `DAT_080161dc` | secondary scope param struct |
| +0x0e4 | 0x24 | `DAT_0801620c` | scope config ext |
| +0x108 | 0x94 | `DAT_080161d8` | primary scope config |
| +0x152 | 0x94 | `DAT_080161d8+0x4a` | secondary scope config (overlaps above) |
| +0x19c | 0x2a | `DAT_08016210` | scope config C |
| +0x1c8 | 0x30 | `DAT_08016214` | scope config D |

When restored (`fw_cali_verify_checksum` with ch=1/2/3), the firmware applies all hardware settings:
timebase, coupling, frontend gain, trigger, bandwidth limit, vertical offset, backlight PWM.

---

## 3. Voltage Cal Float Table (`g_volt_cali_ram_buf`) — 0xec bytes

Loaded at boot from `g_volt_cali_flash_start` by `fw_cali_volt_data()` if `volt_cali_valid` is set.  
Printed by `fw_cali_volt_data_dump()` as a 2×2×10 float table.

```c
// Address formula (from fw_cali_volt_data_dump decompile):
//   &entry = g_volt_cali_ram_buf + mode*0x50 + adc_ch*0x28 + step*4 + 4

+0x000          uint32    header                (version/type tag)  [?]
+0x004..+0x02b  float[10] mode=0, adc_ch=0     ADC-A gain, interleaved single-ch mode
+0x02c..+0x053  float[10] mode=0, adc_ch=1     ADC-B gain, interleaved single-ch mode
+0x054..+0x07b  float[10] mode=1, adc_ch=0     ADC-A gain, dual-ch mode (CH1)
+0x07c..+0x0a3  float[10] mode=1, adc_ch=1     ADC-B gain, dual-ch mode (CH2)
+0x0a4..+0x0eb  0x48 bytes   unknown / unused  [?]
```

This is a mirror of the float table embedded in `g_scope_cali_block+0x1f8`.
See §1d for the confirmed meaning of `mode` and `adc_ch`.

---

## 4. USB Calibration Table (`g_cali_table`) — 0x1e0 bytes

RAM only. Written from PC calibration software via USB cmd 0x0b (sub codes 1–6).
Not persisted to flash independently; PC tool must resend on reconnect.

Stored at SRAM `0x08032f7c` (aliased via `g_usb_cali_databuf_ptr` at `0x080329A0`).

The struct fields are confirmed from `fw_usb_cali_read_handler` format strings (`ptr_usb_fmt_bias_*`):
```
CaliResult.bias_up  [ch=0..1][volt_range=0..9][step=0..9]   — DAC that yields +100 ADC counts
CaliResult.bias_dn  [ch=0..1][volt_range=0..9][step=0..9]   — DAC that yields −100 ADC counts
CaliResult.bias_zero[ch=0..1][volt_range=0..9][step=0..9]   — DAC that yields 0 ADC counts
```

```c
// Address formula:
//   &entry = g_cali_table + band_offset + ch*0x28 + step*4

int32_t g_cali_table[6_bands × 2_ch × 10_steps];   // 0x1e0 = 480 bytes
```

| USB sub code | Band offset | Band name |
|-------------|-------------|-----------|
| 0x05 | +0x000 | DC 1× probe |
| 0x06 | +0x050 | DC 10× probe |
| 0x01 | +0x0a0 | AC low freq |
| 0x02 | +0x0f0 | AC mid freq |
| 0x03 | +0x140 | AC high freq 1 |
| 0x04 | +0x190 | AC high freq 2 |

Each entry is int32. Read back by USB cmd 0x0b readback path (returns as int16 — low 16 bits only).

**Note:** The band names (AC low/mid/high-1/high-2) likely correspond to frequency ranges for
the gain-error correction vs frequency. The exact frequency breakpoints are not yet determined. [?]

---

## 5. DMM Calibration Parameters

The DMM has its own calibration parameter struct at SRAM `0x2000D268` (pointed to by
`g_dmm_cali_param_ptr` @ `0x0802AD04`). Printed by `fw_dmm_screen_refresh` and read back
by `fw_usb_cali_read_handler`.

Confirmed field names from format strings:
```
CaliParam.A_Cal_Factor     float   — DC current (A) calibration scaling factor
CaliParam.DCMA_Cal_Factor  float   — DC milliamp calibration scaling factor
CaliParam.ACMA_Cal_Factor  float   — AC milliamp calibration scaling factor
```
Written via USB cali commands; the exact struct layout and flash save address are not yet determined. [?]

**Serial number:** `CaliParam.Serial_Number` is a **separate 32-byte field** stored in
`g_serial_number_buf` (RAM `0x08032f74`). It is written by USB cali sub-cmd `0x09`
(`fw_usb_cali_write_handler`, pkt byte[4]=`\t`):
```c
fw_memcpy_fast(g_serial_number_buf, pkt + 5, 0x20);   // 32 bytes from PC
fw_fpga_write_config(1);                               // → MachXO2 FPGA flash (volt-cal block)
```
The serial number is stored in **MachXO2 FPGA flash** (embedded in the volt-cal block),
not in STM32 internal flash. It is read back via the same sub-cmd in `fw_usb_cali_read_handler`.

---

## 6. DDS Calibration Values

The `fw_usb_cali_write_handler` contains the format string
`"dds_horiz_cali_data = %f, dds_cali_data = %f\r\n"` (`ptr_usb_fmt_dds_cal` @ `0x08032F6C`),
indicating two DDS-related calibration floats:

- `dds_horiz_cali_data` — horizontal (timebase) calibration factor for the DDS path
- `dds_cali_data` — DDS vertical/amplitude calibration factor

These are printed after the scope vertical calibration data in the cali read-back sequence.
Their storage location (flash offset or SRAM address) is not yet determined. [?]

---

## PC calibration tool vs on-device: who writes what

This maps `HTHardDll.dll` exported functions to cal block fields.

| HTHardDll export | USB pkt (class=3) | Sub-cmd | Target | Notes |
|---|---|---|---|---|
| `dsoZeroCali` (write) | dir=0, sub=`0x0b` | 0x0b | `g_cali_table` RAM only (§4) | Chunked 0x28B/pkt; 6 bands × 2ch × 10 steps; not persisted — PC resends on reconnect |
| `dsoWriteDeviceCali` case 0 | dir=0, sub=`0x06` | 0x06 | `g_cali_ram_buf` (§2) → flash via `fw_cali_save_to_flash` | 8×int16 cal factors |
| `dsoWriteDeviceCali` case 1 | dir=0, sub=`0x07` | 0x07 | `g_scope_cali_block+0x2e8` via `fw_fpga_write_config(2)` | AWG freq cal (int16 pair + sign) |
| `dsoWriteDeviceCali` case 2 | dir=0, sub=`0x08` | 0x08 | `g_cali_ram_buf` region | Scope cal factor (1×int16) |
| `dsoWriteDeviceCali` case 3 | dir=0, sub=`0x0e` | 0x0e | `g_scope_cali_block+0x2e8` (=`g_dmm_cali_param_ptr+4`, DCMA_Cal_Factor) via `fw_fpga_write_config(2)` | DMM DC milliamp cal factor (1×int16) — CORRECTED GH #158, was mislabelled "AWG2 offset cal" at wrong offset +0x2f0 (that offset is actually ACMA_Cal_Factor, written by sub 0x0f) |
| `dsoDeviceSN` (write) | dir=0, sub=`0x09` | 0x09 | `g_serial_number_buf` RAM only | 32-byte SN string; `fw_fpga_write_config(1)` then re-flushes volt cal floats (not SN) |
| `dsoGetDeviceCali` (ch=1) | dir=1, sub=`0x0d` | 0x0d | readback | Returns 2×float (scaled ×0.0001) |
| `dsoGetDeviceCali` (ch=2) | dir=1, sub=`0x0e` | 0x0e | readback | Returns 2×float (scaled ×1/1024) |

**Fields the PC calibration tool never writes:**

| Cal block offset | Content | Why PC never writes it |
|---|---|---|
| `+0x000..+0x1df` | Voffset DAC binary-search table (120×uint32) | Written only by `fw_calibrate` on-device binary search |
| `+0x1e0..+0x1e7` | Final zero ADC-count measurements | Written only by `fw_calibrate` at end of cal sequence |
| `+0x1e8..+0x1f7` | **Unknown 24 bytes** | No writer identified anywhere — factory-only candidate |
| `+0x2f4..+0x2f6` | Validity flags | Set to 1 by `fw_calibrate` only; PC cannot set them |

The volt cal floats (`+0x1f8..+0x2e3`) are set by `fw_fpga_write_config(sel=1)` from `g_volt_cali_ram_buf` — the PC triggers the flush via sub-cmd 0x09 (SN write) but cannot set the float values directly.

---

## Boot calibration restoration sequence

```
fw_cali_apply_ch1()
  → fw_flash_memcpy(g_cali_flash_start, 0x130, g_cali_ram_buf)
  → check buf[1] == 0x01 (dirty flag = written flag)

fw_cali_other_flag_get()
  → if g_scope_cali_block[+0x2f4] != 1: fw_cali_volt_data() → load volt cal floats from flash

fw_cali_zero_flag_get()
  → if g_scope_cali_block[+0x2f5] != 1: fw_scope_ch1_display_init() → restore scope config

fw_cali_htx1_write_flag_get()
  → if g_scope_cali_block[+0x2f6] != 1: fw_cali_apply_ch2() → restore misc 0x10-byte block

fw_cali_verify_checksum(ch=3)
  → verify g_cali_ram_buf[+0] == 0x01; if not → factory defaults applied
  → if valid: restore all scope hardware settings from serialised state
```

---

## On-device calibration procedure (`fw_calibrate`)

Called when user selects the calibrate menu (scope param = 4 + F4 key press). Corrected
2026-07-14 (GH#180): this device has no rotary encoder — physical keypad only, see
`gpio-pinout.md`. The function that reads this key press was `fw_encoder_step_read`,
renamed `fw_keypad_code_read`.

**What the user must prepare:** inputs near 0 V (short CH1/CH2 to GND, or use the scope's
own GND-coupling). The firmware gives no visual instruction to do this — it runs silently.

**What the firmware does:**
1. Sets `g_cali_busy_flag = 1` → suppresses waveform overlay.
2. Enables HF-reject, DC coupling on both channels.
3. Outer loop: pass=0 (`fw_usb_gpio_init` = AC-path) then pass=1 (`fw_scope_timebase_config_init` = DC-path).
4. For each pass × channel × gain step (10 steps × 2 ch × 2 pass = 40 sweeps):
   - Sets FPGA gain via `fw_apply_ch_frontend`.
   - Binary-searches `fw_fpga_ch1/ch2_voffset` DAC for ADC output = +100 counts → saves to +0x0a0 slot.
   - Binary-searches for ADC output = −100 counts → saves to +0x140 slot.
   - Binary-searches for ADC output = 0 counts → saves to +0x000 slot.
5. On completion: `fw_fpga_write_config(0)` + `fw_scope_config_save_flash()` → flushes 0x2f8 bytes to flash.
6. LCD: "Calibration OK" or fail glyph. Waits for a key press.
7. `fw_cali_setup_scope()` → restores pre-calibration scope state, clears busy flag.

**Result:** 120 DAC values stored (12 arrays × 10 steps). Two samples define gain;
three samples define gain + offset per gain step per channel per path.

**Convergence thresholds (confirmed from code):**
- Steps 0–2 (lowest gain / noisiest): threshold = 2 counts  
- Steps 3–9 (higher gain): threshold = 0 (exact match required)  
- Anti-oscillation: if `midpoint` repeats 5× consecutively → force converge (avoids infinite hunting)

**Abort:** If the key read returns 3 (physical F3, back/abort) during any binary-search → `fw_cali_setup_scope()` and return immediately (no flash write).

---

## ADC sample calibration pipeline (`fw_cali_capture_samples`)

This is the per-sample pipeline that converts raw 8-bit ADC bytes from SPI into calibrated
display-pixel values. Called by both the acquisition path and the calibration sequence.

```
raw_byte = fw_spi_sendrecv(0xff)
if invert_flag: raw_byte = 0xff - raw_byte        // polarity flip per channel

scaled_int = (raw_byte - voffset_baseline - 0x80) * volt_cali_float[mode][adc_ch][step]

if interleave_mode == 2:                           // 250 MSPS interleaved path only
    scaled_int *= interleave_ratio[ch]             // ratio = vol_cali[ref_step]/volt_cali[cur_step]

final_int = voffset_baseline + 0x80 + scaled_int

// Interleave-skew correction (per-sample phase-aware delta, applied on alternate samples)
if timebase_step < 7:                              // only at high sample rates
    if sample_parity matches interleave_phase_flag[ch]:
        if fw_scope_param_tbl_a(ch)==0: final_int += interleave_delta[ch]
        else:                           final_int -= interleave_delta[ch]

final_int = clamp(final_int, 0, 255)
channel_buf[sample_idx] = final_int
```

**Interleave phase/delta** are computed by `fw_cali_ch_balance_check`: the firmware reads
`count` SPI bytes, averages even-indexed vs odd-indexed samples separately, applies the
volt-cal gain factor, then compares each to the stored voffset. The closer-to-zero average
is declared the "reference" phase; its delta from the other is stored as `interleave_delta[ch]`.

**The cal header struct** (`g_cali_hdr2_ptr` / `g_render_cali_hdr_ptr` @ 0x2000D258):
```
+0  uint8   (unused)
+2  uint8   cal_bypass_flag   (0=apply volt-cal, 1=passthrough/raw)
+4  uint8   ADC-A ref step index
+5  uint8   ADC-B ref step index
+8  int16   CH1 voffset_baseline (ADC count at zero input for step in use)
+10 int16   CH2 voffset_baseline
```

---

## Scope-state flash slots (`fw_cali_load_channel` / `fw_cali_verify_checksum`)

Three flash sectors (sector1=0x0806A800..AFFF, sector2=0x0806B000..B7FF, sector3=0x0806B800..BFFF)
each hold a 0x1f8-byte serialised scope-config record (not the same as `g_scope_cali_block`).

**Serialised block layout (0x1f8 bytes):**

| Buf offset | Size | RAM source | Content |
|-----------|------|-----------|---------|
| +0x000 | 1 | literal | `0x01` valid flag |
| +0x001 | 1 | `fw_scope_param_get_cur()` | current scope param index |
| +0x002 | 1 | `fw_scope_param_get_cur1b()` | secondary param index |
| +0x004 | 0x10 | `g_cali_hdr_ptr` (0x2000D258) | cal header (see above) |
| +0x014 | 0xb8 | `g_cali_volt_table_ptr` (0x2000CDAC) | main scope param struct |
| +0x0cc | 0x18 | `g_cali_zero_ref_ptr` (0x2000D128) | zero-ref sub struct |
| +0x0e4 | 0x24 | `g_cali_trig_sub_ptr` (0x2000D06C) | trigger sub struct |
| +0x108 | 0x94 | `g_cali_adc_main_ptr` (0x2000CE64) | primary scope config |
| +0x152 | 0x94 | `g_cali_adc_main_ptr + 0x4a` | secondary scope config (overlap) |
| +0x19c | 0x2a | `g_cali_misc_sub_ptr` (0x2000D040) | misc sub struct |
| +0x1c8 | 0x30 | `g_cali_extra2_sub_ptr` (0x2000D010) | extra sub struct |

On restore (`fw_cali_verify_checksum`), if `[+0]==0x01`, the firmware copies all blocks
back to their RAM locations and applies: timebase, coupling, BWL, voffset, trigger coupling,
HF-reject, sweep, trigger band, capture mode, backlight PWM. A special case protects the
low byte of `g_cali_volt_table_ptr[+0xe]` (a calibration sub-index) when restoring from
ch!=3 (user-saved slots).

---

## Factory flash slot (0x0805c000)

16 bytes are read by `0x08014ac4` in the 2D72 firmware → SRAM `0x2000cb9c`.
On the known consumer SWD dump this region is fully erased (0xFF).
Format and content unknown — likely factory trim/serial-number data loaded by QA tools. [?]
