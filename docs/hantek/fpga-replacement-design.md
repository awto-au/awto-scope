# Replacement FPGA bitstream — design notes

## Goal

Replace V07 (the factory bitstream) with a clean, open replacement that uses the
MachXO2's resources efficiently and supports multiple operating modes loaded at
runtime via SRAM configuration.

This is not a re-implementation of V07 feature-for-feature. The factory design
has known inefficiencies (fabric SPI decoder instead of EFB, config flash used as
EEPROM instead of UFM, no SRAM mode switching). A replacement starts clean.

---

## Hard constraints

| Constraint | Source |
|-----------|--------|
| LCMXO2-1200HC-4TG100C (TQFP100, IDCODE 0x012BA043), pinout fixed by PCB | [board.md](board.md) |
| ADC clock 125 MHz to ENCA/ENCB (pins 80/90) | fpga.md / PLL analysis |
| STM32 SPI: PA4=nCS, PA5=CLK, PA6=MISO, PA7=MOSI | fpga-spi.md |
| 17× pad pins defined (ADC data, FPGA_nCS, trigger, sync) | fpga_pins.tsv |
| All SPI registers must remain backward-compatible with STM32 firmware | fpga-spi.md |
| Calibration block (784 B) lives in pages 0–48 of config flash | fpga.md |

---

## SPI interface — use the EFB, not fabric

V07 implements a fabric SPI decoder: FPGA_nCS wires to an EBR write-enable,
MOSI/CLK enter via dedicated config routing (not visible as normal IO). This
burns fabric resources and is hard to understand from the bitstream.

**The MachXO2 EFB has a hardened SPI slave** (Lattice EFB-SPI mode). It provides:
- 8-bit hardware shift register (zero fabric FFs)
- Proper MISO tristate
- WISHBONE bus out: byte data, address strobe, interrupt on RX complete
- Instant 8-bit register access in fabric via WISHBONE read/write

The replacement should enable EFB SPI slave and implement register decode as
WISHBONE address logic. The EFB SPI port uses the dedicated config pins
(CCLK/SI/SO) so the PCB pinout does not change.

**EFB instantiation (Verilog):**
```verilog
EFB #(
    .EFB_SPI("ENABLED"),
    .EFB_SPI_SLAVE_HANDSHAKE("ENABLED"),
    .EFB_WB_CLK_FREQ("125.0"),
    .EFB_WISHBONE_SLAVE("ENABLED")
) efb_inst (
    .WBCLKI(clk_125),
    .WBRSTI(rst_n),
    // WISHBONE bus
    .WBCYCI(wb_cyc), .WBSTBI(wb_stb), .WBWEI(wb_we),
    .WBADRI(wb_addr), .WBDATI(wb_wdata), .WBDATO(wb_rdata), .WBACKO(wb_ack),
    // SPI slave — dedicated config pins (no pad_map entries needed)
    .SPICSN(1'b0),   // always selected (nCS handled internally)
    .SPICLK(), .SPIMISO(), .SPIMOSI()
);
```

---

## Calibration data — keep on STM32

V07 reads the 784-byte calibration block from FPGA config flash pages 0–48 via
`fw_fpga_config_restore`. This is an abuse of config flash as EEPROM — it is
fragile (a partial bitstream load corrupts the block) and consumes config flash
pages that belong to the bitstream CRC space.

**Replacement approach:** Move calibration storage to STM32 internal flash or
external SPI flash. The STM32 has 512 KB internal flash with ~100 KB free after
firmware. Cal block is only 784 B. Benefits:
- Config flash untouched — bitstream CRC clean on every load
- Cal data survives bitstream changes
- Reads/writes from STM32 side are straightforward (no SPI register dance)
- The FPGA only receives cal coefficients via SPI registers at boot, not storing them

The FPGA's job is to apply the calibration constants it receives at startup,
not to store them. The 0x2f8-byte block structure (fpga.md) defines exactly
what coefficients the FPGA needs — these become SPI register writes at init time.

---

## SRAM configuration — multiple runtime bitstreams

MachXO2 supports `SRAM_LM` (SRAM load mode) via the WISHBONE SPI master in EFB.
A second bitstream image would be stored in UFM (User Flash Memory) and loaded into
SRAM at runtime without a full power cycle. On this part it does not fit; see the
capacity check below.

This enables distinct operating modes loaded on demand:

| Mode | Bitstream | When loaded |
|------|-----------|-------------|
| Diagnostic | `diag.bit` — full debug visibility, all regs readable | Boot default (safe) |
| 2-channel scope | `scope2ch.bit` — ADC capture, SPI readout, trigger | Normal operation |
| 1-channel scope | `scope1ch.bit` — one ADC path only, saves power | Low-power mode |
| AWG | `awg.bit` — AWG DDS + DAC active, minimal ADC | AWG-only mode |
| Combined | `scope2ch_awg.bit` — both paths active | Standard full-featured mode |

Loading is triggered by a firmware command to the FPGA (via SPI register write to
an EFB-WISHBONE register), which initiates the UFM→SRAM reload.

### UFM capacity check

- LCMXO2-1200HC internal flash, Configuration Flash + UFM combined: 0.33 Mb.
- One uncompressed bitstream: 0.35 Mb without EBR init, 0.41 Mb with it.
- Source: Lattice *MachXO2 Programming and Configuration Usage Guide* (July 2017), Table 2.
- The V07 image is 42,992 B. A second image does not fit in UFM.

**Practical approach:** Store bitstream images in the STM32's external SPI flash
(or the scope's USB-accessible storage if any). The STM32 streams the bitstream
to the FPGA JTAG/SPI configuration port using `PROGRAM_SPI` flow. This is how
field firmware updates already work — we extend the same path.

Alternatively, a dedicated 1 Mbit SPI flash on the board (check board.md for
any spare SPI flash CS lines) could hold multiple images.

---

## Diagnostic bitstream (first priority)

The first replacement bitstream to build is the **diagnostic mode**. Goals:
- Load cleanly onto hardware (proves the toolchain and pinout)
- Expose all SPI registers with readable status
- No ADC/AWG complexity — just register file and status outputs
- Acts as a baseline for incremental addition of data paths

Register map (compatible with STM32 firmware polling):

| SPI REG | Read returns | Write does |
|---------|-------------|------------|
| 0x00–0x07 | Mirror of write (loopback test) | Store to register file |
| 0x08 | `0xA5` (alive marker) | ignored |
| 0x09 | `0x01` (diag mode ID) | ignored |
| 0x0A | PLL lock status (bit 0) | ignored |
| 0x0B | Pad input snapshot (ADC_D0A etc.) | ignored |
| 0x17 | `0x00` (TRIG_SRC — no trigger in diag) | ignored |

---

## Resource plan (LCMXO2-1200HC)

| Resource | Total | Factory V07 usage | Source |
|----------|-------|-------------------|--------|
| LUT4 | 1280 | 1138 (89%) | [fpga-redesign-plan.md](fpga-redesign-plan.md), [fpga-db.md](fpga-db.md) |
| FFs | — | 1105 | [fpga-db.md](fpga-db.md) |
| EBR (9 kbit) | 7 | 7 (ADC → EBR ring) | [v07-fabric-datapath.md](v07-fabric-datapath.md) |
| Configured pads | — | 45 | [fpga-db.md](fpga-db.md) |
| EFB | 1 | unused | candidate SPI slave |

- V07 leaves little headroom. The redesign budget is tight; see [fpga-redesign-plan.md](fpga-redesign-plan.md).
- A clean EFB-based SPI decoder frees the ~50 FFs that V07 spends on the fabric shift register.

---

## Toolchain

Lattice Radiant (or open-source nextpnr-machxo2 + Trellis). Lattice Diamond also
supported for MachXO2. Synthesis via Yosys is the open-source path.

For SPI slave functionality without EFB (if open-source tools lack EFB support):
implement a simple 8-bit SPI slave in Verilog — only needs ~10 LUTs + 8 FFs.
Simpler and more transparent than V07's EBR-based approach.

---

## Open items before first build

1. Confirm whether nextpnr-machxo2 / prjtrellis supports EFB SPI slave
   (EFB primitives may need Diamond)
2. Identify any spare SPI flash CS on PCB for multi-bitstream storage
3. Decide whether cal block moves to STM32 flash or stays in FPGA (requires
   matching STM32 firmware side — see unlock.md hidden-limiter investigation)
4. Agree minimal register set for diag bitstream (STM32 firmware must poll
   something sensible during diag mode)
