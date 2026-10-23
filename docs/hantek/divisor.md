# FPGA Timebase Divisor Table

## What this is

The FPGA's sample clock is derived from a 250 MHz PLL output (CLKOP from EHXPLLJ, 25 MHz × CLKFB_DIV=10 × CLKOP_DIV=2 = 500 MHz VCO → 250 MHz CLKOP).

The STM32 writes a 32-bit divisor to **FPGA REG 0x0f** via `fw_fpga_timebase_div()`:

```
effective_sample_rate = 250_000_000 / (divisor + 1)
```

Boot-verified default: divisor=0x31 (49) → 250 MHz / 50 = **5.0 MS/s**.

---

## RESOLVED 2026-07-14 — NOT a divisor table (live SWD capture, GH#172)

**Verdict: `0x20001FC0` does NOT hold a `uint32[]` divisor array. It is uninitialized/leftover SRAM
that happens to contain deterministic-but-unrelated pointer data, not the timebase table.**

Live SWD read on the bench unit (probe `26004600160000305748534E`, now BMP firmware, `gdb`
`target extended-remote /dev/ttyACM2` + `monitor swd_scan`/`attach`), after a full normal reset
through the real vendor bootloader → app handoff (confirmed via resumed `BAT_Voltage`/
`ADC_ConvertedValue` UART telemetry on `/dev/ttyUSB2`, mystery-build `2d82-20260713-swd` firmware —
identified by its continuous `BAT_Voltage` debug print, absent from the reference `2d82-20210818`
build). Two independent reset+read cycles, `0x20001FC0`..`0x20001FE7` (10 words):

```
idx 0: 0x01010402
idx 1: 0x2000c81c   <- SRAM pointer
idx 2: 0x2000c86c   <- SRAM pointer
idx 3: 0x2000c81c   <- same pointer as idx 1
idx 4: 0x2000c81c   <- same pointer as idx 1
idx 5: 0x00000000
idx 6: 0x2000c81c   <- same pointer as idx 1 (index 6 should be divisor 49/0x31 — it is not)
idx 7: 0x00000000
idx 8: 0x20002004   <- SRAM pointer
idx 9: 0x20001ff0   <- SRAM pointer
```

Extended read through `0x20002008` (16 bytes past the assumed 10-entry table, `0x20001FE8`-
`0x20002004`): all zero, both boot cycles. One word further out (`0x20002008`) differed between the
two reads (`0x100f000c` vs `0x100f0007`) — that address is live/volatile runtime state, outside
whatever this structure is.

**Determinism**: all 10 words at `0x20001FC0` byte-identical across two independent hardware
resets. Not random garbage — but not the divisor table either.

**Why this rules out the divisor-table hypothesis**:
- No `0x31`/49 (the confirmed boot-default divisor at timebase index 6) appears anywhere in the 40
  captured bytes, at any byte alignment.
- The values are not a monotonic or plausible divisor progression — they are SRAM addresses
  (`0x2000c81c`, `0x2000c86c`, `0x20002004`, `0x20001ff0`, all in valid SRAM range) with the *same*
  pointer repeated at indices 1, 3, 4, 6, plus zero words at 5 and 7.
- Dereferencing `0x2000c81c` shows a populated struct (small integers, and `0x3f800000` = `1.0f`
  as IEEE-754 float, at offset +0x30) — looks like a calibration/config struct, not divisor data.
  Dereferencing `0x2000c86c` (right after it, 0x50 bytes later) shows all zeros — an empty/unused
  neighboring struct slot.

**Interpretation**: `0x20001FC0` most likely holds an array of pointers/handles into other SRAM
structures (possibly a UI widget table, config-struct dispatch table, or similar — not
investigated further, out of scope for the timebase question), OR is leftover/stale SRAM content
from a previous subsystem's use of this memory that happens to be deterministic because STM32/APM32
SRAM power-on state and pre-boot initialization order are themselves deterministic on this specific
board/build. Either way: **this address is not where the FPGA timebase divisor table lives.**
`fw_apply_timebase_div()`'s real source data is still unlocated — the SRAM-pointer theory from
`g_dat_16a0` (below) needs to be re-examined; it may derefence to a genuinely different address than
assumed, or the whole "runtime-populated SRAM table" theory may be wrong.

See GH issue #172 (closed) for the full live-capture writeup, and #171 for the broader SWD
key-injection plan this was blocking.

---

## Original (now superseded) static-analysis writeup

## Where the table lives

| Symbol | Flash address | SRAM address | Content |
|--------|--------------|--------------|---------|
| `g_dat_16a0` | 0x080116A0 | — | Flash literal = SRAM pointer |
| *(table)* | — | 0x20001FC0 | uint32[] divisor per timebase step — **LIVE CAPTURE 2026-07-14 shows this is WRONG, see above** |
| `g_dat_1688` | 0x08011688 | — | Flash literal = SRAM pointer |
| *(table)* | — | 0x20000118 | 8-byte entry[] per timebase step (display config) |

Both tables are in **SRAM, not flash**. The stage-2 app **never writes** to 0x20001FC0 — not in `.data` (ends at 0x20000922), not in `.bss` (starts at 0x200023e4), not in any traced init function. SRAM 0x20001FC0 is in the **uninitialized gap** 0x20000922–0x200023e3, which is only touched by the **stage-1 bootloader** (0x08000000–0x08004FFF, not included in `flash_08005000.bin`). The stage-1 DFU binary (`HTX2D82-2021090901-inner.dfu`) was the suspected source of the divisor values — **live capture now rules this out**, see resolved section above.

`fw_apply_timebase_div()` at 0x08011554:
```c
uint32 divisor = g_dat_16a0[timebase_index];   // SRAM[0x20001FC0 + idx*4]
fw_fpga_timebase_div(b0, b1, b2, b3);          // writes REG 0x0f
```

`fw_timebase_entry_get(idx)` at 0x08010EF0:
```c
return g_dat_1688[idx * 8];                    // 8-byte display entry
```

---

## Timebase step labels (from RAM_scope_params analysis)

Timebase index is stored at `ptr_timebase_cfg+6`. Fast-rate (FPGA REG 0x0e bit1) is set when index < 6.

| Index | Label (manual V1.4) | Total time (12 div) | Target rate | Est. divisor |
|-------|---------------------|---------------------|-------------|--------------|
| 0 | 5 ns/div | 60 ns | 102.4 MS/s | 1 |
| 1 | 10 ns/div | 120 ns | 51.2 MS/s | 3 |
| 2 | 20 ns/div | 240 ns | 25.6 MS/s | 8 |
| 3 | 50 ns/div | 600 ns | 10.24 MS/s | 23 |
| 4 | 100 ns/div | 1.2 µs | 5.12 MS/s | 47 |
| 5 | 200 ns/div | 2.4 µs | 2.56 MS/s | 96 |
| 6 | 500 ns/div | 6 µs | 1.024 MS/s | 244 |
| 7 | 1 µs/div | 12 µs | 512 kS/s | 488 |
| 8 | 2 µs/div | 24 µs | 256 kS/s | 976 |
| 9 | 5 µs/div | 60 µs | 102.4 kS/s | 2441 |
| 10–? | 10 µs–50 s/div | … | … | … |

**WARNING: divisor column is calculated, not confirmed.** Formula assumes:
- Buffer depth = 6144 samples (8 KB per channel)
- Display = 12 div wide (600 px)
- Sample rate fills the buffer for one full screen

Actual values in SRAM may differ if the firmware uses a different buffer-fill strategy or pre-trigger depth.

---

## How to get the real values

**STATUS UPDATE 2026-07-14 (GH#172): live SWD read of `0x20001FC0` (the address these options were
built around) does NOT contain the divisor table — see resolved section above. The options below
are retained for historical reference / in case the table lives elsewhere, but the
`0x20001FC0` premise they share is now known wrong.**

**Option A — Renode emulation:**
1. Boot `renode_emu.py` with the 2D82 firmware
2. Let `fw_app_main` run through `fw_apply_ch_timebase` (called at boot default setup)
3. Set all timebase indices 0–N and capture FPGA SPI writes to REG 0x0f
4. Or: dump SRAM 0x20001FC0 after boot init completes, read uint32 per entry

**Option B — Extract from stage-1 bootloader (most direct):** ~~The divisor table is populated by the stage-1 bootloader, not the stage-2 app. Extract and disassemble `HTX2D82-2021090901-inner.dfu` to find the stage-1 binary at 0x08000000. The stage-1 .data section or init code will contain the uint32[] divisors at the SRAM offset 0x20001FC0.~~

**RULED OUT 2026-07-14 (GH#48).** Read `bootloader_decompiled.c`'s `boot_main` in full (the stage-1 entry point, lines 2189–2260): its only non-DFU-branch SRAM stores are the two app-entry-point/SP bootstrap words for the standard vector-table jump-to-app sequence — no loop, no array write, nothing touching `0x20001FC0` or its 40-byte (10-entry) neighbourhood. Every other function `boot_main` calls (`boot_gpio_init`, three unnamed `FUN_*` setup routines, `boot_init`) is GPIO/USB/timer MMIO peripheral config, not an SRAM data table. The stage-1 bootloader genuinely does not write this table anywhere in its ~2900-line decompile.

Also checked whether the stage-2 app's `fw_data_decompress` mechanism (the LZ77 `.data`-init decompressor found via GH#161, table @ `0x08053330` → SRAM `0x20000000`, 9185/`0x23e1` decompressed bytes) happens to cover `0x20001FC0` (offset `0x1fc0` = 8128, numerically within the 9185-byte range). Transcribed the exact decompression algorithm to Python, cross-validated against GH#161's two known-good checks (both passed exactly), then decoded that offset: values are mostly zero for the first 8 entries (contradicts an expected 10-entry populated table), then a numeric run that degenerates into repeating `0x31313131` (literal ASCII bleeding in from the adjacent `g_fw_soft_str`/`str_car_buildtag` string) well before reaching the next real symbol — i.e. unstructured `.data` padding/gap bytes, not a real bounded divisor array. `fw_data_decompress` is stage-2 app code (`0x08005130`, inside the `0x08005000+` app region, called from the app's own `fw_ctors_run` startup chain) — its compressed blob is a compile-time image and structurally cannot hold a bootloader-populated runtime table anyway.

**Conclusion: not statically resolvable.** Neither the stage-1 bootloader nor any stage-2 app mechanism (traced or numerically-overlapping) writes this SRAM region in any of the flash images in this repo. Getting the real values requires live population capture — Option A (Renode) or Option C below.

**Option C — Logic analyser on SPI1:**
Capture FPGA SPI bus while stepping through timebase on a real device.

---

## Dual-channel vs single-channel rates

`fw_apply_timebase_mode()` (0x08011628) sets FPGA REG 0x0e:
- **bit1 (FAST_RATE)**: set when `timebase_index < 6` (fast timebases only)
- **bit0 (FAST_DUAL)**: set when single-channel AND `timebase_index < 7`

When FAST_DUAL=1 (single-channel fast mode), the FPGA interleaves both ADC slices into one channel → effectively doubles the sample rate. At index 0 in single-ch mode: 250 MHz interleaved = 250 MSPS max (the AD9288 is rated 40 MSPS per slice but operated at ~125 MSPS/slice = 250 MSPS interleaved).

The divisor in 0x20001FC0 is the **base dual-channel divisor**. The FAST_DUAL flag doubles the effective rate without changing the FPGA divisor register.

---

## AWG DDS frequency (same 250 MHz base clock)

AWG uses FPGA REG 0x18 (32-bit DDS frequency word):

```
dds_word = round(freq_hz * 2^32 / 250_000_000)
```

Confirmed from `fw_awg_apply_freq` constants (flt_16b0_25 = 25 and flt_16b4_150 = 150 in the literal pool adjacent to the timebase constants, confirming 250 MHz = 25 × 10 MHz reference factor used in DDS math).
