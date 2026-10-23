# Zephyr board: hantek_2d82 (STM32F103VET6 handheld scope)

A **clean-room starting point** for writing your own Zephyr firmware for the Hantek
2000-series (2D82AUTO / 2D72 / 2D42 / 2C72 / 2C42). Built from firmware RE — see
`../../../vendors/hantek/2d82/docs/hardware/devices/hantek-2000-IO-MAP.md`.

## What's SOLID (verified in firmware / community)
- **SoC = STM32F103VET6** → Zephyr `stm32f103Xe.dtsi` (exact match; FSMC present).
- **Flash layout:** 20 KB vendor **DFU bootloader @0x08000000** (leave it — it's how
  you flash: F1+Power = DFU entry), your **app @0x08005000**. Partitions in the dts.
- **USB FS device** present (DFU + the already-RE'd control protocol — VID 0x0483
  PID 0x2d42, see hkoosha/hanteker `hanteker_lib`).
- **TTL UART debug port** exists (the "Program Arm1" 3.3 V header) → console.
- **FSMC used for ONE device = the LCD** (parallel TFT @0x60000000); init in
  `FUN_08019488`/`FUN_08019562`.
- **FPGA = GPIO/SPI, NOT memory-mapped** (heavy GPIO use; corrected finding).
- **Peripheral usage (literal-pool refs):** GPIOA-E heavy, SPI1 (2), FSMC, TIM2 (52).

## What's PLACEHOLDER — you must trace the board to fill
Every **pin number** in the dts is a guess `[?]`. To make it real:
1. **GPIO init** — read the firmware's GPIO/RCC setup (the `0x40010800..0x40011800`
   accesses) to get which ports/pins are outputs/inputs → fills keys, FPGA ctrl,
   LCD control, DMM.
2. **Keypad matrix** — F1-F4/arrows/Power scan (and/or the 74x595 shift register).
3. **FPGA protocol** — the GPIO/SPI handshake + register map (sample-rate, trigger,
   AWG waveform table). This is the big custom driver (`hantek,machxo2-ctrl`).
4. **LCD** — the panel controller (reads `pGLCD_ID`) + FSMC timings.
5. **CS7721 DMM** — the serial read + range/function GPIOs.
6. **Clock tree** — HSE freq + PLL (assume 8 MHz HSE → 72 MHz until traced).

## Custom drivers to write (bindings referenced in the dts)
- `hantek,machxo2-ctrl` — FPGA config (JTAG bitstream) + runtime register access.
- `hantek,cs7721-dmm` — the dual-slope DMM read.
- LCD display driver (parallel TFT on FSMC).

## Build (once pins are filled)
```
west build -b hantek_2d82 <your-app>
# flash via the vendor DFU (F1+Power), or ST-Link SWD to 0x08005000
```

## ⚠️ Reality check
The original firmware is bare-metal (no Zephyr). This dts lets you **start fresh in
Zephyr**, reusing the hardware. The AWG/auto features are gated in the *original* fw
by a model byte — in your own firmware you control everything (subject to the AWG
hardware — 3PD5651E DAC + EL5166 amp — being populated on C-models).
