# FPGA scope architectures — research for the MachXO2 redesign

Design-lessons survey of reference open FPGA scopes, to guide a clean-room
reimplementation of the 2D82AUTO scope FPGA (MachXO2-1200, AD9288 dual 8-bit
ADC ≤250 MSPS interleaved, 12-bit DAC, STM32 host over SPI). Sources read at
the RTL/source level (not marketing); repos cited inline.

## TL;DR — the one decision that dictates everything

**Host-link bandwidth decides the whole architecture.** The two reference
platforms sit at opposite poles:

| | ThunderScope | Red Pitaya | **Us (MachXO2)** |
|---|---|---|---|
| Host link | PCIe ×4 (~1 GB/s) | Zynq PS + DDR | **SPI (~1–6 MB/s)** |
| FPGA role | dumb data-mover | capture + reduce | **capture + reduce** |
| Trigger | host software | **in FPGA** | **in FPGA** |
| Deep memory | host RAM | DMA to DDR | **on-chip EBR only** |
| Decimation | host (unused on-FPGA) | **in FPGA** | **in FPGA** |

ThunderScope streams *every* sample to the PC and does all scope work there —
its FPGA is a trivial one-bit-trigger data mover on a tiny Artix-7 A35T. That
simplicity is **purchased with ~1 GB/s of bandwidth**. We have ~1–6 MB/s over
SPI, so **every one of those simplifications inverts**: trigger, capture
memory, and decimation all have to live *in* our FPGA. That is the Red Pitaya
model, and Red Pitaya is our blueprint.

The OEM does none of this on-chip (see `waveform-readback.md`): its FPGA is a
dumb raw ring buffer and the STM32 does cal + naive windowed subsampling in
software. So the redesign's whole value is moving the smart parts on-chip.

---

## 1. ThunderScope (`github.com/EEVengers/…`) — the anti-pattern for us

4ch/1 GSPS, HMCAD ADC → Artix-7 → PCIe→Thunderbolt → PC. The data plane is
literally `adc.source → stream.Gate → pcie_dma0.sink` (`thunderscope_litex/
thunderscope.py`) — no capture RAM, no on-chip trigger memory. "Deep memory" is
host RAM. The on-FPGA `Trigger` is a **single enable bit**; real triggering is
host software (`TS.NET/…/ProcessingThread.cs`, classes RisingEdge / FallingEdge
/ AnyEdge / Burst). They even wrote an on-FPGA decimator (`peripherals/
downsampling.py`) and **left it un-instantiated** — streaming raw was cheaper.

**What we take:** (a) the **trigger taxonomy** — rising / falling / any-edge /
burst — implement the first few in HDL. (b) **calibration as a TLV/JSON blob in
flash, applied host-side** (`libtslitex/doc/calibration_data.md`) — cheap, no
FPGA cost. (c) keep AFE gain/coupling/offset **on the host**, FPGA is a
register-file slave + capture engine.

**What does NOT translate:** the entire stream-to-host data plane, LitePCIe/
`S7PCIEPHY` DMA, the LVDS deserialization stack, host-only triggering, and the
no-deep-memory premise. Their decimator *drops samples (aliases)* — we upgrade
to min/max envelope. Sizing note: their production FPGA is a tiny A35T precisely
because a data-mover needs no logic; **we do more relative work**, so the
`-1200 may be too small` once trigger + buffer + decimator land.

---

## 2. Red Pitaya (`github.com/RedPitaya/RedPitaya-FPGA`) — the blueprint

Zynq-7010, dual 125 MSPS 14-bit ADC. Classic scope `rtl/rtl/classic/
red_pitaya_scope.v`; datapath in one line:

```
ADC → DFILT (equaliser) → AVG&DEC (decimate) → BUF (circular BRAM) → SW
ext trig ─────────────────────────────────→ TRIG → write-pointer/delay
```

Reusable blocks (confirmed from RTL):

- **Circular capture buffer + write-pointer + post-trigger delay.** Fast buffer
  is only **16384 samples/ch** even with a whole Zynq + DDR (`rp.h
  ADC_BUFFER_SIZE=16*1024`). "Deep memory" = DMA to DDR, which we lack. So our
  ~8 KB EBR (~4k samples/ch) is in the same ballpark for the *live record*;
  depth comes from decimation + compression, not RAM. Latch `wp_trig` at
  trigger, keep filling `set_dly` post-trigger samples, freeze; pre-trigger is
  free from the ring.
- **Accumulate-and-dump averager — adder + shift, NO multiplier**
  (`rp_decim.v`, `scope_dec_avg.sv`): `sum += x; every N: out = sum >> log2 N`,
  **power-of-two factors only** (they refuse to build a divider for others). It
  is the anti-alias filter *and* the ENOB booster. `hres_en` left-shifts before
  summing to **keep** the gained bits → 8-bit AD9288 → ~12 ENOB at 256×.
- **Trigger = Schmitt comparator with hysteresis** (`tresh ± hyst`), edge-
  detected, source-muxed (A/B/ext/gen ×rise/fall), with **latency alignment**
  (`dat_dly`) so the stored trigger index matches the analog event after the
  pipeline. External trigger: 3-FF sync + pos/neg debounce (~0.5 ms).
- **Flat register-file control**: arm/reset, trig source, threshold, hyst,
  decimation, avg-enable, `wp_cur`/`wp_trig`, BRAM read window. Host: write ARM
  → poll status → read `wp_trig` → burst-read window. **This is our SPI-v2
  register semantics.**
- **Calibration**: per-channel **per-range** gain+offset in EEPROM,
  `sat((x-offset)×gain)` (`osc_calib.v`). **Skip their DSP equaliser**
  (`dfilt1.sv`, `use_dsp="yes"`) — MachXO2 has no DSP and an 8-bit/100 MHz front
  end rarely needs it.

---

## 3. Debug-over-SPI + JTAG-over-SPI (Lattice TNs + repos)

For the SPI-v2 interface (readout / temp / ID / pins / JTAG):

- **MachXO2 EFB** gives a **WISHBONE window into config logic** (USERCODE,
  TraceID, sysCONFIG status), user SPI slave, user I2C, timer — but **no JTAG
  master** (TN1246 / FPGA-TN-02163). USERCODE / chip-ID readout goes over
  EFB→WISHBONE, not JTAG.
- **MachXO2 has NO on-die temperature sensor** (absent throughout TN1246;
  contrast ECP5's hardened DTR). A die-temp readout needs an **external I2C temp
  sensor** — the SPI-v2 temp register is fed from that, not from the FPGA. (A
  future ECP5-based board would have it built in.)
- **JTAG-over-SPI is feasible only to an EXTERNAL target.** MachXO2 SPI slave →
  soft command FSM → 4 user GPIO = `TCK/TMS/TDI/TDO`, ~200 LUT (cf.
  `ben-marshall/verilog-probe` ~187 LUT). Framing: MPSSE-style
  (`opcode+len+data` + a **separate TMS command** for TAP navigation) or
  OpenOCD `remote_bitbang` (1 bit/char). The MachXO2 **cannot master its own
  TAP** — fabric is only an ER1/ER2 *register target* (JTAGF primitive, what
  Lattice Reveal uses); self-config/scan goes through **sysCONFIG SSPI**, a
  different mechanism.
- Protocol patterns: a uniform `CMD / ADDR / burst` frame with auto-increment
  and a status byte (Red Pitaya register semantics; `bugblat/fan`,
  `LucAce/MachXO2-SpiSlaveTestBuild` for MachXO2 SPI-slave register maps).

See `fpga-spi-v2.md` for the resulting protocol spec.

---

## 4. Synthesised design decisions for our redesign

1. **Architecture = capture-and-decimate on-chip** (Red Pitaya model), NOT
   stream-to-host (ThunderScope needs PCIe we don't have).
2. **Circular EBR buffer per channel** + write-pointer + post-trigger delay;
   pre-trigger free. Freeze on trigger, read out slow over SPI-v2.
3. **Trigger in FPGA**: Schmitt + hysteresis + edge, taxonomy rising/falling/
   any/burst, latency-aligned to the decimation pipeline; sync+debounce ext-trig.
4. **Decimation in FPGA**: power-of-two accumulate-and-dump (adder+shift, no
   multiplier) for anti-alias + ENOB (high-res mode → ~12-bit); **min/max
   envelope** decimation for the display path (our upgrade over RP's aliasing
   drop-decimator — see `explore/display_path.py`).
5. **Calibration off-FPGA**, per-channel/per-range gain+offset (STM32 flash or
   EEPROM). Optional offset-subtract in FPGA (adder only). No DSP equaliser.
   The per-range cal LUT (`explore/wave_compress.py`) can correct interleave
   mismatch *before* on-chip compression.
6. **SPI-v2 flat register/readout interface** (`fpga-spi-v2.md`): arm/status/
   readout + temp (ext sensor) + chip-ID (EFB) + pin status + optional
   JTAG-over-SPI-to-external bridge.
7. **Segmented memory**: divide the ring into N segments, per-segment trigger
   re-arm, per-segment timestamp (ThunderScope's eventEngine idea) — many rapid
   events, zero dead-time.
8. **Personalities** (`explore/personalities.py`): bitstream-swap by mode
   (1ch-deep, AWG on/off, procedural vs arbitrary AWG) — SRAM-load ~19 ms.
9. **Part sizing**: budget carefully — once trigger + buffer + decimator +
   compression land, the **-1200 is likely tight**; plan for a MachXO2
   -2000U/-4000 (more EBR) or external async SRAM if records must be longer.

---

## References

- ThunderScope: `github.com/EEVengers/thunderscope_litex` (`thunderscope.py`,
  `peripherals/{trigger,downsampling,eventEngine}.py`), `EEVengers/TS.NET`
  (`ProcessingThread.cs`), `EEVengers/libtslitex` (`doc/calibration_data.md`).
- Red Pitaya: `github.com/RedPitaya/RedPitaya-FPGA`
  (`rtl/rtl/classic/red_pitaya_scope.v`, `rp_decim.v`, `scope_dec_avg.sv`,
  `prj/stream_app/.../{osc_decimator,osc_trigger,osc_calib}.v`);
  `github.com/RedPitaya/RedPitaya` (`rp-api/.../rp.h`, `rp_hw_calib.h`).
- Debug/JTAG: Lattice FPGA-TN-02155 (Prog & Config), TN1246/FPGA-TN-02163 (EFB);
  `tomverbeure/ecp5_jtag` (JTAGF/ER1/ER2), `YosysHQ/prjtrellis`
  (`libtrellis/src/Bels.cpp` JTAGF ports), `ben-marshall/verilog-probe`,
  `LucAce/MachXO2-SpiPixelController` (EFB SPI slave), MPSSE
  (`sipeed/RV-Debugger-BL702`), OpenOCD `remote_bitbang`.
