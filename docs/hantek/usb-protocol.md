# 2000-series — PC↔Device USB Protocol

Sources: static analysis of `HTHardDll.dll` (v2021-08-17, 92 KB, PE32 i386, image base `0x10000000`)
and `Scope.exe` (v2021-08-17, 3.8 MB). Raw disassembly in `tmp/HTHardDll-disasm.txt`.  
Live LAN capture: `tmp/hantek_bridge.log` (2026-06-22, via `vendors/hantek/2d82/scripts/hantek_usb_tcp_bridge.py`).  
Cross-reference: [lucaoli/Hantek](https://github.com/lucaoli/Hantek) (same VID/PID, Linux driver, same packet format).  
Cross-reference: [hkoosha/hanteker](https://github.com/hkoosha/hanteker) (Rust client, same VID:PID
`0x0483:0x2d42`) — independently confirms `IDX=0x00`, `BOH=0x0A`, `AWG_VAL_TYPE_SIN=0x02`, and
distinguishes `TriggerSweep` NORMAL(1)/SINGLE(2) as separate values (see `trigger.md`).  
Cross-reference: [MCU↔FPGA SPI protocol](../../fpga/hantek/docs/fpga-spi.md) — the firmware side of every USB command.

**This document is the OLD/vendor protocol** (RE'd from the existing Hantek firmware/app, for
interoperability and RE reference). For the **NEW clean-room Zephyr firmware's** protocol design
— a fresh, Fluke/SCPI-inspired, streaming-first replacement, not compatible with this one — see
[`../firmware/usb-protocol-design.md`](../firmware/usb-protocol-design.md).

---

## Device Identification

| Field | Value |
|-------|-------|
| USB VID | `0x0483` — STMicroelectronics |
| USB PID | `0x2D42` — Hantek 2D42 |
| Windows HW ID | `vid_0483&pid_2d42` |
| Driver | `Hantek2D42.sys` (custom Cypress-derived KMD; x86/x64/IA64 in `Driver/`) |
| Bulk EP OUT | `0x02` |
| Bulk EP IN | `0x81` |

DFU (firmware update) mode uses a separate ST standard DFU stack: `STDFU.dll` + `STDFUPRT.dll`.

---

## Transport

`HTHardDll.dll` opens the device via Win32 `CreateFileA` + `ReadFile`/`WriteFile` through
`Hantek2D42.sys` (Cypress EZ-USB derived, NOT WinUSB). Two transports selected by `_dsoHTConnectType@12`:

| Transport | Notes |
|-----------|-------|
| USB bulk | Wrappers at `0x10002060` (write) / `0x10002020` (read) |
| TCP/IP (LAN) | `socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)` inside `HTHardDll.dll`; same 10-byte packet format |

**LAN connection model** (confirmed from live capture 2026-06-22):
- Scope.exe is the TCP **client** — it connects out to the configured device IP:port
- Default IP `192.168.1.1`, port `8899` (stored in `%APPDATA%\HTK-DSO\lan.set`)
- On the test machine `192.168.1.1` is a loopback alias → Scope.exe connects to `vendors/hantek/2d82/scripts/hantek_usb_tcp_bridge.py`
- Click **Connect** manually; *Search LAN device* sends UDP broadcast (bridge does not respond to it)

**`HTLANDll.dll`** — separate DLL with a higher-level API (`dsoLANSetCHAndTrigger`, `dsoLANGetData`, etc.)
that does **not** use the 10-byte format. Likely targets a different 2xx2 variant; the 2D82AUTO uses `HTHardDll.dll` exclusively.

**Sigrok:** the 2D82AUTO is not in libsigrok. `hantek-dso` covers DSO-2090/5200A (Cypress FX2, VID `0x04b4`/`0x04b5`, completely different). This doc is the basis for a new sigrok driver.

---

## Packet Format

Every command is a fixed **10-byte** bulk OUT packet:

```
[0]   0x00 / 0x55    transport tag: 0x00=USB path, 0x55=LAN/TCP path
[1]   0x0a / 0x25    packet type:   0x0a=standard (10 B), 0x25=extended (serial number query only)
[2]   register bank  0x00=scope params, 0x02=init/misc, 0x03=device info queries
[3]   flag           0x01=write-only, 0x00=write+read-back
[4]   cmd            command opcode (meaning changes with bank byte[2])
[5]   data[0]        payload LSB, or 0x00
[6]   data[1]        payload byte 1, or 0x00
[7]   data[2]        payload byte 2 (TriggerHPos only), or 0x00
[8]   0x00           padding
[9]   0x00           padding
```

**Flag byte [3]:**
- `0x01` — write-only; device processes command, no USB IN response
- `0x00` — write+read-back; caller additionally reads USB IN after the write

**Response frame** (USB IN): `55 19 <bank> <cmd> <payload...>`
- `[0]` = `0x55` (always present on LAN path)
- `[1]` = `0x19` (constant marker, not a length)
- `[2]` = bank echo, `[3]` = cmd echo
- `[4:]` = payload (variable length, terminated by short USB bulk packet)

---

## Scope Control Commands (bank `0x00`)

### Channel-indexed commands (`cmd = channel × 6 + offset`)

CH0: cmds `0x00–0x05`; CH1: cmds `0x06–0x0b`.

| Offset | DLL export | data[5] meaning |
|--------|-----------|-----------------|
| `+0` | `dsoHTSetCHOnOff` | `0`=off, `1`=on |
| `+1` | `dsoHTSetCHCouple` | `0`=DC, `1`=AC, `2`=GND (note: wire-swapped vs API — see below) |
| `+2` | `dsoHTSetCHProbe` | `0`=1×, `1`=10×, `2`=100×, `3`=1000× |
| `+3` | `dsoHTSetCHBWLimit` | `0`=off, `1`=20 MHz BW limit |
| `+4` | `dsoHTSetCHVolt` | V/div index (see table below) |
| `+5` | `dsoHTSetCHPos` | `round((0xFF − raw_pos) × k)` — **inverted**: 0→`0xFF`, 255→`0x00` |

> **CHCouple API↔wire swap**: API uses 0=AC/1=DC but wire reverses them: API 0 (AC)→wire `0x01`; API 1 (DC)→wire `0x00`; API 2 (GND)→wire `0x02`.

**V/div index table** (from `lucaoli/Hantek` + firmware FPGA reg map):

| Index | V/div | FPGA REG 0x00/0x01 |
|-------|-------|---------------------|
| `0x00` | 10 mV | `0x81` |
| `0x01` | 20 mV | |
| `0x02` | 50 mV | |
| `0x03` | 100 mV | |
| `0x04` | 200 mV | |
| `0x05` | 500 mV | |
| `0x06` | 1 V | |
| `0x07` | 2 V | |
| `0x08` | 5 V | |
| `0x09` | 10 V | `0x68` |

### Global commands (fixed `cmd`)

| `cmd` | DLL export | data | Notes |
|-------|-----------|------|-------|
| `0x0c` | `sc_chansel` / `SCOPE_START_STOP` | `[5]`: `0`/`1` | Run/stop (per [hanteker](https://github.com/hkoosha/hanteker)'s `SCOPE_START_STOP`, same VID:PID — no channel-context semantics). Vendor app never sends it (413 OUT commands, zero hits) — likely relies on implicit continuous-run. An explicit stop→start toggle does NOT fix the NORMAL-mode trigger issue (see `trigger.md`). Not yet re-verified safe for general use in this suite's own code — don't add it to shared helpers without testing. ReadAllSet `[12]` echoes its SRAM value. |
| `0x0d` | ~~`dsoHTSetYTFormat`~~ `sc_rewind` | no payload needed | Resets waveform read cursor (`DAT_0803149c = 0`) — restart readout from start of sample buffer. NOT a display-format cmd despite DLL name. |
| `0x0e` | `dsoHTSetTimeDiv` | `[5:7]` LE uint16: timebase index | `0x00`=5ns … `0x21`=500s |
| `0x0f` | `dsoHTSetTriggerHPos` | 3 bytes — see encoding below | |
| `0x10` | `dsoHTSetTriggerSource` | `[5:7]` LE uint16: `0`=CH1, `1`=CH2, `2`=EXT | |
| `0x11` | `dsoHTSetTriggerSlope` | `[5:7]` LE uint16: `0`=rising, `1`=falling, `2`=either | |
| `0x12` | `dsoHTSetTriggerSweep` | `[5:7]` LE uint16: `0`=AUTO, `1`=NORMAL, `2`=SINGLE | `1` doesn't reliably trigger; use `2` for one-shot capture. See `vendors/hantek/2d82/docs/hardware/trigger.md`. |
| `0x13` | `dsoHTScopeAutoSet` | `[5]`=`0x00`; flag=`0x00` (read-back) | |
| `0x14` | `dsoHTSetTriggerVPos` | `[5]`= FP-scaled vpos byte | Fixed cmd, no channel multiplier |
| `0x15` | `dsoHTReadAllSet` | no payload (flag=`0x01`); returns 25-byte state frame | See ReadAllSet section |
| `0x16` | `SCOPE_START_RECV` | `[5:7]`=LE uint16 start, `[7:9]`=LE uint16 count | Waveform data request — see Waveform section |
| `0x17` | `dsoHTScopeZeroCali` | `[5]`=`0x00`; flag=`0x00` | **CORRECTED 2026-07-13**: `fw_usb_bulk_rx`'s write-side cmd dispatch (the real per-cmd handler for bank 0x00 flag=0x00, called from `fw_usb_cmd_parser`) has no branch for cmd 0x15, 0x16, or 0x17 — confirmed by exhaustive read of the full if/else-if chain in `decompiled_named.c`. This DLL export's packet is a firmware no-op on this build; live-tested, no error/no state change. See `ghidra_names_2d82.tsv` `fw_usb_bulk_rx`/`fw_usb_cmd_parser` entries. |

**TriggerHPos encoding (3 bytes):**
```
data[5:7] = abs(hpos_raw) × 3   (LE uint16)
data[7]   = 0x01 if hpos_raw < 0, else 0x00
```

**`dsoHTSetCHPos`** sends **two** consecutive packets with the same cmd byte: coarse + fine position.

---

## Device Info Queries (bank `0x03`)

Sent by Scope.exe at session start before the init burst. All use flag `0x01` (write-only, i.e. read-request).

| cmd[4] | byte[1] | Request (hex) | Response (hex) | Decoded |
|--------|---------|---------------|----------------|---------|
| `0x07` | `0x0a` | `55 0a 03 01 07 00 00 00 00 00` | `00 00 03 03 01` (5 B) | Device status/caps — bit meanings TBD |
| `0x09` | `0x25` | `55 25 03 01 09 00 00 00 00 00` | `55 19 03 09 <ascii>` (39 B) | Serial number: `"CN25110072085546-70"` |
| `0x0a` | `0x0a` | `55 0a 03 01 0a 00 00 00 00 00` | `55 19 03 0a <ascii>` (24 B) | Firmware version: `"2021090901"` (2021-09-09 rev 01) |
| `0x0c` | `0x0a` | `55 0a 03 01 0c 00 00 00 00 00` | `55 19 03 0c 07` (5 B) | HW version: `7` |

The `0x25` in byte[1] of the serial query signals an extended response.
The device status response (`00 00 03 03 01`) has no `0x55` header — format differs from others.

---

## FPGA Reprogram Sequence (bank `0x03`, cmd `0x02`/`0x03`/`0x04`)

Sent only by `dsoUpdateFPGA` (`HTHardDll.dll` export, gated behind a magic-tagged caller argument —
see [app.md](../app/app.md) "FPGA Update" for the full app-side trace). Full byte-level trace done
2026-07-13 by static disassembly of `HTHardDll.dll`; cross-referenced against the firmware-side
`fw_usb_cali_write_handler` sub-commands already documented in `vendors/hantek/2d82/ghidra/ghidra_names_2d82.tsv`
— confirmed exact match, no discrepancies.

**STEP 1/2 — two standard 10-byte header packets, no payload:**
```
55/00 0a 03 00 02 00 00 00 00 00   cmd=2: begin-config ack (firmware sets a flag only)
55/00 0a 03 00 03 00 00 00 00 00   cmd=3: ISC enable + erase (firmware runs MachXO2 unlock sequence)
```
App sleeps 3000 ms after these two sends (`KERNEL32!Sleep`) before starting page programming, to
cover the firmware's internal busy-poll/delay sequence during ISC enable+erase.

**STEP 3 — extended 64-byte packet, repeated once per 48 bytes of bitstream data:**
```
[0]      0x00                  transport tag
[1]      0x0a                  standard-format tag (same byte value as the 10-byte format's [1],
                                even though this packet is 64 bytes, not 10 — not a length field)
[2]      0x03                  bank
[3]      0x00                  flag (write+readback)
[4]      0x04                  cmd: program page
[5:0x35) up to 48 bytes         bitstream data, 3× 16-byte MachXO2 PROG_INCR_NV pages back-to-back
                                (final packet may be shorter than 48 bytes if the bitstream length
                                 is not a multiple of 48; all earlier packets are always full 48 B)
[0x35]   0x00 / 0x01           LAST-PACKET FLAG — 1 only on the final packet of the transfer, 0
                                otherwise. NOT a page counter (host sets it purely from "is this the
                                last packet", firmware just checks this one byte to trigger cleanup)
[0x36:0x40) 0x00               padding
```
This is a **new, extended packet variant not covered by the base 10-byte format** documented above
— the first 5 bytes are a strict prefix of the standard header, but the payload region extends to
64 bytes total. Confirmed this is the only bank/cmd combination in the entire DLL that uses this
longer format (exhaustive scan of `HTHardDll.dll`'s packet-construction call sites, 2026-07-13).

**Firmware-side handling** (see `ghidra_names_2d82.tsv` `fw_usb_cali_write_handler` for the full
sub-command table): on `[4]==4`, programs 3× `fw_machxo2_prog_incr` calls (16 bytes each, at
`rx_pkt+5`, `rx_pkt+5+0x10`, `rx_pkt+5+0x20`) then checks `rx_pkt[0x35]`; if 1, runs
`fw_machxo2_prog_done` → `fw_fpga_sspi_isc_reset` → `fw_machxo2_isc_erase` → `fw_machxo2_refresh`
and clears the in-progress flags, ending the reprogram sequence.

---

## AWG / DDS Commands (bank `0x02`, cmd[3]=`0x00`)

Source: [lucaoli/Hantek](https://github.com/lucaoli/Hantek) + firmware `fw_usb_awg_write`.  
Dispatch: `cmd[2]=0x02, cmd[3]=0x00` → `fw_usb_awg_write`. Sub-command at `cmd[4]`.

| cmd[4] | Name | Data bytes [5:9] | Notes |
|--------|------|-----------------|-------|
| `0x00` | `AWG_TYPE` | `[5]`=type | `0`=Square, `1`=Ramp, `2`=Sine, `3`=Trap, `4–7`=Arb1–4. Must be sent LAST (after freq/amp/offset) — `fw_awg_build_table` only rebuilds the output table when this value actually changes, so sending it first against an already-matching stored type is a silent no-op. See `hantek_proto.set_awg()`. |
| `0x01` | `AWG_FREQ` | `[5:9]`=LE uint32 Hz | Frequency in Hz |
| `0x02` | `AWG_AMP` | `[5:7]`=LE uint16 mV (raw, not ×1000 — the "abs×1000" header only means "value IS millivolts"), `[7]`=sign | Amplitude in mV; sign=`1` if negative |
| `0x03` | `AWG_OFF` | `[5:7]`=LE uint16 mV, `[7]`=sign | DC offset in mV |
| `0x04` | `AWG_SQUARE_DUTY` | `[5:7]`=LE uint16 ×100 | Square duty cycle %×100 |
| `0x05` | `AWG_RAMP_DUTY` | `[5:7]`=LE uint16 ×100 | Ramp symmetry %×100 |
| `0x06` | `AWG_TRAP_DUTY` | `[5]`=rise×100, `[6]`=high×100, `[7]`=fall×100 | Trapezoid timing |
| `0x07` | `AWG_SHAPE` | `[5]`=len, `[6:9]`=16-bit sample pairs | Arbitrary waveform data chunk |
| `0x08` | `AWG_START` | `[5]`=`0`/`1` | `0`=stop, `1`=enable output — the real enable, calls `fw_fpga_awg_enable()`. **No separate bank-3 "AWG mode select" handshake is needed or sent by the real app** — an earlier hypothesis this session (a bank3/`[4]==0`/`[5]==2` handshake being required before AWG_START) was tested against a live vendor capture and found to be absent from real traffic; removed from `hantek_signal_chain_test.py`. |

**Readback:** `cmd[2]=0x02, cmd[3]=0x01` → `fw_usb_awg_readback` — returns current AWG params.

**CONFIRMED real command order (2026-07-14, live usbmon capture of vendor Scope.exe enabling a 1kHz/2Vpp sine on CH2):**
```
00 0a 02 00 01 e8 03 00 00 00   # AWG_FREQ = 1000 Hz
00 0a 02 00 02 d0 07 00 00 00   # AWG_AMP  = 2000 mV
00 0a 02 00 05 32 00 00 00 00   # AWG_RAMP_DUTY (sent even for sine -- vendor app always writes full param set)
00 0a 02 00 04 32 00 00 00 00   # AWG_SQUARE_DUTY (ditto)
00 0a 02 00 06 14 28 14 00 00   # AWG_TRAP_DUTY (ditto)
00 0a 02 00 03 00 00 00 00 00   # AWG_OFF = 0 mV
00 0a 02 00 00 02 00 00 00 00   # AWG_TYPE = 2 (sine) -- sent LAST, forces table rebuild
00 0a 02 00 08 01 00 00 00 00   # AWG_START = 1 (enable)
```
Byte[0]=`0x00` (USB transport tag, not `0x55` — see Packet Format above; the `55 0a ...` example
below predates this correction and is kept only as a reference for the byte LAYOUT, not byte[0]'s
value on real USB):
```
55 0a 02 00 00 02 00 00 00 00   # AWG_TYPE = 0x02 (sine)
55 0a 02 00 01 e8 03 00 00 00   # AWG_FREQ = 1000 Hz
55 0a 02 00 02 e8 03 00 00 00   # AWG_AMP  = 1000 mV
55 0a 02 00 08 01 00 00 00 00   # AWG_START = 1 (enable)
```

### Screen mode switch (bank `0x03`, cmd[3]=`0x00`)
`cmd[4]` = `0`=Scope, `1`=DMM, `2`=AWG.

**Note:** this (and `fw_usb_mode_switch`, bank `0x01`) only switches which
measurement subsystem is *active/displayed* — it does NOT electrically
connect the DMM front-end to the scope CH1/CH2 or AWG front-ends. The DMM has
its own physical probe jacks; see
[test-setup.md "DMM Probe Connection"](test-setup.md#dmm-probe-connection-separate-front-end-not-auto-connected)
for the physical jumper needed to measure a scope/AWG signal on the DMM.

---

## DMM Commands (bank `0x01`)

Source: `vendors/hantek/2d82/scripts/dmm.py`, `vendors/hantek/2d82/scripts/hantek_signal_chain_test.py`, firmware `fw_usb_mode_switch` /
`fw_usb_dmm_status_pack`. Confirmed live 2026-07-14 (device re-enumerated after fixing the
byte[0] transport-tag bug documented above; all commands below use byte[0]=`0x00`).

### Mode switch (cmd `0x00`, flag `0x00`, write-only)

`fw_usb_mode_switch` (bank `0x01`, flag `0x00`) — the REAL per-function DMM mode setter
(distinct from the bank-3 screen mode switch above, which only switches which
subsystem/screen is displayed, not the measurement function). `[5]` = selector byte:

| Selector | Function |
|----------|----------|
| `0` | AC Current (A) |
| `1` | DC Current (A) |
| `2` | AC Current (mA) |
| `3` | DC Current (mA) |
| `4` | Hold |
| `5` | DC Voltage (V) |
| `6` | AC Voltage (V) |
| `7` | Capacitance |
| `8` | Resistance |
| `9` | Continuity |
| `10` | Diode |

Table confirmed by direct read of `fw_usb_mode_switch`'s decompiled if/else-if chain
(`decompiled_named.c` `@ 0x080305dc`) — the TSV's own summary comment for this function was
found internally inconsistent with the real branch values and should not be trusted over this
table. Mode switch is NOT instant: firmware re-inits/autoranges on mode entry
(`fw_dmm_range_set()` etc.), allow ~1.5s settle before reading status.

### Status read (cmd `0x00`, flag `0x01`, write+read-back)

Response: 14-byte packet from `fw_usb_dmm_status_pack`.

```
[0]     0x55 magic
[1]     0x0b (len)
[2]     0x01 (bank echo)
[3]     display mode index (fw_dmm_mode_xlat output — see DMM_MODE_IDX_NAME in vendors/hantek/2d82/scripts/dmm.py)
[4]     sign/overload flag — LAYOUT NOT CONFIRMED, see below
[5]     range/overload bit — LAYOUT NOT CONFIRMED, see below
[6]     priority-encoded flag (3/2/1/0)
[7:11]  4 digit values (0-9, or 0xF-family for blank/dash segments)
[11]    5-way decoded flag
[12]    5-way decoded flag
[13]    0x55 trailer
```

**Sign/overload byte semantics unconfirmed** — a live cross-check (`vendors/hantek/2d82/scripts/awg.py --dc <mv>`
into the DMM's V/Ω/mA jack, jumpered per
[test-setup.md](test-setup.md#dmm-probe-connection-separate-front-end-not-auto-connected), read
via `vendors/hantek/2d82/scripts/dmm.py --mode dc_v`) found byte[4] constant (`1`) across both +1000mV and -1000mV
AWG inputs even though the physical LCD correctly showed the sign change — ruling out a simple
"byte[4]==1 means negative" reading. byte[5] toggled `0`→`1` between the positive and negative
cases, making it the better sign-bit candidate, but this was not re-confirmed against the LCD
for the negative case before this investigation was stopped in favour of writing up findings.
**Do not trust `vendors/hantek/2d82/scripts/dmm.py`'s current sign decode** until re-verified against several live
LCD readings across both polarities. See `ghidra_names_2d82.tsv`'s `fw_dmm_mode_xlat` entry for
the same caveat recorded at the source-analysis level.

**Digit magnitude IS confirmed accurate**: AWG DC 500/1000/2000 mV inputs decoded to DMM digits
0498/0999/2000 respectively (within ~0.2%) — only the sign-byte interpretation is in question,
not the underlying ADC/digit-decode chain.

---

## Waveform Data (cmd `0x16` — `SCOPE_START_RECV`)

Source: `lucaoli/Hantek` + firmware `fw_usb_scope_readback` case `0x16`.

The PC requests waveform data in 64-byte chunks by sending repeated `0x16` commands:

```
[5:7]  LE uint16  start offset into capture buffer
[7:9]  LE uint16  sample count requested
```

`uVar12 = start + count` = end position. Device returns up to 64 ADC bytes per call, advancing
an internal pointer. When pointer ≥ uVar12, the scope re-arms for the next capture.

**Scope.exe parameters — CORRECTED 2026-07-14** via live usbmon capture of the real vendor
Scope.exe: `01 0a 00 01 16 b0 04 b0 04 14` (byte[0]=`0x01`, the one exception to the
byte[0]=0x00-for-USB rule — see Packet Format above; byte[9]=`0x14`, meaning not yet traced).
- start = `0x04b0` = 1200, count = `0x04b0` = 1200
- total = 1200 bytes = **19 packets** of 64 bytes (18 full + 1 partial)
- This matches the FPGA-side "windowed live-view" read size (`0x4b0` samples/channel per
  display refresh) documented in
  [waveform-readback.md](waveform-readback.md#1-two-independent-read-paths--same-wire-command),
  not the full 6144-byte-per-channel capture size.
- The previously-documented `55 0a 00 01 16 b0 10 60 09 14` (start=`0x10b0`=4272,
  count=`0x0960`=2400, "105 packets") was transcribed from a `vendors/hantek/2d82/scripts/hantek_capture.py` comment
  citing static disassembly, never re-verified against live traffic, and does not match what the
  real app actually sends — kept below crossed out for history, do not use these values.
  ~~start = `0x10b0` = 4272, count = `0x0960` = 2400, total = 6672 bytes = 105 packets~~

**ADC encoding**: offset-binary — `0x80`=0 V, `0x00`=−FS, `0xFF`=+FS.

**Throughput**: MCU main loop polls USB at ~20 Hz (50 ms/command). See
`vendors/hantek/2d82/scripts/hantek_signal_chain_test.py`'s `sc_recv()`/`adc_capture()` for a confirmed-working
capture implementation (live-tested 2026-07-14, full AWG→scope→capture round trip verified);
`vendors/hantek/2d82/scripts/hantek_capture.py` predates the byte[0] and start/count corrections above and has not
been re-verified.

**"Not ready" response** (trigger not yet fired): `00 00 ff ff 00 00 ff ff 00 00 ff ff` (alternating pattern).

---

## ReadAllSet Response Layout (cmd `0x15`)

Response: `55 19 00 15 <21-byte payload>` = 25 bytes total.

Sent by Scope.exe on connect to read the device's persisted state from the previous session.
The app immediately overwrites with a full init burst.

Layout decoded from `fw_usb_scope_readback` (`0x08030f0c`), bVar1 < 0x16 branch:

```
[0]  CH0.OnOff       (0=off, 1=on)
[1]  CH0.Couple      (0=DC, 1=AC, 2=GND)
[2]  CH0.Probe       (0=1×, 1=10×, 2=100×, 3=1000×)
[3]  CH0.BWLimit     (0=off, 1=20 MHz)
[4]  CH0.Volt        (V/div index)
[5]  CH0.Pos         (encoded, +0x64 offset)
[6]  CH1.OnOff
[7]  CH1.Couple
[8]  CH1.Probe
[9]  CH1.BWLimit
[10] CH1.Volt
[11] CH1.Pos         (encoded, +0x64 offset)
[12] ChanSel         (from DAT_08031484+2; channel-context selector set by cmd 0x0c — NOT run/stop)
[13] TriggerHPos lo  (DAT_0803148c[0])
[14] TriggerHPos hi  (DAT_0803148c[3])
[15] TriggerHPos sign lo (DAT_0803148c[9] lo byte)
[16] TriggerHPos sign hi (DAT_0803148c[9]>>8)
[17] TriggerSource   (DAT_08031490[0]: 0=CH1, 1=CH2, 2=EXT)
[18] TriggerSlope    (DAT_08031490[3]: 0=rising, 1=falling, 2=either)
[19] TriggerSweep    (DAT_08031490[6]: 0=AUTO, 1=NORMAL, 2=SINGLE)
[20] TriggerVPos     (DAT_08031490[9] + 0x64)
```

Note: the live-captured response `00 01 00 01 06 95 00 01 00 00 06 64 00 00 0e 96 00 00 00 00 95`
(21 bytes starting at offset [4] of the 25-byte frame) aligns with this layout — byte[0]=CH0.OnOff=1,
byte[4]=CH0.Volt=6, byte[12]=ChanSel=0 (channel-context flag), byte[17]=TrigSource=0 (CH1), etc.

---

## Session Init Sequence (live capture 2026-06-22)

Captured via `vendors/hantek/2d82/scripts/hantek_usb_tcp_bridge.py`. Full sequence for session starting 18:54:48.

### Step 1 — Device info queries
```
OUT: 55 0a 03 01 07 00 00 00 00 00   → DeviceStatus
IN:  00 00 03 03 01

OUT: 55 0a 00 01 15 00 00 00 00 00   → ReadAllSet (read persisted state)
IN:  55 19 00 15 00 01 00 01 06 95 00 01 00 00 06 64 00 00 0e 96 00 00 00 00 95

OUT: 55 0a 03 01 0a 00 00 00 00 00   → FirmwareVersion
IN:  55 19 03 0a 32 30 32 31 30 39 30 39 30 31 00...  ("2021090901")

OUT: 55 0a 03 01 0c 00 00 00 00 00   → HWVersion
IN:  55 19 03 0c 07

OUT: 55 25 03 01 09 00 00 00 00 00   → SerialNumber
IN:  55 19 03 09 43 4e 32 35 31 31 30 30 37 32 30 38 35 35 34 36 2d 37 30 00...  ("CN25110072085546-70")
```

### Step 2 — Scope parameter init burst (17 commands, packed)
```
55 0a 00 00 00 01   CH0.OnOff    = 1
55 0a 00 00 05 95   CH0.Pos      = 0x95
55 0a 00 00 01 00   CH0.Couple   = 0 (DC)
55 0a 00 00 02 00   CH0.Probe    = 0 (1×)
55 0a 00 00 03 00   CH0.BWLimit  = 0
55 0a 00 00 04 06   CH0.Volt     = 6 (1 V/div)
55 0a 00 00 06 01   CH1.OnOff    = 1
55 0a 00 00 0b 31   CH1.Pos      = 0x31
55 0a 00 00 07 00   CH1.Couple   = 0 (DC)
55 0a 00 00 08 00   CH1.Probe    = 0 (1×)
55 0a 00 00 09 00   CH1.BWLimit  = 0
55 0a 00 00 0a 06   CH1.Volt     = 6 (1 V/div)
55 0a 00 00 14 95   TriggerVPos  = 0x95
55 0a 00 00 0f 96   TriggerHPos  = 0x96
55 0a 00 00 10 00   TriggerSrc   = 0 (CH1)
55 0a 00 00 12 00   TriggerSweep = 0 (Auto)
55 0a 00 00 11 00   TriggerSlope = 0 (rising)
55 0a 00 00 0e 0c   TimeDiv      = 0x0c
```

### Step 3 — Init finalisation
```
55 0a 03 00 00 00 00 00 00 00   bank3 cmd0 write+read ×2 (purpose TBD)
55 0a 03 00 00 00 00 00 00 00
55 0a 02 00 08 00 00 00 00 00   bank2 reg08 write+read → 16×0x00 after ~3s delay
```
The bank2/reg08 command is the final init step; strong candidate for `dsoInitHard` or `dsoWorkType`.

### Step 4 — Running (waveform acquisition loop)
```
OUT: 55 0a 00 01 16 b0 10 60 09 14   → SCOPE_START_RECV (every ~400 ms)
IN:  64 bytes ADC data                  (00×64 = no triggered capture; alternating 00/ff pattern = trigger not yet fired)
```

---

## Internal Send Helper (`0x10004440`)

All `dsoHTSet*` functions share this helper:

```
push arg6   ; flag:      0 → packet[3]=0x01 (write-only)
             ;            1 → packet[3]=0x00 (read-back)
push arg5   ; data_len:  bytes to copy from arg4 into packet[5..]
push arg4   ; out_ptr:   pointer to receive read-back value (or NULL)
push arg3   ; cmd_byte:  placed at packet[4]
push arg2   ; data_value: written LSB-first into packet[5..]
push arg1   ; device_idx: selects USB device handle
call 0x10004440
add esp, 0x18
```

Steps: zero 64-byte buffer → write header `[0]=0x00 [1]=0x0a [2]=0x00 [3]=flag` → write cmd/data → call `0x10002060` (USB write) → if `arg6≠0`: call `0x10002020` (USB read) → store at `*out_ptr`.

**`dsoHTReadAllSet`** (`0x10002230`) does **not** use this helper — calls write and read directly.

---

## Waveform-Read Commands — `cmd 0x16` and `cmd 0x17`

**Decoded from firmware `fw_usb_scope_readback` @ 0x41406 (`vendors/hantek/2d82/firmware/2d82-20210818/analysis/decompiled_named.c`).**

### `cmd 0x16` — Streamed processed waveform

**Request:** `55 0a 00 01 16 <data[5:9]> 00 00`
- `data[5:7]` = start address (LE uint16)
- `data[7:9]` = sample count (LE uint16)
- `end_address = start + count`

**Response:** 1–64 bytes per call
- Each call returns up to 64 samples from the internal buffer `DAT_08031bf8`, advancing a read cursor.
- Firmware internally resamples / processes samples from the FPGA capture buffers (MCU applies `sample - zero_offset - 0x80`).
- ADC encoding: offset-binary (`0x80`=0 V, `0x00`=−FS, `0xFF`=+FS).

**Trigger state handling:**
- **Not yet triggered** → `fw_scope_acq_state_set` returns −1 → firmware emits: `00 00 ff ff 00 00 ff ff 00 00 ff ff` (12-byte repeating pattern). Response length is set to 12.
- **Triggered, data pending** → return up to 64 bytes from buffer at current offset.
- **All samples read** (cursor reaches `end_address`) → firmware auto-re-arms:
  1. Reset read cursor to 0.
  2. Call `fw_scope_apply_all(1)` (restore UI / arm mode).
  3. Reset `TriggerHPos = 0x96`.
  4. Re-apply trigger edge.
  5. Set acquisition-run flag.

**Response termination:** When `cursor >= end_address`, the device auto-re-arms and is ready for a new trigger. Loop breaks when fewer than 64 bytes are returned (indicates end of this capture).

**Client use:**
```
Loop:
  Send: 55 0a 00 01 16 <start_lo start_hi count_lo count_hi> 00 00
  Recv: N bytes (1–64)
  If N == 12 and is "00 00 ff ff ..." pattern: device not yet triggered, retry
  Else: accumulate N bytes; if N < 64: capture complete, break
```

### `cmd 0x17` — Direct FPGA capture-buffer dump

**Request:** `00 0a 00 01 17 00 00 00 00 00` (no payload; triggers a read)

**CORRECTED 2026-07-14** (direct read of `fw_usb_scope_readback`'s decompiled body, cross-checked
live): selectors `0x16` and `0x17` share the SAME streaming code path in the firmware (the branch
condition `(v != 0x18) && (v < 0x18)` is a tautology covering both) — this is genuinely raw,
un-decimated per-channel sample data at `g_usb_scope_state_ptr + ch_idx*0x1800`, up to 6144 B/channel
max, but the actual streamed length is bounded by `fw_fpga_read_end_addr(v)`'s trigger/timebase
computation, NOT a fixed 6144 B — expect a shorter response in practice (live-tested: consistently
~1300 B, not 6144+), and treat that as correct/expected, not truncated. Streams 64-byte packets
until `sample_off` reaches the computed end address for all active channels
(`fw_scope_ch_active_count()`, 1 or 2), then stops — a genuinely short final read is normal
completion, not an error; don't retry past it. If both channels are disabled, response is a single
`0x55` byte (confirmed, fuzz-tested 2026-07-13).

**Use case:** un-decimated ADC data vs cmd `0x16`'s resampled/windowed view — but the two selectors
are close enough (same loop, different end-address arg) that the practical difference in capture
content between them is not yet characterized; do not assume `0x17` gives dramatically more samples
than `0x16` without comparing actual byte counts on this firmware. `vendors/hantek/2d82/scripts/hantek_proto.sc_recv_raw()`
implements the request.

**Note on cmd 0x17 flag=0x00 vs flag=0x01**: this section documents flag=`01` (read, via `fw_usb_scope_readback`) — the only firmware-meaningful use of cmd 0x17. The `dsoHTScopeZeroCali` DLL export in the global-commands table above sends flag=`00` instead, which is confirmed (2026-07-13) to be a no-op in `fw_usb_bulk_rx` on this firmware build. Same cmd byte, different flag, different (and non-overlapping) meaning.

---

## Open Questions

| # | Question | Status | Resolution |
|---|----------|--------|------------|
| 1 | Bank `0x03` cmd `0x00` (×2 at end of init) — purpose | **RESOLVED (2026-07-13)** | `fw_usb_cali_write_handler` (0x08031c40), byte[4]==0 dispatches further on byte[5]: 0=scope-mode select, 1=DMM-mode select, 2=AWG-mode select (gated on a live FPGA SPI status re-read for AWG hardware presence — not a stored/spoofable flag). The two init-time calls are almost certainly `[5]==0` (scope mode select) sent twice, or scope+one other mode select during app startup. Full sub-command table (byte[4] 0-0xf) now in `vendors/hantek/2d82/ghidra/ghidra_names_2d82.tsv` — no capability/model gate found in the full audit (see GH #145). |
| 2 | Bank `0x02` reg `0x08` finaliser — purpose | **RESOLVED** | `fw_usb_awg_write` (0x080314a0) sub-command `0x08` = `AWG_START`: `[5]`=`0`/`1` enables/disables AWG output (see AWG table above). The init-time call with all-zero payload is `AWG_START=0` — an explicit "ensure AWG output is off" safety default at session start, not a legacy/undocumented path. |
| 3 | Device status (`cmd 0x07`) response format `00 00 03 03 01` — bit meanings | **Needs capture** | Capture in different device states (armed/idle, different V/div, triggered/waiting) and diff. |
| 4 | Trigger source index→FPGA REG 0x17 bits[7:2] mapping | **Trace needed** | Hook `fw_fpga_trigger_edge` in Renode; cycle DLL trigger-source values (0/1/2 = CH1/CH2/EXT) and log REG writes. |
| 5 | Timebase divisor values per index | **Full table empirically measured 2026-07-14** (`vendors/hantek/2d82/scripts/hantek_samplerate_probe.py --sweep-all`) — supersedes GH#48/#172's "unrecoverable" conclusion | The flash pointer `g_dat_16a0`/`0x080116a0` does NOT point to a divisor table — GH#48 and #172 (live SWD capture) both conclusively found `0x20001FC0` holds unrelated SRAM pointers/calibration data, not divisors, and static/SRAM recovery is a dead end. **This table sidesteps that entirely** by measuring the cmd-0x16 USB *readback* rate empirically (AWG known-frequency + cycle-counting in the 1200-sample capture window) instead of trying to read the FPGA-side divisor value directly. **Important distinction**: this is the effective *readback* rate, not the scope's real ADC rate (250 MSa/s 1ch / 125 MSa/s 2ch per the vendor datasheet) — cmd 0x16 returns a decimated/resampled window, not raw ADC samples (cmd 0x17 shares the same buffer/limitation, see "cmd 0x17" above). **Methodology caveat**: all measurements used AWG amplitude 1500mV, an inherited default from earlier ad-hoc probing this session, not deliberately chosen for this specific sweep — worth re-checking if borderline/implausible rows matter to you. 3 of 26 measured rows were discarded as implausible (rate implies a negative divisor, a measurement artifact of the cycle-counting method at fast probe bands, not real data); 1 index (`0x18`) got no measurement at all (no AWG frequency low enough completed a full cycle in the window). Indices `0x1a`-`0x21` were not measured (sweep stopped for time; the trend through `0x19` was already well past the region relevant to picking a faster default).<br><br>**Full results** (rate=effective readback MS/s, "—"=discarded/no data):<br>`0x00`=231.6, `0x01`=—, `0x02`=**243.2 (fastest reliable, 3/5 usable)**, `0x03`=—, `0x04`=231.6, `0x05`=—, `0x06`=194.6, `0x07`=70.9, `0x08`=48.6, `0x09`=18.7, `0x0a`=11.8, `0x0b`=4.7, `0x0c`=1.7 (prior suite default), `0x0d`=1.0, `0x0e`=0.58, `0x0f`=0.19, `0x10`=0.060, `0x11`=0.049, `0x12`=0.019, `0x13`=0.0081, `0x14`=0.0049, `0x15`=0.0019, `0x16`=0.0019, `0x17`=0.001 (1/5 usable, low confidence), `0x18`=no data, `0x19`=1.459 (1/5 usable, non-monotonic vs `0x18`, low confidence).<br><br>**Recommendation**: `vendors/hantek/2d82/scripts/hantek_proto.configure_scope_channel` now takes an optional `tb_idx` param (still defaults to `0x0c` for compat with existing tuned settle/retry timing) — pass `tb_idx=0x02` explicitly for captures needing to resolve content above ~1MHz without the aliasing seen at `0x0c` (Open Question #7's bimodal-capture problem was very likely this exact aliasing, not a separate bug). Higher indices remain useful for low-frequency/DC work where a longer time window matters more than sample density. Raw data: `tmp/samplerate_sweep.csv`, loaded via `vendors/hantek/2d82/scripts/hantek_samplerate_load_sqlite.py` into `tmp/samplerate_sweeps.sqlite3` (label `careful-settle`) for diffing against future re-measurements.<br><br>**Settle-time methodology confirmed load-bearing, 2026-07-14.** Re-ran the full `--sweep-all` sweep with reduced settle time (1.0s→0.2s) and retry budget (60→15, `--settle-s`/`--retries`/`--csv-out`, loaded as SQLite label `fast-settle`) purely to check whether the careful timing was actually necessary or just conservative. It was necessary: **6 of 34 indices came back physically implausible under fast settling (negative implied divisor / rate exceeding the 250MHz clock) versus 0 under careful settling**, and of the indices both runs could measure, nearly every one diverged 15-220% — including the fastest reliable index (`0x02`: 243.2 MS/s careful vs. 426.7 MS/s fast, a result that isn't even self-consistent with `0x02`'s neighbours). **Do not reduce settle time below 1.0s / retries below 60 for this measurement** — the FPGA/ADC pipeline needs real time to re-lock after a timebase or AWG-frequency change, and readings taken before it settles are wrong, not just noisier. Fast-settle results are in `tmp/samplerate_sweep_fast.csv` / SQLite label `fast-settle` for reference only — not to be treated as usable data. |
| 6 | App/firmware mismatch: does any DLL export construct a bank/cmd the firmware doesn't handle? | **RESOLVED (2026-07-13), correction 2026-07-13, full internal trace added 2026-07-13** | `HTHardDll.dll` export `ddsSDKSetOptions` (ordinal 8, `_ddsSDKSetOptions@8`, code at RVA `0x43c0`) constructs a bank `0x03` cmd `0x0d` packet — a sub-command **absent from both `fw_usb_cali_read_handler` and `fw_usb_cali_write_handler`'s dispatch chains** (dead/unhandled on both sides per their TSV entries). However, **`Scope.exe` never imports `ddsSDKSetOptions`** (checked its full IAT) — this export is unused by the shipped GUI app; it appears to be SDK-only surface (the `ddsSDK*` export family, separate from the `dsoHTSet*`/`dsoGetHTMeasure` families Scope.exe actually uses) for third-party programmatic AWG control. No other bank value beyond `0x00`–`0x03` is constructed anywhere in `HTHardDll.dll` (exhaustive scan of all 18 packet-header-fill call sites in the DLL). `dsoUpdateFPGA` (bank 3, cmd 0x02→0x03→0x04, the FPGA reprogram sequence) is gated behind a magic-tagged value (`ptr & 0xfff00000 == 0x56700000`) that the CALLER OR's onto its own state — **confirmed NOT read from the bitstream file** (see full trace in "FPGA Reprogram Sequence" section above and `app.md` "FPGA Update"). **Correction:** `dsoUpdateFPGA` is *not* updater-exclusive — `Scope.exe` imports it too (IAT slot `0x474190`, single call site `0x43ea63`, same magic-tag gate), reachable via a genuinely hidden `WM_KEYDOWN`-triggered dialog (Ctrl+Alt+C / Ctrl+Alt+F, dialog resource #178 "UpdateFPGA", no menu/toolbar path). See `app.md` "Hidden hotkey" section for the full trace; the underlying reprogram sequence and gate are identical between `Scope.exe` and `HT2000Update.exe` — same call, not a different/safer USB command sequence. The cmd 0x02/0x03/0x04 wire sequence itself (packet-by-packet) is now fully traced and cross-referenced against the firmware side with no discrepancies found — see "FPGA Reprogram Sequence" above. |
| 7 | Analog bandwidth / high-frequency capture reliability | **Partial data — 2026-07-14** | Clean flat plateau 1kHz–1MHz (median span ~50-52/255, `vendors/hantek/2d82/scripts/hantek_awg_characterize.py`, median-of-5 per point, raw arrays logged in `tmp/awg_characterize.log`). Rolloff starts ~500kHz-2MHz, but raw data ≥2MHz is bimodal (either full signal or near-zero, nothing between) in both BWLimit=0/1 — do not quote a -3dB number above ~1-2MHz until root-caused (leading hypothesis: aliasing from the unconfirmed timebase index `0x0c`, row 5). BW-limit does show a small consistent effect where data is clean (100kHz-1MHz: BWLimit=1 runs ~2-3 counts lower than BWLimit=0). |

---

DLL export list, Wine setup, and app files: [app.md](../app/app.md).
