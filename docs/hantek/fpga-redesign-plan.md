# FPGA redesign plan — 2D82AUTO scope gateware

A clean-room reimplementation of the scope FPGA gateware, replacing the OEM's
dumb-ring-buffer design with an on-chip capture/trigger/decimate/AWG engine.
This plan turns the exploration in `fpga/explore/` + the research in
`fpga-scope-research.md` into an ordered build toward real hardware.

## Fixed constraints (decided)

- **Part is fixed and un-changeable** — it is soldered on the scope. It is a
  **genuine Lattice LCMXO2-1200HC** (IDCODE 0x012BA043; prjtrellis and
  nextpnr-machxo2 target it natively). Fixed resources: **1280 LUT4, 7×9-kbit EBR**.
  (The *overclockable Geehy part on this board is the MCU* — a Geehy APM32F103VET6,
  an STM32F103 clone — **not** the FPGA.)
- **Goal: ONE bitstream with every feature** (not personalities). The budget
  says it's feasible but tight — see below. **Personalities are the fallback**,
  used only for the one genuine contention (deep-capture depth vs arbitrary-AWG
  table, both want EBR). SRAM-load swap (~19 ms, reverts on power-cycle) is there
  if needed.
- **Overclock headroom is a design lever.** The OEM already runs this Lattice part
  well past its datasheet nominal — the AD9288 at 250 MSPS (vs 40 MSPS rated) on a
  250 MHz CLKOP fabric clock — so real Fmax headroom above nominal MachXO2 spec is
  demonstrated on this board. Characterize real Fmax on the unit (Phase 1); the
  extra fabric speed buys **faster SPI oversampling → higher readout rate**,
  **higher capture sample rate**, and **deeper DSP pipelines**. Treat any clock
  above nominal as measured-and-validated, never assumed.
- **Toolchain: open-source** — Amaranth HDL → yosys → nextpnr-machxo2 →
  prjtrellis → `.bin`. This is what `explore/` already emits and sims. Diamond
  only if a hardened EFB block (die-temp path is external anyway; USERCODE/config
  WISHBONE) truly needs vendor instantiation.
- **Host link: SPI-v2** (`fpga-spi-v2.md`) — STM32F103 SPI1, ~18 MHz reliable /
  up to 36 MHz pushed (~2.25–4.5 MB/s). Capture-fast-freeze-read-slow.
- **All work lives in awto-2000** (`fpga/`), pluribus is only the sim harness.

## Can every feature fit in ONE bitstream? (budget: `scripts/fpga_budget.py`)

Real yosys synthesis of the built blocks + estimates for the rest, vs a
MachXO2-1200:

| | LUT4 | EBR |
|---|---|---|
| ProcAWG + LineCompressor + ReactiveAWG + SPI-v2 (synth) | 772 | 0 |
| capture + trigger + averager + min/max + cal + segments + arb-AWG (est) | ~430 | 7 |
| **Total** | **~1234 / 1280 (96%)** | **7 / 7** |

**Everything fits in one bitstream — EBR is not actually a constraint.** The key
realisation: for the all-in-one goal, **capture depth is irrelevant** — the ring
is the *elastic residual*, not a fixed budget line. Allocate the fixed EBR
consumers first (cal LUT ~1, arbitrary-AWG table ~1, compression token buffer),
and the capture ring simply takes **whatever EBR is left**. On-chip
line-compression makes even a 1–2 block ring deep in *effective* terms (435× on
flat, ~30× typical), so a small ring costs nothing in practice. There is no
deep-capture-vs-AWG contention — you just don't max the ring.

So the only real budget line is **LUT4 logic ≈ 1234 / 1280 (96%)**, which is
tight for place-and-route + timing (want ~15–20% slack) but **trims easily**:
share the SPI-v2 core, fold the AWG modes behind one mux, drop unused trigger
types, reuse the accumulator between averager and compressor. Target ~85%.

- **Personalities become unnecessary** for the cram goal — everything co-fits.
  Keep a "pure-scope, whole-7-EBR capture" personality only as an *optional*
  extra-depth mode for someone who wants it, not as a requirement.
- **Resources are fixed** at 1280 LUT4 / 7 EBR (genuine LCMXO2-1200) — budget
  against these exactly; there is no clone-bonus upside to hope for.
- **Overclock does NOT change area** — higher Fmax buys speed (SPI/capture/DSP
  depth), not LUT4/EBR. Orthogonal to the cram question.

## Overclocking opportunities

The FPGA is a genuine Lattice LCMXO2-1200HC, but the OEM already clocks it past
its datasheet nominal (AD9288 at 250 MSPS vs 40 MSPS rated; 250 MHz CLKOP fabric
clock), so real Fmax headroom above the nominal MachXO2 spec is demonstrated on
this board. **Overclock buys speed, not area** (no extra LUT4/EBR) — so it does
NOT help the cram budget, but it *does* buy:

1. **Faster SPI readout** — a soft SPI slave needs fabric ≥ ~4× SCK; more Fmax
   → higher sustainable SCK → the frozen-buffer readout drains faster.
2. **Higher capture sample rate** — if the PLL/ADC path can push, deeper live
   record per unit time.
3. **Deeper DSP pipelines / more clocks per sample** — averaging, min/max,
   compression, and the self-cal loops get more cycles per sample, so heavier
   on-chip processing fits the sample budget.
4. **Faster background self-cal** — more measurement windows/sec → the
   interleave/noise/BIST checks converge and refresh quicker.

**Characterize on the real unit** (a Phase-1 task): ramp the PLL, find the
ceiling with margin, validate across temperature. Never assume above-nominal
clocks — treat every overclocked rail as measured-and-validated.

## Verification strategy (every phase)

1. **Self-checking Amaranth sim** — each block asserts its own behaviour (the
   `explore/` pattern), run in CI-style before synthesis.
2. **pluribus harness** — `scripts/sim_replacement.py` for iverilog runs;
   co-sim against **CAND-A** (`replacement/aw2_scope.v`) as the datapath oracle
   where they overlap.
3. **Synthesis gate** — yosys `synth_lattice -family xo2 ; stat` for LUT4/EBR/FF
   budget after each block; fail if over budget.
4. **Hardware bring-up via SRAM load** — every milestone is loadable on the real
   unit non-destructively (reverts on power-cycle), so each phase is validated on
   silicon, not just in sim.

---

## Phase 0 — Foundations  ✅ (done this session)

Research (Red Pitaya / ThunderScope / debug-SPI / JTAG / open-scope protocols),
the hardware pin map, SPI-v2 spec, CAND-A + sim harness, and self-checked
prototypes for every major block (`explore/`): cal LUT, line compressor,
procedural + reactive AWG, min/max display path, personalities model, SPI-v2
debug interface. These are the proofs-of-concept each phase below hardens.

## Phase 1 — MVP capture scope  ⟵ FIRST BUILD

The minimal real, triggered, readable scope — the spine everything bolts onto.

- **ADC front-end** — latch the parallel AD9288 bytes (ch A/B), DDR-merge the
  interleaved phases on ENCA/ENCB → one sample stream per channel. (Simpler than
  ThunderScope's LVDS deserialization — parallel CMOS.)
- **Circular capture buffer** — one EBR ring per channel; continuous write;
  latch `wp_trig` at trigger; keep filling `post_delay` samples; freeze.
  Pre-trigger is free (already in the ring). (Red Pitaya scheme.)
- **Trigger engine** — Schmitt comparator + programmable level/hysteresis, edge
  select rising/falling/any; latency-aligned to the pipeline; 3-FF sync +
  debounce on any external trigger.
- **SPI-v2 control + readout** — the `spi_debug.py` core + the scope registers
  (`0x1C10+`): arm → poll status → read `wp_trig` → burst-read the window.
- **Deliverable:** a real, triggered trace captured and read over SPI on the
  actual unit. **Characterize part Fmax here** (ramp the PLL, find the overclock
  ceiling) — it sets the budget for later phases.
- **Verify:** self-sim + co-sim vs CAND-A capture path + SRAM-load on hardware.

## Phase 2 — On-chip DSP (the value the OEM never shipped)

- **Power-of-2 accumulate-and-dump averager** — adder + shift, no multiplier
  (Red Pitaya `rp_decim`). Anti-alias filter + **high-res mode** (left-shift
  before summing keeps gained bits → 8-bit AD9288 → ~12 ENOB at 256×).
- **Min/max envelope decimation** — per display column keep min AND max so
  glitches survive (our upgrade over RP's aliasing drop-decimator;
  `explore/display_path.py`).
- **Per-range cal LUT** — offset+gain correction (interleave mismatch) applied
  *before* any compression (`explore/wave_compress.py` `CalLut`). Loaded by the
  STM32 per range over SPI-v2.
- **Verify:** ENOB measurement in sim; min/max catches an injected glitch; cal
  restores compressibility.

## Phase 3 — Deep + advanced capture

- **Line/level compression** — the streaming swinging-door/RLE compressor
  (`explore/wave_compress_amaranth.py` `LineCompressor`) for deep effective
  memory (DC 435×, ramp 60×). Add CYCLE/BUCKET tokens to lift PWM/sine.
- **Segmented memory** — divide the ring into N segments, per-segment trigger
  re-arm + 64-bit timestamp (ThunderScope eventEngine idea) — capture many rapid
  events with zero dead-time.
- **Personalities (fallback only)** — the budget shows everything crams into one
  bitstream EXCEPT deep-capture-depth + arbitrary-AWG-table contend for the last
  EBR. If we can't resolve that in one bitstream (ring-shrink + compression, or
  procedural-AWG-default), ship those two as a bitstream-swap pair
  (`explore/personalities.py`): deep-scope vs AWG-arbitrary. Everything else
  (trigger, DSP, compression, procedural/reactive AWG, SPI-v2) stays in the base.

## Phase 4 — AWG

- **Procedural generators** — square/PWM/triangle/sine from a description
  (`explore/awg_procedural.py`: 282 LUT4, 0 EBR, 0.3% sine). Frees R6C20.
- **Reactive AWG** — trigger off a live channel → delay → response; mirror; math
  (`explore/reactive_awg.py`).
- **Arbitrary + streaming** — SPI-loaded table for arbitrary waveforms; RLL token
  stream for sparse/patterned (the LineCompressor run in reverse).

## Phase 5 — Debug / interface completion

- **Full SPI-v2** — die temp (external I2C sensor; MachXO2 has no on-die sensor),
  chip ID via EFB WISHBONE, pin status, GPIO drive (folds in the old spy-bitstream
  probing). Register map + behaviour: [fpga-spi-v2.md](fpga-spi-v2.md).
- **JTAG-over-SPI bridge** — SPI → 4 GPIO to an *external* target's TAP
  (~200 LUT, MPSSE/remote_bitbang framing). Not the FPGA's own TAP. Spec:
  [fpga-spi-v2.md](fpga-spi-v2.md) §"Capabilities beyond the OEM"; rationale:
  [fpga-scope-research.md](fpga-scope-research.md) §3.

---

## Dependency map

```
Phase 1 (capture+trigger+SPI-v2)  ── foundation for everything
   ├─ Phase 2 (avg / min-max / cal)      needs the capture engine
   │     └─ Phase 3 (compression / segments / personalities)
   ├─ Phase 4 (AWG)   mostly independent — can run in parallel
   └─ Phase 5 (debug/JTAG)   builds on the SPI-v2 core (already prototyped)
```

## Risk register

| Risk | Mitigation |
|------|-----------|
| **-1200 too small** once trigger+buffer+DSP+compress land | Personalities (ship features per-bitstream); measure LUT/EBR per block at each synth gate; drop optional blocks per mode. |
| **Overclock instability** | Characterize Fmax on the real unit with margin; validate at temperature; never assume above-nominal clocks. |
| **EFB in the open-source flow** | Die-temp is external (no EFB needed); USERCODE/config via WISHBONE may need Diamond for the EFB macro — isolate to Phase 5, keep the fabric open-source. |
| **Open-source toolchain fidelity on hardware** | The part is genuine Lattice LCMXO2-1200 (yosys/nextpnr-machxo2/prjtrellis target it natively; bitstream decodes cleanly) — low risk, but validate each bitstream on hardware via SRAM load before trusting. |
| **SPI readout bandwidth (~4.5 MB/s)** | Capture-freeze-read + on-chip decimation/compression; never stream full-rate. |

## Immediate next step

Assemble Phase 1: the capture top (AD9288 front-end + circular buffer + trigger
+ SPI-v2 control/readout) as one synthesizable Amaranth design, self-checked and
co-sim'd against CAND-A, then synth-gated for LUT/EBR budget — ready for the
first SRAM-load bring-up on the unit.
