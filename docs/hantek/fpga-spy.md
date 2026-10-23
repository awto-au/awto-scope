# FPGA Spy Bitstream — MachXO2-1200HC

> **Status:** Verilog complete, simulation stubs written, pre-synthesis verification pass done.
> **Next step:** Install iverilog, run `python3 fpga/hantek/scripts/run_spy_sim.py`, then `diamond -t build.tcl`.

---

## Purpose

A self-contained diagnostic bitstream that gives the MCU read/write access to
everything inside the FPGA over the existing 4-wire MCU↔FPGA SPI bus.  Load
via SRAM (zero flash wear, 19 ms, power-cycle reverts to OEM bitstream).

Primary uses:
- **Pin probing** — read actual pad state of every pin including unknowns (3/4/60/61/99)
- **EFB survey** — read USERCODE, TraceID, UFM contents via WISHBONE (issues #71 #74)
- **ADC confirmation** — trigger a snapshot, read DA/DB to confirm ADC wiring
- **RAM test** — 6 KB EBR + 128 B LUTRAM accessible as flat SPI address space
- **GPIO drive** — drive unknown pins LOW individually to probe board destinations

---

## SPI Interface

**Physical pins** (runtime SPI, not SSPI config port):

| Signal | FPGA pin | MCU |
|--------|----------|-----|
| SCK    | 31       | PA5 SPI1_SCK |
| MOSI   | 49       | PA7 SPI1_MOSI |
| MISO   | 32       | PA6 SPI1_MISO |
| nCS    | 27       | PA4 (active LOW) |

**Protocol:** Mode 0 (CPOL=0, CPHA=0), 8-bit, MSB first.

```
CMD byte + 2-byte address + N data bytes, while nCS LOW.

CMD 0x02 = WRITE  [0x02][addr_hi][addr_lo][d0][d1]...
CMD 0x03 = READ   [0x03][addr_hi][addr_lo] → FPGA outputs data bytes
CMD 0x05 = STATUS [0x05] → 1 byte (no address needed)
CMD 0x06 = ADC_CAPTURE → triggers ADC snapshot (no address needed)
```

Burst reads/writes auto-increment address.  Max SPI clock ~6 MHz (limited by
25 MHz fabric clock oversampling; SPI SCK is NOT used as fabric clock).

---

## Address Map

| Address | Size | Description |
|---------|------|-------------|
| `0x0000–0x17FF` | 6144 B | EBR RAM (6 × DP8KC, byte-wide) |
| `0x1800–0x187F` | 128 B  | LUTRAM (distributed, byte-wide) |
| `0x1C00` | 1 B | ADC snapshot: DA[7:0] (after CMD 0x06) |
| `0x1C01` | 1 B | ADC snapshot: DB[7:0] |
| `0x1C02` | 1 B | ADC snapshot: `{6'h0, snap_encb, snap_enca}` |
| `0x1C03` | 1 B | ADC status: `{adc_done, 7'h0}` |
| `0x1D00` | 1 B | GPIO output register `[4:0]` → unknown pins (write) |
| `0x1E00` | 1 B | GPIO pad readback: DA[7:0] actual pad state |
| `0x1E01` | 1 B | GPIO pad readback: DB[7:0] actual pad state |
| `0x1E02` | 1 B | GPIO pad readback: `{ENCB,ENCA,pin99,pin61,pin60,pin4,pin3,0}` |
| `0x1E03` | 1 B | `{adc_done, 7'h0}` |
| `0x1E04` | 1 B | AFE pad state: `{afe2_2,afe2_1,afe2_0,afe1_2,afe1_1,afe1_0,2'b0}` |
| `0x1E05` | 1 B | DAC D[7:0] pad state |
| `0x1E06` | 1 B | DAC `{PD,CLK,D11,D10,D9,D8,2'b0}` pad state |
| `0x1E07` | 1 B | DAC reserved pins `{rsv4,rsv3,rsv2,rsv1,rsv0,3'b0}` pad state |
| `0x1F00` | 1 B | EFB WB: WISHBONE address (write to set) |
| `0x1F01` | 1 B | EFB WB: write data → writing triggers WB write cycle |
| `0x1F02` | 1 B | EFB WB: read data (result of last WB read) |
| `0x1F03` | 1 B | EFB WB: control — write `0x01` to trigger WB read |
| `0x2000` | 1 B | Status — always reads `0xA5` (alive marker) |

### GPIO output register (0x1D00)

| Bit | Pin | OEM net | Notes |
|-----|-----|---------|-------|
| [0] | 3   | n2181 PCLKT3_2 | Write 1 to drive LOW |
| [1] | 4   | n2754 PCLKC3_2 | Write 1 to drive LOW |
| [2] | 60  | n2243 | AWG-domain output in OEM (leading reading = AWG output-amp enable/mute) |
| [3] | 61  | n2496 | Clock-output tap on the shared 125 MHz PLL net — actively driven, drive with caution |
| [4] | 99  | n2513 | DAC-B-phase output in OEM, board destination unknown |

Default on power-up: `0x00` (all tristated).  **Do not drive pin 61 (bit 3)
without first confirming via 0x1E02 that it is not being actively driven** — pin 61
is a **deliberate clock-output fanout** on the shared 125 MHz PLL net (DAC_CLK/
DAC_PD/ADC_ENCB), **not a short** (see [v07-fabric-datapath.md](v07-fabric-datapath.md)
§3). It is an active clock output in the OEM design, so driving it contends with the
PLL.

---

## ADC Snapshot

The AD9288 is run interleaved: ENCA and ENCB at 125 MHz, 180° apart → 250 MSPS
effective.  The spy bitstream captures one pair of samples per CMD 0x06:

1. Send `CMD 0x06` (nCS LOW, send `0x06`, nCS HIGH)
2. Wait ≥1 µs (one ENCA + ENCB edge guaranteed)
3. Poll `0x1C03` until bit[7]=1 (adc_done), or just wait 2 µs
4. Read `0x1C00` = DA sample, `0x1C01` = DB sample

This is a single-sample snapshot, not a ring buffer.  It confirms ADC wiring
and that the pins are live, but does not replace the 6144-byte burst capture.

---

## EFB WISHBONE Access

The EFB exposes UFM, sysCONFIG, and oscillator trim via a 5-bit WISHBONE
address space (TN1246).  The proxy maps this to SPI addresses 0x1F00–0x1F03.

**Read a register:**
```
Write 0x1F00 = <WB_ADDR>    # set address
Write 0x1F03 = 0x01         # trigger read
Wait ~5 µs                  # WB ACK latency
Read  0x1F02                # result
```

**Write a register:**
```
Write 0x1F00 = <WB_ADDR>    # set address
Write 0x1F01 = <DATA>       # triggers WB write immediately
```

Key EFB addresses (TN1246 Table 2):
| WB addr | Register |
|---------|----------|
| 0x70 | UFM page data (reads 16-byte page after LSC_READ cmd) |
| 0x71 | UFM status |
| 0x40 | sysCONFIG status |
| 0x04 | USERCODE byte 0 |
| 0x05 | USERCODE byte 1 |
| 0x06 | USERCODE byte 2 |
| 0x07 | USERCODE byte 3 |
| 0x0C | Die temperature (°C, signed) |
| 0x0F | RC oscillator trim |

---

## File Structure

```
fpga/diamond/spy/
├── spy_top.v          Top level
├── spy_top.lpf        Pin constraints (Diamond LPF)
├── spi_slave.v        SPI slave + address decode
├── adc_capture.v      ADC snapshot (cross-domain: ENCA/ENCB → clk)
├── ebr_ram.v          6 × DP8KC EBR (6144 bytes)
├── lutram.v           128-byte distributed LUTRAM
├── efb_wb_proxy.v     EFB instantiation + WISHBONE master bridge
├── build.tcl          Diamond 3.14 build script
└── sim/
    ├── primitives.v   BB / DP8KC / EFB / GSR / PUR behavioural stubs
    └── tb_spy_top.v   Icarus Verilog testbench (7 test cases)
                       (runner: fpga/hantek/scripts/run_spy_sim.py)
```

---

## Build

```bash
# Simulation (Icarus Verilog)
dnf install iverilog          # if not installed
python3 fpga/hantek/scripts/run_spy_sim.py   # logs → tmp/logs/spy_sim_*.log
gtkwave fpga/hantek/diamond/spy/sim/dump.vcd

# Synthesis (Diamond 3.14)
cd fpga/diamond/spy
diamond -t build.tcl          # or open Diamond GUI and source build.tcl
# Output: spy_impl/spy_top.bit
```

---

## Loading onto Device

```bash
# SRAM load — volatile, power-cycle reverts to OEM bitstream
# Via openFPGALoader (JTAG):
openFPGALoader -b machXO2EVB spy_impl/spy_top.bit

# Or via MCU firmware SSPI path (ISC_ENABLE → bitstream → REFRESH):
# Send bitstream over PB4 SPI using the same flow as fw_fpga_config_restore
# but targeting SRAM (send LSC_REFRESH at end instead of ISC_PROGRAM_DONE).
```

---

## Verification Status

| Check | Status |
|-------|--------|
| Port/LPF name consistency | Fixed — osco_nc removed, all ports match |
| OPENDRAIN=OFF removed from LPF | Fixed — not valid Diamond syntax |
| BB tristate `O=z` in sim | Fixed — stub returns 0 when floating |
| pin61 hard-assign conflict in TB | Fixed — weak pull1 driver |
| adc_trigger pulse width | Known: 1-cycle 25 MHz pulse; reliable if ENCA ≥1 µs after trigger |
| EFB WBADRI as individual bits | Confirmed — efb_wb_proxy uses scalar port connections |
| SYNC2 macro validity | Confirmed valid Verilog-2001 |
| EBR token-paste macro | Under review — may need explicit instantiation for Diamond |
| Simulation | Not yet run — iverilog not installed |
| Diamond synthesis | Not yet run |

---

## Related Issues

| Issue | Link |
|-------|------|
| #71 | EFB/WISHBONE spy concept |
| #72 | JTAG boundary scan via SSPI |
| #74 | EFB full register survey |
| #134 | AWG EBR write CE signals |
| #143 | FPGA SSPI: USERCODE/TraceID readable |
| #147 | This feature (spy bitstream) |
