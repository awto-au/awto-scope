# SPI-v2 — the redesign host interface (supersedes the OEM SPI)

The clean, extensible SPI protocol between the STM32 (master) and the redesigned
MachXO2 scope FPGA (slave). It replaces the OEM's fixed capture/AWG protocol
(`0xa4` raw-ring burst + cal/AWG writes, which exposes nothing else) with a
uniform framed protocol over a flat resource address space — modelled on Red
Pitaya's register semantics (`fpga-scope-research.md` §2) and standard
MachXO2 SPI-slave patterns.

Reference implementation + self-check: `fpga/explore/spi_debug.py`
(synthesizable Amaranth, emits `spi_debug.v`).

## Electrical

Mode-0 SPI (CPOL=0, CPHA=0), MSB-first, FPGA is the slave. Pins from the
recovered hardware map: **SCK=31, MOSI=49, MISO=32, nCS=27**. The SPI clock is
NOT a fabric clock — MOSI/SCK/CS are synchronised into a fast fabric clock and
edge-detected, so SCK is limited to ~1/4 of the fabric clock (a few MHz).

## Framing

```
CS↓  [CMD]  [ADDR_HI]  [ADDR_LO]  [payload …]  CS↑
```

Address auto-increments across a burst, so one framed transaction reads/writes
a contiguous span. Reads return data starting on the byte after `ADDR_LO`.

| CMD | Name | Payload |
|-----|------|---------|
| `0x03` | READ | none → device returns bytes from ADDR, auto-increment |
| `0x02` | WRITE | data bytes → written to ADDR, auto-increment |
| `0x05` | STATUS | none → 1 status byte (no address phase) |
| `0x0A` | JTAG | SPI↔external-JTAG passthrough (see below) |

## Flat address map

| Range | Contents |
|-------|----------|
| `0x0000–0x1BFF` | **capture / EBR readout** — the frozen sample window (per channel/segment) |
| `0x1C00` | **STATUS** — `{overrun, delay_done, triggered, armed, …}` |
| `0x1C01` | **CONTROL** — `{arm, reset, mode…}` (write) |
| `0x1C02–03` | write-pointer `wp_cur` (lo/hi) |
| `0x1C04–05` | trigger pointer `wp_trig` (lo/hi) — where the event landed |
| `0x1C10` | trigger config — source, edge (rising/falling/any/burst) |
| `0x1C11–12` | trigger level + hysteresis |
| `0x1C14` | decimation factor (power-of-two) + `hres_en`, `minmax_en` |
| `0x1C18` | post-trigger delay (lo/hi) |
| `0x1C1C` | segment count / current segment |
| `0x1C20–21` | **readback window** offset (lo/hi) — decouples transferred window from capture |
| `0x1C22–23` | readback window length (lo/hi) |
| `0x1C24` | readback subsample / skip (thin the transfer further) |
| `0x1E00–01` | **I/O pad status** readback (lo/hi) |
| `0x1E10` | GPIO drive register (unknown-pin probing) |
| `0x1F00–01` | **die temperature** (lo/hi) — from an EXTERNAL I2C sensor; MachXO2 has no on-die sensor (see below) |
| `0x1F04–07` | **chip ID / USERCODE / TraceID** — via EFB WISHBONE |
| `0x1F08` | version / capability byte |
| else | `0xA5` alive marker |

## Scope control flow (Red Pitaya semantics)

```
WRITE 0x1C10  trigger config (source, edge)
WRITE 0x1C11  level + hyst
WRITE 0x1C14  decimation / hres / minmax
WRITE 0x1C18  post-trigger delay
WRITE 0x1C01  = arm
poll  0x1C00  until triggered & delay_done      (STATUS cmd, cheap)
READ  0x1C04  wp_trig                            (locate the event)
READ  0x0000+ burst-read the frozen sample window
```

The FPGA captures at full rate into a circular EBR buffer, freezes on
trigger+delay, and the host reads the frozen buffer slowly — so the slow SPI
link never bottlenecks acquisition (the "capture fast, freeze, read slow"
decoupling).

## Capabilities beyond the OEM

- **Full content readout** — any resource in one flat space (capture, registers,
  pin status) via burst READ.
- **Die temperature** (`0x1F00`) — **fed by an external I2C temperature sensor**;
  the MachXO2-1200 has **no on-die temperature sensor** (unlike ECP5's DTR). The
  register is part of the protocol regardless of source; on an ECP5-based
  successor it would read the internal sensor directly.
- **Chip ID / USERCODE / TraceID** (`0x1F04`) — read over the **EFB WISHBONE**
  window (sysCONFIG), not JTAG.
- **Pin status + GPIO drive** — read every pad's state; drive unknown pins for
  board-destination probing (the old spy-bitstream use case, folded in).
- **JTAG-over-SPI passthrough** (`CMD 0x0A`) — the FPGA bridges SPI to **four
  user GPIO wired to an EXTERNAL target's** `TCK/TMS/TDI/TDO`, letting the STM32
  program/scan another chip through the FPGA (~200 LUT). Framing follows MPSSE
  (data-shift bytes + a separate TMS/TAP-navigation command) or OpenOCD
  `remote_bitbang`. **Not** for the MachXO2's own TAP — fabric cannot master its
  own JTAG (it is only an ER1/ER2 register target); the FPGA's own
  config/readback uses the EFB sysCONFIG WISHBONE path instead.

## Design rationale (from the open-scope protocol survey)

Distilled from ScopeFun, BitScope, LiteScope, SUMP/OLS and enxor
(`fpga-scope-research.md` refs); the choices here follow the patterns that scale:

1. **Byte-addressed register file + address pointer + auto-increment** (BitScope
   `@`/`n`/`p`, LiteScope named CSRs) is the most extensible core — new features
   are new addresses, existing offsets never move. SPI-v2's flat map + burst
   auto-increment is exactly this. Preferred over a fixed opcode set (SUMP):
   SUMP buys sigrok interop but every new capability needs a new opcode both
   ends must know.
2. **Config vs status registers are explicit** — `0x1C01` CONTROL (host→device)
   vs `0x1C00` STATUS (device→host). Poll a real `done`/`triggered` bit, never
   guess from timing.
3. **Readback window is parameterized independently of capture** (`0x1C20+`
   offset/length/skip) — "what was captured" is decoupled from "what gets
   transferred," as in LiteScope/BitScope/SUMP.
4. **Drain via one addressing setup** — a single READ frame auto-increments
   through the whole window; no per-word re-addressing.
5. **Self-describing captures** (ScopeFun) — a readout may prefix the sample
   window with the config block (decimation, trigger, cal-ref) so a capture is
   parseable with zero out-of-band state. Reserved: `0x1C08–0x1C0F`.
6. **Reserved space for growth** — gaps in the map (`0x1C05–07`, `0x1C13`,
   `0x1C25–2F`, `0x1F09+`) let the register file grow without moving offsets.
7. **For a future streaming/roll mode**, prefix frames with a MAGIC sync word
   (ScopeFun `0xDDDD`) so the host can re-lock mid-stream. Not needed for the
   freeze-then-read flow, which is inherently framed by CS.

## Status

`spi_debug.py` implements + self-checks READ/WRITE/STATUS burst, temp, chip-ID,
pin-status, write-readback, and the JTAG bridge hooks — all passing, emits
synthesizable Verilog. The scope-control registers (`0x1C10+`) are specified
here and land when the capture top is assembled (trigger + circular buffer +
decimation + segments).
