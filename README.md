# awto-scope

Open firmware and FPGA gateware for the Hantek 2D82 handheld scope (STM32F103VET6 + Lattice MachXO2).

## Contents

| Path | What |
|------|------|
| [code/a2k_board/](code/a2k_board/) | Zephyr board support: LCD (ILI9341 over FSMC), FPGA SPI, DMM, keypad, USB |
| [code/lib/](code/lib/) | Shared library code |
| [fpga/hantek/rtl/](fpga/hantek/rtl/) | Amaranth gateware: `aw2_gateware` top, capture core, interleave calibration, self-tests |
| [fpga/hantek/sim/](fpga/hantek/sim/) | Socket co-simulation of the gateware for Renode |

## Status

- Board support library for the Zephyr `hantek_2d82` board. No application is included in this repo yet.
- Gateware self-checks pass in the Amaranth simulator.
- No hardware bring-up of the gateware yet.

## Build

- Firmware: `code/a2k_board` is a Zephyr module, built by an application that lives outside this repo.
- Gateware: `python3 fpga/hantek/rtl/aw2_gateware.py` runs the self-check and regenerates `aw2_gateware.v`. Requires Amaranth.

## Not included

Vendor firmware dumps, bitstreams, vendor tools, manuals and datasheets, and generated Verilog.
