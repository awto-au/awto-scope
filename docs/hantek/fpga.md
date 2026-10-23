# FPGA reversal

## Naming convention

The bitstream under analysis is from the **2D82AUTO** (2000-series, 200 MHz DSO). In all docs, scripts, and file names:

- **Device shorthand:** `2D82` — use this when referring to the bitstream, database, or FPGA configuration
- **Pluribus label:** `V07` — the specific bitstream version loaded in the Pluribus DB
- **FPGA part:** `LCMXO2-1200HC-4TG100C` — the full device string for prjtrellis/nextpnr
- **prjtrellis device key:** `LCMXO2-1200` — the short form used in the Trellis database

---

## Related docs

| Doc | Purpose |
|-----|---------|
| [fpga-spi.md](fpga-spi.md) | MCU↔FPGA SPI protocol + register map |
| [fpga-spy.md](fpga-spy.md) | EFB/WISHBONE spy bitstream |
| [fpga-can-mod.md](fpga-can-mod.md) | CAN bus hardware mod |
| [fpga-db.md](fpga-db.md) | Pluribus DB schema + postgres setup |
| [../vendors/hantek/2d82/docs/hardware/calibration.md](../../vendors/hantek/2d82/docs/hardware/calibration.md) | ADC/voltage calibration data format + flash layout |
| [pluribus:docs/upstream-contributions.md](https://github.com/awtoau/pluribus) | Diamond toolchain notes + upstream contribution plan (prjtrellis/nextpnr/prjcombine) |

---

## Pipelines

### 2D82 RE (this repo)

The Pluribus RE pipeline rebuilds the full analysis DB from `FPGA_V07.bin.config`:

```bash
python3 fpga/scripts/run_pluribus.py          # full rebuild
python3 fpga/scripts/run_pluribus.py --report # print current DB state only
```

Also available: `fpga/scripts/fpga_tilemap.py` renders an SVG tile map of V07 to `tmp/fpga_tilemap.svg`.

### Generic MachXO2 toolchain (pluribus repo)

The toolchain pipelines (prjtrellis fuzzer, prjtrellis checker, Diamond IOLOGIC fuzz, MachXO2 routing fuzzer) have moved to `<workspace>/pluribus/`:

```bash
# From <workspace>/pluribus:
python3 build_all.py --prjt-fuzz    # prjtrellis fuzzer → bits.db
python3 build_all.py --prjt-check   # verify bits.db against Diamond
python3 build_all.py --diamond      # Diamond IOLOGIC fuzz builds
python3 build_all.py --fuzz         # MachXO2 routing fuzzer
python3 build_all.py --help
```

The spy bitstream Diamond project lives in `fpga/diamond/spy/` (2D82-specific — hardwired to ADC pins 65/98).

### Pluribus pipeline detail

```bash
python3 fpga/scripts/run_pluribus.py          # full rebuild (postgres default)
python3 fpga/scripts/run_pluribus.py --report # print current DB state only
```

```
load → reach → reach2 → reach3 → patterns → auto_name → reach4 → report2 + chains + verilog
```

| Stage | Purpose |
|-------|---------|
| `load.py` | Parse bitstream via prjtrellis, discover pads from JQ/JA arcs + iomap, load netlist into DB |
| `reach.py` | Parallel all-net BFS reachability (NoGIL workers) |
| `reach2.py` | Reverse reachability, FF cones, critical paths, dominators |
| `reach3.py` | Symbolic expansion, clock crossings, const nets, EBR buses |
| `patterns.py` | Structural pattern detection (stuck/orphan pads, shared nets, PCLK lanes, const FFs) |
| `auto_name.py` | Infer net names from LUT init patterns |
| `reach4.py` | Pad→FF influence + FF output cones |
| `report2.py` + `chains.py` + `verilog.py` | Export TSV/text summaries + named Verilog netlist |

Outputs land in `fpga/aw2/out/`. The DB (`fpga_re` PostgreSQL) is rebuilt from scratch on every run.
To update pin or net annotations: edit `fpga/aw2/aw2-pins.tsv` or `fpga/aw2/aw2-nets.tsv`, then rerun.

Pluribus engine lives in `<workspace>/pluribus/` (repo `awtoau/pluribus`).
MachXO2 netlist lifter: `pluribus/lifters/machxo2_lift.py`.

> **⚠ Re-run any pluribus output produced before 2026-07-14.** An inverted
> `REG.SD` polarity in the lifter meant V07 recovered 1081 of 1090 FFs
> with a constant D input — the netlist looked structurally correct
> (cell counts, net names, LUT INITs all fine) but had almost no
> sequential connectivity, so every reachability/cone/critical-path result
> built on it is void. Fixed in `machxo2_lift.py:ff_d_source()`; guard is
> `pluribus: scripts/ffd_stats.py`. See
> [fpga-pluribus-recovery.md](fpga-pluribus-recovery.md) for the
> cross-bitstream pinout corroboration and the REG 0x05 ident-bytes lead.

---

## PLL (confirmed from V07 bitstream)

Crystal input: **25 MHz** (pins 1/2).

PLL config (PT1:GPLL_L0, `FEEDBK_PATH=INT_DIVD`):
- CLKI_DIV=1, CLKFB_DIV=10, CLKOP_DIV=2, CLKOS_DIV=4, CLKOS2_DIV=8, CLKOS3_DIV=2
- VCO = 25 × CLKFB_DIV × CLKOP_DIV = 25 × 10 × 2 = **500 MHz**

Note: raw bitstream words store `value − 1` (e.g. word `0001001`=9 → CLKFB_DIV=10).

| Output | Frequency | Domain |
|--------|-----------|--------|
| CLKOP | **250 MHz** | AWG DDS clock, main fabric pipeline (HPBX0200) |
| CLKOS | **125 MHz** | ADC encode clock (ENCA/ENCB pads), DAC_CLK |
| CLKOS2 | **62.5 MHz** | Secondary fabric domain |
| CLKOS3 | **250 MHz** | = CLKOP (same frequency) |

---

## ADC capture architecture

The AD9288 is run **interleaved**: ENCA and ENCB driven 180° out of phase at 125 MHz, giving **250 MSPS effective** (overclocked vs nominal 40 MSPS datasheet rating). The ADC clock is **fixed** — timebase control is done by the FPGA decimating at a ratio set by REG 0x0f (timebase divisor written by MCU).

```
AD9288 DA0-7 / DB0-7  →  [FPGA fabric]  →  6 × EBR (6144 B interleaved ring)
                                                    ↓
                                           SPI burst readout (cmd 0xa4 reg 0x01)
                                                    ↓
                                           MCU g_ch1_samples / g_ch2_samples
```

CH1 = even bytes, CH2 = odd bytes.

### EBR block RAM layout (confirmed from V07 bitstream)

| Tile | Mode | Width | Capacity | Function |
|------|------|-------|----------|----------|
| R6C1 | DP8KC | 2-bit | 1024 B | ADC sample buffer |
| R6C4 | DP8KC | 2-bit | 1024 B | ADC sample buffer |
| R6C7 | DP8KC | 4-bit | 1024 B | ADC sample buffer |
| R6C10 | DP8KC | 2-bit | 1024 B | ADC sample buffer |
| R6C14 | DP8KC | 4-bit | 1024 B | ADC sample buffer |
| R6C17 | PDPW8KC | 2-bit | 1024 B | ADC sample buffer |
| R6C20 | PDPW8KC | 18-bit | 1152 B | AWG waveform table |

ADC total: 6144 bytes = exact match to firmware `g_ch1_samples` / `g_ch2_samples`. Read ports 2/4-bit wide — FPGA serialises 8-bit samples over MISO in the SPI burst path (cmd 0xa4).

AWG (R6C20) is completely dedicated — separate write clock domain, separate read nets, zero overlap with ADC EBR nets.

### AWG path

```
MCU  →  SPI (REG 0x18 DDS increment, waveform table load)
     →  R6C20 EBR (512 × 18-bit waveform table)
     →  DAC bus D0-D11 (pins 36–53) + CLK (pin 54 = n1147 = CLKOS = 125 MHz)
     →  3PD5651E DAC → EL5166/LMH6702 output amp → Gen-Out BNC
```

DDS phase accumulator increment in REG 0x18 (32-bit LE): `freq_Hz = increment × 250_000_000 / 4_294_967_296`.

---

## Pin summary (V07 bitstream — 45 configured pads)

All 45 pads discovered from JQ/JA arcs in the bitstream + iomap. Full table in `V07-resources.txt`.

| Group | Pins | Count |
|-------|------|-------|
| Crystal / PLL | 2 | 1 |
| SPI config (sysCONFIG) | 27 | 1 |
| ADC channel A (DA0–DA7 + ENCA) | 65–75 | 9 |
| ADC channel B (DB0–DB7 + ENCB) | 83–98 | 9 |
| DAC D0–D11 + CLK + PD | 35–54 | 14 |
| AFE shift reg U7 (DS/SHCP/STCP) | 8–10 | 3 |
| AFE shift reg U1 (DS/SHCP/STCP) | 62–64 | 3 |
| Fabric role resolved, board dest untraced | 3, 4, 60, 61, 99 | 5 |

### Known anomalies (from `patterns.py`)

**Stuck output pads (13):** Output pads driven by a FF with D=0, CE=1, LSR=0 — permanently low. Includes pin 3 (PCLKT3_2), DAC_D1/D3/D4/D5/D8/D9/D10/D11, DAC_CLK, ADC_ENCB. The DAC data + CLK pins being stuck-at-zero in this bitstream is consistent with the DAC being loaded via EBR — the FF output seen here is the register output path, not the EBR readout path, which bypasses the cone-of-influence analysis.

**Orphan output pads (12):** Output pads with no fabric driver in the recovered netlist (fanin=0). Includes pin 4 (PCLKC3_2), U7_DS/STCP, DAC_D0/D2/D6/D7, DAC_PD, ADC_ENCA, U1_STCP, pins 61/99. Cause: spine/global routing, EBR output path, or genuine netlist gap.

**Shared-net pads (2):** Multiple output pads on the same fabric net — netlist recovery gap where a net was discovered via multiple pad arcs:
- n1147: pin 54 (DAC_CLK) + pin 98 (ADC_ENCB) — same 125 MHz clock net (CLKOS)
- n2508: pin 35 (DAC_PD) + pin 61 + pin 65 (ADC_ENCA) — the shared clock-output fanout (pin 61 = a deliberate clock tap, not a short)

> The fabric-trace **verdicts** for these unknown/orphan pads (pin 3/4/60/61/99, U7)
> live in [v07-fabric-datapath.md](v07-fabric-datapath.md) §3 — the "orphan"/"shared-net"
> labels above are raw `patterns.py` output, since resolved there.

---

## ADC → FPGA data bus (confirmed)

AD9288 DA0–7 / DB0–7 + ENCA/ENCB. 270 Ω series resistor packs at ADC driver end. Routing is flat — FPGA pad order follows AD9288 bit order. All 16 data pins confirmed in Pluribus DB.

**Channel A** (right edge, INPUT_LVTTL33):

| FPGA pin | Site | AD9288 signal |
|----------|------|---------------|
| 75 | R2C21:PIOA | D0A (LSB) |
| 74 | R2C21:PIOB | D1A |
| 71 | R3C21:PIOA | D2A |
| 70 | R3C21:PIOB | D3A |
| 69 | R4C21:PIOA | D4A |
| 68 | R4C21:PIOB | D5A |
| 67 | R4C21:PIOC | D6A |
| 66 | R4C21:PIOD | D7A (MSB) |
| 65 | R5C21:PIOA | ENCA |

**Channel B** (top edge, INPUT_LVTTL33):

| FPGA pin | Site | AD9288 signal |
|----------|------|---------------|
| 83 | R0C14:PIOB | D0B (LSB) |
| 84 | R0C14:PIOA | D1B |
| 85 | R0C11:PIOD | D2B |
| 86 | R0C11:PIOC | D3B |
| 87 | R0C11:PIOB | D4B |
| 88 | R0C11:PIOA | D5B |
| 96 | R0C9:PIOB | D6B |
| 97 | R0C9:PIOA | D7B (MSB) |
| 98 | R0C8:PIOB | ENCB |

---

## AFE shift registers — U1, U7

Both are 74HC595 8-bit SIPO shift registers driven by the FPGA. Not daisy-chained — each has its own SER line.

| Chip | FPGA pins | Status | Role |
|------|-----------|--------|------|
| U1 | 62 (DS), 63 (SHCP), 64 (STCP) | confirmed (board-traced) | CH1 AFE relay/gain |
| U7 | 8 (DS), 9 (SHCP), 10 (STCP) | inferred | CH2 AFE relay/gain |

74HC595 pinout: DS=pin14 (serial data), SHCP=pin11 (shift clock), STCP=pin12 (latch clock).

---

## What's still needed for fresh gateware

1. **ADC→EBR write path** — trace DA0–DA7/DB0–DB7 forward to EBR write ports (confirms bit packing and interleave)
2. **EBR→MISO path** — complete the SPI burst read forward cone
3. **Timebase divisor table** — REG 0x0f values for all 10 timebase settings
4. **Board traces** — confirm the physical destinations of pins 3/4/60/61/99 (fabric roles resolved in [v07-fabric-datapath.md](v07-fabric-datapath.md) §3: pin60 = AWG output-amp enable/mute, pin99 = DAC-B-phase output, pin61 = 125 MHz clock-output tap, pin3 = status/control, pin4 = edge-CIB orphan — but which board net each drives is untraced)
5. **Orphan pad investigation** — U7_DS/STCP (pins 8/10) and DAC_D0/D2/D6/D7 are orphan pads — EBR output path or netlist gap (#127)
6. **U600** — unknown 74HC595, FPGA pins 11–26 unconfigured in V07

---

## Bitstream storage and loading

The MachXO2 has three configuration storage areas:

| Area | Volatile | Capacity (1200HC) | Wear limit | Load time (18 MHz SPI) |
|------|----------|--------------------|------------|------------------------|
| Config Flash (CF) | No | 0.33 Mb | 10,000 cycles | ~500ms erase + ~537ms write ≈ **1s** |
| User Flash Memory (UFM) | No | shared overflow with CF | 10,000 cycles | same |
| SRAM | Yes (lost on power) | full bitstream | unlimited | **~19ms** (SPI-limited only) |

**Dual Boot:** CF + UFM can each hold a bitstream; a pin or register selects which loads at power-on.

**SRAM load (Transparent/Refresh mode):** Write bitstream to SRAM via SPI, send REFRESH — FPGA reconfigures in ~19ms with zero flash wear. Power cycle reverts to flash. Ideal for rapid iteration.

**Flash write timing** (343,936 bits = 2688 × 128-bit pages):
- Erase: ~500ms (bulk, poll BUSY — do NOT use fixed delays per TN1204)
- Program: 2688 pages × 200µs/page = ~538ms
- Total: ~1s per flash update

### Calibration data in config flash — confirmed from firmware decompile

The OEM firmware does **not** use UFM for calibration storage. Instead, it stores the 0x2f8-byte
calibration block in **config flash pages 0–48** (49 pages × 16 bytes = 784 bytes), overlapping
the start of the bitstream region. This was confirmed by tracing `fw_fpga_write_config` /
`fw_fpga_config_restore` in `decompiled_named.c`:

- `LSC_WRITE_ADDRESS` (`0x47`) with all-zero operands → targets **page 0** of config flash
- `fw_machxo2_read_page(buf, 0x31)` reads 49 pages (784 B) before erasing
- `fw_machxo2_isc_erase()` erases only the updated sector (not the full bitstream)
- The 49-page write is a **read-modify-write**: existing flash content is read first, one
  sub-block is updated in RAM, then all 48 pages are written back

**Three calibration sub-blocks, selected by `fw_fpga_write_config(sel)`:**

| sel | Offset in 0x2f8 buf | Size | Content |
|-----|---------------------|------|---------|
| 0 | +0x000 | 0x1f8 (504 B) | Scope/DAC cal table (voffset binary-search results) |
| 1 | +0x1f8 | 0xec (236 B) | ADC gain correction floats |
| 2 | +0x2e4 | 0x10 (16 B) | AWG CH2 calibration block |

**Bitstream integrity:** The first 49 config flash pages sit within the 2688-page bitstream region.
A full bitstream reload (`fw_fpga_config_restore`) reads the cal block out first, then after the
full write the caller must call `fw_fpga_write_config(sel)` to restore it — otherwise calibration
is lost. This is a fragile design: a failed/interrupted bitstream update silently corrupts cal data.

**The distributed V07.bin is a factory-clean bitstream — no calibration data.**
Pages 0–48 in V07.bin contain bitstream logic, not cal data. Calibration is written to the
device's flash at runtime by the MCU; it only exists in a live device that has been calibrated.

**If you ever extract a live device bitstream** (via `openFPGALoader --read-flash` or
`LSC_READ_INCR_NV` 0xCA), treat the first 784 bytes (pages 0–48) as **calibration data**,
not bitstream logic — mark them as DATA in any disassembler/RE tool. The 0x2f8 layout is
documented in [calibration.md](calibration.md).

**Reading cal data without a full bitstream dump:** Send `LSC_WRITE_ADDRESS` (0x47 + 3×0x00)
then `LSC_READ_INCR_NV` (0xCA + 0x10 + 0x00 + 0x31) via the SSPI port (CS=PB4) to read
49 pages of cal data non-destructively.

---

## FPGA versions and update path

| Version | Meaning |
|---------|---------|
| `0x0` / `255` | Dead / unprogrammed |
| V02, V04, V05, V07 | Good working builds |

All models (2D42/2D72/2D82/2C42/2C72) share the same bitstream; per-model bandwidth is enforced by the MCU writing a minimum timebase divisor to REG 0x0f, not by the FPGA.

**AutoUpdate can DOWNGRADE the FPGA unwantedly** (e.g. V07→V04). Avoid "Force update FPGA" on 2D82AUTO units.

**Programming path:** MCU bit-bangs the MachXO2 SSPI config port (SPI1, CS=PB4) and polls BUSY.

**Bitstream on disk:** `vendors/hantek/2d82/reference/dso2000/fpga/DS1302_2019071801.bin` (42,992 B, entropy 5.82 = unencrypted). Date 2019-07-18 → likely V05. V07 is forum/email-gated (eediscuss tid=14649).

**External references:** eediscuss tid=13676 (version history), tid=14649 (FPGA update thread).

**openFPGALoader** — reads/writes MachXO2 over JTAG. **Not installed** (not in Fedora 44 `dnf` repos, no copr found) and **not buildable-to-any-purpose this session** — see live-bitstream extraction attempt below; no compatible JTAG cable is connected regardless of whether the tool is built.

---

## Live bitstream extraction attempt — 2026-07-13

**Goal:** do for the FPGA what the MCU firmware investigation (same date) did for the STM32 — confirm whether the physical board's FPGA bitstream is byte-identical to the repo's `FPGA_V07.bin`, or a genuine variant. **Result: blocked at the physical-access layer, on both the JTAG and the SSPI/USB paths. No live bitstream or cal-flash bytes were obtained. This is an access-tooling result, not a "V07 confirmed identical" result — do not read the earlier ident-byte match (`=7`, already documented above) as a full-bitstream confirmation.**

### 1. openFPGALoader

- **Not in Fedora 44's `dnf` repos** (`dnf search openfpgaloader` → no matches; checked enabled copr list too — nothing).
- Build-from-source was considered but **not attempted**, because deciding factor #2 below (no compatible cable) makes a local build unable to unblock anything this session — would just be dead tooling. (Build deps mostly present if this changes: `cmake`, `pkgconf-pkg-config`, `libftdi-devel`, `libusb1-devel` all already installed; `eudev-devel` — optional, for udev rules — is not.)

### 2. JTAG access to the FPGA — physically present, but no compatible cable on this host

`vendors/hantek/2d82/docs/hardware/board.md` (BOM table) already documents a **dedicated 6-pin (2×3) JTAG header on the AFE board** for the MachXO2 (U2), separate from the MCU's own SWD header:

| Header pin | Signal | Note |
|---|---|---|
| 1 | VCC | |
| 2 | TMS | R66 10 kΩ pull-down |
| 3 | TDI | **R3 unpopulated** |
| 4 | TCK | R80 10 kΩ pull-down |
| 5 | TDO | R67 10 kΩ pull-down |
| 6 | GND | |

`vendors/hantek/2d82/docs/hardware/gpio-pinout.md` independently confirms: *"the MCU does NOT drive JTAG… the physical JTAG header on the AFE board is for an external programmer (openFPGALoader) only."* So the header exists and is intended for exactly this use.

**Blocker found this session: no compatible JTAG probe is connected to this Linux host.** `lsusb` shows only:
- `0483:3748` STMicroelectronics ST-LINK/V2 — used for the existing MCU SWD work. openFPGALoader's [cable compatibility list](https://trabucayre.github.io/openFPGALoader/compatibility/cable.html) does **not** include ST-LINK as a supported generic JTAG cable (ST-LINK's JTAG mode is usable for other ST tooling, not exposed as a bit-bang/FTDI-style cable openFPGALoader can drive against a Lattice target).
- `303a:1001` ×2 Espressif "USB JTAG/serial debug unit" — these are ESP32-C3/S3 built-in JTAG-over-USB adapters, fixed to Espressif's own debug protocol; not usable as a generic external JTAG cable for a different vendor's device.
- No FTDI (`0403:*`), CMSIS-DAP, Segger J-Link, Digilent, or Altera/Xilinx USB-Blaster device present.

Separately, **R3 (TDI series/pull resistor) being unpopulated** on the header is a second open question — it may mean TDI isn't fully wired end-to-end even with a cable connected — but this wasn't reached; the cable gap alone is blocking.

**Conclusion: did not attempt any physical rewiring or cable connection this session** (none was available to connect, and populating R3 would be a hardware mod, not just wiring) — flagging as a hardware-access gap for a future session with a generic FTDI/CMSIS-DAP JTAG probe in hand, at which point the R3 question would need resolving too (may just work via the TMS/TCK/TDO-only subset for IDCODE/status, but full bitstream readback needs TDI).

### 3. USB-relayed SSPI readback — checked exhaustively, does not exist

Re-read `fw_usb_cali_read_handler`/`fw_usb_cali_write_handler` (bank 0x03, both directions) in full — every sub-command is already enumerated in `vendors/hantek/2d82/ghidra/ghidra_names_2d82.tsv`:

- **Read side** (cmd[3]=1) exposes: status, device UID, cal-write-index, AWG/trigger status, serial number, firmware version, cal-table-by-band (×6), and **one single FPGA byte** — sub-command `0x0c`, which is just `fw_fpga_read_ident()[6]` (the version byte, already used for the `=7`/"V07" cross-check). **No sub-command reads back raw config-flash/bitstream bytes.**
- **Write side** (cmd[3]=0) exposes the FPGA reprogram sequence (STEP1/2/3, sub-commands `2`/`3`/`4`) — this path is **write-only**: it drives `fw_machxo2_prog_incr` (`PROG_INCR_NV`, opcode `0x70`), never a read-back opcode. Not invoked (per task constraints).
- `fw_machxo2_read_page()` (opcode `0xCA`, `LSC_READ_INCR_NV`) exists in the firmware and is exactly the non-destructive cal-block read the task asked about — but it is **only ever called internally**, from `fw_fpga_config_restore()` (the pre-reprogram cal-preserving read-modify-write, itself part of the write/reprogram flow). **No USB command triggers `fw_machxo2_read_page()` and returns its result to the host.** There is no exposed dump-via-USB path, full-bitstream or cal-only.

Since the MCU's SPI1 bus to the FPGA (both runtime CS=PA4 and SSPI CS=PB4) is not electrically accessible from this Linux host directly (no logic analyzer / SPI sniffer wired in this session), and no USB command relays an `0xCA` read, **there is currently no read-only path to pull FPGA config-flash bytes (bitstream or calibration) off the live board from software alone.**

### What would unblock this

1. **JTAG path:** a generic FTDI-based (or CMSIS-DAP) JTAG probe connected to the AFE board's 6-pin header, openFPGALoader built from source (trivial once a cable exists — deps are already installed), `openFPGALoader --read-flash` or equivalent against `LCMXO2-1200HC`. Would need to also check/populate R3 (TDI) if it turns out not to be wired through.
2. **SSPI relay path:** would require either (a) a logic analyzer / SPI sniffer tapped onto SPI1 (PA5/PA6/PA7 + PB4) while the MCU boots (passively observing the MCU's own `LSC_READ_INCR_NV`-equivalent boot-time reads, if any occur) — passive, read-only, no board mod — or (b) new MCU firmware/a debug build that adds a USB-exposed dump command calling `fw_machxo2_read_page()`, which is a firmware **modification**, out of scope for a read-only investigation of the stock board.

Neither was attempted this session per the read-only / no-rewiring constraint. The **ident byte (`=7`, "V07") cross-check from earlier today remains the only live-device FPGA data point** — it is a strong but partial signal (confirms the FPGA reports the same version string as the on-disk `FPGA_V07.bin`), not a byte-for-byte bitstream match.

**Update:** Dan has since confirmed he deliberately updated the FPGA on this board himself at some point in the past (exact date/method unrecorded, but an intentional action, not an anomaly). This reframes — but does not resolve — the access gap above:

- The physical-access blockers (no JTAG cable compatible with the AFE board's 6-pin header; no USB-exposed `LSC_READ_INCR_NV`/`0xCA` dump path) are unchanged. A byte-for-byte live-vs-`FPGA_V07.bin` comparison still cannot be performed from this host without new hardware (FTDI/CMSIS-DAP JTAG probe, or a logic analyzer on SPI1/SSPI, or a custom MCU debug build exposing the dump command).
- Given Dan's confirmation, if a future session *does* obtain a live dump and finds it differs from `FPGA_V07.bin` beyond the expected pages 0–48 calibration region, the most likely explanation is simply **this confirmed manual update** — not a new mystery requiring deep forensic investigation (unlike the MCU firmware discrepancy, which remains genuinely unexplained). A simple "differs, consistent with Dan's known manual update, here's the page-level diff" is a complete answer in that case.
- Equally plausible: the live FPGA still reports V07 and matches `FPGA_V07.bin` exactly outside the cal region — meaning the manual update either reinstalled the same V07 bitstream, or targeted something else (MCU firmware / calibration data only) and never touched the FPGA bitstream itself.
- Either outcome is a fine, closed result — it does not need the same level of scrutiny the unexplained MCU firmware mismatch got. This note exists so a future session doesn't waste time treating a found difference as suspicious.
