# HantekHTX2021010801.bin — combined bootloader + app image

**Source:** eediscuss.com thread 14722 ("2D42 Firmware update failed"), page 3, posted
by Hantek moderator "amy" with the note *"This link can be downloaded permanently"*:
`https://www.dropbox.com/s/xop4snveegnrj2d/Hantek2000%20Program%20Arm1.zip?dl=0`
Still live 2026-07-31 (six of the seven links in that thread were `fil.email` transfers
that expired within 7 days in 2019; this Dropbox mirror survived).

Archive `Hantek2000 Program Arm1.zip` (36,066,486 bytes) contains:

| Size | File |
|---|---|
| 255,794 | `HantekHTX2021010801.bin` |
| 35,658,363 | `flash_loader_demo_v2.8.0.exe` (ST Flash Loader Demonstrator — not kept) |
| 953,372 | `Program Arm1.docx` (the vendor's own serial-programming instructions) |

## What the .bin is `[fw]`

sha256 `e4cf1868e4fc3fb2253236125fc0adfc376be7d82b9ff33c01980b430f0f0b81`

**A COMBINED image, not an app update.** Two valid Cortex-M vector tables:

| Offset | SP | Reset | Meaning |
|---|---|---|---|
| `0x0000` | `0x20000BF8` | `0x08003021` | bootloader — reset is inside `0x08000000..0x08004FFF` |
| `0x5000` | `0x2000E808` | `0x0803D845` | app — matches the usual `0x08005000` app base |

So it spans `0x08000000`–`0x0803E732`. This is what the moderator means by
"program arm1 … with a TTL serial port tool": the ST Flash Loader writes the WHOLE
flash over UART, bootloader included — a path DFU cannot take, since DFU's alt-setting
`a` (the low 12 KB) is read-only.

## The finding that matters `[fw]`

Compared byte-for-byte against our own 2D82 bootloader
(`../bootloader/flash_08000000.bin`):

- **bytes `0x0000`–`0x342F` are IDENTICAL** — 13,360 bytes of bootloader code
- the only difference in the whole `0x5000` region is the tail `0x3430`–`0x4FFF`:
  this image pads with `0x00`, our SWD dump reads `0xFF` (erased flash)

i.e. **the bootloader carries no per-model code difference at all.** A vendor-supplied
recovery image for the Hantek2000 family and a 2D82 pulled off real hardware run the
same bootloader binary.

That is direct evidence against the "per-model bootloader" hypothesis (see
`../../2d82/docs/hardware/unlock.md`): if the bootloader encoded the model, these two
would differ in code, and they do not.

## What "Arm2" is — ANSWERED from the vendor's own docx `[fw]`

`Program Arm1.docx` (in this directory) is Hantek's own procedure, and it settles it:

> Install `flash_loader_demo_v2.8.0.exe`. Remove the batteries. Connect your scope to PC
> serial interface via RS232-to-TTL … Short circuit the two points in the following
> picture … Open STMFlashLoader Demo … **Select the .bin file, "Erase necessary pages"
> and address (8000000)** … Disconnect the USB cable. **If the scope can't start
> normally, need to update the firmware (arm2) again.**

So:

- **Arm1** = *this* image, written over UART to **`0x08000000`** — the whole chip,
  bootloader included, with "erase necessary pages". That is the low-level unbrick.
- **Arm2** = the ordinary **DFU app update** afterwards (`0x08005000`+), i.e. the
  `HantekHTX*.dfu` releases **we already hold**.

There is no missing third artefact. "arm1 and arm2" is two *steps* of one recovery
procedure, not two unknown files. The boot-mode entry is a **hardware short of two pads**
(BOOT0 strap), not a key combination.
- Which model this image is for. The thread is a 2D42 unbrick, but the file is named
  generically (`Hantek2000`), and a generic recovery image is consistent with
  everything else we have found.
