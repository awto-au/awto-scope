# Adaptive-capture exploration — the "advanced FPGA idea"

Test code for the adaptive-oscilloscope architecture: **calibrate in the FPGA,
tokenize the waveform into lines, store the meaning instead of every sample.**
Feasibility first (real numbers), then synthesizable Amaranth.

## Files

| File | What it is |
|---|---|
| `wave_compress.py`          | Pure-Python **reference model** — quantifies line/level compression, the cal-LUT effect, and MachXO2 fit. Golden model for the RTL. |
| `wave_compress_amaranth.py` | **Synthesizable Amaranth**: `CalLut` (STM32-loadable per-range table) + `LineCompressor` (streaming, divide/multiply-free). Self-checks in the Amaranth sim, emits `line_compressor.v`. |
| `awg_procedural.py`         | **Synthesizable Amaranth**: `ProcAWG` — square/PWM/triangle/sine generated from a *description* (tune/duty/amp), not a stored table. Self-checks, emits `proc_awg.v`. |
| `reactive_awg.py`           | **Synthesizable Amaranth**: `ReactiveAWG` — AWG as a function of a live scope channel (trigger→delay→response, mirror, math) + SPI streaming bandwidth model. Emits `reactive_awg.v`. |
| `personalities.py`          | Capture-personality model — bitstream-selectable memory splits (1ch-deep, AWG on/off, AWG/ch1/ch2 splits) and the resulting depth. |
| `display_path.py`           | Display decimation (min/max envelope), noise reduction (ENOB), and intensity-grading memory fit — deep capture → LCD. |
| `line_compressor.v` · `proc_awg.v` · `reactive_awg.v` | Emitted Verilog (pluribus harness / Diamond / nextpnr-machxo2). |

Run: `python3 wave_compress.py` · `python3 wave_compress_amaranth.py`
(logs → `../tmp/`).

## What the numbers say

**Line/level compression** (20 K samples, ±1.5 LSB reconstruction bound):

| waveform | depth gain |
|---|---|
| DC / flat | **435×** |
| ramp / sawtooth | 60× |
| glitchy (flat + runts) | 20× |
| triangle | 9× |
| PWM / sine (all edges) | 2–3× — want CYCLE (repetition) tokens next |
| pure noise | 1× — correctly falls back to raw |

**The calibration result is the headline.** With an interleaved dual-ADC
mismatch (gain 1.03, offset 3 LSB), compression measured with vs without the
correction LUT:

| waveform | uncorrected | with cal LUT |
|---|---|---|
| DC | **1.0:1** | **435:1** |
| ramp | 1.0:1 | 47:1 |

Uncorrected, *every* waveform is **1.0:1 — no compression at all**: the path
mismatch makes even a flat line zigzag code-by-code, so the line-fitter stores
raw. The Amaranth sim shows the same: uncorrected zigzag → 256 tokens,
corrected → 1 token.

## OEM vs this design (the key architectural point)

- **OEM**: the 784 B per-range cal lives in config-flash pages 0–48 and is
  applied in **STM32 software after capture**. So the FPGA memory holds *raw,
  uncorrected* samples — which are uncompressible (the zigzag above).
- **This design**: the STM32 loads a per-range **combined offset+gain LUT into
  the FPGA** on every range change; correction happens at wire speed **before**
  compression. Same per-range cal sets, moved upstream of the compressor —
  which is what turns 1× into 435×.

Real per-unit cal (config-flash pages 0–48) can be read and dropped straight
into `build_cal_lut()` when a unit dump is available.

## Fit on MachXO2-1200 (1280 LUT4, ~8 KB EBR)

- Active cal LUTs (1 range live, reload on range change): **512 B** — under
  1/16 of one EBR block.
- Streaming compressor: no divide, no multiply (MachXO2 has no DSP) — est.
  150–250 LUT4, leaving >1000 for capture + packing.
- At a typical ~30:1, 8 KB of token RAM ≈ **240 K samples of depth** from the
  same memory a raw design spends on 8 K — a ~30× deep-memory multiplier.

## Procedural AWG — describe the wave, don't store it

Real yosys synthesis of `ProcAWG` (all four shapes + 24-bit phase accumulator +
sine quarter-wave LUT), MachXO2 target:

| | value |
|---|---|
| **282 LUT4** | 22% of the 1280 — the *whole* generator |
| **0 EBR** | sine LUT maps to distributed LUT-RAM (`TRELLIS_DPR16X4`), no block RAM |
| sine accuracy | **0.3%** from a 384 B quarter-wave table |
| frequency | 2²⁴ phase acc → sub-Hz, any freq (vs a fixed 512-pt table) |

vs the OEM: R6C20 = 1152 B block RAM per stored waveform + SPI-load logic, one
fixed-length quantized shape at a time. **Procedural is smaller, better quality,
and infinitely tunable** — only *arbitrary* waveforms still need a stored table.

## Capture personalities (bitstream-selectable, `personalities.py`)

Because standard AWG shapes cost 0 EBR, the whole memory goes to capture:

| personality | depth |
|---|---|
| 2ch raw | 4K/ch |
| 1ch deep (all mem → one channel) | 8K (2×) |
| 1ch deep + compression | **226K (56× the 2ch-raw baseline)** |
| AWG-heavy 80/5/15 (arbitrary table) | flexible split |
| full-memory arbitrary AWG | deep AWG table |

Swap by mode via SRAM load (~19 ms, reverts on power-cycle). AWG-off drops the
DDS/table; 1ch-mode drops the CH2 pipeline; standard AWG is procedural (0 EBR),
arbitrary AWG claims memory as a table.

## Next

- CYCLE / repetition tokens (PWM & periodic → store one period + deviations).
- Min/max BUCKET pyramid for multi-zoom display.
- Wire `CalLut → LineCompressor → token packer → EBR` + `ProcAWG` as real
  personality bitstreams and sim against the CAND-A datapath in the harness.
- Fold real per-unit cal once a unit is read out (needs the spy bitstream — the
  cal flash sectors on the current dump are erased; nominal gains ~2.0× found in
  firmware rodata).
