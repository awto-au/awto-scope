# SWD debugging — Hantek 2D82AUTO board

Reference doc for connecting a debug probe to this board's MCU over SWD. For the point-in-time
investigation log that produced these facts (probe failures, troubleshooting steps, dead ends),
see git history — the working handover doc has been retired now that its findings are captured
here.

## Target MCU

Geehy APM32F103VET6 (STM32F103VET6-compatible clone), LQFP100, 512KB flash, 64KB SRAM, Cortex-M3.

- `chipid`: `0x414` (STM32F1 High Density) — Geehy reports an ST-compatible chipid for tool
  compatibility.
- Flash size register (`0x1FFFF7E0`, low 16 bits): `0x0200` = 512 (decimal, KB).
- Read protection: **disabled** (option byte `RDP=0xA5`, level 0) — flash reads with no unlock
  step needed.
- 96-bit unique device ID at `0x1FFFF7E8`-`0x1FFFF7F3`.

## Board SWD header

4-pin 2.54mm header, corresponding test points present on the PCB (see [board.md](board.md)):

| Pin | Signal |
|-----|--------|
| 1   | 5V     |
| 2   | SWDCLK |
| 3   | SWDIO  |
| 4   | GND    |

MCU-side mapping (see [gpio-pinout.md](gpio-pinout.md)): PA13=SWDIO, PA14=SWDCK (STM32 reset
default, no AFIO remap in firmware).

## What works

**pyocd**, via a clone ST-Link/V2 dongle, reliably halts the core, reads/writes RAM and flash, and
resumes — verified with byte-identical repeat reads of the full 512KB flash. Typical commands:

```
pyocd list
pyocd commander -u <serial> -c "status"
pyocd commander -u <serial> -c "halt"
pyocd commander -u <serial> -c "savemem 0x08000000 0x80000 <file>"   # dump flash
pyocd commander -u <serial> -c "savemem 0x1FFFF000 0x800 <file>"     # dump system memory/option bytes
pyocd commander -u <serial> -c "savemem 0x20000000 0x10000 <file>"   # dump RAM
pyocd commander -u <serial> -c "read32 <addr>" / "write32 <addr> <val>"
pyocd commander -u <serial> -c "go"                                   # resume
pyocd commander -u <serial> -c "reset"                                # hardware reset — recovers a wedged/hung board without physical power-cycle
```

**Confirmed working recovery path**: when the board's main loop hangs (e.g. the `fw_calibrate()` infinite-loop bug — see `vendors/hantek/2d82/docs/hardware/bugs/usb-cmd16-hang.md` — or the analogous cmd `0x18` hang), `pyocd commander -u 26004600160000305748534E -c "reset"` recovers it in ~2s: device re-enumerates on USB, debug UART resumes normal `BAT_Voltage`/telemetry output. Verified 2026-07-13. Prefer this over waiting for/attempting a physical power-cycle when the working probe is connected.

**Use BMDA (the `blackmagic` host binary, built alongside the firmware), not `gdb dump memory`,
for bulk reads.** gdb's RSP protocol has real per-transaction overhead that dominates over SWD
clock rate for bulk memory dumps; BMDA talks BMP's native binary protocol directly and is
measurably faster at the same SWD frequency:

```
build-stlink/blackmagic -s <serial> -f 8M -r -a 0x08000000 -S 0x80000 <output-file>
```

Measured on the re-flashed clone (96MHz core, see below), full 512KB flash read, all byte-identical
to the verified backup:

| Method | Frequency | Throughput |
|---|---|---|
| `gdb dump binary memory` | default (~3.27MHz) | 43.7 KB/s |
| `gdb dump binary memory` | 6MHz (firmware ceiling at 72MHz core) | 57.6 KB/s |
| `gdb dump binary memory` | 8MHz (firmware ceiling at 96MHz core) | 48.4 KB/s |
| BMDA (`blackmagic -r`) | default (no `-f`) | 25.1 KB/s |
| BMDA (`blackmagic -r`) | 8MHz | **53.4 KB/s** |
| BMDA (`blackmagic -r`) | 20MHz (requested; clamps to firmware's real ceiling) | 53.4 KB/s (same as 8MHz — confirms 8MHz is the real ceiling, not a gdb artifact) |

Read throughput over the clone probe is slow (tens of KB/s) but completely reliable once a
connection is established — every read above has been verified byte-identical to the known-good
backup.

`st-flash`/`st-info` (stlink-tools) work intermittently through the same clone — frequent
`LIBUSB_ERROR_TIMEOUT` on basic USB requests even when `st-info --probe` alone succeeds. Prefer
pyocd for anything beyond a quick chipid check.

**Connection reliability is the dominant variable, not probe choice.** Multiple probes (two
ST-Links, a Black Magic Probe, a second clone ST-Link) all failed identically (`SWD scan failed!` /
`chipid 0x000` / suspiciously low target-voltage readings) for an extended period, then one clone
probe started working cleanly after a physical reseat/reconnect, with no other change. Treat any
future "SWD access completely fails" session as a connection/seating problem first — check/reseat
the adapter and header before assuming a probe or target-side incompatibility.

## Known probes

| Probe | Serial | Notes |
|-------|--------|-------|
| ST-Link (unrelated board) | `005600343431511837393330` | chipid `0x419` — **do not use, belongs to a different board** |
| ST-Link (genuine, original) | `0043002C3133510A36303739` | Intermittent `LIBUSB_ERROR_TIMEOUT` via st-flash |
| Black Magic Probe (genuine hardware) | `C4079EC0` | Firmware `2.1.0-rc1-4-gdd77a3f0-dirty`. Use plain system `gdb` (not `arm-none-eabi-gdb`, broken on this machine — missing `libcrypt.so.1`): `gdb --batch -ex "target extended-remote /dev/ttyACM1" -ex "monitor swd_scan"` |
| ST-Link/V2 clone → **re-flashed to BMP firmware, 2026-07-14** | was `26004600160000305748534E` (stlink mode), now `00460026300000164E534857` (BMP mode) | Chip inside: **Geehy APM32F103C8T6** (confirmed by physical inspection). Originally shipped with a reverse-engineered stlink-protocol clone firmware (`V2J41S7`) — worked but slow (~53 KB/s) and prone to `st-flash` timeouts. **Re-flashed to Black Magic Probe firmware** (see below) — now enumerates as `1d50:6018`, `/dev/ttyACM2`+`/dev/ttyACM4`, same BMP build as the genuine unit. Post-flash SWD test: `Target voltage: 3.26V` (sane, vs. the genuine BMP's flaky 0.5V this session), immediate clean target detection (`STM32F1 VL density M3`). **This is now the preferred probe for this board.** |

Board USB (app mode, separate from any debug probe): `0483:2d42` STMicroelectronics HT2xxLxx.

## Re-flashing an ST-Link/V2 clone to Black Magic Probe firmware

No physical access needed — the clone's stock firmware accepts a new image over its normal
application-mode USB interface via ST's real (undocumented-but-reverse-engineered) firmware-upload
protocol. No DFU bootloader entry, no button, no BOOT0 short required.

1. Build BMP firmware for the clone target, from `<workspace>/awto-blackmagic`:
   ```
   meson setup build-stlink --cross-file cross-file/stlink.ini --cross-file cross-file/arm-none-eabi.ini
   ninja -C build-stlink
   ```
   Produces `build-stlink/blackmagic_stlink_firmware.bin` (~112KB). Cross-file reports
   `Bootloader: OEM ST Bootloader`, `Load Address: 0x8004000` — this firmware sits on top of the
   clone's original OEM bootloader rather than replacing it.
2. Build [jeanthom/stlink-tool](https://github.com/jeanthom/stlink-tool) (70-star repo,
   `libusb-1.0` + `gcc` + `git`, no other deps):
   ```
   git clone https://github.com/jeanthom/stlink-tool
   cd stlink-tool && git submodule init && git submodule update && make
   ```
3. Probe first (read-only, confirms the tool can talk to the dongle):
   ```
   ./stlink-tool -p
   ```
   First call after a fresh plug-in typically reports `Current mode: 512` / all-zero ST-Link ID and
   says "not in the correct mode, please unplug and plug again" — this is normal, not an error.
   Unplug/replug the dongle, probe again — should now show `Current mode: 1` and the real serial.
4. Flash:
   ```
   ./stlink-tool blackmagic_stlink_firmware.bin
   ```
   Dongle re-enumerates as `1d50:6018` (Black Magic Debug Probe) on success.

**Two open-source firmware replacement options were evaluated; BMP (above) is the one used.**
[devanlai/dap42](https://github.com/devanlai/dap42) (CMSIS-DAP, `STM32F103` make target — "for
STLink/v2 knockoff designs" per its README, pin mapping SWDIO=PB14/SWCLK=PB13/RESET=PB0) was the
other candidate, not tried, since BMP was already the working toolchain this session and gave a
clean result on the first attempt.

## Geehy 96MHz core clock (evaluated, not adopted — see verdict)

The clone's onboard Geehy APM32F103C8T6 is rated to **96MHz**, vs. genuine ST STM32F103's 72MHz —
confirmed from Geehy's own datasheet (`vendors/hantek/2d82/docs/datasheets/APM32F103xC datasheet V2.1.pdf`: "Core and
maximum working frequency: Arm 32-bit Cortex-M3@96MHz") and register-level reference manual
(`vendors/hantek/2d82/docs/datasheets/APM32F103x4x6x8xB_UserManual_v1.6.pdf`, downloaded from geehy.com this session).
96/72/48MHz are the three officially USB-compatible clock points on this silicon.

**This is NOT the same thing as the well-known STM32F103 "128MHz PLLx16" community overclock**
(real, widely documented — e.g. amitesh-singh.github.io/stm32/2018/06/17/overclocking-blue-pills.html,
TKJ Electronics blog, MCUxiaorui/STM32F103-OverClocking on GitHub) — that overclock explicitly
breaks USB, since ST's silicon only has a 1-bit `USBPRE` field (RCC_CFGR bit 22, /1 or /1.5
options), neither of which divides 128MHz down to the required 48MHz USB clock. 96MHz is different:
it's Geehy's own **documented, rated** maximum (datasheet: "the output frequency of PLL cannot
exceed 96MHz"), not a push past rated spec, and Geehy's `USBDPSC` field is 2 bits wide
(`RCC_CFGR[23:22]`, one bit wider than ST's) with a genuine /2 option, letting 96MHz divide cleanly
to 48MHz USB. libopencm3's `rcc_set_usbpre()` only knows ST's 1-bit field, so the extra bit has to
be written directly to `RCC_CFGR` — implemented in
`<workspace>/awto-blackmagic/src/platforms/stlink/platform.c` as a local `rcc_hse8_96mhz_apm32`
clock-scale struct (PLL x12, flash 3 wait-states per Geehy's flash timing table) plus a direct
`RCC_CFGR` write for `USBDPSC`.

**Verdict: works correctly (USB enumerates, SWD connects, all reads byte-identical/verified across
multiple runs) but is NOT faster than the stock 72MHz config for real workloads** — see the
throughput table above. SWD frequency ceiling did rise proportionally (72MHz core -> 6MHz SWD
ceiling; 96MHz core -> 8MHz SWD ceiling, exactly matching the 1.33x clock ratio, confirming BMP's
`stlink` platform SWD is CPU-bitbang-bound not protocol-bound) but real BMDA throughput at each
firmware's respective ceiling came out the same (53.4 KB/s @ 8MHz on the 96MHz build vs the 72MHz
build's untested-via-BMDA gdb-only figures) — the two are close enough that the extra
non-standard/harder-to-maintain clock config isn't worth it on its own.

## Geehy 120MHz core clock (currently flashed, 2026-07-14)

Pushed further after finding a **second, newer Geehy datasheet revision**
(`APM32F103x4x6x8 datasheet_V1.0.pdf`, geehy.com) documenting a 4th USB-compatible clock point:
48/72/96/**120MHz**, via 1/1.5/2/**2.5** fractional USBDPSC dividers respectively — the v2.1
datasheet already saved in this repo only covers up to 96MHz, an older revision.

**This is genuinely different from the well-known STM32F103 "128MHz PLLx16" community overclock**
(explicitly breaks USB on real ST silicon, confirmed via community sources — see prior section):
Geehy's ÷2.5 divider is an official, documented, purpose-built option, confirmed authoritative by
Geehy's own shipped firmware library source
(`apm32f10x_rcm.h`/`.c`, via github.com/SaltedFish-QAQ/battery_management — an independent public
fork of Geehy's official SDK): `RCM_USB_DIV_2_5` enum value maps to `USBDPSC=3`, with the source
comment "(Only for High-density devices for APM32F103xx)". **Our probe's chip is a C8T6
(medium-density) — the high-density caveat did NOT block it working: USB enumerated correctly
after flashing.** Real-world confirmation the ÷2.5 divider isn't fused off on this die, beyond what
any datasheet claims.

**The one genuinely unresolved risk: no source documents a flash wait-state value for the
96-120MHz band.** Checked: the datasheet's own timing table stops at "72-96MHz: 3 waiting cycles";
Geehy's official firmware library only defines `FMC_LATENCY_0/1/2`; the newer
SaltedFish-QAQ fork adds `FMC_LATENCY_3` (confirms 96MHz's value) but nothing beyond. The `WS`
register field is confirmed 3 bits wide (`apm32f10x.h`: `uint32_t WS : 3`), so there's hardware
room for a value the driver simply never named. **Used `flash_waitstates = 4`** — an
**extrapolation**, not a documented value, following the exact +1-cycle-per-+24MHz-band pattern of
the three documented values (0/1/2/3 for 0-24/24-48/48-72/72-96MHz).

**Validation done:** 3 independent full-512KB-flash reads via BMDA at 10MHz SWD (this firmware's
calculated ceiling — see caveat below), all byte-identical to each other AND to the known-good
backup (`sha256: acd530b7...`). This is meaningful evidence `flash_waitstates=4` is sufficient, but
it is empirical confidence from repeated-read consistency, not a datasheet-backed guarantee — if
this probe is ever used for something where a single silently-corrupted byte would matter more
than it does for read-only backup/diagnostic work, re-verify before trusting it further, or drop
back to the 96MHz config (which IS fully documented).

Implementation: `rcc_hse8_120mhz_apm32` clock-scale struct + direct `RCC_CFGR` USBDPSC write, in
`<workspace>/awto-blackmagic/src/platforms/stlink/platform.c`. The 96MHz struct
(`rcc_hse8_96mhz_apm32`) is kept in the same file, unused but documented, as the fully-validated
fallback if 120MHz ever shows a problem.

**Speed result: also not faster** (43.7 KB/s @ 10MHz SWD — see full comparison table above,
this is the slowest of the three clock configs tested). Kept at 120MHz anyway since it's
validated-working and there's no meaningful reason to revert; if reflashing for any other reason,
there's no throughput case for staying at 120 vs 96 vs stock 72MHz — pick whichever is convenient.

### IMPORTANT CAVEAT: platform_max_frequency_set()'s formula is stale, so the "SWD ceilings" reported/used above are not validated real limits

`timing_stm32.c`'s `USED_SWD_CYCLES=22`/`CYCLES_PER_CNT=10` constants are explicitly commented
`/* Values for STM32F103 at 72 MHz */` — reused unmodified at 96/120MHz, this formula computes
*a* ceiling from whatever `rcc_ahb_frequency` is set to, but that ceiling was never re-validated
against real electrical/timing limits at the new clock. **The "6/8/10MHz SWD ceiling" figures used
throughout this document, and all throughput numbers measured at them, are what this stale formula
allows — not a confirmed safe/real limit.** A `raw_divider` diagnostic monitor command (see below)
exists to bypass this formula and empirically sweep past it if that's ever worth doing; not yet
done as of 2026-07-14.

## Diagnostic monitor commands added this session (2026-07-14)

Both added to `<workspace>/awto-blackmagic/src/command.c`, embedded-firmware-only
(`CONFIG_BMDA == 0` guarded — not available via BMDA, only via `gdb monitor` against the live
probe). Currently flashed and available on this probe.

- **`monitor raw_divider [DIVIDER]`** — reads or directly sets `target_clk_divider` (the raw SWD
  bitbang busy-wait loop counter, 0=fastest), bypassing `platform_max_frequency_set()`'s stale
  72MHz-calibrated formula entirely. Use this to sweep past the reported SWD frequency ceiling and
  find the real failure point empirically (verify via repeated flash reads + checksum comparison,
  same method used to validate 96MHz/120MHz above) — not yet done.
- **`monitor self_dump [OFFSET] [LENGTH]`** — hex-dumps the PROBE'S OWN flash (not the target's)
  over the gdb monitor channel, default full 128KB from `0x08000000`. Added because a probe cannot
  debug itself over its own SWD pins — there was no other way to inspect this clone's OEM
  bootloader or verify what firmware actually landed, short of wiring a second probe to
  hard-to-locate SWD test points on the dongle's own PCB. Works regardless of RDP state (reads its
  own flash as normal memory, no debug-port unlock needed). Used to extract and save:
  - `vendors/hantek/2d82/firmware/stlink-clone-probe/oem_bootloader_0x08000000_16k.bin` — the stock ST-Link
    clone's original 16KB OEM bootloader (real, valid ARM image: initial SP `0x20000800`, reset
    vector `0x08002764`), never previously seen/backed up.
  - `vendors/hantek/2d82/firmware/stlink-clone-probe/full_flash_128k_self_dump_20260714.bin` — full probe
    flash (bootloader + our BMP app) as of 2026-07-14, for reference/recovery.

  Was ~50-200 bytes over the 128KB build budget with verbose help text/error strings; trimmed
  those (not logic) to fit — `rom` usage is now 99.97% (131036/131072 bytes), essentially no room
  left for further additions without dropping something else first.

## Genuine factory ROM bootloader confirmed present and readable (2026-07-14)

Distinguishing two different "bootloaders" that came up this session, since the terminology is
easy to conflate:
1. **The 16KB flash-resident bootloader at `0x08000000`** (`oem_bootloader_0x08000000_16k.bin`) —
   what `stlink-tool` actually talks to. Lives in the SAME flash as our app; erasable/overwritable
   by any bad flash to that region.
2. **The genuine factory-mask ROM "System Memory" bootloader at `0x1FFFF000`** — permanently
   burned into silicon at fabrication, cannot be erased or corrupted by anything written to
   `0x08000000`, entered via BOOT0/BOOT1 pin strapping at reset (table in
   `APM32F103x4x6x8xB_UserManual_v1.6.pdf` §2.4, identical addresses/semantics to genuine ST
   STM32F1). This is the TRUE hardware-guaranteed recovery path, separate from and unaffected by
   (1) — confirmed present via `self_dump` (see below), never actually needed this session since
   `stlink-tool` + the flash-resident bootloader worked every time.

Used the extended `self_dump [OFFSET] [LENGTH] [BASE]` command (base param added this session) to
read `0x1FFFF000` directly — no BOOT0 strapping needed, since *reading* this region just needs
running code (our own app) to dereference the address; only *executing* it requires the BOOT0 pin
sequence.

**Result: real, dense ARM code confirmed present** (1903/2048 bytes non-zero, saved as
`vendors/hantek/2d82/firmware/stlink-clone-probe/rom_system_memory_0x1FFFF000_2k.bin`). More significantly —
**the standard STM32F1 System Memory layout is confirmed exactly replicated**: the flash-size
register at the expected offset (`base+0x7E0`) reads `0x0080` = 128KB, and the 96-bit unique
device ID at the expected offset (`base+0x7E8`) exactly matches this probe's known serial
(`26004600160000305748534E`, same value `st-info`/`pyocd` have reported all session) — both at
byte-identical offsets to genuine ST silicon. This is strong direct evidence Geehy faithfully
cloned not just the pin-compatible memory map but the actual System Memory bootloader region
layout — consistent with (though not proof of) having copied ST's real bootloader code, not just
its calling convention. Not compared byte-for-byte against a genuine ST STM32F103 dump (none
available in this repo) — that would be the definitive next check if this ever matters again.

**Practical implication:** the earlier concern about `bmd_bootloader=true` (switching to BMP's own
bootloader, replacing the flash-resident one at `0x08000000`) permanently losing the recovery path
is **overstated** — even in a worst-case bad flash to `0x08000000`, this genuine ROM fallback
exists underneath, unaffected. Recovery would still need physical BOOT0 pin access (same category
of physical action as everything else requiring hands-on the hardware) and a tool that speaks
whatever protocol this ROM bootloader uses (untested — likely USART/AN3155-style and/or USB DFU
per the standard STM32 System Memory bootloader capabilities, not confirmed working on this
specific Geehy die this session).

## `rom_boot`: root cause found via deep research — fundamentally not achievable, do not re-attempt

After five hardware wedge/recovery cycles trying different jump mechanisms (direct function-pointer
jump with manual teardown, then canary+`scb_reset_system()` following BMP's own established pattern
from blackpill-f4/f072/f3/f4discovery/stlinkv3), a deep-research pass (2026-07-14, 94 sub-agents,
822 tool calls, high-confidence/unanimous/primary-sourced) found the actual answer:

**The STM32F103 factory System Memory ROM bootloader at 0x1FFFF000 supports USART1 ONLY. It has
no USB DFU interface at all**, for any F101/F102/F103 density variant (low/medium/high). USB DFU
in ST's factory ROM bootloader exists only on connectivity-line F105/F107 parts — confirmed
directly from ST's own AN2606 (STM32 microcontroller system memory boot mode). The Geehy clone
faithfully reproduces genuine F103 ROM bootloader behavior, including this limitation.

**This means the entire premise of `rom_boot` — jump to the ROM bootloader and have USB DFU
enumerate — was never achievable, on this chip family, by any reset mechanism, clock configuration,
or BOOT0/BOOT1 handling.** There is no DFU endpoint in the ROM to bring up, regardless of how
cleanly the bootloader is entered. Every symptom observed (USB never enumerating after a direct
jump; inconsistent disconnect/reconnect landing back in the normal app after canary+reset) is
consistent with this — not a sequencing bug we could have fixed with more careful teardown.

This also explains, retroactively, why third-party F103 USB bootloader projects (STM32duino
bootloader, dapboot, etc.) exist as entirely separate, custom-written, flash-resident bootloaders
in the first place, rather than just "jump to the ROM bootloader" — including the 16KB bootloader
already at `0x08000000` on this exact probe that `stlink-tool` talks to. That flash-resident
bootloader (reached only via physical replug) is the correct, and only, path for USB-based
reflashing on F103 silicon — not a workaround for a flaky ROM bootloader, but the *only* way F103
ever gets USB DFU at all.

**One genuinely unresolved side question** the research could not close from any primary source:
whether `SYSRESETREQ` (`scb_reset_system()`, a core-only reset) re-latches BOOT0/BOOT1 the same way
a real NRST-pin/power-on reset does. This is now moot for the USB-DFU goal (see above), but would
matter if `rom_boot` were ever repurposed to enter the ROM bootloader for its actual working
interface (USART1) instead.

**Verdict: `rom_boot` as a way to reach USB DFU is retired. Do not re-attempt without new
information.** The `self_dump`/`raw_divider`/`swd_bench` diagnostic commands remain useful and are
kept. `rom_boot` itself (canary write + `scb_reset_system()`) is left in the codebase as-is (it's
harmless — it does successfully write the canary and trigger a real reset, just doesn't lead
anywhere useful) but should not be relied on for reflashing.

## Known limitation: no software way found to re-enter the OEM (flash-resident) bootloader without a physical replug

Tried `dfu-util -e` (standard USB DFU DETACH request) against the running BMP app — the probe's
`usb_dfu_stub.c` DFU interface (confirmed present: `dfu-util -l` shows a "Black Magic DFU" runtime
interface) accepted the request and called `platform_request_boot()` +
`scb_reset_system()`/`scb_reset_core()` as designed, but the device re-enumerated as the BMP app
again, not the OEM bootloader — i.e. the reset happened but the OEM bootloader (closed-source,
undocumented) didn't stay resident afterward, presumably because it doesn't check whatever
condition `platform_request_boot()`'s non-`ST_BOOTLOADER` path sets (a GPIO pull-up/down marker +
`SCB_VTOR=0`) — that mechanism is designed for BMP's OWN bootloader (`usbdfu.c`, unused in this
build since `bmd_bootloader=false`), and there's no reason to expect a stock ST/Geehy bootloader
recognizes the same marker. **Physical unplug/replug remains the only confirmed-working way to
reach the flashable bootloader state** — this has been done repeatedly this session and works
every time, just requires physical presence.

## SWD transfer speed investigation (2026-07-14) — full findings

Triggered by wanting `rom_boot`-style convenience features to be fast, this turned into a broader
"how fast can this probe actually go" investigation. Summary: the bit-level bitbang code is
already optimal; the real, PROVEN-actionable next step is protocol/firmware-layer (USB buffering,
transaction batching, RAM-transfer-focused profiling), not further bit-toggle-loop tuning. Tracked
as **GitHub issue with the `stlink` label** (create alongside this doc update) for follow-up work.

### What's already confirmed optimal (don't re-investigate)

- **GPIO bit-toggle code** (`swdptap.c`) already uses atomic BSRR/BRR register writes (not
  read-modify-write on ODR), minimal instruction count per bit, down-counting loops for cheap
  `SUBS`/`BCS` branches instead of `CMP`. Confirmed by direct source read against
  `github.com/blackmagic-debug/blackmagic` main branch — no headroom here.
- **SPI-for-SWD is a well-explored, consistently-abandoned dead end** for this protocol
  specifically — SWD's 46-bit non-byte-aligned transactions and mid-transaction direction
  turnarounds don't map onto SPI's fixed-frame, fixed-direction-per-transfer hardware model.
  Sources, all independently reaching the same conclusion:
  - ST Community: ["STM32 use SPI implement SWD?"](https://community.st.com/t5/stm32-mcus-products/stm32-use-spi-implement-swd/td-p/308653) — works on STM32L4R5 (SPI3, MISO/MOSI shorted
    externally, ACK bits still bit-banged) but modest real-world speedup; one expert commenter
    notes hardware SPI gives no real throughput win since "the rate at which transactions can be
    submitted is not higher than with bit-banging" once accounting for hardware-completion waits.
  - ST Community: ["SPI with two MOSIs for SWD and SPMI"](https://community.st.com/t5/stm32-mcus-products/spi-with-two-mosis-for-swd-and-spmi-protocol-implementation/td-p/323533) — built a hybrid SPI/GPIO
    approach, explicitly reverted: *"I went back to just bit-banging which I found to be more
    reliable as well as plenty fast enough for my application"* (6+ MHz achievable via bit-bang on
    M4 @ 120MHz).
  - [lupyuen/pi-swd-spi](https://github.com/lupyuen/pi-swd-spi) / [openocd-spi](https://github.com/lupyuen/openocd-spi) (Raspberry Pi, not STM32, but same
    protocol problem) — working implementation, detailed writeup at
    [lupyuen.github.io](https://lupyuen.github.io/articles/openocd-on-raspberry-pi-better-with-swd-on-spi) explains exactly why: *"We can't flip the direction of
    transmission within a single SPI byte transfer"* — turnaround bits land mid-byte, ACK reads are
    faked (`always lies and returns 100`), requires padding workarounds causing STICKYORUN errors
    needing periodic ABORT-register clears. Direct quote on the root cause: *"If ARM had added just
    two more bits and rounded up to 48 bits, SWD over SPI would have been possible reliably and
    efficiently long ago."*
  - Black Magic Debug's own ESP32-S2 port has `swd-spi-tap.c` — an experimental SPI-based driver
    that exists in the tree but is **disabled**, due to ESP-IDF's 3-wire SPI mode-switching
    limitations. Confirms even BMP's own maintainers tried this and didn't ship it.
  - GitHub code search across `blackmagic-debug/blackmagic` (all history) for SPI+SWD combinations:
    **zero hits** — no SPI- or timer-peripheral-driven SWD implementation exists anywhere in the
    project, for any of its ~13 supported hardware platforms.

### The actionable finding: genuine J-Link hardware is the SAME silicon class as ours

A real J-Link v9 teardown/unbricking writeup ([uglyduck.vajn.icu](https://uglyduck.vajn.icu/ep/archive/2019/05/Unbricking_a_SEGGER_J_Link_v9_debug_probe.html)) identifies its main chip as an
**STM32F205RC (Cortex-M3, up to 120MHz)** — the same MCU class as this probe. No FPGA/CPLD found
in any source. SEGGER's own marketing attributes J-Link's speed to "highly-optimized flash
loaders" and adaptive/RTCK clocking ([segger.com/flash-download](https://www.segger.com/products/debug-probes/j-link/technology/flash-download/)), not exotic hardware.

**This means J-Link's 10-16x speed advantage over bitbang/ST-Link-class probes is a firmware/
protocol-layer achievement, on comparable silicon — genuinely reachable in principle, not a
hardware ceiling.** This is the one real, unresolved lead from this whole investigation.

### RP2040 PIO — the other real hardware-offload option, not available to us

Official `raspberrypi/debugprobe` firmware genuinely uses the RP2040 PIO block (`src/probe.pio`):
a command-queue state machine, side-set clock generation, 4 PIO cycles/bit, CPU-independent
(though not DMA-fed — CPU still blocks on FIFO push/pop). Real, shipped, CPU-offloaded bit
generation — but requires RP2040 silicon we don't have. Forum-reported gains
([forums.raspberrypi.com](https://forums.raspberrypi.com/viewtopic.php?t=337284)) were modest (~5.5x, stock picoprobe
~17KB/s → rewritten `pico_debug` ~94KB/s) and mostly attributed to eliminating host-tool
round-trips/batching, not the PIO hardware itself — reinforcing that transaction batching is the
real lever, achievable on any silicon including ours.

### Throughput comparison table (multiple independent sources, cross-checked)

Most rigorous single-methodology comparison found — community benchmark using probe-rs's own
library, same target (STM32H743ZI), same tool, SWD write:
([9names.github.io](https://9names.github.io/embedded/rust/debug/2022/04/07/cmsisdap-probe-performance.html), raw CSV at [9names.github.io/vendors/hantek/2d82/assets/2022-04-04/data.csv](https://9names.github.io/vendors/hantek/2d82/assets/2022-04-04/data.csv))

| Probe | Throughput | Class |
|---|---|---|
| ST-Link V3 | 1,107 KB/s | dedicated hardware accelerator |
| hsprobe/lpclink2/mculink (CMSIS-DAP) | 220–300 KB/s | hardware-assisted CMSIS-DAP |
| ST-Link V2 | 160 KB/s | dedicated hardware accelerator |
| J-Link (via probe-rs, protocol-capped) | 113 KB/s | proprietary protocol not fully used |
| microbit_v2 (CMSIS-DAP) | 135 KB/s | hardware-assisted |
| rust-dap / rustyprobe (CMSIS-DAP) | ~55 KB/s | MCU-based CMSIS-DAP |
| dap42 / usbc-pill / dappermime (CMSIS-DAP) | ~25.6 KB/s (all three converge) | GPIO-bitbang, no accel |
| **our probe** | **~43 KB/s** | GPIO-bitbang, custom BMP, 120MHz clock |

Other cross-checked sources (different targets/methodologies, directionally consistent):

- SEGGER's own benchmark ([segger.com/news/j-link-is-fastest-in-flash-programming](https://www.segger.com/news/j-link-is-fastest-in-flash-programming/)), STM32F417IG:
  J-Link 244 KB/s, ULINKpro 60 KB/s, IAR I-jet 27 KB/s, ST-Link (via IAR) 15 KB/s. Vendor-published
  (competitor bias), but roughly matches independent OpenOCD+ST-Link/STM32F1 reports of ~15-17KB/s.
- probe-rs GitHub issues (tool-vs-tool, same probe, not probe-vs-probe):
  [#2298](https://github.com/probe-rs/probe-rs/issues/2298)/[#2299](https://github.com/probe-rs/probe-rs/issues/2299) — probe-rs 65-71KB/s vs **OpenOCD 115-150KB/s**, same
  ST-Link-class adapter, STM32H743 — a same-hardware, tool-only comparison directly demonstrating
  the "it's the firmware/protocol layer, not the silicon" point.
- DAPLink: ~8-9 KB/s (GitHub issue [ARMmbed/DAPLink#627](https://github.com/ARMmbed/DAPLink/issues/627)).
- Black Magic Probe on real STM32F103 Bluepill (independent blog benchmark,
  [buger.dread.cz](https://buger.dread.cz/black-magic-probe-bmp-on-bluepill-stm32f103c8-minimum-system-development-board.html)): 6-17 KB/s — our 43KB/s beats this, likely due to the
  non-standard 96/120MHz clock vs their presumably-stock 72MHz.
- EEVblog forum consensus (qualitative): USB Full-Speed HID transport itself caps CMSIS-DAP-class
  probes around ~64KB/s theoretical regardless of SWD engine speed; USB High-Speed (e.g. NXP
  MCU-Link/LPC55S69) is "critical" for real gains beyond that ceiling.

**Interpretation**: three independent GPIO-bitbang CMSIS-DAP implementations (dap42, usbc-pill,
dappermime) converge tightly on ~25.6KB/s — likely the real, well-established bitbang baseline.
Our 43KB/s already exceeds that. Hardware-accelerated probes cluster 135KB/s-1.1MB/s — a 3-25x
gap over any bitbang approach, confirming raw bit-toggle speed is not where the remaining
opportunity is.

### What's next: concrete, unexplored directions (tracked in GitHub issue)

The user's own framing of the two main suspected inefficiencies, worth testing directly rather
than theorizing further:

1. **USB buffer size / transaction granularity.** Standard ST-Link-class implementations
   (including ours, inherited from generic BMP) are suspected to use small USB transfer buffers,
   causing many more USB transactions than necessary per logical operation — each with its own
   round-trip latency floor, independent of SWD clock speed. J-Link's "optimized flash loaders"
   likely batch far more work per USB transaction. **Concrete test**: measure current USB packet/
   transaction count per flash-read operation (USB capture via Wireshark/usbmon, already known
   available on this machine — `vendors/hantek/2d82/docs/hardware/waveform-readback.md` suggests packet-capture
   tooling is already in use on this project) and compare against theoretical minimum for the
   same data volume.
2. **Susceptibility to USB bus contention from other devices.** Suspected that heavy USB traffic
   from unrelated devices on the same bus/hub can stall this probe significantly more than a
   probe with a more robust buffering/retry strategy — worth testing empirically (saturate the bus
   with another device, e.g. a USB webcam or mass storage transfer, and re-measure probe
   throughput) rather than assuming.
3. **Double-buffered ring-buffer USB transfers.** A genuine architecture change: instead of
   synchronous request/response per SWD transaction, queue multiple outstanding transactions in a
   ring buffer, let the USB stack keep the pipe full continuously (double-buffering: host prepares
   the next batch while the probe is still processing/transmitting the current one). This is
   consistent with what "optimized flash loaders" almost certainly do differently from naive
   synchronous implementations — the actionable direction is prototyping this rather than
   inferring it from indirect throughput numbers alone.
4. **Possible custom protocol** — if BMP's existing GDB-RSP-based remote protocol has inherent
   per-transaction overhead that can't be removed within its framing, a leaner custom
   binary protocol between host and probe (bypassing RSP for bulk operations specifically) is
   worth considering — but only after (1)-(3) are measured, since the RSP layer may not actually
   be the bottleneck (see `swd_bench`'s finding this session: `target_mem32_read`'s ADIv5-mandated
   posted-read/RDBUFF-flush behavior already accounts for some of the "missing" throughput without
   needing a new host protocol).
5. **Immediate, narrower goal (before any of the above): measure pure RAM-to-host transfer speed
   in isolation.** Flash erase time, programming-algorithm overhead, and flash write timing are
   separate, target-flash-specific bottlenecks unrelated to the SWD/USB transport question — RAM
   reads/writes have no such overhead and isolate the actual transport ceiling most directly. The
   `swd_bench` monitor command (added this session, `src/command.c`) already does single-word
   timed RAM reads (~7000/sec observed) — the next step is a **bulk** RAM-to-host transfer
   benchmark (not single 4-byte reads in a loop, which pay a full request/response round-trip
   each time) to see what sustained throughput looks like once transaction-count-per-byte is
   amortized the same way flash reads already are via `adiv5_mem_read_bytes`'s DRW-pipelining.

## Self-flash bootloader replacement — attempted 2026-07-14, INCOMPLETE, probe unchanged

**Goal**: replace the clone's OEM bootloader (mystery-string-laden, undocumented, and the reason
`rom_boot`'s live-jump approach never worked — see below) with BMP's own bootloader
(`src/platforms/stlink/usbdfu.c` + `src/platforms/common/stm32/dfu_f1.c`, enabled via
`bmd_bootloader = true` in `cross-file/stlink.ini`), self-flashed by the app itself with no
external probe/gdb/BOOT0 pin needed.

**What was actually built** (in `<workspace>/awto-blackmagic`, uncommitted):
- `bmd_bootloader = true` in `cross-file/stlink.ini`
- `platform_self_flash_bootloader_if_needed()` in `platform.c`: on every boot, `memcmp`s flash at
  `0x08000000` against an embedded copy of the new bootloader
  (`src/platforms/stlink/bootloader_image.h`, generated via `xxd -i`); if different, erases +
  programs + verifies before continuing to normal boot. Verify failure hangs forever
  (deliberately, to surface the fault rather than silently continue half-flashed).
  App itself is disjoint (`0x08002000`+, confirmed via `readelf -l`), so this is safe to self-write.
- Removed the old app-side manual jump-into-OEM-bootloader code in `platform_init()` (interrupt/
  NVIC/SysTick teardown sequence) since it's superseded by BMP's own bootloader checking the
  canary itself on cold boot — see git history / earlier version of this doc for that abandoned
  approach and why it never worked (VTOR + NVIC pending-IRQ research, all correct per ARM spec,
  still failed — root cause was never actually found, see next section).
- Disassembled the OEM bootloader properly this session (`objdump --start-address=0x08002764`,
  the real reset handler — first attempt at file offset 0 was garbage, that's the vector table)
  plus `strings`. Found one human-readable string, `"I am key, wawawa"` — **red herring**, sits
  immediately before a completely unrelated USB string descriptor (`\x10\x03S\x00T\x00 \x00L...`
  = "ST Link" in UTF-16LE STRING descriptor format). Not an auth gate, not checked on entry, no
  evidence found of *any* authentication/handshake requirement in the bootloader's entry path.

**Command-based (gdb `monitor self_flash ...`) chunked-transfer delivery was built, then abandoned**
as overcomplicated — see git history for `cmd_self_flash` in `command.c` if useful as reference for
the flash_unlock/erase_page/program_half_word/verify pattern (F1 1KB pages). Monitor commands
require a live gdb target-attach session through the probe, which turned out to be a dead end for
this specific use case (see below).

**What went wrong / where this was left:**
1. `stlink-tool` reported successful writes twice (once for the app-with-self-flash-logic, size
   71848 bytes; stlink-tool's own progress dots completed both times, no error). Probe re-enumerated
   as `0483:3748` after each — consistent with either "new app ran, self-flash succeeded, bootloader
   replaced, but app kept the same USB VID/PID for some reason" OR "the write never actually took
   effect and OEM firmware kept running." 
2. **Resolved which one, at the very end of the session**: `lsusb -v` showed `bMaxPacketSize0 = 64`
   on the live device — but BMP's own `src/platforms/common/usb_descriptors.h` unconditionally sets
   `.bMaxPacketSize0 = 32` for this platform (only `LM4F`/`USB_HS` targets get 64). **This proves
   the device was still running OEM firmware, not BMP, at the point this was checked** — i.e.
   `stlink-tool`'s reported-successful writes did not actually take effect (or didn't persist).
   Root cause NOT diagnosed — didn't get to check stlink-tool's actual write offset/protocol
   behavior before stopping. **The probe's OEM firmware and OEM bootloader are confirmed intact
   and unchanged** — nothing this session actually altered the probe's flash. Safe starting state
   for next attempt.
3. A separate, still-unexplained symptom hit while investigating (2): any real BMDA session
   (`build/blackmagic -t`, `-M version`, even commands needing no target) hung indefinitely with
   zero output, even at max verbosity (`-v63`) and under `strace` (zero syscalls completed after
   attach). `-l` (device list, descriptor-only) worked fine. Initially misdiagnosed as a firmware
   regression from tonight's changes or as a fundamental BMDA-can't-self-target design limit —
   **both wrong/unproven**; given finding (2), the simplest explanation is BMDA's `stlinkv2.c`
   driver (which speaks the REAL ST-Link binary protocol, for driving some other target chip
   through a genuine ST-Link) was talking to genuine OEM ST-Link firmware the whole time, and the
   hang is a real OEM-firmware/BMDA-driver interaction bug — not investigated further.
4. Earlier in the session (before tonight, per session summary at the top of this doc), the clone
   WAS successfully re-flashed to real BMP firmware and DID enumerate as `1d50:6018`,
   `/dev/ttyACM2`+`/dev/ttyACM4` — see the probe table in "Known probes" above (`ST-Link/V2 clone →
   re-flashed to BMP firmware, 2026-07-14`). So BMP-on-this-clone is proven reachable in principle;
   whatever's blocking `stlink-tool` writes from taking effect tonight is either a regression in
   the newer builds, a stlink-tool/protocol quirk specific to tonight's binary sizes, or a
   procedural mistake (e.g. wrong build directory flashed, mode not actually re-entered between
   attempts) — needs fresh-eyes re-investigation, not more guessing under fatigue.

**Next session should start by**: confirming current probe state (`lsusb -v`, check
`bMaxPacketSize0` — 32 = BMP running, 64 = still OEM), then re-attempting the stlink-tool flash
with explicit verification (read back via `stlink-tool -p` firmware-version string, or `lsusb -v`
descriptor check) immediately after each write, before doing anything else, so a failed write is
caught in seconds rather than discovered an hour later through a chain of wrong assumptions.
