# Pluribus netlist recovery — what it does and doesn't resolve (2026-07-14)

Findings from a cross-bitstream run of the pluribus engine over the three
Hantek FPGA firmware dumps (V02, V4, V07).  Board/RE knowledge — lives
here, not in the generic pluribus repo.

## Pinout is corroborated across all three firmwares

All three firmware dumps of the same board were unpacked and loaded, then
the pad tables diffed (`pluribus: scripts/compare_pads.py`):

- **45 of 45 configured pads have identical direction in all three.**
- **Every one of the 16 ADC data pins is stitched to logic in at least
  one firmware; no pad is stranded in all three.**

So the supplied `aw2-pins.tsv` is confirmed end to end: all 16 ADC data
lines (D0A–D7A, D0B–D7B), both encode clocks (ENCA pin 65, ENCB pin 98)
and the DAC bus are real, live and used.  The earlier "unused ADC
channel" guess is dead — a pin live in V4 but not V07, on the same board,
can only be a tooling gap, never an unused channel.

V2 had no committed `.config`; it was unpacked from
`fpga/v2/DS1302_impl1(8)_V02.bin` (333 frames, 12326 arcs) into pluribus
`tmp/`.  Nothing here was modified by that run.

### Per-pin stitch status

`+fan` = pad's net drives logic in the recovered netlist.  `NOFAN` = the
recovered netlist strands it — a *pluribus lifter gap*, not a board fact.

| pin | signal   | V07   | V02   | V4    |
|-----|----------|-------|-------|-------|
| 35  | DAC_PD   | +fan  | NOFAN | +fan  |
| 66  | ADC_D7A  | +fan  | NOFAN | +fan  |
| 67  | ADC_D6A  | +fan  | +fan  | +fan  |
| 68  | ADC_D5A  | +fan  | +fan  | +fan  |
| 69  | ADC_D4A  | +fan  | +fan  | NOFAN |
| 70  | ADC_D3A  | +fan  | +fan  | +fan  |
| 71  | ADC_D2A  | NOFAN | NOFAN | +fan  |
| 74  | ADC_D1A  | +fan  | NOFAN | +fan  |
| 75  | ADC_D0A  | +fan  | +fan  | NOFAN |
| 83  | ADC_D0B  | +fan  | NOFAN | +fan  |
| 84  | ADC_D1B  | +fan  | +fan  | NOFAN |
| 85  | ADC_D2B  | +fan  | NOFAN | NOFAN |
| 86  | ADC_D3B  | NOFAN | +fan  | NOFAN |
| 87  | ADC_D4B  | +fan  | +fan  | +fan  |
| 88  | ADC_D5B  | +fan  | +fan  | +fan  |
| 96  | ADC_D6B  | +fan  | +fan  | NOFAN |
| 97  | ADC_D7B  | NOFAN | +fan  | +fan  |

All other pads agree across all three with no stranding.  Remaining
`NOFAN`s are tracked as pluribus defects (`pluribus:
docs/pad-fanout-gap.md`), not board unknowns.

## A major pluribus bug was found and fixed while doing this

An inverted `REG.SD` polarity in the MachXO2 lifter meant **V07 was
recovering 1081 of its 1090 flip-flops with a constant D input.** Every
FF/LUT count, net name and LUT INIT still looked correct, so it went
unnoticed.

**Any pluribus reachability or netlist output produced from these
bitstreams before 2026-07-14 was computed on a netlist with almost no
sequential connectivity, and should be regarded as void.** Re-run.

Post-fix FF D-input recovery (`pluribus: scripts/ffd_stats.py`):

| firmware | LUT-fed | FF-fed | routed | const |
|----------|---------|--------|--------|-------|
| V07      | 409     | 345    | 301    | 35    |
| V02      | 437     | 385    | 267    | 19    |
| V4       | 405     | 436    | 241    | 36    |

## What pluribus does NOT resolve: the SPI register bank / REG 0x05 ident

pluribus recovers the full gate-level netlist — 1090 FFs, 1138 LUTs
*with* their INIT Boolean functions, all routing — but it does **not**
statically recover the 8-byte REG 0x05 identification value, and this
section records exactly why, after three rounds of investigation
(2026-07-14/15) walked back two earlier over-confident readings.

### The register interface is soft fabric logic, not EFB hard IP

The OEM `[0xa0][reg][0xa0]` register protocol is implemented as a **soft
fabric SPI decoder** — a ~50-FF shift register whose MOSI/CLK/CS enter
via the config corner (pin 27 `CSSPIN` shows `conn=fabric fan=0 dead` —
invisible to normal-PIO netlist recovery), per
`fpga-aw2/docs/fpga-replacement-design.md`.  The MachXO2 EFB's WISHBONE
is enabled (`PLL_USE_WB`) but is wired to the PLL config trim.  So the
register/ident logic is in the fabric, not behind the EFB hardened
silicon — an early analysis that framed the ident as riding a clean
"EFB WISHBONE JWBDATI bus" was reading **ambiguous pluribus labels**: in
the recovered netlist every `JWBDATO{n}` net is the *same* net as a JTAG
port (`JWBDATO6`=`JF6`, `JWBDATO4`=`JSHIFTDR`, `JWBDATO2`=`JUPDATE`,
`JWBDATO0`=`JTCK`, …).  The register-write outputs (pin 62 U1_DS, the DAC
data pins) do funnel back through those EFB-output nets, so the EFB *is*
on the register datapath — but pluribus cannot say in what role, because
prjtrellis does not fully decode the EFB fixed-connections.

### The ident is NOT recoverable from the recovered constants

An early probe found 52 pure-constant LUTs and 35 constant-D FFs and
guessed the ident was "hardcoded fabric constants, recoverable by
correlation."  That is now **refuted three independent ways**:

1. Those constant nets are not within reach of the readback datapath
   (BFS from the byte-select register finds none of them).
2. The readback is a **time-multiplexed FSM output** — 8 bytes streamed
   sequentially over one bus — not 64 static nets to read off.
3. **Decisive cross-bitstream test:** the constant structure is
   *version-invariant*.  V02, V4 and V07 each have exactly 52 constant
   LUTs, the same 2 distinct INIT values, and **no** INIT value that
   varies across the three builds.  Since the version byte differs
   (V02→2, V4→4, V07→7), the constants provably cannot encode it.  The
   ident-generating logic is therefore in the part pluribus does not
   capture — the config-corner soft-decoder front-end.

### Byte interpretation — corrected

- byte[6] = `7` = version **V07**: confirmed (firmware, USB probe, on-disk
  bitstream).  This is the only established byte.
- bytes [0..3] = `2,0,1,9`: **NOT** an established "2019" build year.  The
  firmware-side #179 note records testing these against real build
  strings and bitstream dates with no convincing match, and the reading
  is internally inconsistent anyway — bytes [4]=19 and [5]=49 exceed a
  single decimal digit, so a uniform date encoding cannot hold.  Report
  as pattern-suggestive only.
- bytes [4],[5],[7] = `19,49,133`: no code-level or fabric meaning found.

### Bottom line

Static decode of REG 0x05 is blocked by two compounding gaps, neither of
which is "just label the constants": (a) the soft SPI-decoder front-end
enters via config-corner routing that normal-PIO netlist recovery does
not trace, and (b) pluribus's EFB fixed-connection model is too ambiguous
(JWBDATO aliased with JTAG) to trust which wire carries what.  Closing it
needs lifter work on the config-corner path and disambiguated EFB
modelling — or a live read (JTAG/SSPI), which is separately blocked.
