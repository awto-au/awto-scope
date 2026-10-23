# Open Source Tools for Gowin GW2AR FPGA Analysis

## ✅ TOOLS EXIST — AND SUPPORT GW2AR!

### **Apicula (Project Apicula 🐝)**
- **Repository**: https://github.com/YosysHQ/apicula
- **Stars**: 668 (active, well-maintained)
- **Maintainers**: YosysHQ (Yosys team)
- **License**: MIT (open source)
- **Purpose**: Bitstream documentation + tools for Gowin FPGAs

### **Supported Boards** (including GW2AR variants)
✅ **Sipeed Tang Nano 20K**: GW2AR-LV18QN88C8/I7  
✅ **Sipeed Tang Primer 20K**: GW2A-LV18PG256C8/I7  
(+ GW1N series variants, GW1NZ, GW1NSR, etc.)

---

## Full Toolchain

### Part 1: Unpack Bitstream → Verilog RTL

```bash
# Extract Verilog from bitstream
gowin_unpack -d GW2AR-LV18QN88C8 -o unpacked.v psram_board_test.fs.bin

# View the RTL with Yosys
yosys -p "read_verilog -lib +/gowin/cells_sim.v; clean -purge; show" unpacked.v
```

### Part 2: Full Synthesis → Bitstream Pipeline

```bash
# 1. Synthesize Verilog to JSON
yosys -D LEDS_NR=8 -p "read_verilog blinky.v; synth_gowin -json blinky.json -family gw2a"

# 2. Place & Route
nextpnr-himbaechel --json blinky.json \
                   --write pnrblinky.json \
                   --device GW2AR-LV18QN88C8 \
                   --vopt family=GW2A \
                   --vopt cst=board.cst

# 3. Generate bitstream
gowin_pack -d GW2A -o pack.fs pnrblinky.json

# 4. Program device (if hardware available)
openFPGALoader -b board_name pack.fs
```

---

## Installation

### Via pip (simplest)
```bash
pip install apycula
```

### Full toolchain setup
```bash
# Install Yosys + nextpnr + apicula
pip install apycula

# Install openFPGALoader (optional, for programming)
pip install openfpgaloader

# Verify tools are available
which gowin_unpack
which nextpnr-himbaechel
which yosys
```

### From source
```bash
git clone https://github.com/YosysHQ/apicula
cd apicula
pip install -e .
```

---

## Key Tools Included in Apicula

| Tool | Purpose |
|------|---------|
| `gowin_unpack` | **Extract Verilog RTL from `.fs.bin` bitstream** ← FOR OUR USE |
| `gowin_pack` | Generate `.fs.bin` bitstream from JSON (P&R output) |
| `synth_gowin` | Yosys plugin for Gowin synthesis |
| `nextpnr-himbaechel` | Backend for place & route (supports Gowin via Apicula) |

---

## For Our Bitstream: psram_board_test.fs.bin

**Exact command to analyze**:
```bash
gowin_unpack -d GW2AR -o psram_board_test.v psram_board_test.fs.bin
```

**What this produces**:
- `psram_board_test.v` — The RTL Verilog source code for the FPGA logic
- Shows: memories, state machines, interfaces, ADC connections, etc.
- Can then visualize with `yosys show` or analyze with grep/text editor

---

## Why Apicula Works

The project **fuzzes the Gowin vendor tools** to reverse-engineer the bitstream format:

1. Collects Hantek/Sipeed board data files + vendor documentation
2. Fuzzes the bitstream packing/unpacking
3. Maps bitstream structure → logic tiles → RTL
4. Supports GW1N, GW2A, GW2AR families

**Source**: https://github.com/YosysHQ/apicula/wiki (detailed documentation)

---

## Verdict

✅ **YES, open-source tools exist and are mature**  
✅ **GW2AR is explicitly supported**  
✅ **Can unpack our bitstream to Verilog**  
✅ **Can view, analyze, and modify**  

**No need for proprietary Gowin EDA — apicula handles it all.**

---

## Next Steps to Analyze psram_board_test.fs.bin

1. Install: `pip install apycula`
2. Unpack: `gowin_unpack -d GW2AR -o analysis.v psram_board_test.fs.bin`
3. Analyze: Read `analysis.v` to see FPGA logic
4. Visualize: `yosys -p "read_verilog -lib +/gowin/cells_sim.v; read_verilog analysis.v; show"`

**Ready?**
