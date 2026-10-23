# awto-scope

Open firmware and FPGA gateware for the Hantek 2D82 handheld scope (STM32F103VET6 + Lattice MachXO2).

## Contents

| Path | What |
|------|------|
| [code/app/](code/app/) | Zephyr application and `hantek_2d82` board definition |
| [code/a2k_board/](code/a2k_board/) | Zephyr board support: LCD (ILI9341 over FSMC), FPGA SPI, DMM, keypad, USB |
| [code/lib/](code/lib/) | Shared library code |
| [fpga/hantek/rtl/](fpga/hantek/rtl/) | Amaranth gateware: `aw2_gateware` top, capture core, interleave calibration, self-tests |
| [fpga/hantek/sim/](fpga/hantek/sim/) | Socket co-simulation of the gateware for Renode |
| [docs/hantek/](docs/hantek/) | Hantek 2000-series reverse-engineering notes |

## Status

- Firmware builds for the `hantek_2d82` board and boots in Renode.
- Gateware self-checks pass in the Amaranth simulator.
- No hardware bring-up yet.

## Build the firmware

Needs the [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html) and a west workspace on Zephyr `main`. Last built against `v4.4.0-18653-g02a337c21da4` (2026-10-24).

```bash
west init zephyrproject
cd zephyrproject && west update
west build -b hantek_2d82 /path/to/awto-scope/code/app --build-dir /path/to/awto-scope/tmp/app_build
```

`code/app` adds its own board (`boards/`), snippets and the `code/a2k_board` and `code/lib` modules, so no other setup is needed. Flash layout and flashing: [docs/hantek/flash-layout.md](docs/hantek/flash-layout.md).

## Run the gateware checks

Needs [Amaranth](https://amaranth-lang.org/).

```bash
python3 fpga/hantek/rtl/aw2_gateware.py          # self-check; regenerates aw2_gateware.v
python3 fpga/hantek/sim/test_scope_sim_server.py # socket co-sim check
```

## License

[GPL-3.0](LICENSE).
