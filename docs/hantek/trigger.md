# Trigger System — FPGA Command Interface

This document describes the trigger system from the code perspective: the SPI register writes the MCU
sends to the FPGA, the arm/poll cycle, boot defaults, and what is still unknown.

For the general SPI framing and register map see fpga-spi.md.  
For the PC→MCU USB commands that initiate trigger configuration see [usb-protocol.md](usb-protocol.md).

---

## Registers involved

All trigger configuration goes via the `0xa8` write bank. The FPGA receives these five registers
before every capture arm:

| Reg  | Bytes | Firmware fn | Purpose |
|------|-------|-------------|---------|
| 0x02 | 1     | `fw_fpga_trigger_mode` @ `0x0801f610`      | Type, coupling, AWG enable, freq-gate |
| 0x03 | 3     | `fw_fpga_trigger_band` @ `0x0801f646`      | Hysteresis window lo/center/hi |
| 0x06 | 2 LE  | `fw_fpga_ch1_trigger_level` @ `0x0801f698` | CH1 trigger level |
| 0x08 | 2 LE  | `fw_fpga_ch2_trigger_level` @ `0x0801f6da` | CH2 trigger level |
| 0x17 | 1     | `fw_fpga_trigger_edge` @ `0x0801f876`      | Edge polarity, slope flag, source |

Arm is via the `0xc8` bank. Status is read via the `0xa0` bank.

---

## REG 0x02 — Trigger Mode

Wire: `[0xa8][0x02][byte]`

```
 7    6          5         4    3       2              1    0
      AWG_ENABLE FREQ_GATE TRIG_TYPE[1:0] TRIG_COUPLING2   TRIG_COUPLING
```

| Bits | Field | Values |
|------|-------|--------|
| [6] | `AWG_ENABLE` | `1`=AWG output on (`\|=0x40`), `0`=off (`&=0xbf`) |
| [5] | `FREQ_GATE` | Set/clear around FPGA period-measurement windows for frequency readout |
| [4:3] | `TRIG_TYPE` | `0x00`=Edge  `0x08`=Pulse-Width  `0x10`=Video  `0x18`=Dual-AB (inferred from struct offsets; edge is the only confirmed value) |
| [2] | `TRIG_COUPLING2` | **RESOLVED 2026-07-13**: HF/noise-reject trigger sub-mode, independent of bit 0 (DC/AC coupling). Set/cleared by `fw_apply_trigger_hf_reject()` from SRAM flag `0x2000d08a` (`g_trig_param_ptr+0x1e`, aliased `g_dat_25e0`/`g_trigger_config_block+0x1e`). Toggled during calibration (`fw_calibrate` sets it to reject noise while binary-searching the trigger DAC; `fw_cali_setup_scope` clears it afterward) AND from a live front-panel UI menu item (`fw_recall_default_settings`'s menu dispatcher, `param_cur==1 && step==1`, same code shape as the adjacent step==0 DC/AC-coupling toggle). No USB command sets it — checked `fw_usb_bulk_rx` and the bank-3 cali write handler exhaustively, neither touches `0x2000d08a`. Likely a "HF Reject"/"Noise Reject" front-panel menu label; exact glyph/string not traced (parked — GUI string work, not hardware-blocked). |
| [0] | `TRIG_COUPLING` | `1`=DC  `0`=AC trigger coupling |

**Firmware paths that write REG 0x02:**
- `fw_fpga_trigger_mode(flags)` — raw write
- `fw_fpga_awg_enable(en)` — tweak bit 6 only, then call `fw_fpga_trigger_mode`
- `fw_fpga_trigger_coupling_set(en)` — tweak bit 1, then call `fw_fpga_trigger_mode`
- `fw_fpga_trigger_dc_couple()` — tweak bit 0, then call `fw_fpga_trigger_mode`
- `fw_apply_trigger_hf_reject()` — tweak bit 2, then call `fw_fpga_trigger_mode`

All are read-modify-write on an SRAM shadow; the whole byte is re-sent each time.

---

## REG 0x03 — Trigger Hysteresis Band

Wire: `[0xa8][0x03][lo][center][hi]`

Three 8-bit values in offset-binary (ADC counts, `0x80`=0 V).

```
center = trigger level in ADC counts
lo     = center - 10
hi     = center + 10
```

The FPGA uses this window to reject noise around the trigger threshold. `fw_apply_trigger_band()`
computes `center` from the param table and calls `fw_fpga_trigger_band(center−10, center, center+10)`.

SRAM shadows: `g_trig_hyst_lo`, `g_trig_hyst_center`, `g_trig_hyst_hi`.

---

## REG 0x06 — CH1 Trigger Level

Wire: `[0xa8][0x06][lo][hi]`  (16-bit LE)

```
0x0000 = most negative
0x8000 = 0 V (midscale)
0xffff = most positive
```

Value is computed by scaling the voltage-calibration table entry against the trigger-level param.
Written by `fw_fpga_ch1_trigger_level(lo, hi)`.

SRAM shadows: `g_ch1_triglevel_lo`, `g_ch1_triglevel_hi`.

---

## REG 0x08 — CH2 Trigger Level

Wire: `[0xa8][0x08][lo][hi]`

Identical encoding to REG 0x06. Written by `fw_fpga_ch2_trigger_level(lo, hi)`.

SRAM shadows: `g_ch2_triglevel_lo`, `g_ch2_triglevel_hi`.

---

## REG 0x17 — Trigger Edge / Source

Wire: `[0xa8][0x17][byte]`

```
 7  6  5  4  3  2    1       0
 TRIG_SRC[5:0]        SLOPE   EDGE_POL
```

| Bits | Field | Values |
|------|-------|--------|
| [7:2] | `TRIG_SRC` | **RESOLVED 2026-07-13 — dead/always-zero.** No code path in the firmware ever writes a nonzero value here. See below. |
| [1] | `SLOPE` | `1`=slope-trigger active. Set by `fw_apply_trigger_edge_slope` when `DAT_08011dbc==1`. |
| [0] | `EDGE_POL` | `1`=rising, `0`=falling. **CONFIRMED** from decompile: UI param `0`(rising) → `bVar4 \|= 1` → EDGE_POL=1. |

**Important:** all code paths (`fw_apply_trigger_edge`, `fw_apply_trigger_edge_no_arm`,
`fw_apply_trigger_polarity`, `fw_apply_trigger_edge_slope`) do read-modify-write only on bits [1:0].

**LIVE CONFIRMED 2026-07-14 (against a DIFFERENT, currently-flashed mystery firmware build — see note below).**
Live SWD verification was performed on the bench unit, whose currently-flashed firmware is
`vendors/hantek/2d82/firmware/2d82-20260713-swd/` — a mystery/unofficial build, NOT this reference build
(`2d82-20210818`). The REG-0x17 shadow lives at a *different* SRAM address on that build
(`0x2000ccf2`, vs this reference build's `0x2000d3ba`) because RAM layout differs between builds;
see `vendors/hantek/2d82/ghidra/ghidra_names_2d82_swd.tsv` (`g_fpga_reg17_shadow_swd`) for the mystery-build-specific
address trace. Procedure: read the mystery build's flash live via SWD and confirmed it matches the
static `flash_08005000.bin` dump byte-for-byte (same firmware, not yet another variant); read the
REG-0x17 shadow byte before and after each of the 3 `dsoHTSetTriggerSource` (USB cmd `0x10`)
selector values (CH1/CH2/EXT) sent over live USB; the shadow's `TRIG_SRC[7:2]` bits stayed `0x00`
across all 3 sends, reproduced identically across two independent runs (before and after an
intervening SWD reset). This directly confirms — on real hardware, not just static trace — that
the USB trigger-source selector never reaches the FPGA-facing REG-0x17 shadow. A write-watchpoint
attempt on the shadow (for an even stronger single-session proof) was inconclusive: arming a
hardware watchpoint via pyocd momentarily halts the core, which was observed to disrupt the
target's USB peripheral enumeration badly enough that the watchpoint approach was abandoned in
favour of the (already conclusive) read-before/after method above. Device recovered cleanly via
the standard SWD reset path after each disruption; see `vendors/hantek/2d82/docs/hardware/swd-debug.md`.
Static analysis below (against the reference build) remains the primary documented trace; the live
run corroborates its conclusion on independent hardware/firmware, it does not supersede it.

**issue #49 RESOLVED — TRIG_SRC bits [7:2] are architecturally dead on this firmware build.**
Exhaustive static trace:
- `fw_fpga_trigger_edge` (the only function that writes REG 0x17) has exactly **4 call sites in the
  entire decompiled corpus**: `fw_apply_trigger_edge`, `fw_apply_trigger_edge_no_arm` (a previously
  undocumented near-duplicate of `fw_apply_trigger_edge` that skips the restart-flag branch),
  `fw_apply_trigger_edge_slope`, and `fw_apply_trigger_polarity`. All 4 are confirmed RMW-only on
  bits [1:0] — none construct or OR in a bits-[7:2] value.
- USB cmd `0x10` (`dsoHTSetTriggerSource`) does **not** feed REG 0x17 at all. Tracing
  `fw_usb_bulk_rx`'s `cmd==0x10` branch shows it writes the raw 0/1/2 selector into a *different* SRAM
  cell — `ptr_usb_scope_param_tbl_b[0]` (SRAM `0x2000d06c`), the same cell that's echoed back verbatim
  at `ReadAllSet[17]` and the bank-0 `v==0x10` readback selector. That cell is read in exactly 2 other
  places in the firmware (both are a channel-select index for `fw_fpga_trigger_coupling_set` inside the
  CH0/CH1 on-off dispatch, unrelated to REG 0x17) — it is never read by `fw_fpga_trigger_edge` or any
  of its 4 callers. **The UI-level "Trigger Source" setting (CH1/CH2/EXT) has no code path to the FPGA
  at all in this firmware build.**
- `fw_trigger_param_struct_init` and `fw_apply_trigger_settings` were both checked in full — neither
  touches the `0x2000d3ba` shadow.
- The shadow's "pre-initialized" appearance is fully explained by `.bss` zero-fill: `0x2000d3ba` falls
  inside the documented single-entry `fw_bss_zero_table` range `0x200023e4`–`0x2000d3f4` (see
  `fw_init_copy_table`/`g_bss_zero_table` in the TSV), which `Reset_Handler`'s startup dispatch runs
  before `fw_app_main`. Bits [7:2] start at 0 from bss-clear and are never written again — there is no
  hidden/untraced init path, just an always-zero field.

**Firmware paths that write REG 0x17:**
- `fw_fpga_trigger_edge(flags)` — raw write
- `fw_apply_trigger_polarity()` — tweak bit 0
- `fw_apply_trigger_edge_slope()` — tweak bit 1
- `fw_apply_trigger_edge()` — reads trigger type/slope, dispatches to the above, then: type=2→`fw_restart_flag_set(1)`, type=1→`fw_acq_arm_set(1)`
- `fw_apply_trigger_edge_no_arm()` — same shape as `fw_apply_trigger_edge` but without the type==2 restart-flag branch (newly identified, 2026-07-13)

Shadow location: SRAM `0x2000d3ba` (pointed to by `DAT_08020090` / `DAT_08011db4` / `DAT_08020310`).

---

## Capture Arm — CTRL REG 0x00

Wire: `[0xc8][0x00][0x07]`  (always the constant `0x07`)

```
Bit 2  ENABLE_ACQ   = 1
Bit 1  ARM_TRIGGER  = 1   (FPGA starts watching for trigger condition)
Bit 0  RUN          = 1
```

Written by `fw_fpga_arm_capture()` @ `0x0801f846`. Called as part of the capture arm step.  
`fw_scope_trigger_rearm()` calls `fw_acq_arm_set(1)` then re-writes REG 0x02 before arming.

> Do NOT confuse with `0xa8` REG 0x00 (CH1 front-end control). Same register index, different bank byte.

---

## Status Poll — READ REG 0x00

Wire: `[0xa0][0x00][0xa0]` → receive 1 byte → store to `g_fpga_status` (SRAM `0x2000d3c3`)

```
Bit 1  BUSY = 1 while capture is running or waiting for trigger
Bit 6  (status << 0x19 < 0) = trigger/AWG input present (inferred)
```

Firmware polls in a loop: `while (status & 0x02) != 0: read again`.

---

## Full Trigger Configuration Sequence

This is the SPI sequence the MCU sends on each capture setup. Trigger config (step 2) always
follows front-end config and precedes the arm.

```
Step  Wire bytes                              Meaning
────  ──────────────────────────────────────  ──────────────────────────────────────────
      ── configure front-end (not shown) ──

1.    [0xa8][0x02][flags]                     REG 0x02: trigger type + coupling + AWG
2.    [0xa8][0x03][lo][center][hi]            REG 0x03: hysteresis band (center±10)
3.    [0xa8][0x06][lo][hi]                    REG 0x06: CH1 trigger level (16-bit LE)
4.    [0xa8][0x08][lo][hi]                    REG 0x08: CH2 trigger level (16-bit LE)
5.    [0xa8][0x17][flags]                     REG 0x17: edge polarity + slope + source

6.    [0xc8][0x00][0x07]                      CTRL: arm (ENABLE_ACQ | ARM_TRIGGER | RUN)

7.    loop: [0xa0][0x00][0xa0] → 1 byte      READ: poll BUSY bit, break when 0
```

---

## Boot Defaults (from Renode trace)

These are the values written at first init (cold boot / factory state):

| Step | Reg | Value | Meaning |
|------|-----|-------|---------|
| 14 | REG 0x02 | `0x01` | Edge trigger, DC coupling, AWG off (3 writes during init) |
| 17 | REG 0x03 | lo=`0x76` ctr=`0x80` hi=`0x8a` | DC trigger at ADC midscale, ±10 band |
| 18 | CTRL 0x00 | `0x07` | Arm capture (fires **before** REG 0x06/0x08/0x17 below — see full 28-step trace in fpga-spi.md §11) |
| 23 | REG 0x06 | `0x0c00` LE | CH1 trigger level = `0x0c00` |
| 24 | REG 0x08 | `0x0600` LE | CH2 trigger level = `0x0600` |
| 25 | REG 0x17 | `0x01` | Rising edge (EDGE_POL=1 confirmed) |

`fw_trigger_param_struct_init()` initialises the SRAM param block first:
coupling=0, mode=1, sweep=2, level=100/−100.

---

## PC → MCU → FPGA Path

The PC sends USB commands (bank `0x00`) to set trigger params. The MCU stores them in an SRAM
param block, then translates to FPGA SPI writes on the next capture cycle.

| USB cmd | DLL export | Payload | FPGA registers affected |
|---------|-----------|---------|------------------------|
| `0x0f` | `dsoHTSetTriggerHPos` | `abs(hpos)×3` LE uint16 + sign byte | affects capture window offset in readback; not directly an FPGA write |
| `0x10` | `dsoHTSetTriggerSource` | LE uint16: `0`=CH1 `1`=CH2 `2`=EXT | **NONE.** RESOLVED 2026-07-13: stores only into `ptr_usb_scope_param_tbl_b[0]` (SRAM `0x2000d06c`, readback/UI-echo only). REG 0x17 bits [7:2] (`TRIG_SRC`) are never written by any code path — see REG 0x17 section above. This USB command has no effect on FPGA trigger-source selection in this firmware build. |
| `0x11` | `dsoHTSetTriggerSlope` | LE uint16: `0`=rising `1`=falling `2`=either | REG 0x17 bit 0 (EDGE_POL) |
| `0x12` | `dsoHTSetTriggerSweep` | LE uint16: `0`=AUTO `1`=NORMAL `2`=SINGLE | `1` doesn't reliably trigger; `2` does — use `2` for one-shot capture. See below. |
| `0x14` | `dsoHTSetTriggerVPos` | scaled vpos byte | REG 0x03 (band center) + REG 0x06/0x08 (level) |

**TriggerHPos encoding** (3-byte payload at USB bytes [5:8]):
```
data[5:7] = abs(hpos_raw) × 3   (LE uint16)
data[7]   = 0x01 if hpos_raw < 0, else 0x00
```

**ReadAllSet response** (USB cmd `0x15`) echoes current trigger state at bytes [13–20]:
```
[13] TriggerHPos lo
[14] TriggerHPos hi
[15] TriggerHPos sign lo
[16] TriggerHPos sign hi
[17] TriggerSource   (0=CH1, 1=CH2, 2=EXT)
[18] TriggerSlope    (0=rising, 1=falling, 2=either)
[19] TriggerSweep    (0=AUTO, 1=NORMAL, 2=SINGLE)
[20] TriggerVPos     (raw byte + 0x64 offset)
```

---

## Trigger State in Waveform Readback (USB cmd `0x16`)

When the PC polls for waveform data before the trigger fires, the firmware returns a 12-byte
not-triggered pattern instead of real ADC data:

```
00 00 ff ff 00 00 ff ff 00 00 ff ff
```

When all samples have been read (`cursor >= end_address`), the firmware auto-re-arms:
1. Reset read cursor to 0
2. Call `fw_scope_apply_all(1)` (restore UI / arm mode)
3. Reset `TriggerHPos = 0x96`
4. Re-apply trigger edge (`fw_apply_trigger_edge`)
5. Set acquisition-run flag

**Full trace of the triggered/real-data case** (packet byte layout, per-packet size, cursor
mechanics, and the underlying FPGA sample read that populates this data) is in
[waveform-readback.md](waveform-readback.md) — that document also covers the continuous live-view
acquisition path (`fw_acq_process_waveform`), which is separate from this USB-polled path but uses
the identical FPGA wire command.

---

## Key Firmware Functions

| Function | Address | Role |
|----------|---------|------|
| `fw_apply_trigger_settings` | `0x08011dc4` | Main reconfiguration path — reads type/coupling/level, calls FPGA write chain |
| `fw_apply_trigger_edge` | `0x08011ad6` | Reads type/slope, calls `fw_fpga_trigger_edge`; dispatches arm/restart by type |
| `fw_apply_trigger_band` | `0x08020266` | Reads coupling + level, calls `fw_fpga_trigger_band(center−10, center, center+10)` |
| `fw_apply_trigger_level_raw` | `0x0800ecec` | Writes FPGA trigger level directly without scaling |
| `fw_apply_trigger_polarity` | `0x080202ba` | Reads polarity flag, tweaks bit 0 of REG 0x17 shadow |
| `fw_apply_trigger_edge_slope` | `0x08011b8e` | Reads slope flag, tweaks bit 1 of REG 0x17 shadow |
| `fw_apply_trigger_edge_no_arm` | `0x08011b38` | Near-duplicate of `fw_apply_trigger_edge` without the type==2 restart-flag branch; also RMW-only on bits [1:0] of REG 0x17 |
| `fw_apply_trigger_hf_reject` | `0x080201f0` | Reads HF-reject flag (SRAM `0x2000d08a`), tweaks bit 2 (`TRIG_COUPLING2`) of REG 0x02 shadow; user-reachable via front-panel menu |
| `fw_fpga_trigger_mode` | `0x0801f610` | SPI write: REG 0x02 |
| `fw_fpga_trigger_band` | `0x0801f646` | SPI write: REG 0x03 |
| `fw_fpga_ch1_trigger_level` | `0x0801f698` | SPI write: REG 0x06 |
| `fw_fpga_ch2_trigger_level` | `0x0801f6da` | SPI write: REG 0x08 |
| `fw_fpga_trigger_edge` | `0x0801f876` | SPI write: REG 0x17 |
| `fw_fpga_arm_capture` | `0x0801f846` | SPI write: CTRL REG 0x00 = 0x07 |
| `fw_scope_trigger_rearm` | `0x08012c08` | Calls `fw_acq_arm_set(1)` then re-writes REG 0x02; used after triggered stop |
| `fw_trigger_param_struct_init` | `0x08020150` | Initialises SRAM param block to defaults |

---

## Open Questions

**issue #49 — REG 0x17 TRIG_SRC bits [7:2] — RESOLVED 2026-07-13, CLOSED. LIVE CONFIRMED 2026-07-14
(mystery firmware build, see REG 0x17 section above for full detail and caveats).**
Exhaustive static trace (see REG 0x17 section above) found bits [7:2] are never written by any
firmware code path — they start at 0 from `.bss` zero-fill (`0x2000d3ba` falls inside
`fw_bss_zero_table`'s documented range) and stay 0 forever. USB cmd `0x10`
(`dsoHTSetTriggerSource`) writes only a UI/readback-echo cell (`0x2000d06c`) that no REG-0x17 writer
ever reads. Conclusion: TRIG_SRC is architecturally dead on this firmware build — the FPGA never
receives a trigger-source selector from the MCU via REG 0x17. Live SWD testing against the
currently-flashed mystery build (different firmware, different shadow address `0x2000ccf2` — see
above) reproduced the same behaviour on real hardware: bits [7:2] never moved across all 3 USB
trigger-source selector values. Open follow-up (not part of #49):
how does the FPGA actually select CH1 vs CH2 vs EXT as trigger source, if not via REG 0x17? Candidate
mechanisms (untested): implicit from which channel's REG 0x06/0x08 level is "live", or a hardwired/
default single trigger source in the FPGA bitstream with the "Trigger Source" UI control being
vestigial/non-functional on this hardware revision. Would require FPGA-side (not MCU) analysis to
resolve further — see `fpga/hantek/docs/`.

**REG 0x02 TRIG_COUPLING2 (bit 2) — RESOLVED 2026-07-13, CLOSED.** Not an open question — see the
REG 0x02 register table above for the full resolved finding (HF/noise-reject trigger sub-mode,
`fw_apply_trigger_hf_reject`, SRAM flag `0x2000d08a`). Kept here only as a historical pointer since
this section previously listed it as open.

**REG 0x02 TRIG_TYPE `0x18` (Dual A/B)** — the fourth TRIG_TYPE value is inferred from the
struct layout (3 valid indices observed). Whether this maps to a real "dual-channel trigger"
mode in the FPGA is unconfirmed.

**TriggerSweep=1 (NORMAL) does not reliably trigger on this firmware; use TriggerSweep=2 (SINGLE)
instead.** Confirmed live 2026-07-14: NORMAL fires in ~6% of attempts across slope/level sweeps;
SINGLE fires 14/14 (100%) on the same test matrix, all slopes, all tested trigger levels. AUTO
(0) also works reliably. Ruled out as causes: re-arm timing, an explicit `cmd 0x0c` (start/stop)
toggle before capture. Not yet found: what makes NORMAL fail at the firmware level. Cross-referenced
against [hkoosha/hanteker](https://github.com/hkoosha/hanteker)'s independently-derived `AUTO=0/
NORMAL=1/SINGLE=2` enum (same VID:PID), which is what prompted testing `2` as a distinct value in
the first place. **Use `TriggerSweep=2` for any one-shot capture test or Zephyr-compat check.**

**Trigger-level (vpos) no-fire boundary found 2026-07-14.** Full-byte-range sweep (CH2, 1kHz/1000mV
sine, SINGLE mode, `vendors/hantek/2d82/scripts/hantek_trigger_characterize.py --boundary` then `--boundary-fine-low`
to pin the exact transition) found the trigger fails to fire only at `vpos=0x00` and `0x01`; every
tested value `0x02`-`0xf8` fired reliably (spans 34-53 counts, consistent with signal-present). This
is a much narrower dead zone than the `0x40`-`0xc0` all-fired range previously tested suggested —
the real edge sits right at the bottom of the byte range, not symmetric top/bottom as might be
expected. Not yet explained at the firmware level why only the bottom 2 values fail (off-screen
trigger level relative to the 0x80-centered signal is the working theory, but 0x00-0x01 being
off-screen while 0xf8 is not is asymmetric and unconfirmed).
