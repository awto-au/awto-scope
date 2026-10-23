# V07 unknown bits — board-specific interpretation

The V07 bitstream (LCMXO2-1200HC, TQFP100) has **157 bits** that
`ecpunpack` / prjtrellis cannot name (`unknown: FxByz` lines in the
`.config`).  None indicate a V07-specific error — every one traces to a
**generic prjtrellis MachXO2 database gap**, which is tracked (with fix
paths and diamond-fuzz evidence) in **pluribus#29** ("prjtrellis MachXO2
DB gaps").  This doc keeps only the *V07-specific* interpretation; see
pluribus#29 for the generic root causes and how to fix the tooling.

## V07 unknown-bit breakdown

```
Tile type            Count  Generic cause (see pluribus#29)
───────────────────  ─────  ─────────────────────────────────────────────
CIB_PIC_B_DUMMY        42   IO config in frames F24–F27 — never fuzzed
EBR1                   36   EBR.MODE bits: prjtrellis DB has wrong address
CIB_EBR1               21   EBR.MODE same bug (CIB mirrors the EBR tile)
CIB_EBR0               18   EBR.MODE same bug
CIB_EBR2               18   EBR.MODE same bug
CIB_PIC_B0             15   IO config bleed — same F24–F27 gap
CIB_EBR_DUMMY           3   EBR.MODE same bug (DUMMY mirror)
PIC_B0                  3   IO F24–F27 gap
CIB_CFG2                1   sysConfig / boot mode; uncatalogued (harmless)
───────────────────  ─────
TOTAL                 157
```

## EBR — mode bits and the prjtrellis DB gap

V07's seven row-6 EBRs are a **mix of five DP8KC + two PDPW8KC** (per-block
modes and contents in [v07-ebr-contents.md](v07-ebr-contents.md) §1, recovered
from the native 0x72 decode).  prjtrellis cannot reproduce that split on its
own: its `EBR1/bits.db` has the **wrong mode-bit address**, and DP8KC and
PDPW8KC set an *identical* bit set `{F1B33, F1B34}` with no distinguishing bit,
so the DB genuinely cannot tell the two apart (pluribus#29 items 1–2).  The
separate read/write clock ports fit the board's clock architecture — the encode
clock (ADC_ENCA/ENCB, ~125 MHz CLKOS) drives the write side, a slower SPI read
clock the read side.  DATA_WIDTH_R/W decodes from the `EBR.PDPW8KC.*` named
enums that are correct.

Per-EBR mode bits actually set in the V07 `.config` (the generic
`F1B35`→`F1B33/F1B34` + `F1B32` third-bit issue is pluribus#29 items 1–2):

| EBR tile   | Mode bits set        | Decodes after the F1B33/F1B34 patch? |
|------------|----------------------|--------------------------------------|
| EBR_R6C11  | F1B33 + F1B34        | PDPW8KC ✓                            |
| EBR_R6C5   | F1B34 only           | still unknown (needs F1B32 decode)   |
| EBR_R6C15  | F1B32 + F1B33 + F1B34| still unknown                        |
| EBR_R6C2   | F1B32 + F1B33        | still unknown                        |
| EBR_R6C8   | F1B32 + F1B34        | still unknown                        |
| EBR_R6C21  | F1B32 only           | still unknown                        |

Until pluribus#29 item 2 is fuzzed, 5 of 6 V07 EBRs keep emitting
spurious `unknown:` mode lines.

## The DAC_D0/D2/D6/D7 missing-driver symptom (board impact)

Four DAC output pads — **DAC_D0/D2/D6/D7, package pins 36/38/42/43**,
bottom IO row R11 — have no fabric-side net resolved in `pad_map`.  A
fabric net (e.g. `n2501` for DAC_D0, `n2520`, …) reaches the IO buffer's
JA2 input in the arc table, but the chain terminates there:

```
R11C10  V02N0201 → H00R0000   (both n2501)
R11C10  H00R0000 → JA2        (both n2501)
```

The arc feeding `V02N0201` from the fabric is the bottom-edge CIB routing
mux, whose config lives in the un-fuzzed F24–F27 frames (pluribus#29
item 3).  So `report.py` shows DAC_D0/D2/D6/D7 with no recoverable fabric
driver — a **tooling gap, not a board fact**.  This is the FPGA-side of
awto-2000 issue #6; the fabric driver will resolve once pluribus#29 item 3
lands (final chip-pin confirmation is still a board trace).

## Bonded-but-unused bottom pads (harmless)

The CIB_PIC_B_DUMMY / CIB_PIC_B0 F24–F27 unknowns and the PB6/PB20
`F4B39`/`F5B39` bits are Diamond configuring the electrical state of
unbonded or bonded-but-unused bottom-edge IO cells (FAILSAFE receiver,
signal-integrity settings for MIPI/SSTL25 neighbours).  Functionally
harmless for V07 — those pads carry no signal.  The missing prjtrellis
enums are pluribus#29 items 3–4.

## Provenance

Extracted 2026-07-15 from the former `pluribus:docs/unknown-bits-analysis.md`
during a docs cleanup: the generic prjtrellis defects moved to pluribus#29
(the engine repo stays board-agnostic), the V07 interpretation lives here.
