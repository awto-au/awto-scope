# Zephyr board: hantek_2d82 (STM32F103VET6 handheld scope)

A **clean-room starting point** for writing your own Zephyr firmware for the Hantek
2000-series (2D82AUTO / 2D72 / 2D42 / 2C72 / 2C42). Built from firmware RE — see
[docs/hantek/](../../../../docs/hantek/).

## What's SOLID (verified in firmware / community)
- **SoC = STM32F103VET6** → Zephyr `stm32f103Xe.dtsi` (exact match; FSMC present).
- **Flash layout:** 20 KB vendor **DFU bootloader @0x08000000** (leave it — it's how
  you flash: F1+Power = DFU entry), your **app @0x08005000**. Partitions in the dts.
  This is the default (Option B, plain `west build`). An MCUboot variant (Option C,
  app @0x0800C000, `west build --sysbuild -- -DSNIPPET=mcuboot-hantek_2d82 ...`) is
  also buildable — see `../../../../docs/hantek/flash-layout.md` for both.
- **USB FS device** present (DFU + the already-RE'd control protocol — VID 0x0483
  PID 0x2d42, see hkoosha/hanteker `hanteker_lib`).
- **TTL UART debug port** exists (the "Program Arm1" 3.3 V header) → console.

UART note: keep the console on USART1 (PA9/PA10), but baud rate does not need to
mirror OEM settings. 115200 is a known-good default, and higher/lower rates are
valid for development if host tooling is configured to match.

USB diagnostics note: app now exposes a USB CDC ACM diagnostic serial port
(`CDC_ACM_0`) in parallel with the UART console. Diagnostic format is runtime-
switchable from shell using `usbdiag fmt native` or `usbdiag fmt oem`.

DMA note: default `prj.conf` keeps DMA off for Renode-safe bring-up. To test DMA-backed
paths, build with `code/app/prj_dma.conf` via `-DEXTRA_CONF_FILE=prj_dma.conf`.
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
