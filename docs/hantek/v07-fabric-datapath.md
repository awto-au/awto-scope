# V07 fabric datapath — SOUND findings (decoder-0x72 safe)

Findings recovered purely from the **fabric** decode (LUT/FF/routing/PIO), which
prjtrellis reads correctly before it truncates V07 at bitstream command `0x72`.
Nothing here depends on EBR *contents*, EFB internals, or feature-row/sysCONFIG
bits — those remain BLOCKED (see the bottom section and
`docs/prjtrellis-fix-plan.md`).

Reproduce: `python3 scripts/trace_datapath.py --bitstream V07`
(log: `tmp/trace_datapath_V07.log`). All net/cell IDs below are from the DB load
of 2026-07-15 (net numbering is **not** stable across loads — always re-derive;
do not cross-reference net numbers from older `pins.tsv`/docs).

Method note: forward `reachability` was never populated for V07, but
`reachability_rev(dst, src)` (1.26 M rows) stores `src→dst` edges and therefore
answers **both** directions — fan-in cone = rows `WHERE dst=X`; forward reach =
rows `WHERE src=X`. The tracer uses this; the empty forward table blocks nothing.

## 1. The 38 "clock domains" are 3 physical clock-spine tracks

`report.py` lists 38 clock domains; `clock_domain_summary.hpbx_track` collapses
them (132 spine-net aliases) to **3 active center-clock tracks**:

| track           | peak FFs | role (from pad/EBR wiring, below)              |
|-----------------|----------|-----------------------------------------------|
| BRANCH_HPBX0200 | 90       | secondary processing / part of EBR write path |
| BRANCH_HPBX0000 | 67       | EBR write/capture clock + AFE control (U1 CH1)|
| BRANCH_HPBX0100 | 30       | DAC output register bank (playback/output)    |

(A 4th track BRANCH_HPBX0300 carries a net (`n2523`) but clocks 0 FFs.)
The many `n####` "clock domains" in the report are per-column branch taps of
these 3 tracks, not distinct clocks.

## 2. Output-pad drivers, clock-resolved

Each output pad's driver net resolved to its producing FF/LUT and clock track
(`net_fanout` PAD row → `ffs.q`/`luts.z` → `clk` → track):

- **DAC data bus, 8 of 12 bits are fabric FFs on HPBX0100** (the DAC output
  register bank): D1=`ff_r8c16_A1`, D3=`ff_r8c16_B1` (clk `n2382`); D4=`ff_r7c18_A0`,
  D5=`ff_r7c18_A1`, D8=`ff_r8c19_A0`, D9=`ff_r8c19_A1`, D10=`ff_r8c19_B0`,
  D11=`ff_r8c19_B1` (clk `n2393`).
- **DAC_D0/D2/D6/D7 have no recoverable fabric driver** — driver nets
  `n2673/n2672/n2675/n2676` terminate at an unresolved routing wire (e.g. `n2672`
  source `V02N0101` @ r11c10 has no upstream arc). Fabric routing-recovery gap,
  same class as the input-pad gap — **NOT 0x72**. Corroborates
  `awto-2000/fpga/docs/fpga-v07-unknown-bits.md` §"DAC_D0/D2/D6/D7 missing-driver"
  (CIB F24–F27, pluribus#29 item 3) via an independent method (arc-chain
  termination).
- **U1_SHCP / U7_SHCP are LUT-driven** (`lut_r8c15_Bk1` / `_Bk0`) — the AFE
  shift-register clocks are combinational, not registered.

## 3. Output-pad verdicts for the unknown pins

Two methods agree on the unknown output pads: per-pad FF/clock/driver resolution
(`net_fanout` PAD → `ffs.q`/`luts.z` → `clk` → track) and an **upstream cone-size
fingerprint** — ranking each output pad by its `reachability.dst` count groups pads
by the datapath that generates them. Reproduce the ranking: order `pad_map` output
pads by `(select count(*) from reachability where dst=net)`.

| pin | cone | verdict |
|-----|------|---------|
| DAC_D0–D5, D8–D11 (10 pads) | **507** | the DAC output word |
| **pin60** | **507** | DAC-A-domain fabric output (identical cone to the DAC word) |
| **pin99** | **507** | DAC-B-phase fabric output (same cone, different phase) |
| U1_DS/SHCP, U7_SHCP/STCP | 507–548 | AFE shift registers (CH1/CH2 gain) |
| **pin3** | 3 | minor status/control output (cf. XTAL_FB cone=2) — not datapath |
| **pin61** | — | clock net (PLL-driven, fanin=0) |
| **pin4** | 0 | orphan — edge-CIB missing-driver group (with DAC_D6/D7, U7_DS) |

- **pin60** — driver FF `ff_r7c19_B1` on **clk_17** (HPBX0100), physically adjacent
  to DAC_D4's FF (r7c18) and sharing its upstream logic (n1998/n2271/n2627) → in the
  **DAC-A data register bank**. ADC inputs reach pad_60 at the same hop counts as
  DAC_D9/10/11, and pad_60 appears in every EBR read-port→output-pad set alongside
  the confirmed DAC bits (§4). It is **NOT a DAC data bit** (see the 2-bit-DAC result
  below); leading reading = **AWG output-amp enable/mute**, gated by SPI REG 0x02
  bit6 `AWG_ENABLE` (`board.md` documents an "AWG enable / EL5166 output-amp enable"
  on this chain). Board trace needed to settle the destination.
- **pin99** — driver FF `ff_r3c11_B1` on **clk_dac_data_b**, a **different
  clock/phase (DAC-B)** and tile region from pin60. Same 507-net DAC-domain cone but
  a **distinct signal** from pin60. Board trace needed.
- **pin61** — a **clock-output tap on the shared PLL clock net, not a short**. Its
  driver is a hard-IP/spine net (no fabric FF/LUT driver, ~22 arcs) that fans out to
  five output pads: DAC_PD(35), DAC_CLK(54), **pin61**, ADC_ENCA(65), ADC_ENCB(98).
  The encode/sample clock is a spine-net family distributed to these pads via A+B PIO
  pins (DDR/differential); pin61 taps the same phase as DAC_PD/ENCB. This is the
  **CLKOS = 125 MHz** net (see [fpga.md](fpga.md) §PLL). The fanout is deliberate —
  only "which board net pin61 clocks" remains open (board trace).
- **pin3** — driver FF `ff_r7c2_A0` on HPBX0100; tiny fan-in cone (3–4 nets), no ADC
  input reaches it → a low-fanin status/control output, independent of the ADC/DAC
  datapath.
- **pin4** — orphan (fanin=0): the driver is not in the recovered fabric cone.
  Grouped with DAC_D6/D7 and U7_DS in the bottom-edge CIB decode gap — a **decoder
  tooling gap** (pluribus#29 item 3), not a design fact. Resolves with the lifter fix
  + a board trace.

### The "2-bit DAC" hypothesis is ruled out (firmware sweep, 2026-07-18)

pin60 and pin99 are both DAC-domain outputs in different A/B phases, but they are
**NOT one 2-bit DAC pair** and not proven to be two of the twelve DAC_D bits. A full
sweep of both decompiled firmware builds (all 41 FPGA-SPI call sites + the complete
register map) found **no OEM routine that writes 2 bits to any DAC**: the voffset DAC
is 16-bit (REG 0x0a/0x0c), AWG amplitude is 16-bit (REG 0x1c), the waveform table is
8-bit (cmd 0x50). The only 2-bit register in the whole system is our own spy-bitstream
GPIO. So pin60/pin99 are FPGA-internal fabric outputs the MCU never bit-addresses —
consistent with pin60 = AWG output-amp enable and pin99 = a separate DAC-B-phase
control. A voffset-DAC PWM output remains possible (there is **no offset-DAC chip in
the BOM**, so the FPGA must generate the offset voltage itself) but is unconfirmed.

## 4. ADC → 7×EBR → DAC macro datapath (structural)

All 7 row-6 EBR blocks (R6C1/4/7/10/14/17/20) receive the **same 8 live ADC bits**
at their write ports; **6 of 7** have read ports that reach the DAC output bus
(pad_37/39/40/41/45/51/52/53) **+ pad_60** (corroborating §3) **+ pad_62/63**
(U1_DS/U1_SHCP, the CH1 AFE control shift register). EBR clocks sit on HPBX0000
(+HPBX0200); the DAC output stage is HPBX0100. This is the sample-capture /
waveform-buffer memory sitting between ADC and DAC/AFE.

- **R6C20 is the outlier**: its read ports reach **no** output pad, and it is the
  only EBR whose clocks include HPBX0100 — a different-purpose RAM (internal
  coefficient/AWG buffer, consumer not a directly-traced output).

### 4a. Read datapath validated in simulation (recovered prefill streams out)

The recovered netlist emits as structural Verilog (pluribus `verilog.py`) that
compiles + runs in iverilog. Each EBR is modelled as a behavioural sync-RAM
carrying its **real** recovered init (native 0x72 decode) with the recovered
read address `ebr_<blk>_dout <= mem[{routed read-addr bits}]`. Simulating this:

- **The read *address* is partly off-fabric.** Some read-address bits are fabric
  FF counters, but others are ghost nets and the block addresses are
  **cross-coupled through the hard-IP edge** — e.g. R6C7's read address includes
  `ebr_main_read_data_3 = ebr_r6c1_dout[3]` (one block's *output* is another
  block's *address* bit). The read-enable/counter sequencing is a ghost. So in
  closed-loop sim the read index resolves to X and no block self-streams.
- **Force a clean address → the recovered prefill streams out.** Driving a plain
  incrementing address onto each block's routed read-address leaf nets
  (pluribus `verilog.py --sim-ebr-sweep`; iverilog `force`, re-applied per cycle)
  reproduces the §1 self-test ramp of `v07-ebr-contents.md` through the emitted
  Verilog: **R6C7** → `00 11 22 33 44 55 66 77` (4-bit ramp, n·0x11), **R6C1** →
  `00 55 aa ff` (2-bit levels 0/1/2/3). This confirms the recovered EBR contents
  **and** read-port model are correct — the sample-buffer read path works.
- **Boundary corollary:** the EBR read-address *sequencing* (counter enable +
  cross-block ordering) lives in the hard IP / SPI-loaded control, not the
  fabric — the same edge the whole recovery is bounded at. A closed-loop "DAC
  plays the ramp" sim needs a behavioural read-address/enable driver (or the
  EFB-SPI model), i.e. modelling the hard IP, not more fabric recovery.

## 5. ADC input bus is only half-recovered (edge-CIB gap, not 0x72)

Of 16 ADC data pins, **8 trace into fabric** (≈1750 reachable nets, 11 output
pads each): ch A D0A,D1A,D3A,D4A,D5A,D7A (6/8) and ch B D1B,D5B (2/8). The other
8 have ≤10 reachable nets (dead in fabric): ch A D2A,D6A; ch B D0B,D2B,D3B,D4B,
D6B,D7B. The dead bits are edge-correlated — **top-edge ch B (row 0) is largely
unrecovered (6/8 dead)**, right-edge ch A (col 21) mostly recovered. This is the
known edge-CIB routing gap (F24–F27 / lifter #57), a **fabric-decoder gap, not a
design fact and not 0x72**. FPGA_nCS (pin27) — annotated as "#57 blocks fabric
trace" — is in fact fully connected in this decode (1747 nets, 11 output pads);
that annotation is stale.

## Remaining FABRIC unknowns (resolvable without 0x72)
- **Clock-spine source not globalised**: all 132 `clock_domain_summary` rows have
  NULL `pll_output`/`freq_mhz`. The 3 tracks are not yet bound in the netlist to
  their PLL-output / primary-clock-input source (PCLKT global mux routing not
  captured). The **frequencies themselves are known** (CLKOP=250, CLKOS=125,
  CLKOS2=62.5 MHz — [fpga.md](fpga.md) §PLL); only the track→output *binding* is the
  remaining fabric gap.
- 8 ADC input bits + DAC_D0/D2/D6/D7 drivers: unrecovered edge-CIB routing (#57 /
  F24–F27, pluribus#29 item 3). Fabric — fixable in the lifter/DB, independent of 0x72.
- pin60, pin99, pin61 board destinations: fabric role resolved (§3), but which
  physical board net each drives still needs a board trace. pin4 is a spine-driven
  edge-CIB orphan (§3).

## Threads BLOCKED on the 0x72 decoder fix (what re-decoding unlocks)
> **UPDATE 2026-07-16 — fence LIFTED.** The native decoder recovers the 0x72
> contents. EBR init + EFB config are now decoded; see
> [`v07-ebr-contents.md`](v07-ebr-contents.md). Summary: the six initialized
> row-6 EBRs hold **full-scale staircase ramp self-test patterns on a decimation
> ladder** (working buffers, not fixed ROMs); R6C20 is the only *wide* (18-bit)
> block and is zero-init. EFB is **SPI-only with the stock-default preload** →
> the SPI ident is NOT an EFB register preload (it is fabric/runtime). USERCODE=0,
> no SECURITY command. The port-width evidence adds a caveat to §4 (the six ramp
> blocks have narrow 2/4-bit read ports) — see that doc §1 "new nuance".

- **EBR contents** — ADC capture / AWG waveform memory init (`.bram_init`). Port
  *wiring* is recovered (§4); *contents* are dropped after 0x72.  **[RECOVERED]**
- **EFB internals + SPI-ident path** — all 17 EFB ports show 0 fabric reach
  (hard-IP only); the ident/config datapath through the EFB is not in fabric.
  **[EFB config RECOVERED (default SPI); fabric ident path still open]**
- **Clock-track → PLL-output binding** — the frequencies are known (CLKOP=250,
  CLKOS=125, CLKOS2=62.5 MHz; encode/sample clock = CLKOS 125 MHz, ENCA/ENCB 180°
  out of phase → 250 MSPS), but binding each fabric spine track to its named PLL
  output is not in the netlist (see "Remaining FABRIC unknowns" above).
- **Feature-row / sysCONFIG** — boot/SPI-slave settings.
- **EBR mode enums** (F1B33/F1B34 vs F1B35, DP8KC↔PDPW8KC) — prjtrellis DB gap
  (pluribus#29 items 1–2), orthogonal to but entangled with the 0x72 truncation.
