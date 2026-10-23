# What this analysis IS (and is NOT) — read before reasoning about boot/init

## We are looking at STAGE 2 (the application), NOT the whole firmware.

This `analysis/` is the **2D82AUTO application image** = `HTX2D82-2021090901.dfu`,
flashed at **0x08005000**. It is the *second stage* of a two-stage boot:

```
0x08000000  ┌──────────────────────────────┐
            │ STAGE 1: vendor DFU bootloader │  <- the CHIP boots here at reset
            │ (~20KB, 0x08000000-0x08004FFF) │     NOT in this DFU.
0x08005000  ├──────────────────────────────┤
            │ STAGE 2: THE APPLICATION       │  <- THIS analysis.
            │ (what we decompiled)           │     Its vector table is @ 0x08005000.
            └──────────────────────────────┘
```

## ⚠️ We have NO visibility into what ran BEFORE this image.
By the time the app's `Reset_Handler` (0x08053d44) runs, the **bootloader has already**:
- initialised the clock/PLL? (maybe — the app re-does SetSysClock anyway)
- set up the stack, possibly configured some peripherals
- **set VTOR = 0x08005000** and jumped into the app
So do NOT assume a clean power-on state — some hardware may already be configured by stage 1.

## Where stage 1 hands off to stage 2 (found in the FULL chip dump)
The bootloader IS available in the full-flash dumps (these start at 0x08000000):
- `firmware/dump-2021/2D72_dump_2021.bin`  (512KB, has bootloader at 0x08000000)
- `firmware/older-dump/27D72_dump.bin`     (512KB)
- `firmware/bootloader/analysis/flash_08000000.bin` (20KB — isolated bootloader only)
The bootloader (reset @0x08003021) sets **VTOR (0xE000ED08) @0x0800236c**, references the app
base **0x08005000 @0x08002944 and 0x08003424**, then jumps to the app reset vector
`[0x08005004]` = 0x08053d44.

**IMPORTANT: The Ghidra project (`ghidra_project/hantek2d82.gpr`) already contains the full
512KB image (`flash_08000000.bin`) loaded at 0x08000000. The stage-1 bootloader code at
0x08000000–0x08004FFF is already in the project — it just has FUN_ names and hasn't been
analysed. Navigate to 0x08000000 in Ghidra to see it. See GitHub issue #22.**

(No real device needed — the dumps + DFUs are all in `firmware/`.)
