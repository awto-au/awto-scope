# Hantek 2D82AUTO — firmware reverse-engineering

Disassembly + decompilation of the 2D82AUTO firmware (STM32, unencrypted).
See `vendors/hantek/2d82/docs/hardware/devices/hantek-2d82auto.md` for the device + provenance notes.

## firmware/
- `HTX2D82-2021090901-inner.dfu` — the DfuSe image (from the official .dfu ZIP).
- `flash_08005000.bin` — raw flash, extracted from the DfuSe element.
  **Load address 0x08005000** (app sits after a 20 KB bootloader). Vector table:
  SP=0x2000f3f8 (→ 64 KB SRAM), reset=0x08053d45 — valid ARM Cortex-M.

## Related: the vendor's combined bootloader+app recovery image

`../../2d72/firmware/arm1-20210108/HantekHTX2021010801.bin` (255,794 bytes) is a
**combined** image — bootloader at offset 0 AND app at offset 0x5000 — recovered from a
2019 Hantek-moderator post (full chain in that directory's `PROVENANCE.md`). It is what
the vendor's "program arm1 with a TTL serial port tool" instructions flash: the ST Flash
Loader writes the whole chip over UART, bootloader included, which DFU cannot do because
DFU alt-setting `a` (the low 12 KB) is read-only.

Why it matters here: its bootloader is **byte-identical to ours** for all 13,360 bytes of
code (`0x0000`-`0x342F`); the only difference in the whole `0x5000` region is `0x00` pad
vs `0xFF` erased flash in the unused tail. A generic Hantek2000-family recovery image and
a 2D82 pulled off real hardware run the same bootloader binary — so the bootloader carries
no per-model difference. See
[`../docs/hardware/unlock.md`](../docs/hardware/unlock.md) for what that rules out.

## ghidra/
Headless Ghidra 12.2 export (`-processor ARM:LE:32:Cortex`, base 0x08005000):
- `disassembly.asm` — 73,594 lines, ARM Thumb listing, **729 functions**.
- `decompiled.c` — 40,353 lines, decompiled C for all 729 functions.

Firmware is **stripped** (no symbols) → functions are `FUN_<addr>`. Auto-analysis
didn't fully resolve string/data xrefs on the raw blob; a follow-up pass should
define the string section + re-run reference analysis to name the USB command
handler (`USB_R_Buffer[]` parser seen in strings) and the calibration tables
(`CaliParam.volt_cali_data[]`).

## Reproduce
```
analyzeHeadless <proj> 2d82fw \
  -import firmware/flash_08005000.bin \
  -processor "ARM:LE:32:Cortex" -loader BinaryLoader -loader-baseAddr 0x08005000 \
  -scriptPath <dir-of-ExportAll> -postScript ExportAll.java <out-dir> -deleteProject
```

## CPU / vendor — honest status
- **Core:** Cortex-M3-*class*. No VFP instructions in the disassembly, BUT that does
  NOT prove M3 — the FPU may simply be left disabled (common; or soft-float build).
  64 KB SRAM (SP=0x2000f3f8), ~55 IRQs, app@0x08005000 → **STM32F103-compatible**.
- **Vendor:** DfuSe footer VID = **0x0483 (ST)** and ST DfuSe format → leans
  **genuine ST**. BUT cheap Hantek gear often uses **register-compatible clones
  (GD32 / APM32 / CKS32)** that copy ST's VID and run the same HAL — so a clone
  CANNOT be ruled out from firmware alone. The 20 KB user-flash bootloader before
  the app (app not at 0x08000000) is a custom-bootloader pattern seen on both.
  **Definitive ID needs the physical chip** (DBGMCU IDCODE / chip markings).
- **Doesn't matter for RE:** ST and the clones share the same ST HAL, so the HAL
  fingerprint-match (reference/stm32f1xx_hal_driver) works either way.

## reference/
- `stm32f1xx_hal_driver/` — ST's STM32F1 HAL (from github.com/STMicroelectronics/
  stm32f1xx_hal_driver) for fingerprint-matching library functions in decompiled.c.

## HAL version? — there is NONE; the firmware is bare-metal register code
Tried to pin the STM32F1 HAL version to fingerprint-match library functions.
Finding (corrects the earlier "much must be HAL" assumption):
- **No `HAL_GetHalVersion`** function (no `return 0x0101xx00;` — the packed-version
  immediate ST's HAL always carries). The `0x1000213` constant found earlier is
  unrelated (RC byte 0x13 ≠ any real HAL, which always has RC=0x00).
- **No HAL/SPL strings:** no `stm32f1xx_hal_*.c`, `assert_param`, `Drivers/`,
  `stm32f10x`, `StdPeriph`, `_conf` anywhere in the image.
- **Direct register access:** peripheral bases poked as immediates (RCC 0x40021000,
  ADC1 0x40012400, TIM1 0x40012c00, USB 0x40005c00, DMA1 0x40020000); only ~32
  register-immediate refs total (a HAL build → hundreds). 
- **Conclusion: bare-metal / hand-rolled C with direct register writes** (CMSIS
  headers only, which leave no version string). Fits a 250 MS/s real-time scope
  loop where HAL overhead/jitter is unwanted.

**Implication:** HAL fingerprint-matching does NOT apply — there is no library layer
to subtract; nearly all 729 functions are Hantek application/driver code. To
understand it, work from the **strings as anchors** (`USB_R_Buffer[]`,
`usb_cable_config()`, `CaliParam.volt_cali_data[]`, the 102 test-preset names) and
follow xrefs in Ghidra to the USB command dispatcher + ADC/timer setup. The
`reference/stm32f1xx_hal_driver/` is still useful as a *register-semantics*
reference (what each poked register does), just not for function fingerprinting.

## Graphics library + LL check (and the shared Hantek-2000 family)
- **Graphics: custom/in-house, no named GUI lib.** Only display string is
  `pGLCD_ID=0x%x` (reads the Graphic-LCD controller ID at init). **No emWin/SEGGER,
  LVGL, UCGUI, or TouchGFX** symbols. Hantek hand-rolled a minimal framebuffer/
  waveform renderer — fits the bare-metal style.
- **LL drivers: unconfirmed, probably not.** LL leaves no string signature so it
  can't be proven/disproven by strings; but the evidence (only ~32 register-immediate
  refs, direct peripheral-base pokes) reads as **plain bare-metal register C**, not
  LL's `LL_*_Init` patterns. Confirming would need function-shape matching of
  `stm32f1xx_ll_*.c` (in `reference/`) against Ghidra funcs (e.g. does
  `LL_RCC_GetSystemClocksFreq`'s prescaler-table logic appear).

### Shared firmware family (one analysis covers many devices)
The **Hantek 2000 series** — 2D72/2D42/2C72/2C42 (product 13174) **and** the 2D82AUTO
— share the **same "Hantek2000" firmware/software/updater** (same 250 MS/s arch).
So these findings (bare-metal, no HAL/RTOS, custom graphics, USB_R_Buffer command
parser) apply across the whole family. Firmware downloads:
- 2000-series firmware: http://www.hantek.com/Products/SoftwareDL?id=41420
- 2000-series sw: id=41404 ; update tool: id=41421 ; manual: id=41304
