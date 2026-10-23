# awto-scope

Open firmware and FPGA gateware for the Hantek 2D82 handheld scope (STM32F103VET6 + Lattice MachXO2).

## Contents

| Path | What |
|------|------|
| [code/app/](code/app/) | Zephyr application and `hantek_2d82` board definition |
| [code/a2k_board/](code/a2k_board/) | Zephyr board support: LCD (ILI9341 over FSMC), FPGA SPI, DMM, keypad, USB |
| [docs/hantek/](docs/hantek/) | Hantek 2000-series reverse-engineering notes |
| [code/lib/](code/lib/) | Shared library code |
| [fpga/hantek/rtl/](fpga/hantek/rtl/) | Amaranth gateware: `aw2_gateware` top, capture core, interleave calibration, self-tests |
| [fpga/hantek/sim/](fpga/hantek/sim/) | Socket co-simulation of the gateware for Renode |

## Status

- Zephyr application and board support for the `hantek_2d82` board. Builds against the Zephyr tree (not verified in this repo).
- Gateware self-checks pass in the Amaranth simulator.
- No hardware bring-up of the gateware yet.

## Build

- Firmware: `code/app` is the Zephyr application; `code/a2k_board` and `code/lib` are the Zephyr modules it builds against. Needs a west workspace with the Zephyr tree.
- Gateware: `python3 fpga/hantek/rtl/aw2_gateware.py` runs the self-check and regenerates `aw2_gateware.v`. Requires Amaranth.
