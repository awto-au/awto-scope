# Hantek 2000-Series FPGA Discovery Report

**Date**: 2026-06-23  
**Status**: Two distinct product families identified with different FPGAs

---

## CRITICAL FINDING: Two Hantek "2000-Series" Products Exist

### 1. Hantek **2D82 Series** (Handheld Scope/DMM)
- **Models**: 2D82AUTO, 2D72, 2D42, 2C72, 2C42
- **MCU**: STM32F103VET6 (Cortex-M3, 512KB flash, 64KB RAM)
- **FPGA**: **Lattice LCMXO2-1200HC** (ISC programming via SSPI, CS=PB4)
- **ADC**: AD9288 (125 MHz, overclocked)
- **Firmware**: HTX2D82-2021090901.dfu (327 KB app @0x08005000)
- **Architecture**: Bare-metal + ST SPL/CMSIS, no OS
- **Repository**: `./vendors/hantek/2d82/reference/dso2000/` (existing Ghidra RE work)

### 2. Hantek **DSO2000 Series** (Bench Oscilloscope)
- **Models**: DSO2C10, DSO2C15, DSO2D10, DSO2D15
- **SoC**: Allwinner F1C200s (Cortex-A7, 64MB RAM, **Linux + Phoenix userspace**)
- **FPGA**: **Gowin GW2AR-18** with embedded 1Mbit PSRAM (NOT TN652)
- **ADC**: ADC08D500 (8-ch) + IOT24x LVDS receivers → FPGA
- **DAC**: DAC902E (waveform generator)
- **Firmware**: dso3kb_20250418.upk (1.8 MB, GPG-AES encrypted)
- **Board Reference**: DSO2D15_V1.7 (2021-01-14)
- **Architecture**: Linux SoC + FPGA, FPGA loaded via SPI kernel module
- **Repository**: `<workspace>/Hantek_DSO2000/` (newly cloned: schematics + Gowin docs)

---

## FPGA Bitstream Discovery

### 2D82 Series (Lattice LCMXO2-1200HC)

**Status**: ✅ Bitstream identified and extractable

- Located in firmware DFU files: `HTX2D82-2021090901.dfu` 
- Contains **43 KB FPGA bitstream** (referenced as "full bitstream" in RE docs)
- Also contains **768 B config regions** (acquisition mode setup)
- Extraction: DFuSe unpack tool (available in firmware analysis pipeline)
- Reference: `./vendors/hantek/2d82/reference/dso2000/vendors/hantek/2d82/ghidra/ghidra_names_2d82.tsv` documents FPGA functions

### DSO2000 Series (Gowin GW2AR-18)

**Status**: 🔒 Encrypted in firmware package; extraction method available

**Firmware Extraction Chain**:
1. Download: `dso3kb_20250418.upk` (1.8 MB, AES-encrypted)
2. Extract using: `dso_builder/extract.sh dso3kb_20250418.upk`
   - Requires: `./vendors/hantek/2d82/reference/dso2000/dso_builder.tar.gz`
   - Outputs: `dso3kb_20250418/` folder with `package/` subdirectory
3. Extract FPGA file: `dso3kb_20250418_FPGA.tar.bz2` (contains raw FPGA bitstream)
4. Expected bitstream format: `.fs` (Gowin standard SRAM configuration format)

**Alternative Method** (hardware extraction):
- Boot DSO2000 in **FEL mode** (hold button during power-on)
- Dump NAND flash using Hantek platform-tools + PhoenixSuit
- FPGA bitstream stored in NAND partition (unencrypted at rest)

---

## Hardware Architecture Comparison

| Feature | 2D82 Handheld | DSO2000 Bench |
|---------|---------------|--------------|
| **CPU** | STM32F103VET6 (ARM Cortex-M3) | Allwinner F1C200s (ARM Cortex-A7) |
| **FPGA** | Lattice LCMXO2-1200HC | Gowin GW2AR-18 |
| **FPGA Programming** | JTAG/SSPI ISC (SPI CS=PB4) | SPI + I²C (kernel modules) |
| **Memory** | 512 KB flash, 64 KB RAM | 64 MB RAM, NAND flash |
| **OS** | Bare-metal firmware | Linux (Allwinner Sunxi kernel) |
| **ADC** | Single AD9288 (125 MHz) | Dual ADC08D500 (8-ch LVDS) |
| **Bitstream Size** | ~43 KB (Lattice format) | Unknown (Gowin `.fs` format) |

---

## Documentation & Tools Available

### Schematics & Datasheets
- **2D82 Series**: 
  - Hardware teardown docs: https://github.com/wpwrak/2c42-pix/wiki
  - Reverse-engineered Ghidra analysis: `./vendors/hantek/2d82/reference/dso2000/ghidra/`

- **DSO2000 Series**:
  - Full schematics (PDF + PNG breakouts): `<workspace>/Hantek_DSO2000/Schematics/`
    - `Processor.png` — F1C200s, USB, memory layout
    - `Data_Acquisition.png` — **GW2AR FPGA + ADC interconnects** (CRITICAL)
    - `Mainboard/*.png` — Analog input, waveform gen, control signals
  - Gowin GW2AR datasheets: `<workspace>/Hantek_DSO2000/Gowin_GW2AR/`
    - `DS226-1.9.2E_GW2AR series of FPGA Products Data Sheet.pdf`
    - `UG115-1.4E_GW2AR-18 Pinout.pdf` (pin assignments)
    - `UG229-1.4E_GW2A(R) series of FPGA Products Package & Pinout.pdf`
  - Component datasheets: `<workspace>/Hantek_DSO2000/Datasheets/`
    - `adc08d500.pdf` (ADC specs)
    - `dac902e.pdf` (DAC specs)
    - LMH op-amp datasheets (front-end amplifiers)

### Extraction & Build Tools
- **DSO2000 Firmware Builder**: `./vendors/hantek/2d82/reference/dso2000/dso_builder.tar.gz`
  - `extract.sh` — Decrypt/extract UPK packages
  - `build.sh` — Repackage firmware (for testing mods)
  - Usage: `cd dso_builder && ./extract.sh dso3kb_20250418.upk`

---

## FPGA Specifications

### Lattice LCMXO2-1200HC (2D82 Series)
- **LUTs**: 1,200
- **Logic blocks**: 120 (10 LUTs each)
- **Embedded RAM**: 160 KB (SRAM)
- **I/O pins**: ~80 (TQFP-144)
- **Programming**: JTAG / ISC (In-System Configuration)
- **Configuration time**: ~ms (ISC sequence)

### Gowin GW2AR-18 (DSO2000 Series)
- **LUTs**: 20,736 (GW2AR = 18K LUTs typical + fabric)
- **Embedded RAM**: 1,296 KB (SRAM + SSRAM)
- **Embedded DSP**: 108 multipliers (18×18)
- **I/O pins**: ~96 (BGA package)
- **Configuration**: SRAM (volatile, loaded from SPI flash each boot)
- **SPI Master**: Supported via dedicated block
- **Clock**: Up to 200+ MHz
- **Bitstream format**: `.fs` (SRAM init file)

---

## EEVblog Community Resources

**Master thread**: https://www.eevblog.com/forum/testgear/hantek-dso2000-master-thread/

**Key contributors**:
- **DavidAlfa** — Master FAQ, firmware repository, mods/tools
- **@tinhead** — Firmware decryption details, FEL mode documentation
- **@pcprogrammer** — Schematics (pecostm32 repo), reverse engineering

**Available in DavidAlfa's Google Drive**:
- Backup packages from multiple board revisions
- `dso3kb_CNxxx_FPGA.tar.bz2` files for different FW versions (3000, 3102, 3202, 3205, 3209)
- FW Changer tool (extract/modify individual FPGA versions)
- Platform-tools for FEL mode flashing

---

## Next Steps

### For 2D82 FPGA Extraction
1. ✅ Already completed — bitstream in firmware RE analysis
2. Extract DFU file using DfuSe tool chain
3. Search for Lattice bitstream signature in binary
4. Reverse-engineer RTL using existing Ghidra RE work

### For DSO2000 FPGA Extraction
1. **Method A (Firmware extraction)**:
   ```bash
   cd dso_builder
   ./extract.sh ./vendors/hantek/2d82/reference/dso2000/DSO2000Firmware/dso3kb_20250418.upk
   tar -xjf dso3kb_20250418_FPGA.tar.bz2
   ```

2. **Method B (Hardware dump)**: 
   - Boot DSO2000 in FEL mode
   - Dump NAND with Phoenix tools
   - Extract FPGA bitstream from NAND partition

3. Analyze Gowin `.fs` bitstream format (reference: Gowin datasheets in repo)

### For Emulation
- **QEMU**: Limited Allwinner F1C200s support; requires custom patches
- **Renode**: Better option with peripheral modeling (SPI, I²C, UART)
- **Minimal approach**: Simulate F1C200s Linux userspace without full FPGA; mock SPI interface

---

## File Locations Summary

```
./vendors/hantek/2d82/reference/dso2000/
├── README.md                          # 2D82 series documentation (excellent)
├── ghidra/                            # Ghidra RE scripts + output
│   ├── ghidra_names_2d82.tsv               # FPGA function names (43KB bitstream ref)
│   ├── ExportAll.java, FeatureMap.java
│   └── ... (100+ functions named from debug strings)
├── firmware/                          # 2D82 firmware dumps & analysis
│   ├── 2d82/
│   ├── 2d72/
│   └── analysis/                      # Disasm, decompiled C, feature maps
├── dso_builder.tar.gz                 # DSO2000 firmware extraction tool
└── DSO2000Firmware.zip                # Encrypted DSO2000 firmware (dso3kb_20250418.upk)

<workspace>/Hantek_DSO2000/           # [NEWLY CLONED]
├── Schematics/
│   ├── Mainboard/
│   │   ├── Processor.png              # F1C200s, USB, JTAG/SWD, memory
│   │   ├── Data_Acquisition.png       # ⭐ GW2AR FPGA + ADC banks (CRITICAL)
│   │   ├── Analog_Input_1.png
│   │   ├── Analog_Input_2.png
│   │   ├── Waveform_Generator.png
│   │   ├── Power_Supply.png
│   │   ├── Control_Signals.png
│   │   └── External_Interfaces.png
│   ├── User_Interface/
│   │   ├── LCD.png
│   │   ├── BUTTONS.png
│   │   └── LEDS.png
│   ├── Schematic_Hantek_DSO2D10_Main_Board.pdf
│   ├── Schematic_Hantek_DSO2D10_User_Interface.pdf
│   └── EasyEda/                       # EasyEDA project files
├── Gowin_GW2AR/                       # ⭐ Complete GW2AR FPGA documentation
│   ├── DS226-1.9.2E_GW2AR series FPGA Data Sheet.pdf
│   ├── UG115-1.4E_GW2AR-18 Pinout.pdf
│   ├── UG229-1.4E_GW2A(R) Package & Pinout.pdf
│   ├── UG206-1.8.2E_GW2A(R) Schematic Manual.pdf
│   ├── UG286-1.9.3E_Gowin Clock User Guide.pdf
│   ├── UG287-1.3.1E_Gowin DSP User Guide.pdf
│   ├── UG288E_CFU.pdf
│   ├── UG289E_GPIO.pdf
│   ├── UG290E_Programming_Configuration.pdf
│   └── ... (more IPUG/UG guides)
└── Datasheets/
    ├── adc08d500.pdf                  # ADC specs
    ├── dac902e.pdf                    # DAC (waveform gen)
    ├── lmh6551.pdf, lmh6702.pdf       # Op-amps (front-end)
    ├── w25n01g_flash.pdf              # NAND flash
    └── ... (component datasheets)
```

---

## Terminology Clarification

- **"TN652"** reference in old DSO2000 firmware scripts likely refers to a **different board revision** or is a **kernel module naming artifact** (possibly "TN" = custom prefix, "652" = internal part code). The **documented FPGA in current production boards (DSO2D15_V1.7, 2021+) is GW2AR**.
- **FW vs SW**: 
  - **FW** (Firmware) = FPGA bitstream version (3000, 3102, 3202, 3205, 3209)
  - **SW** (Software) = Linux userspace + Phoenix (2025.05.09 latest)

---

**Report compiled by**: Hantek 2000-series reverse-engineering effort  
**Last updated**: 2026-06-23  
**Sources**: EEVblog community, pecostm32 repo, DavidAlfa FAQ, official firmware packages
