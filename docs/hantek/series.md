# 2000-series — firmware + community intel

The 2D82AUTO / 2D72 / 2D42 / 2C72 / 2C42 family.
For hardware BOM see [board.md](board.md). For model detection and feature gates see [unlock.md](unlock.md).

> Community sources (eediscuss 25-page thread, EEVblog, GitHub teardowns) are **unverified** unless stated.

---

## Version table — all known MCU firmware + FPGA builds

Single reference table consolidating every build this project has identified, in one place. "Used
size" excludes trailing `0xFF` erased-flash padding. See the sections below for full evidence per row.

### MCU firmware (app image, `0x08005000` onward)

| Version | Date | Model(s) | Used size | Source | Status |
|---|---|---|---|---|---|
| `2018110201` | 2018-11 | 2C42/2D42/2C72/2D72 | ~226 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_Firmware2018110201.zip` | Official, archived |
| `2018111501` | 2018-11 | 2C42/2D42/2C72/2D72 | ~226 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000 Firmware2018111501.zip` | Official, archived |
| `2018120601` | 2018-12 | 2C42/2D42/2C72/2D72 | ~226 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_Firmware2018120601.zip` | Official, archived |
| `2019010401` | 2019-01 | 2C42/2D42/2C72/2D72 | ~226 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_Firmware2019010401.zip` | Official, archived |
| `2019022001` | 2019-02 | 2C42/2D42/2C72/2D72 | ~226 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_Firmware2019022001.zip` | Official, archived — **leaks build path** `Hantek1xA_SW_DMA_C\Project\EWARM\Debug\Exe` (IAR EWARM) |
| `2019030201` | 2019-03 | 2C42/2D42/2C72/2D72 | ~226 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_Firmware2019030201.zip` | Official, archived |
| `2019041501` | 2019-04 | 2C42/2D42/2C72/2D72 | ~226 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_Firmware2019041501.zip` (+ dup `(1).zip`) | Official, archived |
| `2019060601` | 2019-06 | 2C42/2D42/2C72/2D72 | ~226 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_Firmware2019060601.zip` | Official, archived |
| `2019082401` | 2019-08 | 2C42/2D42/2C72/2D72 | — | not archived | **Void — never released** (per eediscuss tid=13676) |
| `2019082801` | 2019-08 | 2C42/2D42/2C72/2D72 | ~226 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_Firmware2019082801.zip` | Official, archived |
| `2019110801` | 2019-11 | 2C42/2D42/2C72/2D72 | 226,390 B | `vendors/hantek/2d72/firmware/releases/Hantek2000_Firmware2019110801.zip` | Official, archived — last version modded by AxGxP |
| `2020102501` | 2020-10 | 2C42/2D42/2C72/2D72 | — | not archived, forum-mentioned only | Official, base for "quinstaller" mod |
| `AxGxP_001` | 2019-12-12 | 2C42/2D42/2C72/2D72 | 226,390 B (patch, size-preserving) | `vendors/hantek/2d72/firmware/releases/HantekHTXAxGxP0001.zip` | **Community hex-patch** on `2019110801` — 76-byte diff, UI/button changes |
| `AxGxP_002` | 2019-12-13 | 2C42/2D42/2C72/2D72 | 226,390 B (patch, size-preserving) | `vendors/hantek/2d72/firmware/releases/HantekHTXAxGxP002.zip` | **Community hex-patch** on `2019110801` — 123-byte diff, 2 bugfixes |
| *(quinstaller mod)* | 2021-04 | 2C42/2D42/2C72/2D72 | — (not archived) | forum-mentioned only, no file in repo | **Community hex-patch** on `2020102501` |
| `2021090901` | 2021-09 (secondary build tag `CAR2021081801` = 2021-08-18; published 2021-11-18) | **2D82 / 2D82AUTO** | 327,099 B | `vendors/hantek/2d82/firmware/2d82-20210818/HTX2D82-2021090901-inner.dfu` | **Official — only public 2D82(AUTO) release**, hash-verified 2026-07-13 against a fresh hantek.com download, and against 3 Wayback Machine crawls spanning 2025-10 to 2026-06 (all byte-identical, same filename, no other version ever hosted at that path) |
| *(unlabeled — "mystery build")* | unknown | **2D82AUTO** (this board) | 434,677 B | `vendors/hantek/2d82/firmware/2d82-20210818/swd-dumps/flash_full_20260713_swd_pyocd.bin` (SWD-dumped 2026-07-13) | **Unofficial — origin unresolved.** Self-reports version string `2021090901` (relocated/decompressed RAM copy, not a flash literal — see version-string mechanism section below) but is a genuine alternate compile: +32.9% size, near-zero same-offset byte identity vs. `2021090901`, non-uniform vector-table shift (recompile signature, not hex-patch — see quantitative test below). Missing the official build's secondary `CAR2021081801` build-date tag entirely (present in official, not found anywhere in mystery build — weak additional evidence for "different build," not conclusive). 87% function-level match via Version Tracking; added boot-time UART debug prints + expanded DMM/USB debug instrumentation vs. official. Ruled out: AxGxP-style hex-patch (wrong size class, wrong shift signature — quantitatively disproven), any known official release (none newer than `2021090901` found published anywhere, incl. regional `e.hantek.com` portal and Wayback history), PC-app/cloud-updater-embedded firmware (none found). Leading theory: internal Hantek engineering/debug build — **unconfirmed**. |

### FPGA gateware (Lattice MachXO2, internal codename `DS1302`)

| Version | Date | Size | Source | Status |
|---|---|---|---|---|
| `V02` | 2018-12 | ~43 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_FPGA(V02).zip` | Official, "good working build" |
| `V04` | 2019-07-30 | ~43 KB | `vendors/hantek/2d72/firmware/releases/Hantek2000_FPGA(V04).zip` (`DS1302_2019071801.bin`) | Official, "good working build" — AutoUpdate can wrongly downgrade V07→V04 |
| `V05` | ~2019 | — | not archived | "Good working build" per docs; not in repo. Hantek support redistributes only via direct email (`caiyijun@hantek.com`) per eediscuss tid=14649 |
| `V07` | 2020-05-30 | 42,992 B | `vendors/hantek/2d82/firmware/FPGA_V07.bin` (from `FPGA_V07.zip`), `fpga/hantek/v7/FPGA_V07.bin` | Official, "good working build" — this board's FPGA ident register reads `7` (live-confirmed 2026-07-13), matching this version. **Full bitstream content unverified** — no JTAG probe available this session to extract and byte-compare the live bitstream (only the version-ident byte was read); Dan has confirmed he manually updated this board's FPGA himself at some point, so a difference (if ever found) would be expected, not a new mystery |
| `0x0` / `255` | — | — | — | Dead / unprogrammed marker values, not real versions |

---

## Firmware build (confirmed)

- **CPU:** Geehy APM32F103VET6 (STM32F103VET6-compatible clone, Cortex-M3), app @ 0x08005000, 20 KB custom DFU bootloader
- Bare-metal register C, GCC (`push {r7,lr}` prologue), no HAL/SPL/RTOS, custom GLCD
- Not encrypted (entropy ~6.1); properly stripped (no symbols/paths/asserts)
- Debug strings include pinyin `cai ji wan cheng` = "acquisition complete"
- **Automotive diff:** 112 functions are automotive-only (preset UI); 497 in common with 2C/2D variants

---

## USB control protocol (already RE'd — use these)

- **[hkoosha/hanteker](https://github.com/hkoosha/hanteker/tree/main/hanteker_lib)** — Rust library, cleanest impl. VID `0x0483` PID `0x2d42`; OUT=EP2 IN=EP1 bulk; 9-byte command frame `[0x00][0x0A][func][cmd][val..][0x00]`; func codes scope=0x0000, capture=0x0100, AWG=0x0002, screen=0x0003
- **[lucaoli/Hantek](https://github.com/lucaoli/Hantek)** — original C/libusb RE

---

## DFU / flashing / recovery

- **DFU entry:** scope off → USB → hold **F1 + Power** until lower-right flickers; OR remove both batteries, hold F1 while plugging USB. Flash with DfuSe Demo
- **[!] Flash from NATIVE Windows** — VMware corrupts the flash
- **[!] Do NOT use AutoUpdate "Force update ARM/FPGA"** — unconditionally flashes 2D82AUTO firmware onto any model
- **Recovery ladder:** battery-pull DFU reset → **UART/TTL 3.3 V** Flash-Loader ("Program Arm1", RX↔TX crossed) → **ST-Link SWD** full-dump reflash (last resort; dump includes per-unit calibration)
- **FPGA** versioned separately: 0x0/255 = dead, V02/V05/V07 = good; can be downgraded by AutoUpdate

---

## Firmware downloads

- 2000 handheld fw: hantek.com SoftwareDL id=41420 (`Hantek2000 Firmware.zip`), id=41423 (`Hantek2D82`)
- Update tool: id=41421/41422; manual: id=41304
- Bench DSO2000 (different platform): id=46105
- Community dumps: circuit-specialists/Hantek-2D72 (2019 dump + dfu); IcedOdin fork PR#3 (2021 full SWD dump)

---

## Official handheld (2C/2D-series) firmware history — pre-2021, all archived under `vendors/hantek/2d72/firmware/releases/*.zip`

All are single-file DfuSe `.dfu` images, app @ 0x08005000, same DfuSe container format as the 2D82. This is
the **2C42/2D42/2C72/2D72 handheld family**, not the 2D82AUTO — a separate, smaller (~226 KB vs 2D82's 327 KB)
firmware image, but same manufacturer codebase/debug-string lineage (shared `cali_data`/`VoltScale` debug prints).
Chronological official releases (per eediscuss tid=13676, moderator "amy"):

`2018110201` → `2018111501` → `2018120601` → `2019010401` → `2019022001` → `2019030201` → `2019041501` →
`2019060601` → `2019082401` (void, never released) → `2019082801` → `2019110801` (last official release seen
on the thread). FPGA gateware shipped separately: `V02` (2018-12), `V04`/`DS1302_2019071801` (2019-07-30),
`V07` (2020-05-30) — internal codename `DS1302`, ~43 KB raw bitstream, no readable strings.

**`HantekHTX2019022001.dfu` leaks a build-machine path** (uninitialized memory in the DfuSe container's
255-byte target-name field, right after the standard `"ST..."` placeholder — not scrubbed by the packaging
tool). Decoded (partial GBK Chinese fragment + path):
```
...:\<drive-relative>示波器用\Hantek1xA_SW_DMA_C\Project\EWARM\Debug\Exe\HantekHTX2019022001.dfu
```
(`示波器用` = "for oscilloscope"). Confirms: built with **IAR Embedded Workbench for ARM** (`EWARM\Debug\Exe`
is IAR's standard output path), internal project codename **`Hantek1xA_SW_DMA_C`** ("SW_DMA" — likely a
software-DMA acquisition variant). No other firmware in the sweep (official 2D82, 2D72, or any AxGxP patch)
carries this leak — it is unique to this one 2019-02-20 release. Several other releases leak unrelated
Windows shell/property-store garbage in the same buffer (WinSxS manifest path revealing a **Windows 10
build 16299, zh-CN locale** packaging machine for `2018110201.dfu`; a `CLSID\{0340F119-...}` shell-folder
GUID for `2019041501.dfu`) — cosmetic OS-shell noise, not project-specific.

---

## AxGxP community-patched firmware — full analysis (`HantekHTXAxGxP0001.zip`, `HantekHTXAxGxP002.zip`)

Both are **binary patches applied directly to the official `HantekHTX2019110801.dfu`** (confirmed: identical
226699-byte DfuSe container, same DFU target-name garbage bytes as the 2019110801 base, both differ from
that base by only 76 (`0001`) / 123 (`002`) bytes out of 226390). `HantekHTXAxGxP002.zip`'s own zip comment
states directly: *"Firmware update for Hantek2000series. Based on HantekHTX2019110801. Created by AxGxP,
13.12.2019."* The firmware's own embedded version string (file offset `0x37428`, flash address ≈`0x8005000+
0x37303`) is changed from `'201911080` to `'AxGxP_001` / `'AxGxP_002` respectively — self-identifies, does
not spoof the official version.

Author "AxGxP" states on the forum (machine-translated from Russian, page 1 of the 25-page eediscuss
tid=13676 thread): *"I don't have the source code, only the ability to edit the factory firmware, limited
by its addressing... a lot of unnecessary stuff there, some can be discarded, slightly redone, but only
within the existing code and its size."* — confirms hand-patching of the shipped binary, not a from-source
rebuild. AxGxP0001 changelog (translated): reduced menu display timeout 17s→8s, remapped Left/Right to
amplitude and Up/Down to timebase during channel/trigger setup, made F1–F4 activate settings directly
without opening the menu; known regression: Roll mode no longer auto-switches to Y-T above 100 ms/div.
AxGxP002 changelog: fixes "shutdown when switching from 50ms to 20ms" and "Roll→Y-T auto-switch" bugs.

All byte-level diffs between AxGxP0001/002 and the 2019110801 base are 2-byte changes **inside Thumb-2 `bl`
instruction offset fields** (verified via `arm-none-eabi-objdump -m arm -M force-thumb`) — e.g. at flash
`0x0801ee9c`, base calls `bl 0x801dce8`, both AxGxP0001 and AxGxP002 redirect the same call site to
`bl 0x801de56` (a structurally similar sibling function — both begin `uxtb r2,r2; cmp r2,#0`, both use
`ldrsh`-based min/max compares consistent with timebase/trigger boundary clamping). This is pure call-target
patching — redirecting existing calls to other existing functions already present in the same image, exactly
matching AxGxP's own description of the technique. No new code was injected; nothing was added beyond what
already existed in the 2019110801 binary.

**Relationship to the 2D82AUTO "mystery firmware" (see the SWD/version-tracking investigation dated
2026-07-13): none.** Byte size alone rules it out — AxGxP app payload is 226390 bytes vs the 2D82's official
327099-byte app and the board's 503808-byte-region SWD dump. Direct comparison confirms: reset vector and
first bytes differ immediately (`cmp` byte-1 diff), and a naive same-offset byte match is ~4% (noise level
for two unrelated binaries). These are firmware for the smaller-flash 2C42/2D42/2C72/2D72 handheld family,
built from the `2019110801` lineage (2019-11), not the 2D82AUTO's `2021090901` lineage (2021-09) that the
mystery build is derived from. **The AxGxP zips do not explain the mystery build's origin.**

**Correction (2026-07-13, second pass):** an earlier note here claimed forum user "cfinner" asked *"does
this work with 2d82auto?"* on 2020-01-26. A full scripted read-through of **all 25 pages** of tid=13676
(234 unique posts, not just page 1) found **no post from any user named "cfinner", and no post asking that
question anywhere in the thread** — see the full read-through section below. This appears to be a
misattribution or hallucination from whatever partial view produced it; retracted. What the full
read-through *does* confirm independently: 2D82/2D82AUTO is mentioned in exactly 4 of 234 posts, and every
one of them is a user who got 2D82AUTO firmware force-flashed onto their 2D72/2D42/2C72/2C42 by the
AutoUpdate tool bug (see below) — not a request for, or claim of, a 2D82-targeted patch.

`153742eppv9c99eczk9cl4.png` is a screenshot of the official **HT2xxAutoUpdate** GUI tool (referenced in the
forum's official update instructions) — not related to AxGxP; documents the "Force update Arm"/"Force update
FPGA"/"Rescue" options and the note that a locally-supplied firmware file name must be an all-digit version
number (explains the `YYYYMMDDNN.dfu` naming convention parsed by the tool).

The saved PDF (`Update the firmware of Hantek2D72...pdf`) is a same-day (2026-07-13) browser print-to-PDF of
page 1/25 of the live eediscuss thread — not an independently-dated archival copy, so it adds nothing the
live thread doesn't already show, beyond being useful as a static, searchable local copy of that one page.

---

## eediscuss tid=13676 / tid=14649 — full 25-page + sibling-thread read-through (2026-07-13)

The AxGxP-zip binary forensics above was done from page-1 forum text alone. This section is the
follow-up: **all 25 pages of tid=13676** (234 unique posts; pagination confirmed as
`forum.php?mod=viewthread&tid=13676&extra=&page=N`, N=1..25) and **both pages of tid=14649** (19
unique posts) were fetched and read via a scripted read-only browser session (playwrong, dedicated
instance on port 8751, no login/registration/attachment-download attempted). Findings:

**Thread identity.** tid=13676 is overwhelmingly an **official Hantek support thread**, not a
modding thread — "amy" (Hantek support account, 35 posts) posts each dated official firmware
release as it ships. Every official version number from `2018110201` through `2021090901` (our
board's own self-reported string) appears in the thread as a normal, dated support post — confirms
`2021090901` is simply the next release in the ordinary official cadence, nothing unusual about the
version number itself. The bulk of the 234 posts are routine support traffic: DFU flashing
failures, DMM/AWG bug reports, calibration questions, "why no changelog" (amy: none provided,
confirms `unlock.md`'s existing note that Hantek keeps no public changelog), and — most frequently
— confused users who ran AutoUpdate's "Force update" and got the **wrong model's firmware pushed
onto their device** (2D82AUTO firmware onto a 2D72/2D42/2C72/2C42, or an unwanted FPGA downgrade).

**2D82/2D82AUTO discussion: absent as a mod target, present only as an AutoUpdate accident.**
2D82/2D82AUTO is named in exactly 4 of 234 posts. All 4 are users reporting that AutoUpdate's
"Force update" flashed 2D82AUTO firmware onto their non-2D82 unit by mistake, or asking how to
revert — e.g. pid 19139 ("do NOT use 'force update', it will install the 2D82auto FW in your
device, no matter what option you choose"), pid 20645 ("I update firmware on the automatic way...
it install the 2D82 firmware. how could I install the 2D72 last firmware again?"). This
independently corroborates (from live community testimony, not just code analysis) the exact bug
already documented in [unlock.md](unlock.md). **No post proposes, requests, or claims a
2D82AUTO-targeted patch** — see the correction above retracting the earlier "cfinner" citation.

**AxGxP's own words, translated in full (corroborates and extends the byte-diff analysis above).**
AxGxP posts 19 times across the thread (2019-01 to 2021-01). The technical substance, machine- and
context-translated from Russian:

- *(pid 19412, 2019-12-12, release post)* "Well... the developers are ignoring us, I had to do it
  myself. Grab it and enjoy! I've made fixes. Firmware built on top of `HantekHTX2019110801`.
  Changes: menu display time reduced 17s→8s; fixed button assignment in amp/timebase setup (right
  arrow now increases amplitude, left decreases; down arrow slows timebase, up speeds it up — control
  like a normal scope!); and, probably most important, you can now switch settings without bringing
  up the menu — F1–F4 directly activate Ch1 amp / Ch2 amp / Trig / Time without a menu popup.
  Everything else works as in the original. New bug introduced: ROLL mode no longer auto-switches to
  Y-T above 100ms/div."
- *(pid 19438, 2019-12-13)* "Bug with shutdown when switching from 50ms to 20ms is fixed! Bug with
  automatic switching from Roll mode to Y-T mode is fixed! Firmware is the same on all these
  oscilloscopes: 2C42/2D42/2C72/2D72." (task brief's quoted text — verbatim match confirmed)
- *(pid 19446, 2019-12-13, reply)* "Theoretically anything's possible... but practically very hard.
  I don't have the program's source code, only the ability to edit the factory firmware, limited by
  its own addressing... I can't just add functionality that isn't there at all without removing
  something else. It's quite possible writing a fully own firmware from scratch would be easier than
  fixing all of this one's shortcomings."
- *(pid 21403, 2021-01-28, his last substantive post)* "Their firmware is written, to put it mildly,
  like garbage... patch on top of patch. Some things could be fixed by **disassembling the
  firmware**, which is what I did. But to add genuinely new functionality you'd need to write a
  completely new firmware from scratch. Further 'torturing' this firmware is a dead end."

Every technical description from AxGxP himself is consistent with the byte-diff findings above:
hex-patching call targets within the existing 2019110801 binary via disassembly, never a claim of
source access or recompilation. A second, later modder **"quinstaller"** appears once (pid 21716,
2021-04-26): "Modded build 2020102501: menu display time changed to 3 sec; activated backlight of
Power button; cosmetic fonts changes" — same style of patch, based on the later `2020102501`
release. User "Sobakin76" (pid 22126, 2021-07-29) notes AxGxP's mod "is based on old version and
has no updates" — confirming AxGxP's effort was abandoned well before `2021090901` shipped and was
never rebased onto it.

**No debug-instrumentation characteristics mentioned anywhere.** Searched all 234 posts for
"debug"/"UART"/"source code"/"decompil"/"recompil" and Russian equivalents: zero posts describe
anything resembling added debug prints, expanded instrumentation strings, or a from-source rebuild.
AxGxP/quinstaller mods are discussed purely in terms of user-facing UI/button/timeout behavior.
This independently supports the byte-diff conclusion above: **neither this thread's text nor the
AxGxP zip files' binary contents describe or explain our board's specific mystery firmware**
(UART debug prints, expanded DMM/USB instrumentation, 87% VT function match, `2021090901`
version). The "internal Hantek engineering/debug build" hypothesis is neither confirmed nor
ruled out by this thread — it simply describes a different, unrelated community phenomenon.

**Attachments:** both threads gate attachments behind forum SSO login
(`account.lemontreex.com`) — 13 login-gated attachment blocks found across tid=13676's 25 pages, 4
across tid=14649's 2 pages. No filename, size, or description is visible pre-login for any of them
(Discuz's placeholder reveals only "this post contains more resources... Login... Register"). One
tid=14649 post (user "Xakeron") offers "FPGA V7 — take it" with what is presumably an attachment,
filename still not visible pre-login. **No account was created, no login attempted, no attachment
downloaded** — per task constraint.

**tid=14649 ("FPGA update", 19 unique posts, 2 pages)** independently confirms, via direct
community testimony, exactly what fpga.md already documents from
code/bitstream analysis: AutoUpdate's "Force update" **downgrades FPGA V07→V04** unprompted
(three independent users — Alexei, Sobakin76, Igor_kamensk — report exactly this). Hantek support's
only substantive reply to requests for the missing V05 build is to email `caiyijun@hantek.com`
directly — i.e. even Hantek's own redistribution of older FPGA versions was informal/email-based,
not a forum download. No 2D82/2D82AUTO-specific content in this thread — it only covers
2D72/2D42/2C72/2C42, same scope as the main thread.

---

## Mystery 2D82AUTO app firmware: hex-patch vs recompile — quantitative test (2026-07-13)

Follow-up to the SWD/Version-Tracking investigation above. Two hypotheses were live: (A) the
board's app firmware (self-reports `2021090901`, ships from `vendors/hantek/2d82/firmware/2d82-20210818/
swd-dumps/flash_full_20260713_swd_pyocd.bin`, app region = bytes `[0x5000:]`) is a genuine
internal Hantek recompile; (B) Dan may have flashed a community hex-patch of his own at some
point (AxGxP-style: small in-place byte edits to an otherwise-untouched compiled image, no
relinking). This is checkable: a hex-patch cannot change overall file size or move code, so it
leaves long runs of same-offset byte-identical data; a recompile shifts nearly everything via
linker relocation, even in functions whose logic didn't change.

**Files compared** (raw app image, no DfuSe wrapper, both start at flash `0x08005000`):
official = `vendors/hantek/2d82/firmware/2d82-20210818/flash_08005000.bin` (327,099 used bytes, no trailing
`0xFF` padding at all in this 320 KB file); mystery = `vendors/hantek/2d82/firmware/2d82-20260713-swd/
flash_08005000.bin` (434,677 used bytes after stripping trailing `0xFF` erased-flash padding;
verified this file is exactly `flash_full_20260713_swd_pyocd.bin[0x5000:0x5000+434677]`, i.e. the
full SWD dump's app slice with padding already stripped).

**1. File size.** Official used size 327,099 B vs mystery used size 434,677 B — a difference of
**107,578 bytes (+32.9%)**. A hex-patch cannot add or remove bytes (it edits in place); this size
delta alone is close to conclusive against a pure hex-patch of *this* official build as the
direct source.

**2. Same-offset SHA256 chunk comparison**, over the overlapping region (`min(len)` = 327,099 B):

| granularity | total chunks | identical | differing | identical % |
|---|---|---|---|---|
| 256 B | 1,277 | 5 | 1,272 | **0.39%** |
| 4,096 B | 79 | 0 | 79 | **0.00%** |

Only 2 short identical islands exist at 256 B granularity, both **inside the first 20 KB**:
`0x000200–0x000600` (1,024 B — a run of Thumb-2 `b` trampoline instructions, i.e. an IRQ
default-handler jump table, compiler boilerplate) and `0x000a00–0x000b00` (256 B — early
runtime-init code, also boilerplate). Both are byte-for-byte identical between the two builds.
Banding the whole comparable region into 16 equal ~20 KB bands shows **0% identical chunks in
every band except band 0** (which contains the two islands above) — i.e. there is no low level of
background identity carrying on through the file; identity is confined to generic startup
boilerplate at the very front, then drops to exactly zero for the entire remaining ~300+ KB,
all the way to the end of the comparable region. This is the opposite of the hex-patch signature
(long stretches of identity with a few small patched islands) — it is uniform, comprehensive
divergence, consistent with every function having moved.

**3. Vector table** (first 48 words, Cortex-M3): only **5/48 (10.4%) match exactly**, and those 5
are the always-zero reserved vector slots (indices 7,8,9,10,13) — i.e. **zero real interrupt
vectors match**. The real handlers show non-uniform deltas, not one constant shift:

| delta (mystery − official) | count of vectors |
|---|---|
| −79,796 | 29 |
| −82,812 | 11 |
| −79,860 | 1 |
| −1,736 | 1 (initial SP) |
| +30,692 | 1 (IRQ30, positive — moved the *other* direction) |

Five distinct delta clusters, including one outlier moving in the opposite direction from the
rest. A single localized hex-patch (a few edited immediates) cannot move unrelated interrupt
handlers by tens of kilobytes in five different amounts, let alone move one of them the opposite
way; this pattern is exactly what cascading linker relocation across a differently-sized recompile
produces (different functions accumulate different amounts of preceding-code growth depending on
where they land relative to whatever grew).

**4. Control case — the bootloader, where the ground truth is already known.** The
`bootloader-20260713-swd` build vs the existing bootloader (`ghidra_names_bootloader_swd.tsv`) is
a case with **confirmed** ground truth: instruction-for-instruction identical control flow in every
traced function, the *only* structural change being two added `boot_printf_swd` debug-print calls
at the top of `boot_main_swd` (`str_bootloader_start_ptr` / `str_plus_banner_ptr`, TSV lines 22/29/30).
Running the identical methodology on this known-small-addition pair:

- Raw byte diff: 19,438 / 20,480 bytes differ = **94.9%** differ (matches the TSV's own figure).
- Chunked SHA256: **0.00% identical at both 256 B and 4,096 B** granularity (0/80 and 0/5).
- Vector table: **5/48 (10.4%) match exactly** — same reserved-slot-only pattern as the app region.
  Real deltas cluster into exactly **two** values: −226 (11 vectors — handlers after the insertion
  point whose neighbourhood only shifted by the two print calls) and −188 (36 vectors — handlers
  further out that picked up additional shift from something else in between, e.g. compiler/linker
  ordering differences), plus the initial-SP entry at −56.

This confirms the task's premise: address-shift cascade from even a *tiny*, purely-additive,
non-structural change (two print statements) is already enough to erase same-offset byte identity
almost entirely (94.9% differ) and scramble the vector table down to the same 10.4%-match floor
seen in the app region. **A low raw-match percentage by itself cannot distinguish hex-patch from
recompile** — both hypotheses predict a low percentage once relocation cascade is in play.

**What does distinguish them here: cluster count and shift magnitude, not the raw percentage.**
The bootloader's *known* 2-line addition produces exactly **2** delta clusters (−226, −188, plus a
trivial −56 for the stack pointer) — i.e. one shift point, then done — and a byte-size identical
before/after (bootloader region size didn't change, only content within the fixed 20 KB carve-out).
The app region's mystery build instead shows **5** delta clusters with a magnitude of up to
**82,812 bytes** (830× larger than the bootloader's biggest shift) — a shift that big requires around
**80–83 KB of new/relocated code and data appearing ahead of most of the vector targets**, consistent
with the observed **+107,578-byte size growth**, not with "a few debug prints." A hex-patch, by
definition, cannot grow the image at all; this one grew it by a third, with cascading relocation
distributed non-uniformly enough to require five separate shift amounts (one running the opposite
direction), which needs code inserted/removed/reordered at multiple distinct points through the
image, not one small localized edit.

**Conclusion: recompile-like, not hex-patch-like — reasonably confident, not absolute proof.**
Every falsifiable prediction of the hex-patch hypothesis fails: file size is not preserved (+32.9%,
should be exact under a patch); same-offset identity is near-total absence outside generic startup
boilerplate (should be 80–95%+ under a patch, per the AxGxP precedent above which showed sustained
high identity outside small patched islands); the vector table is comprehensively different with
non-uniform, opposite-signed deltas (should be exactly unchanged under a patch, since patching
individual instructions doesn't relocate other functions). The control comparison against the
bootloader's known small addition shows that address-cascade alone can produce a low raw
byte-match number, but it produces a *small, few-cluster, size-preserving* cascade — the app
region's cascade is neither small, few-cluster, nor size-preserving. This is most consistent with
hypothesis A (a genuine alternate compile of similar-but-not-identical source, matching the
already-established 87% Version-Tracking functional match and the added DMM/USB debug
instrumentation strings) and does not support hypothesis B (a hand-patched community image derived
directly from the known official `2021090901` build). It does **not** rule out hypothesis B in a
different form — e.g. a patch applied to some *other*, not-yet-located base build closer in size to
434,677 bytes than the official 327,099-byte release — but no such alternate base is currently
known or in the repo; this analysis only tests patch-vs-recompile relative to the one confirmed
official 2021090901 build in hand.

Method/scripts used for this pass were ad hoc (SHA256-per-chunk same-offset comparison, struct-
unpacked vector table diff) and not committed to `scripts/`; re-derivable from the file paths and
chunk sizes (256 B, 4096 B) stated above against `flash_08005000.bin` in each firmware directory.

---

## Addendum 2026-07-13: full triage of the 86 VT-unmatched functions — no new functionality found

Following the Version Tracking pass above (87% function match), the 86 functions VT could **not**
match (`vendors/hantek/2d82/firmware/vt/fw_2d82auto/only_in_dest.tsv`) were individually examined by reading
their full decompiled bodies (paired against the 143 functions VT flagged as "removed" on the
known-good side, `only_in_src.tsv`, for cross-reference). This is the direct evidence for what the
+32.9% size growth documented above actually *is*.

**Result: every examined function is a recompiled/restructured equivalent of an already-documented
2D82AUTO function — no new USB commands, no new DMM modes, no new hardware interaction, no
test/factory-only code paths were found.** Specifically:

- **USB command surface**: `fw_usb_cmd_parser`'s complete bank/sub-selector dispatch tree (banks
  0-3, each with sub 0/1) was walked exhaustively in the new build. Every bank and sub-selector
  matches the known-good build exactly — no 5th bank, no new sub-selector values. The calibration
  bank's `fw_usb_cali_write_handler` equivalent (byte[4] selector 0/2/3/4/5/6/7/8/9/0xa/0xb/0xc/0xd/
  0xe/0xf) was checked branch-by-branch and has an **identical set of byte[4] values** to the
  known-good build, including the FPGA MachXO2 reprogramming sequence (byte[4]==2/3/4), the AWG-
  present override, and all cal-factor/cal-table/name-table/serial-number writes. No new
  calibration sub-command exists in this build.
- **DMM/current-measurement expansion**: the debug-string additions found in the earlier string-diff
  pass (`DMM_Mea_CurrentA`, `now is ACMA, so DMM_Select`, `A_Cal_Factor = %f, DCMA_Cal_Factor = %f,
  ACMA_Cal_Factor = %f`, etc.) do **not** correspond to any new function in the unmatched set. The
  new build's `fw_usb_dmm_status_pack` equivalent (found via VT-unmatched function `0x0803190a`,
  260 bytes, exact size match to the known-good's 0x080307da) produces a **field-for-field identical
  14-byte USB response** to the known-good build — same magic/length/mode-index/sign/range/digit
  layout, same 11 DMM modes via `fw_dmm_mode_xlat` (unchanged). The CS7721 digit-decode and
  digit-validity-check helper functions (also in the unmatched set) are likewise unchanged in logic.
  **The debug strings' actual calling code could not be located anywhere in this build's decompiled
  call graph** — their flash addresses (~0x0803e51c-0x08040300) fall in a region of the binary that
  Ghidra's function-boundary analysis did not reach as named functions (the surrounding ~0x08036500-
  0x0803e500 region contains only already-named, already-understood USB/DFU/startup code with no
  raw string-address references in the decompiled C). This is most consistent with the strings being
  **linked-but-unreached debug instrumentation** (e.g. a shared debug-print library object compiled
  into this build but whose call sites either got optimized out, live in a function Ghidra didn't
  identify, or are simply dead code from a debug/logging module that ships across Hantek's internal
  builds) rather than a wired-up new DMM feature. This refines (does not overturn) the earlier
  "expanded DMM/USB debug instrumentation" finding: the strings are real and present in the binary,
  but no code path invoking them was found by static analysis of the decompiled functions.
- **New hardware interaction**: none found. Every hardware-tagged unmatched function (FPGA SPI/
  MachXO2, ADC-via-I2S-DMA capture, USART/GPIO, TIM input-capture, DMM CS7721 bit-banging) matches
  an already-documented known-good function 1:1 by call pattern and near-identical body size,
  including the STM32F103 I2S/SPI1-silicon-sharing ADC-capture trick (`fw_adc_dma_capture_init`)
  which is a known pattern already fully understood in the known-good build, not new peripheral use.
- **Test/factory/debug-only code paths**: none found beyond what's already documented (the 2-line
  bootloader UART debug print, already fully traced in an earlier session). No hidden menu, no
  undocumented USB sub-command, no disabled-but-present feature flag was found across the 86
  functions.
- **Size growth explanation**: the largest unmatched functions (`fw_usb_cali_write_handler` 3390→
  3346 B, `fw_acq_process_waveform` 2858→3274 B, `fw_scope_waveform_render_c` 3874→3740 B, etc.) show
  body-size deltas of roughly ±5-15%, consistent with recompilation under different compiler/register-
  allocation settings, not feature addition. Cross-referencing against the 143 "removed" functions in
  `only_in_src.tsv` shows a clean 1:1 "old function replaced by new function doing the same job" for
  every large/significant pair checked — no orphaned removed functionality with no replacement, and
  no net-new function calling genuinely new helper functions anywhere in the 86-function unmatched
  set (checked exhaustively via a full-file scan for `fw_*(` callee names with no counterpart in the
  known-good build's names TSV — every hit resolved to a prior "was wrongly named X" correction, not
  a new function).

**Net effect on the "internal Hantek engineering/debug build" hypothesis**: neither strengthened nor
undermined by this pass. The build is still most consistent with a genuine alternate compile (this
addendum adds direct functional confirmation, not just size/structural evidence), but this triage
found no smoking-gun evidence of an in-development feature, hidden factory-test mode, or unlock
mechanism — the extra ~107 KB is overwhelmingly explained by compiler/toolchain differences
recompiling the *same* feature set, plus whatever unreached debug-string/logging code contributes
to the linked image without being called. Full per-function findings and TSV entries:
`vendors/hantek/2d82/ghidra/ghidra_names_2d82_swd.tsv` (search for the "2026-07-13 VT only_in_dest.tsv triage" comment
block).

---

## Cross-model: bench DSO2000 is a different platform

The bench **DSO2000** (DSO2C/2D10/15) is NOT the same firmware family — firmware is a `.upk` package, 1.8 MB, **encrypted** (entropy 8.00), not DfuSe. Different SoC. Not the same BOM.

---

## Known hardware bugs (EEVblog community)

- **2-channel skew = ½ ADC clock**: AD9288 ENCODE_A/B run 180° antiphase → CH1/CH2 sampled up to ~50 ns apart at ≥10 µs/div. Fixable in replacement FPGA gateware by aligning timestamps.
- **AWG glitches at high frequency**: DAC setup/hold violation. Critical FPGA timing constraint for replacement gateware.
- **AWG→scope feed-through via shared −5 V rail**: max ~5 kHz before AWG couples into scope frontend.
- **AWG can't reach full swing**: −5 V rail runs ~−3.4…−4 V. Fix: resistor change.
- **R14/R15 DMM input** (10 kΩ/1 kΩ): burn out on AC-mains in Ohm mode — extremely common failure.

---

## External references

- wpwrak teardown: <https://github.com/wpwrak/2c42-pix/wiki>
- circuit-specialists 2D72 fw + mod: <https://github.com/circuit-specialists/Hantek-2D72>
- nickbel hack writeup: <https://www.nickbel.com/2019/01/28/hantek-2c42-2d42-2c72-2d72-hack.html>
- AxGxP custom fw: binary-patched UI/timeout/button fixes on 2019110801 (no source) — full analysis above
- eediscuss tid=13676 (FPGA version history), tid=14649 (FPGA update thread) — login-gated
