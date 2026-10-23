# Calibration Process Walkthrough — 2D82AUTO

Companion to [`calibration.md`](calibration.md), which documents the **data formats** (flash layout,
cal buffers, FPGA registers, the DAC path). This doc traces the **runtime process** — what actually
runs, in call order, when the scope calibrates itself, boots, or is calibrated over USB. Every line
number refers to `vendors/hantek/2d82/firmware/2d82-20210818/analysis/decompiled_named.c`.

There are three distinct calibration flows:

1. **Interactive self-calibration** — `fw_calibrate` (0x08018c18), the offset/gain sweep the user triggers.
2. **Boot restore** — cal data loaded from flash + FPGA NVCM into RAM at startup, then applied.
3. **USB calibration** — the PC writes cal factors / triggers a flash save over the USB protocol.

---

## 1. Interactive self-calibration — `fw_calibrate`

Triggered from the scope UI (physical F4 with the CH-param cursor at index 4 — see
`fw_recall_default_settings` / `ptr_scope_param_cur`, and the **hang caveat** at the end). This is a
closed-loop routine: it drives the FPGA vertical-offset DAC, captures the resulting ADC level, and
**binary-searches the DAC code** until the measured trace sits at a target pixel position. It does
this for every (pass × channel × gain-step × offset-target) combination, then persists the results.

### 1a. Setup (26203–26236)

```
save ch1/ch2 gain-range steps + tb_index (restored on exit)
tb_index = 7                              → fastest timebase for a stable flat capture
fw_apply_capture_mode(); redraw timebase
enable HF-reject (g_trigger_config_block+0x1e = 1)
if acq_mode is INTERLEAVE or FAST → force ACQ_MODE_NORMAL (single-shot, not interleaved)
g_cali_busy_flag = 1                      → blocks re-entry / other consumers
draw "Calibrating" string (fw_gfx_draw_string_raw, ptr_cali_str_calibrating)
ch1/ch2 coupling = CH_COUPLING_DC         → cal is done DC-coupled
fw_apply_ch1_coupling(); fw_apply_ch2_coupling()
fw_cali_set_trigger_flag(1)
```

The enum names (`ACQ_MODE_INTERLEAVE`, `CH_COUPLING_DC`) come from the struct-field enums — the raw
firmware stores 0/1/2 here.

### 1b. The three nested loops (26239–26413)

```
for cal_phase in 0, 1:                    # two calibration passes (see note on phase meaning)
    for ch_idx in 0 (CH1), 1 (CH2):
        select per-(phase,ch) timebase init:
            phase0,ch0 → fw_scope_timebase_interleave_init
            phase0,ch1 → fw_scope_timebase_ch2_only_init
            phase1     → fw_scope_timebase_config_init
        for gain_step in 9 down to 0:      # 10 vertical-gain ranges, coarse→fine
            skip if that channel's on_flag == 0
            set that channel's gain_range_step = gain_step; fw_apply_ch_frontend()
            tolerance = 2 for gain_step ≤ 2, else 0   # looser convergence at low gain
            for offset_target_idx in 0, 1, 2:         # three target levels ↓
```

`cal_phase` counts 0, 1, then **2 triggers the finish/save branch** (the `if (1 < cal_phase)` at
26240). It is the outer sweep index; both passes run the identical binary search — the difference is
the timebase-init selected per (phase, ch). (The older "0=AC / 1=DC" note is not borne out by the
body: coupling is forced DC for the whole routine at 26232. The two passes differ in timebase setup,
not coupling.)

### 1c. The binary search (26311–26408) — the heart of it

For each **offset target** the routine drives a different vertical position and searches for the DAC
code that lands the trace there:

| `offset_target_idx` | `target_position` (ADC px) | stored at `g_scope_cali_block +` |
|---|---|---|
| 0 | **+100** | `phase*0x50 + ch*0x28 + step*4 + 0xa0` |
| 1 | **−100** | `phase*0x50 + ch*0x28 + step*4 + 0x140` |
| 2 | **0** (GND) | `phase*0x50 + ch*0x28 + step*4 + 0x000` |

The search itself (26333–26388):

```
search_lo = search_hi;  search_hi = span + search_lo;  search_mid = search_hi >> 1
anti-stall: if search_mid repeats 5× (stall_count), force a jump to break oscillation
if F3 pressed (keycode == 3) → abort: restore scope, clear trigger flag, return
write DAC:  ch0 → fw_fpga_ch1_voffset(mid & 0xff, hi>>9 & 0xff)   # FPGA REG 0x0a, 16-bit LE
            ch1 → fw_fpga_ch2_voffset(...)                        # FPGA REG 0x0c
fw_delay_ms(50)                          # relay/DAC settle
re-arm trigger, wait for a completed capture (fw_apply_trigger_settings loop)
fw_scope_wave_process(ch, 0x4b0, &measured_level, ...)   # capture + measure mean level
error = |measured_level − target_position|
converged if error ≤ tolerance → store search_mid, next target
else narrow the search window by the sign of the error (with the stall override)
```

So the stored cal value is the **DAC midpoint code** that centres the trace at +100 / −100 / 0 ADC
for that gain step. `fw_fpga_ch1_voffset` writes FPGA SPI REG 0x0a (offset-binary, 0x8000 = 0 V) —
this is the one place cal data reaches live hardware through a runtime register (Classification B in
`calibration.md`); everything else is MCU-side software correction applied at capture time.

The on-screen `fw_printf_1arg(ptr_cali_fmt_*)` calls (search / up / down / zero / ch1_gnd / ch2_gnd)
emit the UART/telemetry progress lines you see during a cal run.

### 1d. Finish + persist (26240–26275, the `cal_phase ≥ 2` branch)

```
fw_scope_ch2_display_init(); re-arm; fw_draw_waveform_panel()
capture GND reference per channel:
    fw_scope_wave_process(0, ...) → g_scope_cali_block + 0x1e0   (CH1 GND level)
    fw_scope_wave_process(1, ...) → g_scope_cali_block + 0x1e4   (CH2 GND level)
fw_scope_display_init_all()
fw_fpga_write_config(0)          → persist the 0x2f8 cal block to the FPGA MachXO2 NVCM (sel=0 sub-block)
fw_scope_config_save_flash()     → persist to STM32 flash
draw result box + PASS/FAIL:
    display_mode_cur == 0 → "Completed" / "press key" strings
    display_mode_cur == 1 → PASS glyph table / FAIL glyph table
wait for any keypress (fw_keypad_code_read loops until non-zero)
fw_cali_setup_scope(saved_tb_index, saved_ch1_gain, saved_ch2_gain)   # restore pre-cal state
fw_cali_set_trigger_flag(0); return
```

Result status lives in `g_cali_result_status` (a `ScopeParamLimits*`): field `+0xe` 0 = OK path
(Completed string), 1 = PASS/FAIL glyph path.

### Data landing map (per gain step, both passes)

```
g_scope_cali_block + cal_phase*0x50 + ch_idx*0x28 + gain_step*4 +
    0x000  → zero-target DAC code   (offset_target_idx 2)
    0x0a0  → +100-target DAC code   (offset_target_idx 0)
    0x140  → −100-target DAC code   (offset_target_idx 1)
g_scope_cali_block + 0x1e0/0x1e4    → CH1/CH2 GND reference (captured once at finish)
g_scope_cali_block + 0x2f4/0x2f5/0x2f6 → volt/scope/DMM valid flags
```

Later, `fw_scope_ch1_voffset_apply` linear-interpolates between these three ref points (+100 / 0 /
−100) to compute the live DAC code for any user vertical position — that is how the cal result is
consumed in normal operation.

---

## 2. Boot restore

Runs from the main init path (`fw_app_main`) before the event loop:

```
fw_cali_apply_ch1()        → memcpy 0x130 B flash → g_cali_ram_buf   (dirty-flag check only, no FPGA write)
fw_fpga_config_restore()   → read the FPGA MachXO2 NVCM cal mirror back into RAM (unconditional)
fw_cali_verify_checksum()  → check valid flag [+0] == 0x01
fw_cali_apply_ch2()        → load 16 B DMM current-clamp cal → g_dmm_cali_param_ptr (0x2000d268); NOT scope-CH2
fw_cali_volt_data / fw_volt_cali_float_table_init → load 0xec voltage table from flash
```

Neither `fw_cali_apply_ch1` nor `fw_cali_apply_ch2` writes an FPGA register — the vertical-offset DAC
write happens later via `fw_scope_ch1_voffset_apply` / `fw_apply_ch2_position` (after frontend/coupling
setup), and ADC gain correction is applied per-sample at capture time. See `calibration.md` →
"FPGA Registers Written by Calibration" for the full trace.

---

## 3. USB calibration protocol

Handled by `fw_usb_cali_write_handler` / `fw_usb_cali_read_handler` under `fw_usb_cmd_parser`
(bank byte = `USB_BANK_CALI` = 3). Two mechanisms:

```
band data (cmd 0x0b, sub 1..6)  → written to g_cali_table (RAM only)
flash save (cmd 0x0c, sub 6)    → fw_cali_save_to_flash(): g_cali_ram_buf (0x130 B) → flash at g_cali_flash_start
```

`fw_fpga_write_config` is also reachable from the USB cali path with `sel=1` (volt/serial/AWG-DDS
sub-block) and `sel=2` (DMM DCMA/ACMA sub-block), vs `sel=0` used by `fw_calibrate` for the scope
DAC-cal sub-block — three sub-blocks of the same 0x2f8 NVCM cal region.

---

## Known hang — `fw_calibrate` can get stuck

The binary search can fail to converge and spin (>1000 non-converging "search" prints). Reproduced
2026-07-13 with only valid in-range USB commands. The exact path that reaches `fw_calibrate` with the
CH-param cursor == 4 is fragile — a USB path can set the cursor to 4 without a physical press
(`ptr_scope_param_cur`), and then any event landing in `fw_recall_default_settings` with cur == 4
fires `fw_calibrate` unconditionally (26-line note at `fw_recall_default_settings`). Recovery needs a
physical F3 (the keycode==3 abort at 26348) or an SWD reset — there is no USB-only recovery. Full
detail: `vendors/hantek/2d82/docs/hardware/bugs/usb-cmd16-hang.md`.

The convergence guard that *should* prevent the spin is the `stall_count` anti-oscillation logic
(26337–26345, 26371–26387): if `search_mid` repeats 5×, it forces a window jump. The hang implies a
case where the measured level never reaches tolerance even after the jump — most likely when the ADC
capture returns a level that the DAC range cannot reach (e.g. no signal / wrong frontend state), so
every candidate is equally wrong and the search oscillates between two non-converging endpoints.

---

## Where this connects to the emulator

Under Renode, `fw_calibrate` will call `fw_fpga_ch1_voffset` (SPI write) → `fw_delay_ms(50)` →
`fw_scope_wave_process` (which reads back captured samples). To make the search *converge* in
emulation, the FPGA/ADC stub must return a `measured_level` that responds monotonically to the DAC
code just written — otherwise it hits exactly the non-convergence hang above. The screen-capture
harness (`code/renode/hantek_emu.py`) can drive to the cal screen and snapshot the "Calibrating" /
PASS-FAIL panels; making the search *complete* is a separate step that needs the stub to model the
DAC→ADC transfer, not just ACK the SPI writes.
