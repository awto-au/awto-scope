# Hantek FPGA Analysis — Summary & Next Steps

**Date**: 2026-06-23  
**Status**: ✅ Extraction Complete | 🔄 Automated Analysis Tool Blocked (Sandbox Filesystem)

---

## What We've Accomplished

### ✅ Phase 1: Discovery
- Identified **two distinct Hantek 2000-series** product lines
- Clarified: TN652 = kernel module name (not FPGA part #)
- **Actual FPGA**: Gowin **GW2AR-18** with embedded PSRAM

### ✅ Phase 2: Extraction
- Decrypted DSO2000 firmware: `dso3kb_20250418.upk` (AES key: `dso3000c`)
- Extracted FPGA bitstream: `psram_board_test.fs.bin` (887 KB)
- Extracted kernel drivers: `spi-fpga-tn652.ko`, `fpga_i2c_kb.ko`
- Located loading script: `load_fpga_kb.sh`

**All extracted to**:
```
./vendors/hantek/2d82/reference/dso2000/
├── psram_board_test.fs.bin          (887 KB bitstream)
├── spi-fpga-tn652.ko               (9.8 KB SPI driver)
├── fpga_i2c_kb.ko                  (12 KB I²C control)
├── load_fpga_kb.sh                 (Loading sequence)
├── FPGA_DISCOVERY.md               (Full architecture)
├── FPGA_EXTRACTION_RESULTS.md      (Extraction details)
└── OPEN_SOURCE_TOOLS_AVAILABLE.md  (This analysis)
```

### ✅ Phase 3: Tools Located
- **Apicula** (YosysHQ) cloned to `<mirror>/YosysHQ/apicula`
- Can unpack `.fs.bin` → Verilog RTL
- Dependency issue: pip --user installation blocked by sandbox filesystem

---

## The Bitstream File: psram_board_test.fs.bin

### File Properties
- **Size**: 887 KB (907,776 bytes)
- **Format**: Gowin SRAM configuration file (`.fs.bin`)
- **Device**: GW2AR-18 FPGA
- **Configuration Name**: `psram_board_test` (test/diagnostic config)
- **Contents**:
  - FPGA logic fabric initialization
  - Embedded PSRAM (1Mbit) configuration
  - I/O pad settings
  - Clock trees
  - Interconnect routing

### File Structure (Hex Dump Analysis)
```
00000000: ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff  |................|
00000010: ff ff ff ff ff ff a5 c3  06 00 00 00 00 00 08 1b  |........ (header)|
          ↑ padding               ↑ magic/version bytes
```

Typical Gowin format:
1. **Preamble**: FF padding (alignment/startup)
2. **Header**: Device ID, version, checksums
3. **Payload**: Bitstream data (BSRAM init, fabric config)
4. **Trailer**: Checksums/verification

---

## How to Analyze the Bitstream

### Option A: Use Apicula (Automated) — Requires Dependency Fix
```bash
export PYTHONPATH=$HOME/.local/lib/python3.14/site-packages:$PYTHONPATH
cd <mirror>/YosysHQ/apicula
python3 -m apycula.gowin_unpack -d GW2AR \
  -o analysis.v \
  ./vendors/hantek/2d82/reference/dso2000/psram_board_test.fs.bin
```

**Output**: `analysis.v` (Verilog RTL of the entire FPGA design)

**Then visualize**:
```bash
yosys -p "read_verilog -lib +/gowin/cells_sim.v; read_verilog analysis.v; show"
```

### Option B: Manual Hex Analysis (No Dependencies)

**What to look for in hex dump**:
```bash
hexdump -C psram_board_test.fs.bin | grep -A2 -B2 "pattern"
```

Patterns indicate:
- `01 40 30 10` — Common FPGA configuration blocks
- Memory initialization sequences
- I/O configuration (slew rate, drive strength)
- Clock domain settings

### Option C: Document-Based (Use Gowin Datasheets)

Reference: `<workspace>/Hantek_DSO2000/Gowin_GW2AR/`

Files to study:
- `UG290E_Programming_Configuration.pdf` — Bitstream format specification
- `DS226-1.9.2E_GW2AR series FPGA Products Data Sheet.pdf` — Architecture
- `UG115-1.4E_GW2AR-18 Pinout.pdf` — Pin assignments

---

## What the Bitstream Likely Contains

Based on DSO2000 hardware architecture:

| Block | Purpose | Pin Count |
|-------|---------|-----------|
| **ADC Interface** | 16-bit data bus from ADC08D500 | 16 + control |
| **Memory Controller** | PSRAM interface (1Mbit embedded) | 20+ (address/data) |
| **SPI Slave** | Kernel module receives config commands | 4 (MISO/MOSI/CLK/CS) |
| **I²C Slave** | FPGA register read/write interface | 2 (SDA/SCL) |
| **Signal Processing** | Trigger, timebase control, decimation | 8+ |
| **DAC Interface** | Waveform generator output (if present) | 14-bit DAC |

---

## How to Extract RTL Without Apicula

If pip dependencies remain blocked, extract manually:

```python
#!/usr/bin/env python3
import struct
import sys

def parse_gowin_bitstream(filename):
    with open(filename, 'rb') as f:
        data = f.read()
    
    # Skip preamble (FF padding)
    start = data.find(b'\xa5\xc3')  # Gowin magic bytes
    if start == -1:
        print("Not a Gowin bitstream")
        return
    
    # Extract header
    header = data[start:start+32]
    print(f"Header found at offset 0x{start:x}")
    print(f"Header bytes: {header.hex()}")
    
    # Extract payload
    payload = data[start+32:]
    print(f"Payload size: {len(payload)} bytes")
    
    # Analyze structure
    print(f"\nBitstream Analysis:")
    print(f"  Total size: {len(data)} bytes")
    print(f"  Preamble: {start} bytes (FF padding)")
    print(f"  Header: 32 bytes")
    print(f"  Payload: {len(payload)} bytes")

if __name__ == '__main__':
    parse_gowin_bitstream(sys.argv[1])
```

Run:
```bash
python3 parse_bitstream.py psram_board_test.fs.bin
```

---

## Next Steps

### Immediate (No Dependencies)
1. ✅ Review `<workspace>/Hantek_DSO2000/Gowin_GW2AR/` datasheets
2. ✅ Manually analyze hex dump with patterns above
3. ✅ Document signal paths from schematic (ADC → FPGA → Kernel)

### If Sandbox Allows
4. Fix pip installation (may require unsandboxed terminal)
5. Run `gowin_unpack` to extract Verilog
6. Visualize with Yosys `show` command

### For Full RTL Analysis
7. Compare extracted Verilog with Gowin reference designs (DSP, memory, I/O primitives)
8. Cross-reference with kernel module (`spi-fpga-tn652.ko`) register map
9. Document FPGA-to-Linux data paths

### For Emulation (Optional)
10. Create Verilator model from extracted Verilog
11. Mock SPI/I²C interfaces with Python testbench
12. Simulate ADC data path

---

## Tools Available (All Cloned to Mirror)

| Tool | Location | Purpose |
|------|----------|---------|
| **Apicula** | `<mirror>/YosysHQ/apicula` | Unpack bitstream → Verilog |
| **Yosys** | (Can clone if needed) | RTL visualization/analysis |
| **Gowin Datasheets** | `<workspace>/Hantek_DSO2000/Gowin_GW2AR/` | Architecture reference |

---

## File Locations (Final Summary)

```
./vendors/hantek/2d82/reference/dso2000/
├── 📄 FPGA_DISCOVERY.md              ← Full architecture comparison
├── 📄 FPGA_EXTRACTION_RESULTS.md     ← Extraction process details
├── 📄 OPEN_SOURCE_TOOLS_AVAILABLE.md ← This document
├── 🔧 psram_board_test.fs.bin        ← FPGA BITSTREAM (887 KB)
├── 🔧 spi-fpga-tn652.ko              ← SPI kernel driver
├── 🔧 fpga_i2c_kb.ko                 ← I²C control module
└── 📜 load_fpga_kb.sh                ← Loading sequence

<mirror>/YosysHQ/apicula/
└── 🛠️  Complete Gowin bitstream tools (apycula)

<workspace>/Hantek_DSO2000/
├── 📋 Schematics/Mainboard/          ← Hardware block diagrams
├── 📚 Gowin_GW2AR/                   ← FPGA technical documentation
└── 📄 Datasheets/                    ← Component specs
```

---

**Ready for next phase?**

Options:
1. **A** — Fix pip, run automated extraction (requires sandbox access)
2. **B** — Manual bitstream analysis (use Python script above + hex dump)
3. **C** — Study datasheets + schematics offline
4. **D** — Set up emulation/simulation environment
