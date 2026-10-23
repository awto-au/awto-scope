# Hantek FPGA Extraction Results

**Date**: 2026-06-23  
**Status**: ✅ Successfully extracted DSO2000 FPGA bitstream

---

## DSO2000 FPGA Extraction — ✅ COMPLETE

### Extracted File
**Path**: `./vendors/hantek/2d82/reference/dso2000/dso_builder/20250418/package/fpga/psram_board_test.fs.bin`

**Size**: 887 KB (907,776 bytes)  
**Type**: Gowin GW2AR SRAM Configuration File (`.fs.bin` format)  
**Configuration**: `psram_board_test` (likely test/diagnostic configuration)  
**Format**: Binary Gowin SRAM init file (bitstream)  

### Extraction Process

1. **Source firmware**: `dso3kb_20250418.upk` (1.8 MB, GPG-AES encrypted)

2. **Decryption key**: `dso3000c` (hardcoded in dso_builder scripts)

3. **Tool used**: `dso_builder/extract.sh`
   ```bash
   cd ./vendors/hantek/2d82/reference/dso2000
   tar -xzf dso_builder.tar.gz
   cd dso_builder
   bash extract.sh /tmp/DSO2000Firmware/dso3kb_20250418.upk
   ```

4. **Output directory**: `20250418/` (created by extract.sh)
   ```
   20250418/
   ├── package/
   │   ├── fpga/
   │   │   └── psram_board_test.fs.bin  ← FPGA BITSTREAM
   │   ├── spi-fpga-tn652.ko            ← SPI kernel driver
   │   ├── fpga_i2c_kb.ko               ← I²C control module
   │   ├── tp-adc.ko                    ← ADC driver
   │   ├── load_fpga_kb.sh              ← Loading script
   │   └── root/dso/app/                ← Linux userspace
   └── upend                            ← Version metadata
   ```

### Loading Sequence (from `load_fpga_kb.sh`)

```bash
#!/bin/sh

# 1. Load SPI FPGA driver (kernel module)
insmod /dso/etc/spi-fpga-tn652.ko

# 2. Load bitstream via SPI to FPGA
/dso/app/cfg_fpga /dev/tn652_fpga_cfg psram_board_test.fs.bin 

# 3. Load I²C control module (after FPGA is configured)
insmod /dso/etc/fpga_i2c_kb.ko

# 4. Mount USB interfaces
/dso/etc/usb_bord_mount.sh 
/dso/etc/insmod_usb_driver 
```

### Kernel Module Details

| Module | Type | Arch | Build Hash | Purpose |
|--------|------|------|-----------|---------|
| `spi-fpga-tn652.ko` | ELF 32-bit ARM EABI5 | Cortex-A7 | b84af2e1... | **SPI driver for FPGA configuration** (TN652 = kernel module name, not FPGA part #) |
| `fpga_i2c_kb.ko` | ELF 32-bit ARM EABI5 | Cortex-A7 | 064eceb3... | I²C control interface for FPGA register access |
| `tp-adc.ko` | ELF 32-bit ARM EABI5 | Cortex-A7 | ec2665e0... | ADC (touchpanel/test) driver |

### Critical Finding: "TN652" Clarification

- **"TN652" is NOT the FPGA part number** — it's the **Linux kernel module name** (`spi-fpga-tn652.ko`)
- **Actual FPGA**: Gowin **GW2AR-18** (as confirmed by schematics and EasyEDA files)
- **SPI Interface**: Generic SPI master to load bitstream (standard for Gowin SRAM-based FPGAs)

---

## 2D82 FPGA Extraction — 🔄 IN PROGRESS

### Source Files Located
- **Main firmware**: `./vendors/hantek/2d82/firmware/2d82-20210818/flash_08005000.bin` (327 KB)
- **Bootloader**: `./sources/firmware/flash_08000000.bin` (64 KB, includes bootloader)
- **DFU package**: `./vendors/hantek/2d82/firmware/2d82-20210818/HTX2D82-2021090901.dfu`

### Known Information
- **FPGA Type**: Lattice LCMXO2-1200HC
- **Bitstream Size**: ~43 KB (documented in Ghidra RE notes)
- **Programming Interface**: JTAG/SSPI (CS=PB4)
- **Embedded in**: Firmware app section (@0x08005000)
- **Format**: Lattice ISC format (custom MachXO2 commands)

### Ghidra Notes Reference
From `./vendors/hantek/2d82/reference/dso2000/vendors/hantek/2d82/ghidra/ghidra_names_2d82.tsv`:

```
SSPI bitstream programming (CS=PB4): MachXO2 ISC cmds 0x0E/0x26/0x46/0x47/0x70/0x5E/0x79.

fw_fpga_write_config(sel): writes a config block to MachXO2 via SSPI ISC sequence.
- Writes a SMALL config region (768B), not the full 43KB bitstream
- Likely acquisition-mode setup or config-page update

fw_fpga_config_restore: config-mode block restore
- Calls fw_machxo2_refresh_rti + ISC erase + memcpy of bitstream
- Programming path only
```

### Extraction Challenge
The 43 KB bitstream is embedded in the firmware app binary but not trivially separable without:
1. **Reverse-engineering the DFU unpack**: Ghidra script to find bitstream data section
2. **Lattice ISC command sequence analysis**: Extract the binary blob from compiled ISC routine
3. **Hardware extraction alternative**: Read STM32F103 flash via ST-Link (full 512 KB dump) and locate bitstream

### Next Steps for 2D82 Extraction
1. Use Ghidra to find memory references to the 43 KB bitstream
2. Look for alignment markers or magic bytes in firmware binary
3. Or: Use existing 512 KB ST-Link dump from community (`27D72_dump.bin`) to identify bitstream signature
4. Extract via Lattice tools if found (`.bit` → `.jed` conversion if needed)

---

## File Locations Summary

```
./vendors/hantek/2d82/reference/dso2000/

dso_builder/
├── extract.sh                    ← Decryption tool
├── 20250418/                     ← ✅ EXTRACTION OUTPUT
│   ├── package/
│   │   ├── fpga/
│   │   │   └── psram_board_test.fs.bin    ✅ **GOWIN BITSTREAM (887 KB)**
│   │   ├── spi-fpga-tn652.ko     ← SPI driver
│   │   ├── fpga_i2c_kb.ko        ← I²C control
│   │   └── load_fpga_kb.sh       ← Loading script
│   └── upend

firmware/
├── 2d82/
│   ├── flash_08005000.bin        ← App @ 0x08005000 (contains embedded 43 KB bitstream)
│   ├── HTX2D82-2021090901.dfu    ← DFU package
│   └── analysis/
│       ├── decompiled.c          ← Ghidra output (FPGA functions documented)
│       └── flash_08005000.bin    ← Analysis copy
├── flash_08000000.bin            ← Bootloader @ 0x08000000 (512 KB total)
└── 27D72_dump.bin                ← Community ST-Link dump (512 KB raw, includes bootloader)

ghidra/
├── ghidra_names_2d82.tsv              ← FPGA function names (includes bitstream refs)
├── CheckData.java                ← Bitstream pointer extraction script
└── [RE analysis tools]
```

---

## Bitstream Specifications

### Gowin GW2AR-18 Bitstream (DSO2000)
- **File format**: `.fs.bin` (SRAM configuration)
- **Size**: 887 KB
- **Configuration name**: `psram_board_test`
- **Loading method**: SPI master (kernel module `spi-fpga-tn652.ko`)
- **Load time**: ~ms (typical for SPI-based SRAM config)
- **Gowin tools**: Can view/modify with Gowin EDA IDE
- **Reverse engineering**: Gowin format partially documented (see `<workspace>/Hantek_DSO2000/Gowin_GW2AR/`)

### Lattice LCMXO2-1200HC Bitstream (2D82)
- **File format**: Lattice ISC (In-System Configuration)
- **Size**: ~43 KB (estimated, not yet extracted)
- **Programming interface**: JTAG / SSPI (CS=PB4)
- **ISC commands**: 0x0E, 0x26, 0x46, 0x47, 0x70, 0x5E, 0x79
- **Embedded location**: In STM32 firmware at offset 0x08005000+
- **Reverse engineering**: Lattice ispLEVER tools can parse ISC format if extracted

---

## Verification & Testing

### DSO2000 FPGA Bitstream Validation

**File integrity**:
```bash
ls -lh ./vendors/hantek/2d82/reference/dso2000/dso_builder/20250418/package/fpga/psram_board_test.fs.bin
# -rwxr--r--. 1 dan dan 887K Mar 26  2024 psram_board_test.fs.bin

md5sum ./vendors/hantek/2d82/reference/dso2000/dso_builder/20250418/package/fpga/psram_board_test.fs.bin
# [compute hash for archive]
```

**To analyze bitstream**:
1. Install Gowin EDA IDE
2. Open bitstream in EDA for inspection (shows RTL blocks, memories, peripherals)
3. Compare with reference designs in `<workspace>/Hantek_DSO2000/Gowin_GW2AR/`

---

## Next Phase: Analysis & Emulation

### Reverse Engineering Approaches

1. **Static Analysis** (no hardware needed):
   - Extract Gowin GW2AR bitstream structure (`.fs.bin` format)
   - Use Gowin EDA IDE to visualize logic blocks and interconnects
   - Document ADC interface, SPI slave (for kernel module), I²C slave blocks

2. **Functional Analysis**:
   - Compare bitstream against Gowin reference designs (UART, SPI, I²C, memory controller)
   - Cross-reference with hardware schematics to identify signal mappings
   - Document FPGA → ADC → Linux kernel data path

3. **Emulation** (for testing):
   - Load bitstream into Gowin simulation tools
   - Mock SPI/I²C interfaces with test vectors
   - Validate ADC sample path without hardware

4. **Hardware Validation**:
   - If DSO2000 device available: capture SPI waveforms during FPGA load
   - Measure FPGA pin activity (Analyzer or DSO)
   - Confirm kernel module driver behavior

---

## Archive & Preservation

✅ **Extracted FPGA files preserved at**:
- Working copy: `./vendors/hantek/2d82/reference/dso2000/dso_builder/20250418/`
- Backup: Consider moving to `<workspace>/debris/` after analysis complete

✅ **Documentation**:
- This report: `FPGA_EXTRACTION_RESULTS.md`
- Discovery report: `FPGA_DISCOVERY.md`
- Schematic reference: `<workspace>/Hantek_DSO2000/Schematics/`
- Gowin FPGA docs: `<workspace>/Hantek_DSO2000/Gowin_GW2AR/`

---

**Report compiled by**: Hantek reverse-engineering workflow  
**Extraction date**: 2026-06-23  
**Status**: DSO2000 ✅ Complete | 2D82 🔄 In Progress
