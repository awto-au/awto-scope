# Mod: program the FPGA to do REAL CAN decoding (the good idea)

## The opportunity
The 2D82AUTO's "CAN Bus" menu is **fake** -- just scope presets, no decoder
(firmware-verified: 0 decoder keywords). BUT the board has a **Lattice MachXO2 FPGA
on the capture path** -- and **CAN protocol decoding is a textbook FPGA task.** So you
can turn a "CAN waveform viewer" into an **actual CAN protocol analyzer** the OEM
never shipped.

## Why it's a strong fit
- **CAN decode is small + standard:** bit sampling -> de-stuffing -> frame parse
  (SOF/ID/DLC/data/CRC/ACK) -> error flags. ~300-800 LUTs. The **MachXO2-1200 has
  1280 LUTs + ~8KB EBR** -- comfortably fits alongside the existing capture logic.
- **The FPGA already sees the raw signal** and runs at the sample clock -- it can
  decode in real time, which the 72 MHz Cortex-M3 cannot.
- **Hardware is ideal:** 2 analog channels (CAN-H / CAN-L differential), the AD9288
  front-end, a 240x320 ILI9341 screen -- everything an analyzer needs.
- **Ready IP to start from:** OpenCores CAN controllers, sigrok's `can`/`can2`
  protocol-decoder logic as a reference model, published FPGA CAN cores.

## What it would take (honest)
1. **Write your own FPGA bitstream** -- we only have the compiled vendor bitstream
   (no source). So: reproduce the capture path + add the CAN decoder, in Lattice
   Diamond/Radiant, flash via the "Force update FPGA" route. Real FPGA work, but CAN
   decode is well-trodden and small.
2. **Own MCU firmware** to read decoded frames from the FPGA and render them -- which
   you're doing anyway in the clean-room **Zephyr** build (see ../../zephyr/).
3. Define the FPGA->MCU register/FIFO interface for decoded frames.

## Why this is the RIGHT synthesis
Instead of fighting the vendor's thin automotive layer, **build a real protocol
analyzer on their cheap hardware:** FPGA decodes, Zephyr firmware displays, and the
decoded frames feed the **canapple bus-fingerprinting catalog** -- the "this looks
like a Bentley body-control bus" goal. Extends naturally to LIN, K-line, FlexRay,
SENT, etc. (all small FPGA decoders). The FPGA goes from "opaque blocker" to **the
asset that makes the device genuinely useful.**

## Caveats
- Deep capture (external RAM) is a separate, harder mod (see hantek-2000-LCD-FPGA.md)
  -- CAN decode does NOT need deep memory (it streams frames out, tiny).
- Analog front-end: CAN is 2.0-3.5V differential; the scope's input path + the AD9288
  digitize it fine for decode (you're sampling the bus voltage, then thresholding in
  the FPGA). Verify input range/threshold.

## Bandwidth: CAN is EASY (the numbers)
**Why streaming decoded CAN is trivial -- even over USB Full Speed:**
- **USB on the STM32F103 = Full Speed = 12 Mbit/s signaling ~ ~1 MB/s usable**
  (NOT 12 MHz/MB -- it's megaBITS; ~1 MB/s after overhead, no High-Speed on F103).
- **CAN data rates are tiny:** HS-CAN maxes at **1 Mbit/s** wire (most auto = 500
  kbit/s); after stuffing+framing, real payload ~30 KB/s @500k, <100 KB/s even
  saturated @1M.
- => **USB FS (~1 MB/s) has 10-30x the headroom** to stream decoded CAN frames.

**The trick that makes it easy:** you stream **decoded frames** (ID+DLC+<=8 data ~ 16
bytes/frame), NOT the raw waveform. Raw ADC is 250 MB/s (250x over USB -- would
choke); decoded frames are ~30-100 KB/s (well under USB FS). FPGA decodes -> compact
frames -> USB -> host. Trivial bandwidth-wise.

## Toolchain (installed + verified)
- **[Amaranth HDL](https://github.com/amaranth-lang/amaranth)** 0.5.8 -- `LatticeMachXO2Platform` confirmed available (targets our exact chip).
- **Yosys** + **nextpnr-machxo2** + **Project Trellis** -- installed as `yowasp-yosys` + `yowasp-nextpnr-machxo2` (verified: nextpnr v0.10). Full Verilog/Amaranth -> MachXO2 `.bin` flow, open-source, no Lattice Diamond.
- **openFPGALoader** -- JTAG flash / extract, to install when wiring to hardware.

## Progress: CAN decoder core written + simulated (2026-06-20)
- `vendors/hantek/2d82/reference/dso2000/fpga/gateware/can_rx.py` -- `CANDecoder` (CAN 2.0A standard 11-bit: SOF -> ID -> RTR/IDE/r0 -> DLC -> data -> CRC-15, with bit de-stuffing), `CANBitSampler` (RX-line bit recovery + resync), and `CANRx` top.
- `vendors/hantek/2d82/reference/dso2000/fpga/gateware/test_can_rx.py` -- self-checking Amaranth sim: pure-Python CAN frame builder (CRC-15 + bit stuffing) makes stimulus AND expected values. Run: `python3 test_can_rx.py`.  
  Verified: 4 frames (incl. all-ones/all-zeros stuffing stress) decode with all fields + CRC OK, through both bit-strobe and full-sampler paths.
- `CANRx` elaborates to RTLIL (~53 KB) -- synthesizable, not sim-only.

## TODO (remaining for end-to-end)
- Per-byte data extraction for the host/MCU side
- Extended (29-bit) frames
- MCU<->FPGA register/FIFO interface for decoded frames
- Capture path: AD9288 -> threshold -> feed `CANRx.rx`
- Synthesize end-to-end to a `.bin` and flash

**Possible shortcut -- the STM32F103 has a built-in bxCAN controller** (@0x40006400).
IF CAN-H/L were routed to a CAN transceiver on the MCU's CAN pins, the STM32 decodes
CAN in *hardware* -- no FPGA needed for CAN at all. (Checked this firmware: bxCAN base
refs = see commit; the scope path digitizes via the analog front-end/FPGA, so the
stock board likely doesn't wire bxCAN -- but a hardware mod adding a transceiver to
the MCU CAN pins is an even simpler route than reprogramming the FPGA.)
