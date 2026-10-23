# HTHardDll ↔ our firmware RE — cross-reference

Source: `pablogventura/decompilador` (GitHub, pushed 2026-03-26, 0 stars, MIT-absent/no
license). Independent RE of the **2D42/2D72** — same "Hantek2000" firmware family as our
2D82AUTO. Clone: `<workspace>/github.com/pablogventura/decompilador`. Filed per GH#211
lead 2.

- Their side: the Windows consumer DLL `HTHardDll.dll` (host, decompiled) + a working
  libusb CLI (`pyhantek`) + hardware-validated index maps from a real 2D42.
- Our side: the STM32 device firmware (`fw_usb_*` in `ghidra_names_2d82.tsv`).
- The two meet at the USB wire. Their export→opcode map and our dispatcher agree on
  every opcode — mutual confirmation from opposite ends of the same link.

Confidence: their maps are `[oss]` hardware-validated on a **2D42**, not our 2D82. Same
firmware, but re-confirm per-index physical values on a 2D82 before treating as `[fw-data]`.

## What it CONFIRMS (independent, opposite end of the wire)

- 10-byte cmd frame `[00][0A][00][flag][opcode][payload LE…]`, `flag=1` write-only — matches
  `fw_usb_cmd_parser` (0x08032ea0) exactly.
- Scope opcodes: per-channel `ch*6+{0..5}` = OnOff/Couple/Probe/BWLimit/Volt/Pos; globals
  0x0C ChanSel, 0x0D YT, 0x0E TimeDiv, 0x0F TrigHPos(3B), 0x10 TrigSource, 0x11 TrigSlope,
  0x12 TrigSweep, 0x13 AutoSet, 0x14 TrigVPos — matches `fw_usb_scope_param_cmd_dispatch`
  (0x080308de) byte-for-byte.
- ReadAllSet (0x15): 25-byte reply `[55][19][00][15]` + 21 state bytes — matches our
  corrected 25-byte finding (was wrongly "64" earlier).
- `0x16` capture is **CH1,CH2,CH1,CH2…** byte-interleaved for dual channel — matches our
  `fw_acq_read_correct_samples` interleave note.
- `dsoHTScopeZeroCali` = opcode **0x17** shows "Calibrating" on a 2D42. Our side found
  0x17 is a **no-op via flag=0 (scope bank)**; theirs reaches it via the **cali path**,
  not the scope-param path. No contradiction — different bank. (Their CLI aliases it
  `trig-force`, which is misleading; it is zero-cal, not force-trigger.)

## What THEY add that we lacked — hardware-validated index → physical maps (2D42)

These give physical meaning to the raw indices our dispatcher decodes but never tabulated.
Full JSONs in the clone under `pyhantek/*_empirico.json`.

**TIME_DIV (opcode 0x0E) → time/div.** Index = our timebase `cur` into the divisor table
`g_dat_16a0 → SRAM 0x20001FC0` (GH#178). **34 rows, idx 0..33 valid, idx 34 breaks the
display.** 2-5-10 progression:

| idx | t/div | idx | t/div | idx | t/div |
|--|--|--|--|--|--|
| 0 | 5 ns | 12 | 50 µs | 24 | 500 ms |
| 1 | 10 ns | 13 | 100 µs | 25 | 1 s |
| 2 | 20 ns | 14 | 200 µs | 26 | 2 s |
| 3 | 50 ns | 15 | 500 µs | 27 | 5 s |
| 4 | 100 ns | 16 | 1 ms | 28 | 10 s |
| 5 | 200 ns | 17 | 2 ms | 29 | 20 s |
| 6 | 500 ns | 18 | 5 ms | 30 | 50 s |
| 7 | 1 µs | 19 | 10 ms | 31 | 100 s |
| 8 | 2 µs | 20 | 20 ms | 32 | 200 s |
| 9 | 5 µs | 21 | 50 ms | 33 | 500 s |
| 10 | 10 µs | 22 | 100 ms | | |
| 11 | 20 µs | 23 | 200 ms | | |

Ties to `fw_fpga_timebase_div` REG 0x0F: `effective_rate = 250 MSPS/(divisor+1)`. With the
per-index t/div known, each divisor-table value is now predictable/checkable — the missing
half of GH#178 (the table is runtime-populated so it never resolved statically).

**CH_VOLT (ch*6+4) → V/div:** 0=10mV 1=20 2=50 3=100 4=200 5=500 6=1V 7=2 8=5 9=10.
(idx 10/11 read ambiguous on their 2D42 — confirm.)
**CH_COUPLE (ch*6+1):** 0=AC 1=DC 2=GND. **CH_PROBE (ch*6+2):** 0=1× 1=10× 2=100× 3=1000×
(idx4 = incoherent UI, avoid).
**TRIG_SLOPE (0x11):** 0=rising 1=falling 2=double. **TRIG_SWEEP (0x12):** 0=Auto 1=Normal
2=Single.

## Cal read/write is USB-reachable WITHOUT SWD — the GH#211 headline

`HTHardDll` exports a full cal read/write surface, all over plain bulk USB — no ST-Link:

| DLL export | Wire | Our handler (bank 0x03) |
|--|--|--|
| `dsoGetDeviceCali` | W(10) R64 | `fw_usb_cali_read_handler` (0x08032a00) |
| `dsoWriteDeviceCali` | W(5+n) | `fw_usb_cali_write_handler` (0x08031c40) |
| `dsoZeroCali` | W(10/var) R64 | (cali bank) |
| `dsoFactorySetup` | `[00 0A 03 00]` op 0x0A + `Sleep(3000)` → ReadAllSet | `fw_usb_cali_write_handler` [4]==0 mode-select then reinit |
| `dsoDeviceSN` | W(10 or 0x25) R64 | [4]==9 serial (RAM-only, our note) |
| `dsoWriteBanner` | W(0x1E) 30B | — |

Our firmware side is the more complete of the two here — theirs marks the cal subcodes
"variable length, inspect the .c". We have them fully: `fw_usb_cali_write_handler` [4] =
{0 mode, 2/3/4 FPGA reflash, 5 AWG override, 6 volt-cal, 7 AWG-DDS, 8 shunt, 9 serial,
0xb cal-table-by-band, 0xc name-table, 0xe/0xf DMM DCMA/ACMA}. And two bugs they never saw
because they only have the host side:
- **[4]==0xc OOB write** (GH#191): unchecked `[5]` slot index → multi-KB SRAM overrun from a
  single USB packet; `[5]==6` persists it to flash.
- **[4]==2/3/4 MachXO2 busy-poll** with no timeout (GH#193): DoS if the AWG board is absent.

Implication for **GH#211 / GH#206**: reading a `-40` unit's cal page (or a 2C53T's) may be a
plain `dsoGetDeviceCali` USB call, not an SWD dump — no case opening. `pyhantek` already
implements `get-device-cali`. Untested against our units; the OOB bug above means be careful
with the *write* side.

## Other artefacts in the clone
- `Hantek_2D72_2D42_Manual.pdf` + `dev_docs/Hantek_2D72_2D42_Manual.txt` (OCR).
- `dev_docs/hantek/MANUAL_FIRMWARE_GAPS.md` — features in the manual with no DLL export
  (e.g. Force-trigger — they conclude it is UI/firmware-only, matching our "0x17≠force").
- `dev_docs/firmware/ghidra_pool_table_report.txt` + their own Ghidra scripts.
