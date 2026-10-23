# Hantek 2000-series — functional signal model

Describes the end-to-end signal path as understood from RE of V07 bitstream + 2D82AUTO firmware.
For raw pin data see [fpga.md](fpga.md). For SPI register map see [fpga-spi.md](fpga-spi.md).

---

## System overview

```
Probe → BNC → AFE (relay/gain) → AD9288 ADC → FPGA fabric → EBR ring buffer
                                                                      ↓
                                              MCU ← SPI burst read ←─┘
                                                ↓
                                           LCD waveform render / USB export / measurements
```

```
MCU → SPI → FPGA DDS → R6C20 EBR → DAC bus → 3PD5651E/DAC902 → Gen-Out BNC
```

---

## Clock architecture (confirmed from V07 bitstream)

Single EHXPLLJ PLL at R0C0, driven by 25 MHz crystal (pin 1/2):

VCO = 25 MHz × CLKFB_DIV(10) × CLKOP_DIV(2) = **500 MHz** (`FEEDBK_PATH=INT_DIVD`; bitstream words store value−1).

| PLL output | Freq      | Divisor | Primary use |
|------------|-----------|---------|-------------|
| CLKOP      | **250 MHz** | ÷2    | AWG DDS clock, main ADC pipeline (HPBX0200) |
| CLKOS      | **125 MHz** | ÷4    | ADC encode clock (ENCA/ENCB) + DAC CLK |
| CLKOS2     | **62.5 MHz**| ÷8    | Secondary fabric domain |
| CLKOS3     | **250 MHz** | ÷2    | Same frequency as CLKOP |

**No CLKDIV primitive exists in the bitstream.** All 38 fabric clock domains are derived from these
4 PLL outputs via the MachXO2 clock spine — frequencies are fixed at synthesis time.

---

## Bandwidth / sample rate

**Bandwidth is NOT switched by the FPGA.** All 2D42/2D72/2D82/2C42/2C72 models use the
identical bitstream. Per-model limits are enforced by the MCU:

- The MCU writes a minimum timebase divisor to FPGA SPI REG `0x0f`
- `effective_sample_rate = 250_000_000 / (divisor + 1)`
- A higher minimum divisor = lower maximum sample rate = lower analogue bandwidth
- 2D42 = 40 MHz, 2D72 = 70 MHz, 2D82 = 80 MHz — enforced here, not in FPGA

The ADC encode clock (ADC_ENCA, pin 65) is driven directly from the PLL spine at 125 MHz
and **cannot be changed at runtime**. There is no clock MUX in the FPGA.

The `fw_apply_ch1/ch2_bandwidth_limit()` functions in firmware control an **analogue front-end
filter** (bit 2 of the channel config byte sent to FPGA REG 0x00/0x01), not an ADC clock switch.
This is a separate 20 MHz hardware BW-limit filter switchable per-channel independent of timebase.

---

## ADC capture path

AD9288 runs interleaved: ENCA and ENCB are driven 180° out of phase at 125 MHz → **250 MSPS effective**.

```
AD9288 DA0-7 (pin 66-75) → FPGA right edge (R2-4 C21) → distributed RAM → EBR ring
AD9288 DB0-7 (pin 83-97) → FPGA top edge  (R0 C9-14)  → distributed RAM → EBR ring
```

### Distributed RAM stage (confirmed from V07 bitstream + machxo2_lift.py)

Data flows through MachXO2 distributed RAM (DPRAM/RAMW slice triplets) before reaching EBR.
Each site has three adjacent slices:

- `SLICEC (MODE=RAMW)` — write port; D0/D1 are the two write-data bits
- `SLICEA (MODE=DPRAM)` — read port, bit 0; K0.F/K1.F are async read outputs
- `SLICEB (MODE=DPRAM)` — read port, bit 1; K0.F/K1.F are async read outputs

This path has no config arc or fixed_conn in prjtrellis — modelled in Pluribus as pseudo-LUTs
(`INIT=0xAAAA` pass-through). 44 pseudo-LUTs inserted at 22 RAMW sites.

**Currently traced (Pluribus V07, BFS depth=8):**
- ADC_D7A (n2783) → DPRAM → fabric (dpram_r4c11_d1k0) ✓
- ADC_D3A (n2779) → DPRAM → fabric (dpram_r4c14_d1k0) ✓
- ADC_D0A (n343) → FF/LUT fanout ✓
- ADC_D3B (n1977) → FF fanout ✓
- ADC_D4A (n2081) → FF CE inputs (clock-enable path, not data) ✓
- ADC_D6A/D5A/D2A/D1A/D0B/D1B/D2B/D4B/D5B/D6B/D7B — no fanout yet in DB

**ADC → EBR write path not yet traced** within BFS depth — likely > 8 hops through the
distributed RAM + fabric pipeline. See `V07-chains.txt` section 3.

### EBR ring buffer

6 EBR tiles, total 6144 bytes — matches firmware `g_ch1_samples`/`g_ch2_samples` exactly.

| Tile  | Mode     | Width | Clocks writing |
|-------|----------|-------|----------------|
| R6C1  | DP8KC    | 2-bit | clk_14, clk_1, clk_9 |
| R6C4  | DP8KC    | 2-bit | clk_14, clk_1, clk_9 |
| R6C7  | DP8KC    | 4-bit | clk_14, clk_7, clk_27/28/12/17/1 |
| R6C10 | DP8KC    | 2-bit | clk_14, clk_1, clk_9, clk_12 |
| R6C14 | DP8KC    | 4-bit | clk_14, clk_7, clk_27/28/1/17/12 |
| R6C17 | PDPW8KC  | 2-bit | clk_1, clk_14, clk_15, clk_12, clk_9 |

CH1 = even bytes, CH2 = odd bytes (interleaved). MCU reads via SPI cmd 0xa4.

---

## AWG path

Completely separate from ADC EBR — zero net overlap.

```
MCU → SPI (cmd 0x50 reg 0x00, 1024 bytes) → R6C20 EBR (PDPW8KC, 18-bit width)
    → DAC bus D0-D11 (pins 36-53)
    → CLK pin 54 = n1147 = 125 MHz (CLKOS)
    → 3PD5651E DAC (U12)
    → EL5166/LMH6702 output amp
    → Gen-Out BNC
```

DDS: REG 0x18 = 32-bit frequency word. `freq_Hz = word × 250_000_000 / 4_294_967_296`  
(DDS accumulator clock = 250 MHz = CLKOP; confirmed from `fw_awg_apply_freq` constants and PLL config)

R6C20 write clocks: clk_22 (n1943, 19 FFs), clk_20 (n2237, 2 FFs).
Write CE signals (n2080, n2090) have no fabric driver visible — sourced from clock spine.

---

## AFE control (shift registers)

Two 74HC595 SIPO shift registers driven by FPGA, controlling relays/gain on the analogue front-end.

| Chip | FPGA pins       | Role              |
|------|-----------------|-------------------|
| U1   | 62/63/64 DS/SHCP/STCP | CH1 AFE relay/gain |
| U7   | 8/9/10 DS/SHCP/STCP   | CH2 AFE relay/gain |

U7's DS/SHCP/STCP pin order is inferred by mirroring U1 (CH1), not individually
board-traced; the chip identity and role are confirmed in [board.md](board.md).
Not daisy-chained. CH config byte layout (REG 0x00/0x01):
`[7:5]=voltage_range[2:0], [4]=coupling, [2]=BW_limit_filter, [1:0]=gain_band`

---

## SPI interface (MCU → FPGA)

Passive SPI slave (`CSSPIN`, pin 27 = n2518). MCU is master (SPI1, CS=PB4, 8x 0xFF init).
All register writes go into FPGA via this interface — not through fabric pads (no MOSI pad in netlist).

FPGA SPI CS (n2518) reaches **0 FF inputs** in the reachability DB — the SPI decode logic is
in the EFB hard IP, invisible to the fabric netlist recovery.

Key registers: REG 0x00/0x01 (CH1/CH2 frontend), REG 0x0e (timebase mode), REG 0x0f (timebase
divisor), REG 0x17 (trigger edge), REG 0x18 (AWG DDS increment). Full map: [fpga-spi.md](fpga-spi.md).

---

## Open questions

- ADC → EBR write path full trace (BFS depth too shallow — extend or use Renode sweep)
- Timebase divisor table (SRAM at 0x20001fc0, populated at boot — needs Renode sweep)
- clk_22 (n1943) and clk_20 (n2237) — which PLL output these derive from
- 2-channel skew measurement — ENCA/ENCB phase offset vs fabric pipeline latency

Previously-unknown FPGA output pads 3/4/60/61/99 are now resolved by cone-size
fingerprinting (full analysis: [../../fpga/hantek/docs/v07-fabric-datapath.md](../../fpga/hantek/docs/v07-fabric-datapath.md)).
Destinations still want a physical board trace to fully confirm:

- **pin60** — AWG-datapath fabric output; most plausibly the AWG output-amp enable/mute
  (gated by SPI REG 0x02 bit 6 `AWG_ENABLE`; drives the EL5166 output-amp enable — see [board.md](board.md)).
- **pin99** — a separate DAC-B-phase output (same DAC cone as pin60, opposite clock phase).
- **pin61** — a clock-output tap on the shared PLL sample-clock net (deliberate fanout to
  DAC_CLK / DAC_PD / ADC_ENCA / ADC_ENCB), **not a short**.
- **pin3** (PCLKT3_2) — small control/status output (tiny fan-in cone, not datapath).
- **pin4** (PCLKC3_2) — edge-CIB decoder-gap orphan (netlist-recovery artifact, not a design signal).
