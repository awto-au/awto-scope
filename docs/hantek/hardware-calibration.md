# Calibration Data Format — 2D82AUTO

Derived from decompiled OEM firmware (`decompiled_named.c`) and `ghidra_names_2d82.tsv`.

For SPI register writes used during calibration (REG 0x0a/0x0c vertical offset DAC) see fpga-spi.md.  
For the USB protocol that transfers calibration data from PC see [usb-protocol.md](usb-protocol.md).

---

## Flash Layout

> **Address note (corrected 2026-07-13):** the addresses in the "Symbol location" column below are
> where each symbol's *literal-pool pointer cell* lives in flash (part of the dumped 327KB app
> image) — the compiler loads these via `LDR Rn,[PC,#imm]`. The actual flash/RAM data addresses are
> the **stored values** of those cells (confirmed live via Ghidra `DiagAddr`), shown in the "Real
> address" column. Several real flash addresses fall outside the 327KB dumped image (their byte
> contents can't be read from this static dump; only the pointer values are confirmed). See
> "FPGA Registers Written by Calibration" → "Address correction" below for full detail.

| Block | Symbol location (literal-pool cell) | Real flash address | Size | Symbol location (RAM ptr) | Real RAM address | Purpose |
|-------|--------------------------------------|--------------------|------|----------------------------|-------------------|---------|
| ADC calibration | `g_cali_flash_start` (`0x08016218`) | `0x0805C800` (outside dump) | 0x130 (304 B) | `g_cali_ram_buf` (`0x08016b90`) | SRAM `0x2000CB90` | ADC gain/offset factors, all bands/channels |
| Voltage cal | `g_volt_cali_flash_start` (`0x08016220`) | `0x08059800` (outside dump) | 0xEC (236 B) | `g_volt_cali_ram_buf` (`0x08016228`) | SRAM `0x2000CCC0` | Floating-point vertical-scaling tables |
| Scope config + cal | `g_scope_flash_start` (`0x08019100`) | `0x0806F000` (outside dump) | 0x1F8 (504 B) | `g_scope_cali_block` (`0x080190f4`) | SRAM `0x2000BF78` | Full scope config + calibration block (0x2F8 = 760 B in RAM) |

---

## The voltage-cal block also carries the per-unit SERIAL — and that sets the model name

The `0x08059800` block above is documented as "floating-point vertical-scaling tables",
which is what most of it is. But at **offset +0xB8** it also holds an ASCII **serial +
model-suffix string**, and that string is what names the instrument on screen.

Found in the 2D72 full-chip dump
(`vendors/hantek/2d72/firmware/2d72-20190415/analysis/flash_08000000.bin`, 512 KB) at
flash `0x080598B8`:

```
...00 60 89 3f  "CN19010072001598-70"  00 00 00 ... ff ff ff ff
   ^ IEEE-754 cal words              ^ NUL pad    ^ erased flash
```

### The full chain, arithmetically closed `[fw]`

```
fw_cali_volt_data_dump (0x08014A60)                    ← 2D72 build
  fw_flash_memcpy(src=0x08059800, len=0xEC, dst=0x2000C5E4)

  serial at flash 0x080598B8  = cal-page offset +0xB8   (inside the 0xEC block)
  0x2000C5E4 + 0xB8           = 0x2000C69C
                              = g_model_string_rx, the buffer the suffix logic reads
```

then, in the model-name builder (see
[`unlock.md`](unlock.md#the-non-auto-build-does-have-runtime-suffix-selection--and-it-confirms-the-above-gh181)):

```c
iVar2 = fw_strchr(str, '-');       // find the dash
memcpy(two_chars, iVar2 + 1, 2);   // "70"
if      (fw_strcmp(two_chars, "70") == 0) fw_strcat(out, "72");
else if (fw_strcmp(two_chars, "40") == 0) fw_strcat(out, "42");
else                                      fw_strcat(out, "XX");
```

So `...-70` displays as **2D72**, `...-40` as **2D42**, anything else as **2DXX**.

### Why this matters, and a correction

This is the mechanism behind the community `-40` → `-70` hex edit: dump via ST-Link,
edit the ASCII suffix **in the cal page**, reflash. It changes the displayed model.

An earlier revision of `unlock.md` claimed the opposite — that no model string existed
in the firmware to edit. That conclusion came from searching the **app image**, where
the string genuinely is absent, and not asking where else it could live. USB command 9
writes the same SRAM buffer, but it is an *additional* writer, not the only one.

### Note: the RAM destination differs per build

| Build | flash src | RAM dst | serial lands at |
|---|---|---|---|
| 2D72 (`2d72-20210909`) | `0x08059800` | `0x2000C5E4` | `0x2000C69C` `[fw]` |
| 2D82 (per the table above) | `0x08059800` | `0x2000CCC0` | `0x2000CD78` `[guess]` — same +0xB8 offset, not yet confirmed by trace |

Our 2D82 SWD dump has this cal region **erased** (all `0xFF`, and no serial anywhere in
the whole 512 KB), so that particular bench unit has no factory serial to load. That is
consistent with the mechanism, not evidence against it.

### What it does NOT do

Nothing traced so far connects the suffix to any capability. The sample-rate divisor
table is ordinary `.data`, byte-identical on every unit (see below), and no capability
gate has been found in the app, the bootloader, the option bytes, or an ADC strap. On
current evidence the suffix is a **display label**.

---

## ADC Calibration Buffer (`g_cali_ram_buf`, 0x130 bytes)

| Offset | Size | Type | Name | Purpose |
|--------|------|------|------|---------|
| +0x00 | 1 | uint8 | valid_flag | 0x01=data valid, 0x00=uninitialised |
| +0x01 | 1 | uint8 | dirty_flag | 0x01=modified, needs flash write; cleared after `fw_cali_save_to_flash` |
| +0x02 | 0x12E | — | cal factors | Per-band/per-channel int32 entries (layout: use `g_cali_table` formula below) |

---

## In-RAM Calibration Table (`g_cali_table`, 0x1E0 bytes)

Primary table for ADC correction and USB calibration protocol.

**Formula:** `&entry = g_cali_table + band_offset + ch*0x28 + step*4`

### Band layout

| band_offset | Band name | Probe | Frequency range |
|------------|-----------|-------|----------------|
| +0x000 | DC_1x  | 1× | DC |
| +0x050 | DC_10x | 10× | DC |
| +0x0A0 | AC_low | 1× | Low frequency |
| +0x0F0 | AC_mid | 1× | Mid frequency |
| +0x140 | AC_hi1 | 1× | High frequency band 1 |
| +0x190 | AC_hi2 | 1× | High frequency band 2 |

Each band = 0x50 bytes: 2 channels × 10 steps × 4 bytes/int32.

```
band_offset + ch*0x28 + step*4    (ch=0→CH1, ch=1→CH2; step=0..9)
```

Steps are stored descending (9,8,…0) in the binary-search calibration loop.

### USB calibration protocol mapping

| USB sub-command | Band |
|----------------|------|
| 1 | AC_low (+0x0A0) |
| 2 | AC_mid (+0x0F0) |
| 3 | AC_hi1 (+0x140) |
| 4 | AC_hi2 (+0x190) |
| 5 | DC_1x  (+0x000) |
| 6 | DC_10x (+0x050) |

---

## Scope Calibration Block (`g_scope_cali_block`, 0x2F8 bytes in RAM)

Contains interactive calibration results from `fw_calibrate` (binary-search DAC tuning).

### Memory map

| Offset range | Size | Content |
|-------------|------|---------|
| +0x000..+0x09F | 0xA0 | Pass 0: zero-ref DAC values, CH1+CH2, 10 steps each |
| +0x0A0..+0x13F | 0xA0 | Pass 0: pos-ref DAC values (ADC target +100) |
| +0x140..+0x1DF | 0xA0 | Pass 0: neg-ref DAC values (ADC target −100) |
| +0x1E0 | 4 | int32 — CH1 final zero-point measurement result |
| +0x1E4 | 4 | int32 — CH2 final zero-point measurement result |
| +0x1F8..+0x2E3 | 0xEC | Voltage calibration floats (see below) |
| +0x2E4..+0x2F3 | 0x10 | Misc calibration block |
| +0x2F4 | 1 | uint8 — `volt_cali_valid` |
| +0x2F5 | 1 | uint8 — `scope_cali_valid` |
| +0x2F6 | 1 | uint8 — `misc_cali_valid` |
| +0x2F7 | 1 | (reserved) |

### Two-pass calibration access formula

```
g_scope_cali_block + pass*0x50 + ch*0x28 + step*4 + measurement_type_offset
```

- `pass` = 0 (AC) or 1 (DC)
- `ch` = 0 (CH1) or 1 (CH2)
- `step` = 0..9
- `measurement_type_offset` = 0x000 (zero-ref), 0x0A0 (pos-ref), 0x140 (neg-ref)

---

## Voltage Calibration Table (`g_volt_cali_ram_buf`, 0xEC bytes)

```
+0x00..+0x03   uint32   header (purpose unknown)
+0x04..+0x?    float[2ch][2type][10steps]
```

Stride: `ch*0x50 + type*0x28 + step*4`

- `type` 0 and 1 = two calibration types (likely gain and offset, or DC and AC path).
- Loaded at boot by `fw_cali_volt_data()`; used for waveform vertical scaling.

---

## Could the cal data itself be the bandwidth limiter? — tested, and NO `[fw]`

A genuinely good hypothesis, and the only one left standing after the divisor table,
option bytes, ADC strap, BW-limit bit and bootloader were all ruled out: calibration data
*is* in the signal path, and the table has explicit **frequency bands** — `AC_low`,
`AC_mid`, **`AC_hi1`, `AC_hi2`**. A factory tool that wrote low gains into the high-frequency
bands would attenuate high-frequency signal *in software*, downgrading the scope without
any hardware difference. It would look exactly like a bandwidth limit.

**It does not work that way.** `g_cali_table` — the 6-band table — has 28 references in the
app, and every one belongs to just three functions:

| Function | Role |
|---|---|
| `fw_usb_cali_read_handler` (`0x08032a00`) | host **reads** cal data out (13 refs) |
| `fw_usb_cali_write_handler` (`0x08031c40`) | host **writes** cal data in |
| `fw_acq_read_correct_samples` (`0x0801e314`) | **comment only — not code** |

So the banded table is a **store**, read and written over USB, and is **never consulted on
the live acquisition path**. The per-sample correction in `fw_acq_read_correct_samples` uses
a single `volt_cali_float` plus an interleave ratio:

```
scaled = (raw - voffset_baseline - 0x80) * volt_cali_float
       * (interleave ratio, if interleaved)
final  = clamp(voffset_baseline + 0x80 + scaled + interleave_delta, 0, 255)
```

There is no frequency-band index in that expression, and nothing computes one at
acquisition time. The bands are selected by **probe attenuation and measurement context**
for the *calibration procedure itself*, not per captured sample.

Two further reasons the mechanism could not produce a bandwidth limit even if it were
consulted: the correction is a **per-sample scalar multiply**, which is a gain/amplitude
change, not a frequency-selective filter — you cannot band-limit a signal by scaling every
sample by the same constant. And it is applied *after* the samples have already left the
FPGA, so any real high-frequency content has already been captured or lost upstream.

## FPGA Registers Written by Calibration

This section traces the FPGA-facing side of calibration at the instruction level: exact wire
bytes, the binary-search convergence math, and — critically — which parts of calibration touch
an FPGA register at all versus which are pure MCU-side software. Investigated 2026-07-13 by deep
Ghidra decompile trace of `fw_cali_apply_ch1`/`fw_cali_apply_ch2`, `fw_fpga_ch1_voffset`/
`fw_fpga_ch2_voffset`, `fw_scope_ch1_voffset_apply`/`fw_apply_ch2_position`,
`fw_fpga_write_config`/`fw_fpga_config_restore`, and `fw_cali_capture_samples`.

### Summary — the split

| Calibration component | Where applied | FPGA register touched? |
|---|---|---|
| Vertical offset / position (DC level shift) | MCU computes DAC code, writes it | **Yes** — REG 0x0a (CH1) / REG 0x0c (CH2), runtime `0xa8` SPI bank |
| ADC gain / per-band scaling (`g_cali_table`, `g_volt_cali_ram_buf`) | MCU multiplies raw ADC bytes after readback | **No** — pure software post-processing, no FPGA write of any kind |
| Interleave-phase / delta correction | MCU add/subtract on raw bytes | **No** — same as above |
| Persisted copy of all 3 cal sub-blocks | FPGA's own MachXO2 NVCM (via ISC programming) | **Yes, but not a register write** — a bitstream-config-memory program/read, a completely different mechanism from the runtime `0xa8`/`0xc8`/`0xa0` register banks |

**There is no FPGA gain-trim DAC or digital-gain-multiplier register.** The 6-band cal table
(`DC_1x`/`DC_10x`/`AC_low`/`AC_mid`/`AC_hi1`/`AC_hi2`) and the per-step `g_volt_cali_ram_buf` gain
floats are both applied entirely on the STM32 side, as arithmetic on ADC sample bytes that have
already been clocked out of the FPGA over the `0xa4` burst-read bank. Only the DC vertical-offset
(position) component reaches an actual FPGA register.

### REG 0x0a / REG 0x0c — Vertical Offset DAC (the only runtime-register calibration path)

`fw_fpga_ch1_voffset` @ `0x0801f724` / `fw_fpga_ch2_voffset` @ `0x0801f766`.

Wire (identical framing to the trigger-level registers in [trigger.md](trigger.md)):
```
CS low (fw_gpio_reset_bits(GPIOA, 0x10))
→ [0xa8][0x0a][lo][hi]   (CH1, 4 bytes, one fw_spi_send_n call)   -- or [0xa8][0x0c][lo][hi] for CH2
CS high (fw_gpio_set_bits(GPIOA, 0x10))
```
Header bytes confirmed live via Ghidra (`DiagAddr`, 2026-07-13): the literal-pool cell
`flt_ff18_4p01en34` @ `0x0801ff18` holds pointer `0x08054090`, which itself stores the 16-bit LE
word `0x0AA8` = bytes `A8 0A`. CH2's equivalent cell (`flt_ff20_4p01en34` @ `0x0801ff20` →
`0x08054094` → `0x0CA8` = bytes `A8 0C`) confirms REG 0x0c the same way.

**Encoding:** 16-bit wire field, offset-binary, full range `0x0000`–`0xffff`, `0x8000` = 0 V — same
convention as the CH1/CH2 trigger-level registers (0x06/0x08). The wire write is unambiguously a
16-bit field; the FPGA-generated offset likely resolves ≤10 effective bits (firmware header comments
describe the DAC as both "10-bit" and "16-bit" — 16-bit is the wire width, not necessarily the
physical resolution). No dedicated offset-DAC chip exists in the BOM, so the FPGA generates the
offset voltage itself (mechanism unconfirmed).

**Two different sources feed this register:**

1. **`fw_calibrate`'s binary search** (interactive on-device calibration) writes the *raw search
   midpoint* directly — the full 16-bit range is swept with no scaling applied. Convergence loop
   (per pass × channel × gain-step × target):
   ```
   lo_bound = 0, hi_bound = 0xffff
   loop:
     mid = (lo_bound + hi_bound) >> 1
     write mid to REG 0x0a/0x0c  → lo=mid&0xff, hi=(mid>>8)&0xff  (i.e. uVar9=2*mid; hi=(uVar9>>9)&0xff, mathematically identical to mid>>8)
     delay 50 ms
     measured = fw_scope_wave_process(...)          // read back the resulting trace level in ADC counts
     if |measured - target| <= threshold: converged, store mid
     else: narrow [lo_bound,hi_bound] toward target (classic binary search)
   ```
   Targets: `+100` ADC counts (pos-ref), `-100` ADC counts (neg-ref), `0` ADC counts (zero-ref).
   Threshold: `2` counts for gain steps 0–2, `0` (exact) for steps 3–9. Anti-oscillation: if the
   midpoint repeats 5× in a row, the search force-converges on that value (prevents infinite
   bouncing from ADC noise). Converged 16-bit DAC codes are stored into `g_scope_cali_block` at
   `pass*0x50 + ch*0x28 + step*4 + {0x000=zero, 0x0a0=pos, 0x140=neg}` — this is the *only* place
   raw DAC codes are persisted.

2. **Normal runtime application** (`fw_scope_ch1_voffset_apply` @ `0x080108ec` / `fw_apply_ch2_position`
   @ `0x08010b40`, called whenever the position knob or V/div changes) computes the DAC code by
   **linear interpolation from the 3 calibrated reference codes**, using the live position-knob
   value (ADC-count units) as the interpolation input:
   ```
   step  = active gain/range index (0-9)
   zero_ref = cal_table[step]              (+0x000, DAC code that produced 0 ADC counts during cal)
   pos_ref  = cal_table[step + 0xa0/4]     (+0x0a0, DAC code that produced +100 ADC counts)
   neg_ref  = cal_table[step + 0x140/4]    (+0x140, DAC code that produced -100 ADC counts)
   position = ptr_scope_meas_params[+0x3c] (CH1) or [+0x86] (CH2), signed ADC-count units,
              negated if coupling-mode byte == 1

   if position < 0:  slope = (zero_ref - neg_ref) / 100;  code = neg_ref  + position*slope
   else:             slope = (pos_ref  - zero_ref) / 100;  code = zero_ref + position*slope

   write code to REG 0x0a/0x0c  → lo=code&0xff, hi=(code>>8)&0xff
   ```
   i.e. the calibration table converts "how many DAC codes per ADC-count of requested position" at
   the *current* gain step, then the knob position (in ADC counts) is projected through that slope
   from the calibrated zero point. `g_volt_cali_table_ptr` (used by this formula) and
   `g_scope_cali_block`/`g_cali_table` are CONFIRMED (via Ghidra `DiagAddr`, 2026-07-13) to all
   resolve to the **same underlying SRAM buffer at `0x2000BF78`** — there is one shared cal-table
   RAM structure, accessed through several differently-offset pointer aliases in different call
   sites, not three separate tables.
   A `g_scope_cal_flag_ptr` (SRAM `0x2000D3A6`) escape hatch bypasses the table with fixed generic
   constants when calibration itself is actively running (avoids feeding back a half-updated table
   into the position-apply path mid-calibration).

Both paths converge on the exact same 16-bit DAC code space — `fw_fpga_ch1_voffset`/
`fw_fpga_ch2_voffset` never scale or reinterpret the value; whatever 16-bit integer they're handed
is written to the wire as-is.

### ADC Gain Correction — confirmed MCU-side software, not an FPGA register

Traced `fw_cali_capture_samples` @ `0x0801e314` (the function that applies the calibration table
to live captured samples) end-to-end: every SPI transaction in its body — 7 call sites, all
`fw_spi_sendrecv(0xff)` — is a raw ADC sample byte being clocked out of the FPGA's burst-read
shift path (dummy `0xff` written to receive one byte back, the same idiom documented for the
`0xa4` bank in fpga-spi.md). **No `fw_fpga_*()` register-write function is called
anywhere in this function.** The gain/offset math —
```
scaled = (raw_byte - voffset_baseline - 0x80) * volt_cali_gain_float
if interleaved: scaled *= (per-channel interleave ratio, from two adjacent g_volt_cali_ram_buf steps)
final  = clamp(voffset_baseline + 0x80 + scaled + interleave_delta_correction, 0, 255)
```
is carried out entirely as STM32-side integer/float arithmetic on the byte *after* it has already
left the FPGA. The FPGA has no gain-trim DAC and no digital-gain-multiplier register in this
design — it only ever emits raw 8-bit offset-binary ADC samples (`0x80` = 0 V) and accepts the one
DC vertical-offset DAC value discussed above. All 6 calibration bands (`DC_1x`/`DC_10x`/`AC_low`/
`AC_mid`/`AC_hi1`/`AC_hi2`) and the interleave-balance correction (`fw_cali_ch_balance_check`) are
purely software scaling factors selected by probe-attenuation and frequency-band context, applied
identically regardless of which physical front-end relay/range is engaged.

### `fw_cali_apply_ch1` / `fw_cali_apply_ch2` — corrected: neither touches the FPGA

Prior documentation (and the function names) implied these two apply ADC frontend corrections to
hardware at boot. Direct decompile trace (2026-07-13) shows otherwise:

- **`fw_cali_apply_ch1`** (`0x08016138`): body is *only*
  `fw_flash_memcpy(g_cali_flash_start, 0x130, g_cali_ram_buf)` + a check of the dirty-flag byte.
  No FPGA/SPI call, no frontend apply, no gain/offset computation. It just refreshes the RAM copy
  of the flash cal block.
- **`fw_cali_apply_ch2`** (`0x080161c0`): body is *only*
  `fw_flash_memcpy(flt_6b98_4p051en34, 0x10, g_dat_6bb8)` — and tracing what those two operands
  actually resolve to (both are literal-pool pointer cells; see "Address correction" below) shows
  this copies 16 bytes of **DMM (multimeter) current-clamp calibration factors** into the DMM
  `CaliParam` struct (SRAM `0x2000D268`, same address as `g_dmm_cali_param_ptr`) — nothing to do
  with oscilloscope CH2 ADC/vertical calibration at all, despite the name.

Neither function writes any FPGA register. The actual application of the vertical-offset DAC
value happens later in the boot sequence via `fw_scope_ch1_voffset_apply`/`fw_apply_ch2_position`
(see above), and the actual application of ADC gain correction happens per-sample at capture time
via `fw_cali_capture_samples` (also above) — not at boot, and not in either "apply" function.

### FPGA-side persistence — MachXO2 ISC/NVCM, a second calibration-data home

**LIVE-VERIFIED 2026-07-14, CONFIRMED POSITIVE.** A follow-up live-SWD session located the mystery
build's (`vendors/hantek/2d82/firmware/2d82-20260713-swd/`) equivalent of `fw_fpga_config_restore` — it is
**`fw_fpga_config_restore` @ `0x080157e8`** on this build (renamed from the auto-generated
placeholder `fw_awg_present_check_variant`; do NOT use the reference-build address `0x08016dc8` on
this hardware, the two builds have completely different addresses). Found by reading
`vendors/hantek/2d82/firmware/2d82-20260713-swd/analysis/decompiled_named.c` for a function matching the
documented shape (ISC bit-bang sequence, called unconditionally early in boot) — confirmed a match
because its body (`fw_memset_word` → `fw_machxo2_init_address` → poll `fw_machxo2_read_status` BUSY
→ `fw_machxo2_read_page(buf,0x31)` → `fw_machxo2_isc_erase` → `fw_fpga_sspi_init` → split into 3
RAM sub-blocks) is identical to the reference build's `fw_fpga_config_restore`, and its single
caller (`decompiled_named.c:41842`) sits at the exact same boot position: immediately after the
`fw_cali_apply_ch1` equivalent, unconditionally, before `fw_scope_config_load_flash`.

**Test procedure and results** (probe `26004600160000305748534E`, one continuous pyOCD Python-API
session per the standing anti-USB-disruption guidance; script `vendors/hantek/2d82/scripts/hantek_fpga_config_restore_probe.py`
plus follow-up ad-hoc diagnostics run the same way):

1. **Does it fire at boot?** HW breakpoint set at `0x080157e8`, device reset via SWD
   (`reset_and_halt` + breakpoint armed + `resume`). **The breakpoint fired on every reset** —
   core halted at PC=`0x080157e8` within ~2s of resume, confirming this boot path genuinely
   executes on real hardware, not dead/unreached code.
2. **Is it really calling into the ISC/NVCM read path?** A nested breakpoint at
   `fw_machxo2_read_page` (`0x0801ec5c`) was armed after the outer breakpoint hit, then resumed —
   it fired essentially immediately (0 poll iterations), confirming `fw_machxo2_read_page` is
   actually reached from inside `fw_fpga_config_restore` during this exact boot sequence.
3. **Is it doing real SPI/ISC bit-bang traffic, not just a same-shaped decoy?** A nested breakpoint
   at `fw_spi_sendrecv` (`0x0801cac0`, the hardware SPI1 send/receive primitive) was hit repeatedly
   with **r0 = `0x74`** — one of the exact MachXO2 ISC opcode-set bytes — called from inside
   `fw_machxo2_init_address` (`0x0801eb6c`). At every one of these hits, **`GPIOB_ODR` bit 4 read
   0 (PB4 LOW)** — PB4 is `FPGA_SPI1_SSPI_CS` per [gpio-pinout.md](gpio-pinout.md), the FPGA
   bitstream-programming chip-select. The port pointer this call site actually uses
   (`DAT_0801ee14`, live-read via SWD) resolves to `0x40010c00` = **GPIOB base**, not GPIOA —
   confirming the ISC path runs over the **SSPI bus** (PB4 CS + shared SPI1 SCK/MISO/MOSI), not
   the runtime PA4-CS register bus used by REG 0x0a/0x0c. This is definitive: real hardware SPI1
   traffic to the FPGA's ISC/SSPI interface during boot, CS correctly asserted for the duration of
   the transaction, opcode byte matching the documented ISC command set.

**One byte-level discrepancy worth flagging for a future session**: this specific
`fw_machxo2_init_address` call site (immediately preceding `fw_machxo2_read_page`) sends
`[0x74, 0x08, 0x00, 0x00]`, not the `[0x46, 0x00, 0x00, 0x00]` (LSC_INIT_ADDRESS) documented for
the function's other call sites in `ghidra_names_2d82_swd.tsv`. Not yet resolved whether `0x74` is
a distinct ISC opcode used specifically ahead of a page read, or whether the shared "sends 0x46+3
zeros" description is itself imprecise per-call-site — see the TSV entry for
`fw_fpga_config_restore` (`0x080157E8`) for full detail.

**Conclusion: the NVCM-restore claim from the original static trace is CONFIRMED, not merely
plausible.** The FPGA's own MachXO2 non-volatile config memory genuinely is read back over a real
SPI transaction, with correct CS sequencing, unconditionally on every boot, on the actual bench
hardware — this is not a code path that only exists in the reference build or that goes unreached
on this unit.

Beyond the runtime `0xa8`/`0xc8` register banks, calibration data is *also* mirrored into the
FPGA's own non-volatile configuration memory (Lattice MachXO2 NVCM, programmed via the ISC
protocol documented in fpga-spi.md §5b) — a completely separate mechanism from
both the STM32-internal flash blocks and the runtime SPI register writes.

- **`fw_fpga_write_config(sel)`** @ `0x08016c70` — `fw_calibrate` calls this with `sel=0`
  immediately after its binary-search sweep completes. Depending on `sel`, it marks one of 3
  sub-blocks of the shared cal buffer (`g_cali_main_struct_ptr`, SRAM `0x2000BA5C`) valid and
  copies it in, then ISC-programs the whole `0x2f8`-byte buffer into the FPGA's NVCM:
  `sel=0` → scope DAC-cal table (`0x1f8` B, same buffer as `g_scope_cali_block`/`g_cali_table`),
  `sel=1` → ADC volt-cal gain floats (`0xec` B), `sel=2` → AWG/DMM cal factors (`0x10` B). USB
  calibration commands also drive this (`sel=1` for volt-cal/serial/AWG-DDS writes, `sel=2` for
  DMM DCMA/ACMA cal writes — see `fw_usb_cali_write_handler`).
- **`fw_fpga_config_restore()`** @ `0x08016dc8` (reference build, `2d82-20210818`) — called
  **unconditionally at boot**, immediately after `fw_cali_apply_ch1()`. Reads the FPGA's NVCM
  config sector back over SPI (`fw_machxo2_read_page`, `0x2f8` bytes) and splits it into the same 3
  RAM sub-blocks (`g_cali_default_buf_ptr`/`g_cali_zero_sub_buf_ptr`/`g_cali_gpio_nibble_ptr`). This
  is the matching read-back for `fw_fpga_write_config`'s writes. **Live-verified on the mystery
  build's equivalent** (`fw_fpga_config_restore` @ `0x080157e8` in `2d82-20260713-swd` — different
  address, same role) — see "LIVE-VERIFIED 2026-07-14" below for the full test procedure and
  evidence confirming this genuinely executes and does real SPI/ISC traffic on actual hardware.

This means the FPGA's own bitstream-config memory holds a live, independently-readable copy of
the same calibration data that also lives in STM32 internal flash (`g_cali_ram_buf`/
`g_scope_cali_block` via `fw_cali_save_to_flash`/`fw_scope_config_save_flash`) — two separate
non-volatile stores for the same data, kept in sync by the calibration/apply call chain. A
reimplementation targeting only the runtime `0xa8`/`0xc8` register protocol will reproduce the
scope's electrical behaviour correctly without needing the MachXO2 ISC path at all — this
mechanism only matters if the goal is bit-for-bit persistence-path compatibility (e.g. accepting
the OEM PC calibration utility's writes).

### Address correction — several "flash/SRAM address" TSV entries were pointer cells, not data addresses

While tracing this, a systemic error was found and fixed in `ghidra_names_2d82.tsv`: several
addresses previously documented as *being* the calibration data (`g_cali_flash_start`,
`g_volt_cali_flash_start`, `g_volt_cali_ram_buf`, `g_cali_ram_buf`, and the `flt_*`/`g_dat_*`
operands feeding `fw_cali_apply_ch2`) are actually **literal-pool cells in flash whose stored
4-byte value is the real address** — the compiler emits `LDR Rn, [PC, #imm]` to load these
constants, and Ghidra's decompile renders the *cell's own address* as if it were the symbol,
obscuring the one extra level of indirection. Confirmed via Ghidra `DiagAddr` reads (2026-07-13):

| Symbol (flash cell address) | Old assumption | Actual stored value (real address) |
|---|---|---|
| `g_cali_flash_start` (`0x08016218`) | flash addr of ADC cal block | `0x0805C800` (outside the 327KB dumped image) |
| `g_cali_flash_end` (`0x0801621C`) | — (undocumented) | `0x0805CFFF` |
| `g_volt_cali_flash_start` (`0x08016220`) | flash addr of volt cal table | `0x08059800` (outside dump) |
| `g_volt_cali_ram_buf` (`0x08016228`) | RAM addr of volt cal table | SRAM `0x2000CCC0` |
| `g_cali_ram_buf` (`0x08016b90`) | RAM addr of ADC cal block | SRAM `0x2000CB90` |
| `flt_6b98_4p051en34` (`0x08016B98`) | float constant `4.05e-34` | `0x0806A000` (flash addr, outside dump) |
| `g_dat_6bb8` (`0x08016BB8`) | "ptr to SRAM 0x2000D268" (already correct) | confirmed `0x2000D268` = `g_dmm_cali_param_ptr` |

`0x0805C800`/`0x08059800`/`0x0806A000` all fall **outside** the 327KB dumped app image (which ends
around `0x08054C00`) — consistent with genuine STM32F103 flash sectors beyond the captured dump
region, not with corrupted/garbage pointers. Their byte contents cannot be read from this static
firmware dump; only the pointer *values* are confirmed. This does not change any of the RAM buffer
layouts/formulas documented earlier in this file (band/channel/step indexing is unaffected) — it
only corrects which physical address each symbol names.

---

## Calibration Flows

### Boot sequence

```
fw_cali_apply_ch1()         → load 0x130 bytes flash → g_cali_ram_buf (memcpy + dirty-flag check only, no FPGA write)
fw_fpga_config_restore()    → read FPGA's own MachXO2 NVCM cal mirror back into RAM (unconditional; see below)
fw_cali_verify_checksum()   → check [+0]==0x01 valid flag
fw_cali_apply_ch2()         → CORRECTED: NOT ADC frontend — loads 16B DMM current-clamp cal factors
                               into g_dmm_cali_param_ptr (SRAM 0x2000D268); no FPGA write, no scope-CH2 role
fw_cali_volt_data()         → load 0xEC voltage table from flash
```

Neither `fw_cali_apply_ch1` nor `fw_cali_apply_ch2` writes an FPGA register — see "FPGA Registers
Written by Calibration" below for the full trace. The actual vertical-offset DAC write happens
later via `fw_scope_ch1_voffset_apply`/`fw_apply_ch2_position` (called from the same boot init
function after frontend/coupling setup), and ADC gain correction is applied per-sample at capture
time, not at boot.

### Interactive calibration (`fw_calibrate`)

```
g_cali_busy_flag = 1
for pass in (0=AC, 1=DC):
    for ch in (CH1, CH2):
        for step in (9..0):
            binary search FPGA DAC → store to g_scope_cali_block
fw_scope_config_save_flash()   → save 0x2F8 bytes to flash
g_scope_cali_block[+0x2F5] = 1  (scope_cali_valid)
g_scope_cali_block[+0x2F4] = 1  (volt_cali_valid)
```

### USB calibration protocol

```
PC sends cmd 0x0B sub=1..6 → band data written to g_cali_table (RAM only)
PC sends cmd 0x0C sub=6    → triggers fw_cali_save_to_flash()
fw_cali_save_to_flash()    → saves g_cali_ram_buf (0x130 B) to flash at g_cali_flash_start
```

---

## New Firmware Implementation Checklist

1. **Read ADC cal block** (0x130 B) from flash at `g_cali_flash_start`:
   - Check byte +0x00 == 0x01 (valid flag).
   - Extract int32 entries per formula: `band_offset + ch*0x28 + step*4`.
   - Apply as **MCU-side software scaling** to raw ADC sample bytes after readback (CORRECTED —
     this is NOT an FPGA register write; there is no ADC gain-trim register. See "FPGA Registers
     Written by Calibration" above): `scaled = (raw - voffset_baseline - 0x80) * gain_factor`,
     selecting `gain_factor`/band by probe attenuation + frequency content.

2. **Read voltage cal block** (0xEC B) from flash at `g_volt_cali_flash_start`:
   - Parse 4-byte header (purpose unknown).
   - Extract `float[2ch][2type][10steps]` for vertical scaling.
   - Same as above: applied as MCU-side multiplication on readback, not an FPGA write.

3. **Read scope config block** (0x1F8 B) from flash at `g_scope_flash_start`:
   - Restore timebase, coupling, trigger, display params.
   - Check validity flags at `g_scope_cali_block` +0x2F4/+0x2F5/+0x2F6 before applying.

---

## Key SRAM Addresses

> Columns: "Symbol addr" = where the symbol's literal-pool cell lives (flash, in the dumped
> image); "Real addr" = the value stored in that cell (the actual data address), confirmed via
> Ghidra `DiagAddr` 2026-07-13 — see "Address correction" in the FPGA-registers section above.
> `g_cali_busy_flag`/`g_cali_result_status` are plain SRAM addresses, not pointer cells.

| Symbol addr | Real addr | Name | Purpose |
|---------|------|------|---------|
| `0x08016218` | `0x0805C800` (flash, outside dump) | `g_cali_flash_start` | Flash start of ADC cal block |
| `0x0801621C` | `0x0805CFFF` (flash, outside dump) | `g_cali_flash_end` | Flash end (exclusive) |
| `0x08016220` | `0x08059800` (flash, outside dump) | `g_volt_cali_flash_start` | Flash start of voltage cal table |
| `0x08016228` | SRAM `0x2000CCC0` | `g_volt_cali_ram_buf` | RAM copy of 0xEC volt table |
| `0x08016b90` | SRAM `0x2000CB90` | `g_cali_ram_buf` | RAM copy of 0x130 ADC cal block |
| `0x080190f4` | SRAM `0x2000BF78` | `g_scope_cali_block` | 0x2F8-byte scope config + cal data |
| `0x08019100` | `0x0806F000` (flash, outside dump) | `g_scope_flash_start` | Flash start of scope config |
| `0x08032f7c` | SRAM `0x2000BF78` (SAME buffer as `g_scope_cali_block`) | `g_cali_table` | 0x1E0-byte in-RAM USB cal table |
| `0x08019104` | — (plain SRAM, not a pointer cell) | `g_cali_busy_flag` | 1=calibration in progress |
| `0x08019114` | — (plain SRAM, not a pointer cell) | `g_cali_result_status` | Result block; [+0xE]=pass/fail code |
