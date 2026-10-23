# Waveform Readback — FPGA → MCU → USB Sample Data Path

This document traces the actual bulk **ADC sample data** path: how raw waveform samples get from
the FPGA's ring buffer into MCU RAM, and how the MCU serves them to the PC over USB. It is
deliberately separate from [trigger.md](trigger.md) (trigger *configuration*) and
[calibration.md](hardware-calibration.md) (calibration *tables*) — this document is about the sample bytes
themselves and their formats at each hop.

For the FPGA-side SPI register map and the raw burst-read wire protocol see
fpga-spi.md §7 — this document does not repeat that framing, it
extends it with the MCU-side call chain and the USB-side packet format that fpga-spi.md does not
cover.

---

## 1. Two Independent Read Paths — Same Wire Command

A key finding of this trace: **there are two separate MCU-side call chains that both read FPGA
sample data using the identical wire command**, but they exist for different purposes and write to
different SRAM destinations. Conflating them was a real risk during this trace; they are kept
distinct here.

| | Live-view / display path | USB polled-readback path |
|---|---|---|
| **Entry point** | `fw_acq_process_waveform` @ `0x08012e08` | `fw_usb_scope_readback` cmd `0x16` @ `0x08030f0c` |
| **Driven by** | Every UI frame tick (`fw_scope_frame_tick`, unconditional, 2 call sites, same args each time) | PC-initiated USB request cadence |
| **Read helper** | `fw_fpga_write_block` (misnamed — see §3) → `fw_cali_capture_samples`, or `fw_fpga_read_samples_windowed`(`0x0801fc04`) → `fw_scope_spi_read_samples` | `fw_scope_read_captured_samples` (`0x080128f0`) → `fw_fpga_read_samples_full` (`0x0801e14a`) → `fw_fpga_capture_process` |
| **Wire command** | `[0xa4][0x01][addr_lo][addr_hi]` + `[0xa4][0x01]` repeat (same framing) | `[0xa4][0x01][addr_lo][addr_hi]` + `[0xa4][0x01]` repeat (identical) |
| **Read size** | Windowed: 0x4b0 (1200) samples/channel per display refresh | Full: 0x1800 (6144) bytes per channel per capture |

> **USB-level cross-check (2026-07-14):** a live usbmon capture of the real vendor Scope.exe's
> `cmd 0x16` (`SCOPE_START_RECV`) requests found `start=0x04b0 count=0x04b0` (1200/1200) — the
> USB-visible request size matches this table's "windowed" 0x4b0 read size exactly, not the
> `0x10b0`/`0x0960` (4272/2400) values a static-disassembly comment in `vendors/hantek/2d82/scripts/hantek_capture.py`
> had assumed (that comment has not been re-verified against live traffic and may be stale — see
> [usb-protocol.md](usb-protocol.md) "Waveform Data" for the full correction). This suggests the PC
> app is requesting the same windowed/display-refresh sample set described here, not the full
> 6144-byte per-channel capture.
| **Destination** | `fw_ch_buf_ptr(ch)` = same `g_ch1_samples`/`g_ch2_samples` base, but only the display window is refreshed | `g_ch1_samples` (`0x200023e4`) / `g_ch2_samples` (`0x20003be4`), full 6144-byte capture |
| **Per-sample processing** | Calibration scaling applied inline during the SPI clock-in (`fw_cali_capture_samples`) | Raw byte in, then `zero_offset` correction + clamp (`fw_fpga_capture_process`) |

**Naming trap found in this trace, RESOLVED 2026-07-14:** two *different* functions in this
firmware used to share the decompiler-assigned name `fw_fpga_read_samples`. Both addresses
independently disassemble to different bodies — a genuine Ghidra symbol collision, not a
documentation error. They have since been disambiguated in the TSV and now have distinct names:
- `0x0801e14a` → **`fw_fpga_read_samples_full`** — the one documented in fpga-spi.md §7, called by
  `fw_scope_read_captured_samples` (USB polled-readback path), reads the full 6144-byte capture,
  4 params `(start_addr, ch, mode, count)`.
- `0x0801fc04` → **`fw_fpga_read_samples_windowed`** — a second, distinct function, called from the
  live-view path, wrapping `fw_scope_spi_read_samples` instead of `fw_fpga_capture_process`; takes
  6 params `(buf, ch, mode, n, byte_a, byte_b)` including an explicit destination buffer.

Also mislabeled: **`fw_fpga_write_block`** (`0x0801fb40`) — despite the name, this is a **read**
function. It sends the same `0xa4`/reg `0x01` burst-read preamble and then calls
`fw_cali_capture_samples`, which clocks bytes *in* from the FPGA via `fw_spi_sendrecv(0xff)`. There
is no data written to the FPGA in this call at all. The name is being kept for now for
searchability/history but is corrected in the TSV entry.

---

## 2. FPGA Wire Format (Unchanged From fpga-spi.md — Confirmed, Not Re-derived)

Both paths use the exact burst-read command already documented in
fpga-spi.md §7:

```
CS LOW
→ [0xa4][0x01][start_lo][start_hi]   (4 bytes)
→ [0xa4][0x01]                        (preamble REPEATED — FPGA requires this)
← [byte_0]...[byte_N-1]               (N raw ADC samples, offset-binary 8-bit)
CS HIGH
```

Confirmed independently during this trace via the literal command bytes:

| Symbol | Address chain | Value |
|--------|---------------|-------|
| `ptr_spi_cali_cmd_a` | `0x08020134` → `0x080540b0` | `0x01a4` (cmd `0xa4`, reg `0x01`) |
| `ptr_spi_cali_cmd_b` | `0x0802013c` → `0x080540b8` | `0x01a4` (identical) |
| `ptr_spi_samples_cmd` (fpga-spi.md's reference) | `0x08020138` → `0x080540b4` | `0x01a4` (identical) |

All three command-byte pairs used by the display path, the calibration-average path, and the
full-capture path are byte-identical. **There is one wire protocol for reading ADC samples from
the FPGA ring buffer**, not several — only the surrounding software (read length, destination
buffer, and post-processing) differs by call site.

**Sample encoding** (unchanged, offset-binary 8-bit, confirmed by both read paths independently):

```
0x00 = negative full-scale (-FS)
0x80 = zero (0 V, midscale)
0xFF = positive full-scale (+FS)
```

CS assert/deassert is via GPIOA bit 4 (`fw_gpio_reset_bits`/`fw_gpio_set_bits(ptr_gpioa_crl, 0x10)`)
in both paths — same pin, same polarity as documented in fpga-spi.md §1.

**No DMA in either path.** Every byte is clocked with `fw_spi_sendrecv(0xff)`
(full-duplex, CPU-polled). This matches the "No DMA on capture path" note already in fpga-spi.md
§12 and confirms it applies to the live-view path too, not just the full-capture path.

---

## 3. Live-View Path Detail — `fw_acq_process_waveform`

`fw_acq_process_waveform` (`0x08012e08`) runs on **every UI frame tick** (called unconditionally
from `fw_scope_frame_tick`, the per-key/per-tick UI dispatcher — both its call sites pass identical
arguments regardless of which key triggered the tick). This makes it the continuous live
acquisition-and-render loop, not a one-shot request handler.

Per invocation:

1. **Gate check.** If `fw_scope_ch_state_get()==0` (live capture, not synthetic/demo mode) and
   `fw_fpga_read_status()` shows not-busy (same status byte/bit as fpga-spi.md READ REG 0x00),
   proceed; otherwise bail with `0xff`.
2. **Read end address.** `fw_fpga_read_endaddr()` — identical REG 0x03 read documented in
   fpga-spi.md — gives the current ring-buffer write pointer.
3. **Sanity bound.** If the end-address value is `>= 0xc01`, abort with an error print
   (`ptr_str_bad_end_address`) and return `0xff` — a basic range guard on the 13-bit-ish ring
   address.
4. **Timebase + trigger-position math.** Computes pixel-domain scale factors
   (`fw_apply_timebase_freq2`, `fw_timebase_entry_get`) and a horizontal trigger pixel offset
   (`fw_calc_trigger_pixel_offset`), then runs a small edge-crossing search over the *previous*
   frame's already-captured samples to refine the on-screen trigger cursor position (rising/falling
   edge test against the configured trigger level, searching ±4 pixels around the naive offset).
5. **Compute ring-buffer read-start.** Same style of formula as the full-capture path:
   `start = (end_addr*2 − active_ch_count*window_size/2 − pretrig/2) mod 0x1800`, using the active
   channel count and a `0x4b0` (1200-sample) display window rather than the full `0x1800`
   (6144-byte) capture window.
6. **Read samples.**
   - If `fw_scope_ch_state_get()==0`: call `fw_fpga_write_block(addr, ch, mode, count, b5, b6)` —
     really a read, see §1 — which sends the `0xa4`/`0x01` burst preamble then calls
     `fw_cali_capture_samples` to clock bytes in *with calibration scaling applied per-sample*
     (subtract voffset baseline, multiply by per-step volt-cal float, optional interleave-ratio and
     alternating-parity delta correction for crosstalk, clamp 0..255), writing the result straight
     into `fw_ch_buf_ptr(ch)` (i.e. `g_ch1_samples`/`g_ch2_samples`).
   - Else (synthetic/demo waveform mode): call `fw_setup_ch_buf_ptrs`, which `fw_flash_memcpy`s a
     built-in flash-ROM waveform table into the same buffers instead of reading hardware — this is
     a self-test/demo-pattern mode, not a live ADC path.
7. **Render.** For each active channel, allocate a `0x30`-byte display "event" (`fw_event_alloc`),
   call `fw_waveform_ch_render` to resample/centre the trace into the event's output buffer, copy
   the rendered trace back into the SRAM ring window (`fw_memcpy_fast`), then free the event.
8. **Optional measurement.** If `fw_meas_get_flag()` is set, runs frequency/period measurement
   (`fw_meas_pre_process`/`fw_meas_freq_display`) over the freshly-read buffers.

**Wrap-at-0x4b0, not 0x1800:** the no-calibration sibling helper `fw_scope_spi_read_samples`
(called via the alternate `fw_fpga_read_samples_windowed`@`0x0801fc04` route) wraps its ring index at `0x4b0`
(1200 samples) — this is the **on-screen column count**, a smaller window than the full 6144-byte
(`0x1800`) capture ring used by the USB polled-readback path. Do not confuse the two ring sizes.

**Retry-on-blank:** both `fw_cali_capture_samples` and `fw_scope_spi_read_samples` detect the
FPGA's "no data yet" sentinel (an all-`0xFF` read) and retry (2–3 attempts via a small SRAM
counter) before giving up and returning a bad-read indicator — this is a genuine liveness check
against reading a ring buffer the FPGA hasn't finished writing.

---

## 4. USB Polled-Readback Path Detail — `fw_usb_scope_readback` cmd `0x16`

`fw_usb_scope_readback` (`0x08030f0c`) dispatches on the USB request's selector byte
(`rx_buf[+4]`). Most values 0x00–0x14 return a single scope-parameter byte (fully mapped in the
TSV's `fw_usb_scope_readback` entry / `0x15` ReadAllSet table — not repeated here). This document
covers `0x16` (waveform data transfer) in full, since it is the actual bulk sample path.

### Not-yet-triggered case

```c
if (*g_usb_rx_buf_ptr == 0 &&                      // fresh transfer (cursor at 0)
    fw_scope_read_captured_samples(ch_idx) == -1) { // FPGA not capture-done yet
    // 12-byte fixed "no data" marker:
    tx = 00 00 ff ff 00 00 ff ff 00 00 ff ff
    *g_usb_rx_buf_ptr = 0;
    tx_len = 0x0c;
}
```

`fw_scope_read_captured_samples` (`0x080128f0`) is the function that performs the trigger-complete
check and, on success, the full 6144-byte capture read via `fw_fpga_read_samples_full`
(`0x0801e14a`)/`fw_fpga_capture_process` (`0x0801ded8`) — i.e. the SAME hardware read documented in
fpga-spi.md §7, storing raw+calibrated (`zero_offset`-corrected) bytes into `g_ch1_samples`/
`g_ch2_samples`. This confirms the "not-triggered 12-byte pattern" documented in trigger.md is
produced by this function's non-blocking `-1` return, not by a separate readback code path.

### Triggered / real-data case (newly documented here)

```c
wave_cursor = g_usb_rx_buf_ptr;                       // persists across calls, tracks bytes sent
ch_idx = rx_buf[+7] + rx_buf[+5];                      // 16-bit wrapping add: target end offset
sample_off = min(ch_idx - *wave_cursor, 0x40);         // this packet's byte count, capped at 64

for (i = 0; i < sample_off; i++)
    tx_buf[i] = g_usb_scope_buf2_ptr[wave_cursor + i]; // copy up to 0x40 bytes

*wave_cursor += sample_off;
tx_len = sample_off;
// completion check:
if (ch_idx <= *wave_cursor) {
    *wave_cursor = 0;
    fw_scope_apply_all(1);
    fw_status_bar_dirty_set(1);
    fw_set_backlight(1);
    g_usb_cs7721_buf_ptr[9] = 0x96;          // TriggerHPos reset
    fw_apply_timebase_display(0x96);
    fw_apply_trigger_edge();                  // re-arm
    fw_set_acq_run_flag(1);
}
```

**Packet format:** up to `0x40` (64) raw sample bytes per USB transaction, **no header or trailer
bytes** within the payload itself — the response is pure sample data, offset-binary 8-bit, same
encoding as the FPGA wire format. The framing (how many total bytes to expect) is driven entirely
by `ch_idx` (computed from the request's own `data[5:7]`/`data[7:9]` int16 fields, not returned in
the response), and the host must keep calling with `wave_cursor` implicitly advancing server-side
until `ch_idx <= wave_cursor`, at which point the scope auto-re-arms for the next capture. This
matches trigger.md's existing "Trigger State in Waveform Readback" section, extended here with the
exact byte-count/cursor mechanics.

**Channel interleaving in the USB stream:** whether consecutive bytes in this stream are
single-channel or CH1/CH2-interleaved is determined entirely by which acquisition mode
(`fw_fpga_capture_process`'s `mode` 1 or 2) was active when `g_ch1_samples`/`g_ch2_samples` were
populated — the USB layer here does no further interleaving/deinterleaving of its own; it is a flat
byte-stream copy from whatever `g_usb_scope_buf2_ptr` (see below) contains.

### RESOLVED (2026-07-14): `g_usb_scope_buf2_ptr` writer found — it is a separate capture buffer, not a passthrough

**Root cause of the original "no writer" result:** the original search grepped for the raw literal
`0x200053e4` / `0x08031bf8` as text. That only matches the *reading* call site
(`fw_usb_scope_readback`, which happens to use the Ghidra-assigned name `g_usb_scope_buf2_ptr`) and
TSV comment lines. The actual writer lives in a **different function that references the same
runtime address through a different flash literal-pool cell**, which Ghidra's decompiler assigned
an unrelated symbolic name (`g_dat_e304`) to — so the C source line doing the write never contains
the string `0x200053e4` or `0x08031bf8` at all. A plain address grep structurally cannot find this;
only tracing the call graph into every function that touches the region does.

**The writer:** `fw_fpga_capture_process` (`0x0801ded8`) — the same function already documented in
§4 as the full-capture SPI backend used by the USB polled-readback path
(`fw_scope_read_captured_samples` → `fw_fpga_read_samples_full` → `fw_fpga_capture_process`). Its loop
body was previously (incorrectly) summarised as storing "to CH1 buffer 0x200023e4 or CH2
0x20003be4" — that summary was never actually read off the decompiled C, it was inferred from the
function's apparent purpose. Re-reading the loop body directly:

```c
for (uVar7 = 0; uVar7 < (ushort)count; uVar7 = uVar7 + 1) {
    if ((char)mode == '\x01') {
        /* ... zero-offset correction, clamp 0..0xff ... */
        *(char *)(g_dat_e304 + (uint)uVar7) = (char)iVar6;      // mode 1 (single-ch)
    }
    else if ((char)mode == '\x02') {
        /* ... same correction, CH1/CH2 calibration selected by uVar7 parity ... */
        *(char *)(g_dat_e304 + (uint)uVar7) = (char)sVar4;      // mode 2 (dual-interleaved)
    }
}
```

`g_dat_e304` (flash `0x0801E304`) is a **separate flash literal-pool cell holding the same runtime
constant `0x200053e4`** as `g_usb_scope_buf2_ptr`'s cell (`0x08031BF8`) — confirmed by reading both
literal words and cross-checking against the TSV. Both `mode==1` and `mode==2` branches write
unconditionally into this one flat buffer indexed purely by loop counter `uVar7`; the `ch` parameter
is used **only** for calibration-table lookups (`g_cali_adc_main_ptr2 + ch*0x4a`), never for
destination-address selection. There is **no direct write to `g_ch1_samples`/`g_ch2_samples`
anywhere in the decompiled codebase** (confirmed by exhaustive grep) — those buffers are populated
exclusively by the unrelated **live-view path** (`fw_cali_capture_samples`, via
`fw_ch_buf_ptr(ch)` = `g_dat_1da8 + ch*0x1800` = `0x200023e4`-based), a structurally separate code
path from the USB polled-readback capture.

**Two more literal-pool aliases of the same address were found during this trace**, closing out the
"exhaustive search found nothing" gap entirely:

- `g_dat_58c4` (flash `0x080158C4`) — used in `fw_ch_flash_buf_base(ch)` (flash-buffer base
  addressing helper) and `fw_load_waveform_from_flash` (as a `fw_flash_memcpy` destination when
  restoring a saved CH2 capture from flash on boot).
- `g_dat_5c5c` (flash `0x08015C5C`) — **also a writer**: `fw_apply_acq_params` (`0x08015054`) copies
  live sample data from `fw_ch_buf_ptr(ch)` (i.e. `g_ch1_samples`/`g_ch2_samples`) byte-by-byte into
  `g_dat_5c5c + ch*0x1800 + i` across three acquisition-mode branches (normal/roll variants),
  populating this SAME `0x200053e4` region from a second, independent code path. Also read by
  `fw_flash_verify` as a comparison source.

**Conclusion:** `0x200053e4` (`g_usb_scope_buf2_ptr`) is a genuine, dedicated **third capture
buffer** — not stale SRAM, not a passthrough alias of `g_ch1_samples`/`g_ch2_samples`, and not DMA
(the project already confirmed "no DMA on capture path" independently — see `mmio_DMA2` TSV entry
and §2 above). It is populated two ways: (1) directly by `fw_fpga_capture_process` during a fresh
FPGA SPI burst read (the primary/normal path for cmd `0x16`), and (2) by `fw_apply_acq_params`
copying from the already-populated `g_ch1_samples`/`g_ch2_samples` live-view buffers in certain
acquisition-mode branches (roll mode and others) as an explicit re-sync/mirror step. The
address-arithmetic adjacency to `g_ch1_samples`/`g_ch2_samples` noted in the original open question
(`g_ch1_samples + 0x1800 + 0x1800 = 0x200053e4`) is very likely intentional layout — this buffer is
sized and positioned to hold a full dual-channel capture immediately after the two per-channel
buffers, used specifically as the USB-transfer staging area, kept separate so a slow/partial USB
drain (cmd `0x16` streams 64 bytes at a time) cannot race against the live-view path continuously
overwriting `g_ch1_samples`/`g_ch2_samples` on every UI frame tick.

No live capture is required to settle this — the resolution came entirely from tracing the two
additional call sites that a literal-address grep cannot find. **Do not assume `g_usb_scope_buf2_ptr`,
`g_dat_58c4`, `g_dat_5c5c`, and `g_dat_e304` are unrelated variables in any future analysis pass —
they are 4 different Ghidra names for one SRAM location, `0x200053e4`.**

### `0x17` — sample count / pre-trigger info (for completeness)

The `v < 0x18` branch (selector `0x17`) calls `fw_fpga_read_end_addr` then
`fw_scope_ch_active_count()`, and on success streams `0x40`-byte chunks directly from
`g_usb_scope_state_ptr` (confirmed identical address to `g_ch1_samples`, `0x200023e4`) with a
`0x1800`-byte stride between channels — i.e. this selector genuinely reads the raw capture buffers
directly, unlike `0x16`. See the TSV `fw_usb_scope_readback` entry for the previously-documented
edge-case behaviour (empty response when no channels are active).

---

## 5. Full Sequence Diagrams

### Live-view path (continuous, once per UI frame tick)

```
fw_scope_frame_tick (every tick)
  └─ fw_acq_process_waveform
       ├─ fw_fpga_read_status()        CS_L [0xa0][0x00][0xa0] RX[1] CS_H   (busy check)
       ├─ fw_fpga_read_endaddr()       CS_L [0xa0][0x03][0xa0] RX[2] CS_H   (ring write pointer)
       ├─ compute read-start (timebase + trigger-hpos + active-ch-count)
       ├─ fw_fpga_write_block(addr,ch,mode,...)      [really READS]
       │    CS_L [0xa4][0x01][addr_lo][addr_hi] [0xa4][0x01] RX[N] CS_H
       │    └─ fw_cali_capture_samples: per-sample calibration scaling -> g_ch1/ch2_samples window
       ├─ fw_waveform_ch_render (per active channel): resample/centre into display event
       └─ (optional) frequency/period measurement
```

### USB polled-readback path (PC-driven, cmd 0x16)

```
PC: 55 0a 00 01 16 [data5..9]    (bank=0, flag=read, cmd=0x16, ch_idx encoded in data[5:9])
MCU fw_usb_scope_readback:
  if wave_cursor==0:
    fw_scope_read_captured_samples(ch_idx)
      ├─ fw_fpga_read_status()      (capture-done check, non-blocking)
      ├─ fw_fpga_read_endaddr()     REG 0x03
      └─ fw_fpga_read_samples_full() CS_L [0xa4][0x01][addr][addr] [0xa4][0x01] RX[0x1800] CS_H
                                     -> fw_fpga_capture_process -> g_ch1_samples / g_ch2_samples
    if not capture-done: return 12-byte "00 00 ff ff ×3" marker, reset cursor
  else:
    copy up to 0x40 bytes from g_usb_scope_buf2_ptr[wave_cursor..] to tx_buf   [see §4 open question]
    wave_cursor += bytes_sent
    if wave_cursor >= ch_idx: reset cursor, re-arm trigger, resume live acquisition
PC ⟵ raw sample bytes (offset-binary 8-bit, no header/trailer), repeat until wave_cursor >= ch_idx
```

---

## 6. Summary — Answering the Original Question

1. **FPGA → MCU sample read wire format:** one command, `[0xa4][0x01][addr_lo][addr_hi]` +
   repeated `[0xa4][0x01]` preamble, CPU-polled (no DMA), 8-bit offset-binary samples
   (`0x00`=−FS, `0x80`=0V, `0xFF`=+FS). Single-channel mode = one byte per sample; dual-channel
   mode = alternating CH1/CH2 bytes. This is unchanged from fpga-spi.md §7 — confirmed identical
   across all three call sites found in this trace (live-view, calibration-average, full-capture).

2. **MCU → USB waveform packet format (cmd `0x16`):** up to `0x40` (64) raw sample bytes per USB
   IN transaction, no per-packet header/trailer, offset-binary 8-bit, transfer framed by a
   server-side cursor (`g_usb_rx_buf_ptr`) compared against a client-specified `ch_idx` end offset;
   completion triggers scope re-arm. Not-yet-triggered state returns a fixed 12-byte
   `00 00 ff ff ×3` marker (already documented in trigger.md, now traced to its source function).

3. **Whether/how they differ:** the wire protocol from FPGA is identical in both major MCU-side
   paths. The **live-view path** applies calibration scaling (offset + gain correction) to each
   sample as it's clocked in, writing directly into `g_ch1_samples`/`g_ch2_samples`
   (`fw_cali_capture_samples` via `fw_ch_buf_ptr(ch)`). The **USB polled-readback path**'s capture
   (`fw_fpga_capture_process`, lighter-weight `zero_offset`-only correction) writes to a genuinely
   separate third SRAM buffer, `g_usb_scope_buf2_ptr` (`0x200053e4`) — confirmed writer, not a
   passthrough of `g_ch1_samples`/`g_ch2_samples` (§4). `fw_apply_acq_params` additionally mirrors
   `g_ch1_samples`/`g_ch2_samples` content into this same buffer in some acquisition-mode branches,
   making it the dedicated USB-transfer staging area, decoupled from the live-view path's
   continuous per-frame overwrites.

## Open Questions

- ~~`g_usb_scope_buf2_ptr` (`0x200053e4`) has no confirmed writer~~ — **RESOLVED 2026-07-14** (§4):
  written by `fw_fpga_capture_process` (primary) and `fw_apply_acq_params` (mirror from
  `g_ch1_samples`/`g_ch2_samples` in some acquisition modes). It is a dedicated third capture/staging
  buffer, not stale SRAM and not a passthrough alias.
- **Interleave-ratio and delta-correction constants** in `fw_cali_capture_samples` (the
  `local_3c`/`local_40` scaling applied during live-view reads) are traced structurally but their
  exact calibration-table sourcing has not been cross-checked against a live calibration run.
- Whether the synthetic/demo waveform mode (`fw_scope_ch_state_get()!=0`, routed to
  `fw_setup_ch_buf_ptrs`) is ever reachable from a normal UI flow, or is a factory/test-only mode,
  was not determined in this pass.
