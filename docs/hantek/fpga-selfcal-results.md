# FPGA self-calibration — rationale and measured results

Records the **validation evidence** for the replacement-RTL self-calibration blocks.
The code lives in `../rtl/` and the design intent is in
[`fpga-redesign-plan.md`](fpga-redesign-plan.md); what was missing was the measured
justification — *why* these blocks exist and what they were shown to achieve.

Written up 2026-07-20 from the working session that produced the blocks. Re-deriving
these numbers means re-running the sims; the conclusions they justify are what drove
the design.

---

## 1. The architecture finding that motivates all of it

**The stock design puts no calibration in the FPGA.** The FPGA is a dumb,
uncalibrated ADC-sample source:

- **To the FPGA** over SPI: only the 14 control-register writes plus the 1024-byte
  AWG table. No gain factors, no interleave-correction data, no calibration-table
  floats ever cross the bus.
- **From the FPGA**: raw 8-bit ADC sample bytes only.
- **All** correction — gain, interleave-ratio, interleave-delta, and the voltage-cal
  float multiply — happens on the MCU, in software, *after* the samples have left the
  FPGA.
- The only calibration-adjacent value that reaches FPGA silicon is the per-channel DC
  offset DAC code (`REG 0x0a` / `0x0c`).

Consistent with
[`../../../vendors/hantek/2d82/docs/hardware/calibration-process.md`](calibration-process.md),
which records that everything beyond the DC offset is MCU-side software correction
applied at capture time.

So this is **not** a system where the MCU pushes a calibration profile into the FPGA
and the fabric applies it. That is precisely the opportunity: moving correction into
the fabric is a genuine improvement over the OEM, not a reimplementation of it.

## 2. Why in-FPGA correction matters — the compression result

The AD9288's two interleaved slices (A on `ENCA`, B on `ENCB`) have offset/gain
mismatch. Uncorrected, that mismatch makes even a **flat DC line zigzag code-by-code**.

Measured effect on the on-chip line compressor, DC input:

| configuration | compression ratio |
|---|---|
| without per-range cal LUT | **1.0 : 1** — no compression at all |
| per-range cal LUT applied *before* compression | **435 : 1** |

The cal LUT is not a nice-to-have: it is the difference between 1× and 435×.
**Correction must run before compression**, or compression cannot work at all — the
zigzag destroys every run of equal samples the compressor depends on.

## 3. Automatic interleave calibration — `rtl/interleave_cal.py`

The OEM corrects interleave mismatch **one-shot**, in STM32 software
(`fw_cali_ch_balance_check`, averaging even- vs odd-indexed samples).

The replacement does it **continuously and adaptively in fabric**: a statistical
even/odd detector driving an adaptive LMS loop — self-measuring, tracking temperature
drift, with no manual calibration procedure.

**Measured:** injecting a **+6 LSB** slice mismatch, the loop converged to exactly
**−6** and cut the interleave spur **153×** (2.98 → 0.02 LSB).

Properties:

- Handles any magnitude — it is linear feedback, so ±1, ±2 or larger all work.
- Detects **sub-LSB** mismatch by averaging.
- Exposes check outputs readable over SPI-v2, including `meas_offset` (the true
  measured mismatch), so the correction is observable rather than opaque.

## 4. Phase-1 integration — `rtl/aw2_gateware.py`

Front-end (2-channel interleaved + automatic interleave-cal) → capture core, with the
SPI-v2 slave wired to the capture register map, driven **end-to-end over SPI** in
simulation: configure the trigger, arm, capture, poll `STATUS` (`0b110` =
triggered + frozen), read `wp_trig`, then burst-read the window back.

The captured window (`[60, 66, 200, 206, …]`) **visibly shows the interleave zigzag** —
the injected +6 LSB B-slice mismatch, before the cal loop has fully converged inside a
160-sample capture. The mismatch signature is directly observable in the returned data,
which is what makes the correction verifiable rather than assumed.

**Test status when written: 9/9 green** — Phase-1 end-to-end scope top plus the
cal/self-test family (interleave cal, noise/dither check, bit-toggle BIST).

---

## Why this document exists

The RTL was committed and the design intent was recorded in `fpga-redesign-plan.md`,
but the **measured results were written down nowhere**: no document referenced
`interleave_cal` by name, and `rtl/` had no README. A transcript audit of the session
that produced these blocks flagged the gap.

The numbers above are the evidence for two design decisions that are otherwise just
assertions:

1. **Correct before you compress** — otherwise compression yields nothing (1.0 : 1).
2. **Adaptive beats one-shot** — continuous in-fabric correction tracks drift the
   OEM's single STM32 pass cannot.
