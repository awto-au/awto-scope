# Hantek 2000-series image hunt — public-web sweep

Date: 2026-07-31 (AEST). Tooling: `scripts/hunt_hantek_images.py` (awto-playwrong headed
Chrome + Google; DDG's image CAPTCHA is unsolvable by `/solve`) plus `gh search`.
Log + fetched page text were `tmp/` scratch (deleted). URLs and sizes below were
checked with `curl -sIL` — **not** playwrong, so treat sizes as `[oss]`, re-verify
any download through playwrong per the repo web rules.

Confidence tags per `docs/reversing/reliability-convention.md`. **Everything sourced from a
forum post or third-party repo is `[oss]` — a LEAD, not a fact.**

---

> **Convergence note.** While this sweep ran, a parallel session independently found the same
> string in the local dump (commit `9b067c9`, "FOUND the model string: it is in a per-unit cal
> page, not the app", #181). That session established *where* it is from our own data; this
> sweep independently supplies the *external* evidence for **what it means** — the community
> `-40`→`-70` bandwidth edit, and the 2D42→2D82-by-DFU report. Two independent evidence kinds
> now agree, which is the strongest state under the convention.

## Headline: the working theory is contradicted, and the answer was already in our data

The task's premise was "the vendor's Windows cal tool writes a **per-model bootloader**, and
that is where the model/bandwidth capability lives." Three independent `[oss]` sources say
otherwise, and one of them I was able to **promote to `[fw-data]` against our own dumps**:

> The model/bandwidth is a **serial-number ASCII string in the calibration page near the end
> of the app region**, not in the bootloader. Its last two characters are the bandwidth in MHz.

**Verified in our own files** `[oss→data]`: `vendors/hantek/2d72/firmware/2d72-20190415/27D72_dump.bin`
contains `CN19010072001598-70` at flash **`0x080598B8`**. Surrounding bytes are IEEE-754
floats (`00 40 8a 3f` = 1.0801, `00 20 8f 3f` = 1.1182 …) — i.e. per-channel gain
coefficients. This is the **calibration/config block**, sitting inside the app region
(0x08005000+), *not* the 0x08000000 bootloader.

The 2D82 SWD full-chip dump has **no ASCII serial at all** anywhere; its only non-erased tail
block is at `0x0806F000` and is pure binary. So the two models don't even use the same cal
layout — worth a separate look, but that is our data, not a web finding.

### Why this kills the per-model-bootloader theory
- `[oss]` A 2C42 owner reports the scope self-identified as **2D42** after a firmware update,
  concluding "the firmware recognizes the difference between 2c and 2d based on the presence of
  components" — i.e. runtime hardware sensing for the C/D axis.
- `[oss]` Another reports turning a **2D42 into a 2D82 by DFU app update alone** (reply #175),
  and that the "automotive" features are *just presets* — "Even CAN bus analyzer... just scope
  with timing/voltage preset. There is no data parser, no id monitoring."
- `[oss]` The documented BW upgrade is a **hex edit of that serial string**: dump via ST-Link,
  "look for the serial number in it, it will end with `-40`, just change it to `-70`, save and
  reprogram." No bootloader involved.
- `[oss]` DFU descriptor from a real device: `@Internal Flash /0x08000000/06*002Ka,250*002Kg`
  — the first **6 × 2 KB = 12 KB** are alt-setting **`a` (readable only)**, the remaining
  250 × 2 KB are `g` (readable/erasable/writeable). That is the mechanical reason DFU can
  never touch the bootloader, and it is a device property, not a per-model one.

**Consequence:** a non-2D82 bootloader would probably *not* settle the question, because the
evidence says the capability is not stored there. The higher-value artefact is a **2D42 or
2C42 calibration page** (the `-40` variant of `0x080598B8`) to diff against our `-70`.

---

## Findings table

| # | What | Where (URL) | What it actually is | New to us? |
|---|------|-------------|---------------------|-----------|
| 1 | **`pablogventura/decompilador`** | https://github.com/pablogventura/decompilador | Independent Hantek **2D42/2D72** RE project, active to 2026-03. No firmware binaries, but: full USB protocol doc (`dev_docs/pyhantek/PROTOCOLO_USB.md`, 26 KB), **`HTHardDll.dll` export→USB-opcode map** (`dev_docs/hantek/EXPORTS_HTHardDll.md`), Ghidra pool/xref reports, and a working `pyhantek` CLI. Documents write-side APIs `dsoWriteDeviceCali`, `dsoGetDeviceCali`, `dsoFactorySetup` (opcode `0x0A`, then 3 s sleep), `dsoWriteBanner` (30 B), `dsoDeviceName`, `dsoDeviceSN` (10 or 0x25 B). Repo has 0 stars — almost certainly unseen by us. | **NEW — top finding** `[oss]` |
| 2 | **The `-40`/`-70` serial-suffix bandwidth mechanism** | https://www.eevblog.com/forum/testgear/hantek-2c422c722d422d72/50/ (reply ~#63) and /100/ (reply #109) | The actual model/BW storage location + the community upgrade procedure. Confirmed present in our own 2D72 dump at 0x080598B8. | **NEW (as knowledge)** `[oss→data]` |
| 3 | **2D42 → 2D82 by DFU alone; automotive = presets** | https://www.eevblog.com/forum/testgear/hantek-2c422c722d422d72/175/ (#175, #180) | Directly refutes "per-model bootloader". Also a caution: one user's 2C42 became a 2D82 showing "FPGA V255" and they could not revert. | **NEW** `[oss]` |
| 4 | **DFU alt-setting descriptor `06*002Ka,250*002Kg`** | https://www.eevblog.com/forum/testgear/hantek-2c422c722d422d72/200/ | Mechanical proof the low 12 KB is DFU-read-only. Explains why official DFUs start at 0x08005000. | **NEW (as evidence)** `[oss]` |
| 5 | `HantekHTX2019031201.dfu` | referenced on EEVblog /200/, not linked | Firmware release **we do not have**. Notable: it is the *only* release that exposed a user calibration feature (added to repair cal data garbled by an earlier buggy release; removed again later). Wanted for the cal-write code path. No live download found. | **NEW — but no URL** `[oss]` |
| 6 | `HantekHTX2020070701.dfu` | referenced on EEVblog /200/ with a full `dfu-util` transcript (element 0x08005000, size 229436) | Firmware release **we do not have**, between our 2019110801 and 2021090901. No live download found. | **NEW — but no URL** `[oss]` |
| 7 | Official current firmware `HantekHTX2021090901.zip` | https://www.hantek.com/uploadpic/hantek/files/20211118/HantekHTX2021090901.zip — **127,265 B** | Hantek's *latest* Hantek2000 firmware, dated 2021-11-18. Same build as our `HTX2D82-2021090901.dfu`. | **Duplicate** `[spec]` |
| 8 | `Hantek2000_Software.zip` | https://www.hantek.com/uploadpic/hantek/files/20210817/Hantek2000_Software.zip — **12,690,271 B** | The consumer PC app (contains `HTHardDll.dll` — the DLL item #1 reverses). Only worth fetching if we want our own copy of the DLL. | Probably duplicate `[spec]` |
| 9 | `Hantek2D42.zip` (driver) | https://www.hantek.com/uploadpic/hantek/files/20230831/Hantek2D42.zip — **140,860 B** | USB driver package, 2023-08-31. Small; the only 2023-dated handheld artefact. | Unknown `[spec]` |
| 10 | `DfuSe_Demo.zip` "Update Firmware Tool" | https://www.hantek.com/uploadpic/hantek/files/20211118/DfuSe_Demo.zip — **13,566,853 B** | Stock STMicro DfuSe demo, rebadged. Not a Hantek tool. | Not useful `[spec]` |
| 11 | `circuit-specialists/Hantek-2D72` | https://github.com/circuit-specialists/Hantek-2D72 | `27D72_dump.bin` (512 KB full-chip 2D72). **We already have this exact file** at `vendors/hantek/2d72/firmware/2d72-20190415/27D72_dump.bin`. Repo also has PCB/debug-port photos and the CS7721 DMM datasheet. | **Duplicate** `[oss]` |
| 12 | eediscuss "2D42 Firmware update failed" (4 pages, 60,566 views) | https://www.eediscuss.com/forum.php?mod=viewthread&tid=14722 | Hantek-affiliated forum unbrick thread. EEVblog claimed "the download seems to include the bootloader" — **attachments are not visible to guests**. Read all 4 pages: only `HantekHTX2019110801.dfu` named, which we have. Recovery route discussed is UART/USB-serial + ST-Link. **Registering an account would expose the attachments** — the single most promising untried lead for an actual 2D42 image. | **Unresolved lead** `[oss]` |
| 13 | `hkoosha/hanteker` + `hanteker_gui` | https://github.com/hkoosha/hanteker | Rust host-side control tool for 2D42/2D72. Host software only, no firmware. | Not useful `[oss]` |
| 14 | `lucaoli/Hantek` | https://github.com/lucaoli/Hantek | 2D72 Linux tool. Host software only. | Not useful `[oss]` |

---

## Explicitly searched and found NOTHING

- **A bootloader image from any non-2D82 model.** Zero hits anywhere. The only public
  full-chip dump in existence appears to be the one 2D72 dump we already hold.
- **A vendor factory / calibration / "setter" tool.** Hantek publishes no such thing. I
  enumerated *every* download in Hantek's Handheld-Oscilloscope category across all four keys
  (`zxgj` firmware, `yyrj` app software, `qd` driver, `sdk`) — the complete public set is
  items 7–10 above. The nearest thing to a cal writer is the `dsoWriteDeviceCali` /
  `dsoFactorySetup` exports **already inside the public `HTHardDll.dll`** (item #1) — i.e. the
  cal-write capability ships in the consumer DLL, which is consistent with there being no
  separate secret tool.
- **Firmware newer than 2021090901.** Hantek's own page confirms 2021-11-18 is the latest.
- **GitLab / Codeberg / Bitbucket.** No Hantek 2000-series content at all.
- **Chinese / Russian keyword sweeps.** Nothing beyond the EEVblog and eediscuss threads
  already covered. `forum.nickbel.com` timed out and was not retried.

---

## Recommended next steps (in value order)

1. **Register on eediscuss.com** and re-read thread 14722 — attachments are guest-hidden and
   an actual 2D42 image may be sitting there. (Item #12.)
2. **Clone `pablogventura/decompilador`** and read `EXPORTS_HTHardDll.md` +
   `PROTOCOLO_USB.md` against our TSV. The `dsoWriteDeviceCali` / `dsoFactorySetup` opcodes
   are the real "cal tool" and may let us read/write the cal page over USB without SWD.
3. **Re-frame the open question**: stop hunting bootloaders; hunt a **2D42/2C42 calibration
   page**. Ask on EEVblog for a `dfu-util -s 0x08059000 -U` dump from a `-40` unit — that is
   a 2 KB read anyone can do without opening the case.
4. **Resolve why our 2D82 dump has no ASCII serial** where the 2D72 does. Either the cal page
   moved, or 2D82 encodes the model differently — this is a `[?]` in our own data and is
   answerable by code trace, not by more web searching.

## Downloads NOT performed
Per instruction, nothing large was fetched. Sizes reported above; all URLs verified live
with `curl -sIL` (HTTP 200 + content-length).
