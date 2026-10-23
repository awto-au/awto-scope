# Flash layout options -- first_stage

Two buildable variants of `code/first_stage`, chosen at build time, plus a documented
(not-yet-implemented) direct-flash option. See [issue #39](https://github.com/awtoau/awto-2000/issues/39)
for the original discussion.

**Both Option B and Option C are real, independently buildable, Renode-verified targets** —
this is a deliberate project-owner decision (2026-07-14): rather than picking one default and
tracking the other as a future follow-on, both are maintained side by side. Pick per your
situation:

- **No SWD hardware / lowest barrier to entry → Option B.** Works today exactly as it always
  has (`dfu-util`, hold F1 at power-on). No change from prior sessions.
- **SWD-equipped and want signed-image verification (a step toward OTA later) → Option C.**
  Requires a one-time SWD flash to install MCUboot; the *intended* long-term benefit is that
  further app updates then no longer need SWD (MCUboot's own recovery/update path) — **not yet
  true in this build**, see "Option C — known gaps" below. Until that gap is closed, Option C
  updates also require SWD, same as Option B's initial install would if you didn't have DFU.

---

## Option B -- Vendor DFU bootloader (plain `west build`, no `--sysbuild`)

Vendor bootloader occupies `0x08000000`-`0x08004FFF` (20 KB, MD5 `629ec3f9`).
App links at `0x08005000` via `CONFIG_FLASH_LOAD_OFFSET=0x5000` (`code/first_stage/prj.conf`).
This is the plain, non-sysbuild `first_stage` build — nothing about it changed in the
Option C work below.

**DFU entry:** hold **F1** while powering on -> release when RUN/STOP LED flashes -> plug USB.

```bash
# Build (uses esp32-zephyr workspace; see tools/scripts/build_first_stage.py)
cd ~/git/esp32-zephyr && source zephyr/zephyr-env.sh
west build -b hantek_2d82 /path/to/awto-2000/code/first_stage \
  --build-dir /path/to/awto-2000/tmp/first_stage_build

# Or via the convenience script from the repo root:
python3 tools/scripts/build_first_stage.py

# Flash (app only, bootloader untouched)
dfu-util -a 0 -s 0x08005000:leave -D tmp/first_stage_build/zephyr/zephyr.bin
```

**Shell:** connect to USART1 (PA9 TX / PA10 RX, 115200 8N1) on the TTL debug header.

**Verified 2026-07-14:** clean `west build` (58.32% flash used, 293808/492KB), boots to
`supervisor: running` with a working shell prompt in Renode
(`python3 code/renode/hantek_emu.py --binary tmp/first_stage_build/zephyr/zephyr.elf headless`).

### Option B — safety review (2026-07-14, GH #157, Basalt-Query)

**Verdict: SAFE.** The vendor bootloader does real hardware setup before handoff, but Zephyr's
own startup unconditionally re-initializes everything it needs — Option B's "odd stuff" in the
bootloader is a non-issue for Zephyr regardless of how well-understood it is for the vendor app.

**Full bootloader hardware/SRAM inventory** (fresh disassembly-level pass, not just the existing
TSV summary — `vendors/hantek/2d82/firmware/bootloader/analysis/bootloader_decompiled.c`, 2895 lines, 110
functions):
- **Clocks:** `boot_clock_and_vtor_init` (0x08002218, called first from `Reset_Handler`) resets
  RCC_CR/RCC_CFGR to a known state, sets FLASH_ACR=0x9f0000 (wait states + prefetch for 72 MHz),
  calls `boot_set_sysclock` (HSE bypass check, PLL9 config, SYSCLK switch to PLL — 8 MHz HSE ->
  72 MHz), **then writes SCB_VTOR = 0x08000000** — a previously-undocumented finding, now in the
  TSV as `boot_clock_and_vtor_init`. This is the bootloader's only VTOR write; it is never
  updated again before the app-jump in `boot_main`, so VTOR still points at the bootloader's own
  vector table at the moment of handoff. Not a bootloader bug and not a Zephyr hazard (see below)
  — the vendor app's own startup explicitly re-asserts VTOR=0x08000000 itself
  (`fw_scb_vtor_set(0x8000000)`, decompiled_named.c:47741), taking ownership of VTOR rather than
  trusting whatever the bootloader left.
- **GPIO:** `boot_gpio_init` enables GPIOA clock, clears an unidentified RCC bit (0x10000000,
  possibly CEC/DAC-related), disables LSE (`fw_rcc_bdcr_lse_cfg(0)`), configures PA0 as
  input pull-up/down (power-button sense, polled by `boot_main` to gate DFU-wait vs jump-to-app).
  `boot_init` and the three anonymous helpers called from `boot_main` (`FUN_080029bc`,
  `FUN_08002aac`, `FUN_08002b30`) configure further GPIO pins (PB12 sense, LED/status pins) and
  set up the SysTick-like software timer used for the bootloader's own delay loops — all MMIO
  peripheral config, no new SRAM writes found beyond what's below.
- **NVIC/SCB:** `FUN_08002200`/`FUN_08002208`/`FUN_080021a8`/`FUN_080021b0` write NVIC priority
  and enable/pending registers (0xE000E100/180/400 region) and one more SCB register
  (0xE000ED0C, likely SCR for sleep-on-exit or similar) — standard USB/timer IRQ setup for the
  bootloader's own DFU loop, not app-relevant state.
- **SRAM:** the *only* SRAM writes anywhere in `boot_main`'s non-DFU (app-jump) path are the two
  app-entry-point bootstrap words (copying the app's SP/PC into scratch before the jump — the
  standard "vector table read, MSR MSP, BLX" sequence). No loop, no array write, nothing touching
  the `g_dat_16a0` SRAM gap (`0x20001FC0`, see below) or any other address outside the
  bootloader's own small scratch-RAM region (`0x20000000`-`0x200002ff`-ish, all `DAT_` pointers
  resolved and none overlap the app's `.data`/`.bss`/gap regions).
- **DMA:** zero DMA usage anywhere in the 20 KB bootloader — confirmed by grepping both
  `bootloader_decompiled.c` and `bootloader_disassembly.asm` for any DMA1/DMA2 register
  reference; none exist. Rules out the "DMA copy left running" theory from the task brief
  entirely — there is no DMA controller touched by this bootloader at all.

**The `g_dat_16a0` / `0x20001FC0` SRAM-table mystery** (FPGA timebase divisor table, GH #48 /
#151): re-attacked with a byte-level exhaustive scan (not just decompiled-C grep) of *both* raw
flash images for any 32-bit little-endian literal in `[0x20001fc0, 0x20001fc0+0x28]` at every
unaligned byte offset. Result: **zero hits in the 20 KB bootloader** binary, and **exactly one
hit in the app** binary — the `g_dat_16a0` pointer constant itself at `0x080116a0`, not a
writer. This independently reconfirms the existing TSV finding (which had already ruled out the
`fw_data_decompress` .data-restore mechanism and manually read `boot_main` end-to-end) on
stronger evidence: no `movw`/`movt` pair or literal-pool slot anywhere in the bootloader's
disassembly loads any `0x2000xxxx` SRAM-gap address other than the already-known app-SP
validation constant, and there is no DMA engine to set up an indirect copy in the first place.
**Status: still unresolved via static analysis** (genuinely requires a live SRAM capture —
Renode SRAM dump mid-boot, or SWD read on real hardware, neither performed this session) — but
now with much stronger evidence that it is *not* a missed static code path. The
"garbage-that-looks-structured" (uninitialized-SRAM-residue) theory remains plausible and was
not ruled out or confirmed either way; distinguishing it from "real bootloader-populated data"
still requires the live capture.

**Is this mystery relevant to Zephyr? No — completely irrelevant, independent of resolution.**
`code/a2k_board`, `code/lib`, and `code/first_stage` have **no timebase-divisor-table concept
implemented at all yet**: `code/a2k_board/include/a2k_board/fpga.h` is a bare
`fpga_write_reg`/`fpga_read_reg` register shim, and there is no reference anywhere in the Zephyr
tree to `0x20001fc0`, `g_dat_16a0`, or any equivalent construct. Whatever this address turns out
to mean for the vendor app, it does not block or affect Option B for the current Zephyr
firmware. When Zephyr eventually implements FPGA timebase/sample-rate control it should compute
divisors directly from the known formula (`effective_sample_rate = 250 MHz / (divisor + 1)`,
see `fw_fpga_timebase_div` in the TSV) rather than replicate the vendor's opaque
runtime-populated-table mechanism — cleaner and sidesteps the mystery entirely.

**Does Zephyr depend on any other bootloader-left state? No — checked clock, VTOR, and GPIO
specifically:**
- **Clock:** `hantek_2d82.dts` independently declares the full clock tree (`clk_hse` = 8 MHz,
  `pll` mul=9, `rcc` = 72 MHz SYSCLK, AHB/APB1/APB2 prescalers) matching the vendor's config by
  coincidence of correct engineering, not by inheritance. Zephyr's STM32 SoC init
  (`soc/st/stm32/stm32f1x/soc.c: soc_early_init_hook`) unconditionally sets
  `SystemCoreClock = 8000000` (assumes HSI-8MHz reset-default state) before the clock driver
  runs. Zephyr's clock-control driver (`drivers/clock_control/clock_stm32_ll_common.c:
  stm32_clock_control_init`, registered `DEVICE_DT_DEFINE`/`PRE_KERNEL_1`, runs automatically
  every boot) then unconditionally enables HSE, waits for HSE-ready, configures and locks the
  PLL, switches SYSCLK source to PLL, and sets all bus prescalers from the DT values — with no
  "already configured, skip" check anywhere. This runs identically whether the bootloader left
  RCC in its 72 MHz config (Option B) or fully reset (Option C, post-MCUboot) or even HSI-8MHz
  factory-default (a hypothetical Option A). Zephyr does not and cannot skip its own clock init.
- **VTOR:** Zephyr's own Cortex-M reset/startup and linker-driven vector-table placement do not
  read or depend on prior VTOR state; the app sets its own vector table location as part of its
  own boot sequence, same as the vendor app does via `fw_scb_vtor_set`.
- **GPIO/LSE:** no LSE or RTC devicetree nodes exist in `hantek_2d82.dts` at all, so the
  bootloader's LSE-disable is moot either way. GPIOA and other GPIO ports are brought up via
  Zephyr's pinctrl/GPIO driver from the DT `status = "okay"` + pinctrl properties, which program
  MODER/CRL/CRH explicitly per pin — not conditional on prior state. No PA0/power-button
  dependency exists anywhere in the current Zephyr tree (that pin is bootloader/vendor-app DFU-UI
  logic only).

**Bottom line:** Zephyr's own startup is fully independent of whatever the vendor bootloader did
or didn't do — it re-derives clocks, vector table, and GPIO state from its own devicetree/Kconfig
on every boot, unconditionally. Option B is safe to use as-is. The SRAM-table mystery remains a
genuinely open, interesting question for full vendor-firmware understanding, but it sits
entirely on the vendor-app side of the fence and has zero bearing on Zephyr's correctness or
safety in either flash-layout option.

---

## Option C -- MCUboot (sysbuild, `mcuboot-hantek_2d82` snippet)

Replaces the vendor bootloader with MCUboot (Zephyr's own port, ED25519-signed images,
software SHA256 — this MCU has no hardware crypto accelerator). Single-slot only for this
first cut (no OTA staging slot yet — see "known gaps"): MCUboot verifies and boots
`slot0_partition`, full stop.

**Flash map** (`code/first_stage/snippets/mcuboot-hantek_2d82/boards/hantek_2d82.overlay`):

| Region      | Start        | Size    |
|-------------|-------------|---------|
| mcuboot     | `0x08000000` | 48 KB   |
| slot0 (app) | `0x0800C000` | 460 KB  |
| nvs         | `0x0807F000` | 4 KB    |

(48 KB for MCUboot is measured, not guessed: ED25519 + software SHA256 via mbedTLS actually
uses 32028 B / 65.16% of the 48 KB budget — an earlier 32 KB budget overflowed by 11548 B
during Renode build verification.)

**Build** (sysbuild, `-S mcuboot-hantek_2d82`; `BOARD_ROOT`/`SNIPPET_ROOT` are required
because `hantek_2d82` and the snippet are both out-of-tree, living under `code/first_stage/`
rather than the Zephyr workspace):

```bash
cd .
ZEPHYR_BASE=<workspace>/esp32-zephyr/zephyr ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk-1.0.1 \
west build -b hantek_2d82 -d tmp/first_stage_mcuboot_build code/first_stage --sysbuild -- \
  -DBOARD_ROOT=./code/first_stage \
  -DSNIPPET_ROOT=./code/first_stage \
  -DSNIPPET=mcuboot-hantek_2d82
```

Produces two images: `tmp/first_stage_mcuboot_build/mcuboot/zephyr/zephyr.bin` (the
bootloader) and `tmp/first_stage_mcuboot_build/first_stage/zephyr/zephyr.signed.bin` (the
app, ED25519-signed by imgtool automatically as part of the sysbuild).

**Flash (one-time SWD install — both images):**
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program tmp/first_stage_mcuboot_build/mcuboot/zephyr/zephyr.bin 0x08000000 verify" \
  -c "program tmp/first_stage_mcuboot_build/first_stage/zephyr/zephyr.signed.bin 0x0800C000 verify reset exit"
```
or with pyocd (this project's preferred tool — see [swd-debug.md](../hardware/swd-debug.md)):
```bash
pyocd flash -t stm32f103vet6 -a 0x08000000 tmp/first_stage_mcuboot_build/mcuboot/zephyr/zephyr.bin
pyocd flash -t stm32f103vet6 -a 0x0800C000 tmp/first_stage_mcuboot_build/first_stage/zephyr/zephyr.signed.bin
```

**Signing key:** left at MCUboot's own bundled default
(`$ZEPHYR_MCUBOOT_MODULE_DIR/root-ed25519.pem` — a well-known public dev/sample key shipped
with every MCUboot checkout, same one every Zephyr MCUboot sample uses unless overridden).
Fine for bring-up/Renode verification. Anyone shipping signed field updates for real must
generate their own key (`imgtool keygen -k mykey.pem -t ed25519`) and override
`SB_CONFIG_BOOT_SIGNATURE_KEY_FILE` in `code/first_stage/sysbuild.conf`.

**Verified 2026-07-14:** clean sysbuild (mcuboot: 32028/49152 B = 65.16%; app:
294432/470880 B = 62.53%), both images link at the correct addresses (mcuboot @
`0x08000000`, app @ `0x0800C000`, confirmed via `readelf -h`/linker MEMORY region), and the
app image boots to `supervisor: running` with a working shell prompt in Renode
(`python3 code/renode/hantek_emu.py --binary tmp/first_stage_mcuboot_build/first_stage/zephyr/zephyr.elf headless`
— loads the app ELF directly, bypassing MCUboot's own chain-load/verify step, since Renode
has no model of MCUboot's boot process; this confirms the app itself boots correctly at its
MCUboot-derived link address, not that MCUboot's verify-and-jump handoff works in Renode,
which was not exercised).

### Option C — known gaps

- **Serial recovery (the "no SWD needed again" update path) is NOT enabled.** MCUboot's
  `boot_serial`/`serial_adapter` requires (a) a `mcuboot_button0` GPIO devicetree alias to
  detect the recovery-entry trigger at boot — this board has no free/traced GPIO wired up for
  that yet — and (b) `CONFIG_UART_CONSOLE=n`, since serial recovery repurposes the console
  UART (USART1) for its own protocol, which would need first_stage's normal shell/log console
  to coexist with or yield to it. Both are real hardware-integration decisions, not just
  Kconfig flips, so this was deliberately left for a follow-on rather than guessed at. Until
  it's wired up, **updating the app under Option C still requires SWD** — the "one-time SWD
  install, no SWD again after" story is the design goal, not yet the delivered behavior.
- **No OTA/dual-slot.** `MCUBOOT_MODE_SINGLE_APP` was chosen deliberately as the more
  realistic first cut for a 512 KB part (dual-slot OTA — MCUboot + slot0 + slot1 + scratch —
  roughly doubles the app's flash budget). If OTA is wanted later, switch
  `SB_CONFIG_MCUBOOT_MODE_SINGLE_APP` to `SB_CONFIG_MCUBOOT_MODE_SWAP_USING_MOVE` (or
  `_OFFSET`) in `sysbuild.conf`, add a `slot1_partition` + `scratch_partition` to the
  snippet's DTS overlay, and re-check the size budget.
- **MCUboot's own chain-load/image-verify handoff was not exercised in Renode** — only the
  app image booting directly (loaded by `LoadELF` at its real link address) was verified.
  Renode has no model of MCUboot's own verify-and-jump process (reading the image TLV/header,
  checking the ED25519 signature, setting up the handoff state) — that step is real-hardware-
  or-BMDA-only territory. The app booting correctly at the right address is meaningful
  evidence the flash layout and link address are correct, but is not proof MCUboot itself
  will successfully verify and jump to it on real silicon.

---

## Option A -- Direct flash via SWD, no bootloader at all (not implemented)

De-prioritized (2026-07-14) in favor of B+C above — documented here for completeness since
issue #39 raised it, not because it's an active target. App would link at `0x08000000`,
full 512 KB available, at the cost of losing vendor DFU recovery entirely (no bootloader of
any kind). Not built or tested this session.

```bash
# hypothetical — CONFIG_FLASH_LOAD_OFFSET removed/0, plain west build (no sysbuild)
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/zephyr/zephyr.bin 0x08000000 verify reset exit"
```

---

## Bootloader binary backup

The vendor bootloader is preserved in the repo and verified across four independent
flash dumps (all identical):

```
vendors/hantek/2d82/firmware/bootloader/flash_08000000.bin
MD5: 629ec3f9a19fe1c9c8d38d0442786e54   size: 20480 bytes
```

(Path corrected 2026-07-14 — older references to
`vendors/hantek/2d82/reference/dso2000/firmware/bootloader/analysis/flash_08000000.bin` are stale; the
file lives at `vendors/hantek/2d82/firmware/bootloader/flash_08000000.bin` in this repo.)

To restore the vendor bootloader after an Option C SWD flash (going back to Option B):
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program vendors/hantek/2d82/firmware/bootloader/flash_08000000.bin 0x08000000 verify reset exit"
```
then rebuild/reflash `first_stage` the plain (non-sysbuild) way so it goes back to linking at
`0x08005000` — see Option B above.
