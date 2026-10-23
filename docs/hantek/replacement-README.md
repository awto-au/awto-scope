# V07 scope — replacement RTL

Clean, fully-simulatable reimplementations of the 2D82AUTO scope FPGA
(LCMXO2-1200, bitstream V07), reconstructed from the **pluribus static
recovery** plus the traced protocol docs.

## Why reimplement

The pluribus recovery gives a *structural* netlist bounded at the hard-IP
edge: the fabric logic and EBR contents are recovered, but the off-fabric
control (EBR read sequencing, EFB SPI internals, PLL) and half the ADC bus
are not in the bitstream. That netlist is a **spec, not a runnable design**.

A replacement turns the recovery + docs into a real design that *runs*: it
supplies the off-fabric control the bitstream never carried, and has no ghost
nets, no port aliasing, no decoder gaps.

## Spec provenance

| Source (awto-2000/fpga/docs) | What it fixes |
|---|---|
| `v07-fabric-datapath.md`  | ADC → 7×EBR ring → DAC; the 3 clock rails; I/O map |
| `v07-ebr-contents.md`     | the six self-test ramp blocks (decimation ladder) |
| `fpga-spi.md`             | MCU↔FPGA SPI command / register map |

## Candidate ladder

All checked against the same oracle (recovered structure + datapath co-sim):

- **CAND-A — faithful mirror** (`aw2_scope.v`) — closest to the recovered
  silicon datapath: the six ramp EBRs as a decimation ladder, ADC capture
  writes, and the read sequencer *supplied* (the off-fabric part). Validates
  the recovery. **← current**
- **CAND-B — FF-generated ramp** (planned) — drop the 6.8 KB static prefill;
  roll the self-test from a counter/LFSR at the capture point (see
  `v07-ebr-contents.md` design note). Smaller bitstream, same behaviour.
- **CAND-C — behavioral-first** (planned) — clean datapath + explicit SPI FSM,
  ignoring tile placement. Most readable / portable.

## CAND-A design (`aw2_scope.v`)

- `ramp_ebr` — sync dual-port RAM preloaded with a digit-doubled ramp,
  **generated** from `(DWELL, LEVELS, STEP)` — the exact recovered content,
  written as intent instead of 1024 opaque words.
- `awg_ebr` — blank RAM, zero-init (MachXO2 EBR power-up = 0), SPI-loaded.
- `spi_cmd_slave` — mode-0 SPI slave; decodes `fpga-spi.md` cmd `0x50` and
  streams the AWG table into R6C20.
- `dds` — phase accumulator addressing the AWG table.
- `aw2_scope` — ties it together: ADC DDR capture → ring, a **read sequencer**
  (read-address counter + enable, the part the bitstream lacked), the AWG/DDS
  path, and a DAC output mux (scope playback vs AWG).

## Simulate (pluribus is the harness)

```
python3 <workspace>/pluribus/scripts/sim_replacement.py \
    --rtl aw2_scope.v --tb aw2_scope_tb.v --top aw2_scope_tb
```

Defaults already point at CAND-A. Log + verdict → `pluribus/tmp/sim_replacement.log`.

### Status — both self-checks PASS

- **Scope playback**: DAC streams `00 11 22 33 44 55 66 77` (R6C7 4-bit ramp,
  dwell 2, digit-doubled) — reproduces `v07-ebr-contents.md` §1.
- **AWG load**: SPI `0x50` + 18-bit words → R6C20 → DDS → DAC output defined
  and following the loaded table.

## Not yet modelled (next)

- The DDR ADC front-end phase detail (ENCA/ENCB timing) is simplified to a
  single `enc` mux.
- The `0xA4` capture-readout SPI command (host reads the ring back).
- The U1/U7 AFE control shift registers (74HC595 gain/coupling).
- Co-sim **diff** vs the recovered netlist (Oracle A/B) — currently CAND-A is
  self-checked; the direct recovered-vs-replacement comparison is the next step.
