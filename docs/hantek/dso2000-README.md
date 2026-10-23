# Hantek 2000-series firmware corpus + analysis pipeline

Systematic reverse-engineering of the Hantek 2000-series handheld scopes
(**2D82AUTO, 2D72, 2D42, 2C72, 2C42** — they share one "Hantek2000" firmware).

## ⚠️ PRIOR ART — much is ALREADY DONE (don't redo)
- **USB control protocol = FULLY reverse-engineered:** https://github.com/lucaoli/Hantek
  (C/libusb). VID `0x0483`, PID `0x2d42`; OUT=EP2 IN=EP1 bulk; **9-byte command frame**
  `[idx=0x00][0x0A][func][cmd][val..][0x00]`; func codes scope=0x0000, capture=0x0100,
  AWG=0x0002, screen=0x0003. Rust port: https://github.com/hkoosha/hanteker (+ _gui).
  → So our `FUN_080308de`/`FUN_08032ea0` USB dispatcher is already documented; use these.
- **Hardware fully documented:** https://github.com/wpwrak/2c42-pix/wiki — confirms
  **STM32F103VET6** MCU (← our CPU, exact part), **Lattice LCMXO2-1200HC FPGA**,
  **AD9288 ADC** (overclocked ~125 MHz), **Semic CS7721C DMM**, 74x595, P028H026 LCD.
- **RE hub + extraction:** https://github.com/circuit-specialists/Hantek-2D72 (ST-Link
  SWD readout, DFU recovery, hw mods, the 512 KB dump + the 2019 firmware we grabbed).
- **EEVblog megathread:** https://www.eevblog.com/forum/testgear/hantek-2c422c722d422d72/
  — firmware identical across 2C42/2C72/2D42/2D72 (model = a stored name byte);
  cross-flash to 2D82auto via DfuSe/"Force update ARM+FPGA" unlocks the menu presets,
  but **bandwidth (40→70 MHz) is hardware-binned** (AD9288/analog), not a pure sw unlock.
- **Version history (date codes):** eediscuss tid=13676 lists 11 releases 2018-11 →
  2021-09 (older .dfu binaries gated behind forum login; not on Wayback).

## firmware/  (the corpus, standard layout)
- `2d82/` — `HTX2D82-2021090901.dfu` (latest, 327 KB app @0x08005000)
- `2d72/` — `HantekHTX2021090901.dfu` (latest shared, 247 KB)
- `_older-from-github/` — `HantekHTX2019082801.dfu` (2019, 2 yrs older) +
  `27D72_dump.bin` (**512 KB raw ST-Link dump from 0x08000000 — includes the
  bootloader** the DfuSe files lack).
Each gets an `analysis/` with: `flash_*.bin`, `disassembly.asm`, `decompiled.c`,
`feature_map.txt`, `strings.txt`, `debris.txt`, `info.txt`.

## scripts/decompile.py  (repeatable pipeline)
One command re-runs the whole thing on any firmware: unwrap DfuSe → Ghidra
(ARM:LE:32:Cortex) disasm+decompile → string-anchored feature→func map →
strings + entropy/debris report. Idempotent (`--force` to redo, `--only <substr>`).
`ExportAll.java` / `FeatureMap.java` are the Ghidra post-scripts.

## Findings so far
- **CPU:** STM32F103VET6 (confirmed via wpwrak teardown) — Cortex-M3, 512 KB flash,
  64 KB RAM. App @0x08005000 (20 KB custom DFU bootloader: "STM32 will enter dfu mode").
- **Build:** bare-metal register C, no HAL/SPL/RTOS, custom graphics (GLCD), 729 funcs.
- **Feature tree** (debris @0x08047000): `1.Vehicle Diagnosis / 2.Oscilloscope / 3.AWG /
  4.DMM`; automotive submenu `Ignition / Sensors / Bus Diagnosis / Engine /
  Startup&Charge` + 102 named test presets.
- **Key funcs:** USB cmd parser `FUN_080308de`/`FUN_08032ea0`; calibration hub
  `FUN_08031c40` (`volt_cali_data[ch][range][pt]`); ADC core `FUN_08018c18`.
- **Debris:** low-entropy data regions = menu/preset tables + font/glyph data; the raw
  dump shows the bootloader + 74 erased tail blocks (unused flash). No encrypted blobs
  (0 high-entropy regions) — all in the clear.

## Security posture — what we actually know (and what we DON'T)
**Correction:** an earlier draft claimed "RDP is off, chip wide open." That over-read
the evidence. The honest separation:

- **What WE have = the official `.dfu` from hantek.com.** We obtained it by *downloading*
  — **no SWD/ST-Link involved.** A vendor publishing its own firmware update is normal and
  says **NOTHING about chip Read-Out Protection (RDP).** It only tells us Hantek ships the
  update **unencrypted/unsigned** (entropy ~6.1, plain DfuSe) — i.e. the *image* is open,
  the *update channel* takes plain `.dfu`.
- **Custom bootloader exists** (0x08000000, 13 KB, "STM32 will enter dfu mode") — so the
  bootloader is real, not forgotten.
- **RDP / SWD-readability = UNKNOWN from our work.** We did NOT read the chip. There is a
  third-party 512 KB `27D72_dump.bin` (circuit-specialists) *described* as an ST-Link
  readout — IF genuine it would imply RDP was off on that unit, but that's their claim on
  their chip, not something we verified. Do not assert it.
- **To actually know:** put an ST-Link on a real device and try to read flash / read the
  FLASH_OBR option-byte RDP field. Only that settles whether SWD dumping works.

Net: the firmware *image* and the *DFU update path* are open (that's why we have it +
why community reflashing works). On-chip read protection is **untested by us** — open
question, not a stated fact.

## Function naming (firmware RE)
`scripts/name_funcs.py` names firmware functions from their debug strings: extract
all strings → find ARM literal-pool pointers (4-byte words == a string addr) → map to
the containing function → name it. **100 of 729 functions named** (`<fw>/analysis/
func_names.txt`, also in funcdb `name` column). Ceiling: 100 named + 241 tiny/liblike
stubs (no string, unnameable) = 47%; the rest have no debug string (internal helpers,
nameable only by call-graph inference). High-value names recovered: `fw_333_MachXO2_
BUSY` (FPGA programming), `fw_LCD_ID` (ILI9341 detect), `fw_*USB_R_Buffer*` (USB cmd
parser), `fw_CaliParam_volt_cali_data` (cal table), `fw_UNInter_acq_points_num`
(capture), `fw_Calibrating`, `fw_ht_meas_pre_process`.

## Library identification (CORRECTED — it IS ST SPL/CMSIS, not raw)
Earlier I claimed "bare-metal, no library" from string absence — **wrong method**.
Proven properly from the **clock-setup code** (the definitive library tell):
`FUN_08052344` is, line-for-line, ST's **SPL/CMSIS `SetSysClockTo72()`** from
`system_stm32f10x.c`: HSE on → wait HSERDY (bit17) → FLASH latency=2 → APB1 /2 →
**PLL ×9 from HSE (0x1d0000) = 8MHz→72MHz** → PLLON → wait PLLRDY (bit25) → SW=PLL →
wait SWS. The magic constants are verbatim ST.
- **Verdict: ST Standard Peripheral Library (SPL) + CMSIS** — confirmed by TWO
  independent tells (not string absence):
  1. **Clock** = CMSIS `SetSysClockTo72()` verbatim (ships with SPL).
  2. **Peripherals** = the **SPL init-struct idiom** (the decisive, SPL-specific one):
     `FUN_08030048` (ADC+DMA capture init) builds an `ADC_InitTypeDef`/`DMA_InitTypeDef`
     **struct on the stack** (the `local_50=0x20; local_4d=0; local_4c=...` field fills),
     calls `RCC_APB2PeriphClockCmd`-style clock-enable first, then `ADC_Init(ADC1,&s)`.
     Bare CMSIS would write registers directly with NO init-struct; HAL would show
     `HAL_ADC_Init` + a handle + `assert_param`. The init-struct-on-stack = **SPL**.
  - So: **SPL/CMSIS** (obsolete pre-CubeMX ST library), NOT hand-rolled raw, NOT HAL.
    The `set_bit(reg,mask,on)` helpers are app-level conveniences on top of SPL.
  - **What SPL means for the RE:** the peripheral configs map to known SPL structs —
    so an init-struct's stack layout decodes against `ADC_InitTypeDef`/`DMA_InitTypeDef`/
    `GPIO_InitTypeDef`/`TIM_TimeBaseInitTypeDef` field order → recover the exact ADC
    mode, DMA channel, GPIO pins/speed, timer prescaler. Big shortcut for the capture
    path. SPL source: ST `STM32F10x_StdPeriph_Lib` (archived; the structs are public).

## Peripheral classification (classify_periph.py)
Classifies every function by which STM32 peripheral register bases it loads (the
address reveals purpose with no strings). Result: **only 38 of 729 functions touch
hardware** (the thin driver layer); 691 are pure logic/math — itself evidence of a
library/driver abstraction, not raw register code everywhere. Key hardware funcs
identified: `FUN_08052344`=SetSysClock(72MHz PLL), `FUN_08030048`=ADC+DMA capture init,
`FUN_0800cb6a`=LCD init (FSMC+GPIO+DMA+TIM), `FUN_080196de`=DMA setup, the SPI1/USB/
ADC1/USART drivers. Output: `<fw>/analysis/periph_class.txt` + funcdb `periph` column.

## SPL anomaly = an inherited (pre-2014) codebase, likely Tekway/DSO-lineage
**Observation (user):** a "newish" product (2018-2021) built on **SPL is odd** — ST
deprecated SPL ~2014 (EOL) in favour of HAL/CubeMX. A 2018 team starting fresh would
use HAL; choosing SPL only makes sense if **continuing a codebase that predates 2014.**
So the *codebase* is old even though the *product* is new.

**Evidence + assessment:**
- **No attribution in ANY firmware** — grep for copyright/vendor/date/"Tekway"/build
  banner = nothing. The image is stripped of provenance strings (doesn't settle origin,
  but consistent with carried-forward house code).
- **SPL EOL timing** is the decisive tell: SPL-in-2018 ⇒ inherited pre-2014 skeleton.
- **Documented lineage:** the EEVblog/eediscuss threads place the 2D72/2D82 as
  descendants of **Tekway / the older Hantek DSO designs** (the AD9288-overclock is
  noted as a long-standing Tekway/Hantek trick). So an older-design ancestor exists.
- **The mod-author angle (AxGxP).** A forum user **AxGxP** made a custom-firmware mod
  for the 2D72/2C42 by **binary-patching the factory firmware** (no clean source; worked
  within existing addressing/size limits) — he fixed the 100ms→50ms-shutdown bug and
  added F1–F4 menu-less shortcuts. The thread discussed whether he had compilable
  disassembler output or was patching the binary directly. That a competent modder had
  to *binary-patch* (rather than rebuild from source) fits an old, hard-to-rebuild
  codebase. [Sourced: EEVblog
  https://www.eevblog.com/forum/testgear/hantek-2c422c722d422d72/ + eediscuss tid=13676.]
  *(NB: an earlier draft attributed the phrase "patches upon patches" to him as a quote
  — that wording is UNVERIFIED (came from a summarizer, not the post); the binary-patch
  facts above are confirmed, the verbatim quote is not.)*

**Most likely:** Hantek's/Tekway's own **decade-old SPL DSO codebase carried forward**
(internal reuse), NOT lifted from an unrelated third party (Hantek/Tekway ARE the
original design house for this class — others copy them). "Lifted" only in the sense
of inheriting their own old Tekway/DSO code.

**Why it matters for RE:** if it's a shared old DSO-family codebase, the **capture-path
+ SPL driver code is likely common across the family** (2D72/2D82/bench DSO2000/older
DST/DSO5000) → cross-referencing those firmwares could fill gaps in ours. Worth a
cross-family function diff (the funcdb bodyhash already supports this).

## AWG hardware-detect (R65) + unused board features (analysis)
- **AWG presence is hardware-detected via R65** (community finding, eevblog): R65
  forwards the **DAC-enable signal from the FPGA to an MCU GPIO**; the firmware reads it
  to gate the AWG menu (else prints "This model has no AWG"). On a C-model the AWG
  hardware (3PD5651E DAC + EL5166) is depopulated and R65 sets the detect line
  accordingly. (R65 value uncertain — a forum guess of 560Ω vs 50Ω in a RU video; needs
  confirmation on a real 2Dx2 board.) **In firmware:** the "no AWG" branch lives in
  **`FUN_08025738`** (string ref @0x0802c75c) — read this func to confirm whether the
  gate reads the R65 GPIO, the stored model byte, or both. (Recall the auto-vs-base diff
  also showed a model-byte gate; the R65 GPIO is the *hardware* detect — likely both
  exist: HW detect sets the model, model gates the menu.)
- **Unused board features (latent hardware the vendor firmware ignores):**
  - **SD-card holder** present on the board → **ZERO firmware support.** Verified two
    ways: (1) no `sd`/`mmc`/`fatfs`/`microsd` strings in ANY firmware; (2) the **SDIO
    peripheral (0x40018000) is NEVER referenced** in any firmware version. The
    STM32F103VET6 (high-density) **does have SDIO** — so native SD is possible in
    silicon. **SD only works on fixed pins:** SDIO = PC8/9/10/11 + PC12(CLK) + PD2(CMD),
    OR SPI-mode SD on an SPI's pins + a CS GPIO. The firmware DOES drive GPIOC/GPIOD
    (FUN_0801fa30, FUN_08030250, FUN_08020cd4) but for ADC/LCD/USART — **whether any of
    those touch the SDIO pins PC8-12 needs a board trace.** So: SD is provisioned in
    silicon (SDIO exists, pins fixed) but unconfigured by firmware; if the holder is
    wired to the SDIO/SPI pins, a clean-room firmware enables it with NO board change.
    **Where the code would go:** there is no vendor SD code to find (SDIO untouched) —
    a clean-room driver would init SDIO (or SPI+CS) on those fixed pins + FatFs. The
    only open question is the holder's actual pin wiring (trace it).
  - **ESP-12 (ESP8266 WiFi) module + MicroSD** reportedly soldered on a unit
    (eediscuss) — UNVERIFIED (secondhand, no photo). **An ESP attaches only via UART /
    SPI / I²C**, so the analysis checked those: the firmware uses **USART1** (debug
    console) and a SECOND serial **USART2** (`FUN_0802cea0` = USART2+GPIO+TIM1) **whose
    peer is unexplained** — and a UART is exactly how an ESP8266 connects (AT commands /
    transparent bridge). So **USART2 is the prime ESP-link candidate** (a configured
    serial that isn't the console). Free alternatives: SPI2/SPI3, I2C1/I2C2 (all 0 refs).
    **BUT no ESP protocol anywhere** — no `AT+`/`+CW`/`+CIP`/wifi/8266 strings — so even
    if USART2 IS the ESP, the firmware only inits the UART and never speaks to a module
    (consistent with "present but unused"). **Is the code there? No** — just a
    configured UART of unknown purpose; confirming it's the ESP needs a board trace of
    where USART2's TX/RX go. (ESP-12 = a ~$2 ESP8266 WiFi module.)
  - Same pattern as gated-AWG and the fake-CAN menu: **the hardware is consistently
    more capable than the vendor firmware exploits** — the recurring opportunity for a
    clean-room firmware (SD = screenshot/waveform save; WiFi = remote; AWG-enable;
    real CAN decode).

## ⚠️ LIMITATION: the peripheral/GPIO map is INCOMPLETE (not pin-validated)
**Honest correction (user challenged it — rightly).** The `classify_periph.py` scan
found peripherals by **full 32-bit base-address literals only**. That is provably
incomplete:
- **929 bit-band-alias-region literals (0x42xxxxxx) were MISSED** — the STM32 bit-band
  region sets/clears *individual* GPIO/peripheral bits via computed addresses that never
  contain the base literal. So a large amount of GPIO/flag access is invisible to the
  scan. ⇒ **"only 38 funcs touch hardware" is an UNDERCOUNT.**
- **GPIO BSRR (base+0x10) atomic pin set/clear** and any **base+offset** access (base
  loaded once, used via `STR rX,[base,#off]`) are likewise undercounted; if SPL passes
  the port as a function arg, the callee shows no literal.
- **No pin-level validation done at all.** "GPIOC is used" was never resolved to WHICH
  pins, input/output, or speed. **Not a single `GPIO_Init` pin-mask (GPIO_Pin) or
  CRL/CRH value has been decoded.** So statements like "USART2 is the ESP link" or
  "SDIO never touched" are **directionally indicated, NOT pin-verified.**

**What a RIGOROUS pass requires (TODO):** (1) scan the bit-band region + BSRR/IDR/ODR
offsets, (2) find every SPL `GPIO_Init(port,&cfg)` call and decode the `GPIO_Pin` mask +
Mode + Speed from the struct, (3) read CRL/CRH writes, (4) build the actual port/pin →
function map. Only then is the pinout validated. Until then the peripheral findings are
LEADS, not a confirmed map — and the dts pin assignments remain placeholders needing a
board trace regardless.


## ⚠️⚠️ GPIO PIN MAP IS NOT STATICALLY RECOVERABLE (verified the hard way)
**Stronger correction (the user kept pushing — correctly).** I tried three methods to
get the pinout from the firmware. All hit the same wall:
1. **Base-address literal scan** — found peripherals but UNDERCOUNTS (missed 929 bit-band
   accesses) and MIS-CLASSIFIES (tagged a func "GPIOC" that actually just *reads*
   GPIOA.CRL — not a pin driver at all).
2. **Resolve DAT_ pointers** — showed `FUN_0801fa30` *reads* `GPIOA.CRL` + a flash table
   into RAM; it does NOT configure pins. The "all GPIOA-E write CRL" claim was an
   ARTIFACT of catching register *reads*, not config writes. **Retracted.**
3. **Value-pattern decode** (the user's method: decode only VALID CRL/CRH/BSRR patterns)
   — found **0 inline-constant GPIO config sites.** Because the firmware uses **SPL
   `GPIO_Init(port,&struct)`**: the struct is built on the stack, the **CRL/CRH value is
   COMPUTED AT RUNTIME** from the struct fields, and the port is a **function argument**
   — so neither pins nor modes exist as constants anywhere a static scan can reach.

**Honest verdict: the GPIO pin assignments CANNOT be derived statically from this
firmware.** The only ways are (a) **interactive Ghidra** — type the `GPIO_InitTypeDef`
struct, follow each stack struct into `GPIO_Init`, decode the `GPIO_Pin`/Mode/Speed
fields per call; or (b) **a board trace / live register read** on real hardware. Every
GPIO pin in the dts/HARDWARE.md is a PLACEHOLDER and must come from one of those. Prior
GPIO-specific claims (which func drives which port, USART2=ESP pin location, SDIO pins)
are LEADS, not established. (Peripheral *presence* — that ADC1/DMA/SPI1/USART/FPGA/LCD
are used — does stand; their exact PINS do not.)

## ✅ SOLUTION: emulate the firmware → observe GPIO writes (user's idea — it WORKS)
The "not statically recoverable" wall is beaten by the user's three suggestions, and
**emulation is the decisive one:** the GPIO config is computed at runtime, so *run it*
and watch the registers. `scripts/emu_gpio.py` (Unicorn ARM Cortex-M): maps flash+RAM+
peripherals, stubs the RCC ready-bits so clock-polls exit, runs from the reset vector,
and hooks every write to the GPIO register space → decodes CRL/CRH (pin modes) + BSRR
(driven pins).

**First run already recovered VALIDATED pins** (self-checking — these are textbook):
- **PA9 = AF-PP-50MHz + PA10 = float** → **USART1 TX/RX** (the debug console — exact STM32
  UART pinout, CONFIRMS the console UART pins). ✅
- **PA0 = input-pull** (a button/sense), **PB9 / PE1 = push-pull out** (control lines).

This proves the method: emulation gives the *real* pinout where static analysis can't.
(The first run captured only the early init — 8 writes before an unhandled access; a
deeper run, handling the faults + driving more init functions, yields the full map.)
The other two routes also work: (a) **call-graph propagation** — trace `GPIO_Init`'s
callers and the base/struct they pass; (b) **interactive Ghidra** — type the
`GPIO_InitTypeDef` struct + give `GPIO_Init` a signature → Ghidra constant-propagates
each caller's pins. **Recommended: extend `emu_gpio.py`** (handle faults, run the full
init sequence) for the complete validated pin map — no hardware needed.