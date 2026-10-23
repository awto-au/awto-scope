# 2D82AUTO — USB Protocol Testing

Reference doc for `vendors/hantek/2d82/scripts/hantek_test_protocol.py` and `vendors/hantek/2d82/scripts/hantek_adc_watch.py`.

## Physical setup

```
AWG output BNC ──T-piece──► CH1 BNC input
                         └──► multimeter (optional, for AWG amplitude verification)
```

## Packet format

All packets are exactly 10 bytes:

```
[0x55][0x0a][bank][flag][cmd][d0][d1][d2][d3][d4]
```

| bank | flag | path | purpose |
|------|------|------|---------|
| 0x00 | 0x00 | `fw_usb_bulk_rx` | scope control writes |
| 0x00 | 0x01 | `fw_usb_scope_readback` | ReadAllSet / waveform read |
| 0x02 | 0x00 | `fw_usb_awg_write` | AWG sub-commands |
| 0x03 | 0x00/0x01 | device info | status/serial/version queries |

## Scope control commands (bank=0x00, flag=0x00)

| cmd | name | data | notes |
|-----|------|------|-------|
| 0x00 | CH0.OnOff | `[0/1]` | **requires sc_chansel(1) first** |
| 0x01 | CH0.Couple | `[0=DC,1=AC,2=GND]` | **requires sc_chansel(1) first** |
| 0x02 | CH0.Probe | `[0=1x,1=10x,2=100x,3=1000x]` | **requires sc_chansel(1) first** |
| 0x03 | CH0.BWLim | `[0/1]` | **requires sc_chansel(1) first** |
| 0x04 | CH0.Volt | `[0..10]` index into VDIV table | no chansel needed |
| 0x05 | CH0.Pos | `[raw]` stored as `raw-100`, echoed as `raw` | no chansel needed |
| 0x06 | CH1.OnOff | `[0/1]` | **requires sc_chansel(1) first** |
| 0x07 | CH1.Couple | `[0=DC,1=AC,2=GND]` | **requires sc_chansel(1) first** |
| 0x08 | CH1.Probe | `[0=1x,1=10x,2=100x,3=1000x]` | **requires sc_chansel(1) first** |
| 0x09 | CH1.BWLim | `[0/1]` | **requires sc_chansel(1) first** |
| 0x0a | CH1.Volt | `[0..10]` | no chansel needed |
| 0x0b | CH1.Pos | `[raw]` | no chansel needed |
| 0x0c | **chansel** | `[0/1]` | channel-context selector — gates cmd 0x00-0x03/0x06-0x09 routing. **NOT RunStop** (see #62) |
| 0x0d | YTFormat | `<H>` 0=YT,1=XY,2=Roll | |
| 0x0e | TimeBase | `<H>` index | |
| 0x0f | TrigHPos | `<H> abs(raw)*3` + `[sign]` | sign byte: 1=negative |
| 0x10 | TrigSrc | `<H>` 0=CH1,1=CH2 | |
| 0x11 | TrigSlope | `<H>` 0=rise,1=fall | |
| 0x12 | TrigSweep | `<H>` 0=AUTO,1=NORMAL,2=SINGLE | |
| 0x13 | AutoSet | `[0x00]` | write-only, endpoint busy during processing |
| 0x14 | TrigVPos | `[raw]` stored as `raw-100`, echoed as `raw` | |
| 0x15 | ReadAllSet | flag=0x01 | returns 25-byte response (see below) |
| 0x16 | WaveRecv | flag=0x01, `<HH>start,count` + `[0x14]` | 105 packets × 64B = 6720B |
| 0x17 | ZeroCali | `[0x00]` | write-only, endpoint busy during processing |
| 0x18 | AltTrigVPos | `[raw]` | alternate TrigVPos path, same encoding |

### CRITICAL: chansel (cmd 0x0c) routing rule

`fw_usb_bulk_rx` uses `DAT_08031484+2` (set by cmd 0x0c) to gate writes for cmds 0x00–0x03 and 0x06–0x09. When the value is 0 (default), these writes land in **invisible** parameter slots — ReadAllSet never sees them and the hardware is never updated. **Always send `sc_chansel(1)` before any on/off, couple, probe, or bwlimit write.**

This is confirmed in firmware at `decompiled_named.c` lines 41162–41215 and 41255–41313.

## ReadAllSet response (cmd 0x15, flag=0x01)

25-byte response. Header = bytes 0–3 `[0x55][0x0a][??][0x15]`. Payload = bytes 4–24:

| offset | field | notes |
|--------|-------|-------|
| p[0]  | CH0.OnOff | |
| p[1]  | CH0.Couple | 0=DC,1=AC,2=GND |
| p[2]  | CH0.Probe | 0=1x,1=10x,2=100x,3=1000x |
| p[3]  | CH0.BWLim | |
| p[4]  | CH0.Volt | index 0..10 |
| p[5]  | CH0.Pos | raw (= written value) |
| p[6]  | CH1.OnOff | |
| p[7]  | CH1.Couple | |
| p[8]  | CH1.Probe | |
| p[9]  | CH1.BWLim | |
| p[10] | CH1.Volt | |
| p[11] | CH1.Pos | |
| p[12] | chansel | `DAT_08031484+2` — NOT RunStop (see #62) |
| p[13] | TrigHPos_lo | |
| p[14] | TrigHPos_hi | |
| p[15] | (unknown) | |
| p[16] | (unknown) | |
| p[17] | TrigSrc | 0=CH1,1=CH2 |
| p[18] | TrigSlope | 0=rise,1=fall |
| p[19] | TrigSweep | 0=AUTO,1=NORMAL,2=SINGLE |
| p[20] | TrigVPos | raw |

## Volt/div index table

| idx | mV/div |
|-----|--------|
| 0   | 2 |
| 1   | 5 |
| 2   | 10 |
| 3   | 20 |
| 4   | 50 |
| 5   | 100 |
| 6   | 200 |
| 7   | 500 |
| 8   | 1000 |
| 9   | 2000 |
| 10  | 5000 |

## AWG commands (bank=0x02, flag=0x00)

| subcmd | name | data |
|--------|------|------|
| 0x00 | Type | `[0=sine,1=square,2=ramp,3=pulse,4=noise,5=DC,6=trapezoid,7=sinc,8=gauss,9=lorentz,10=haversine]` |
| 0x01 | Freq | `<I>` Hz |
| 0x02 | Amplitude | `<H>` mV + `[sign]` |
| 0x03 | Offset | `<H>` mV + `[sign]` |
| 0x04 | SquareDuty | `<H>` × 100 (e.g. 5000 = 50.00%) |
| 0x05 | RampDuty | `<H>` × 100 |
| 0x06 | TrapDuty | `[rise][high][fall]` each 0–99 |
| 0x07 | (unknown) | |
| 0x08 | Start | `[0=stop,1=start]` |

## Device info queries (bank=0x03)

| cmd | flag | response |
|-----|------|----------|
| 0x07 | 0x01 | DeviceStatus (64B) |
| 0x09 | 0x01 | SerialNumber (64B, null-terminated string) |
| 0x0a | 0x01 | FirmwareVersion (64B, null-terminated string) |
| 0x0c | 0x01 | HWVersion (64B, null-terminated string) |

## ADC encoding

Offset-binary: `0x80` = 0V, `0x00` = −FS, `0xFF` = +FS.

Waveform capture: 105 × 64-byte packets = 6720 bytes total. Use first 6672 bytes (6672 / 64 = 104.25 — last partial packet discarded). Firmware source address `0x10b0`, count `0x0960`.

## Mode switch (bank=0x03, cmd=0x00)

`[0=scope, 1=DMM, 2=AWG]` — switches the active UI screen.

## Outstanding / open questions

| issue | topic |
|-------|-------|
| #62 | cmd 0x0c confirmed channel-context selector not RunStop. **docs/hantek-2000-PC-USB-PROTOCOL.md cmd 0x0c entry still says "RunStop" — needs correction** |
| #30 | 74HC595 output bit → relay assignment not yet mapped |
| #43 | 74HC595 Q-output → relay/AFE wiring needs board trace |
| #48 | Timebase divisor table at flash 0x080116a0 not yet extracted |
| #49 | REG 0x17 trigger source bits not fully decoded |
| #56 | Timebase divisor sweep + EDGE_POL polarity + trig_level encoding |
| —   | Real RunStop USB command not yet identified (not cmd 0x0c) |
| —   | `read_allset()` in test script labels p[12] as "RunStop" — should be "chansel" |

## Scripts

### `vendors/hantek/2d82/scripts/hantek_test_protocol.py`

Exhaustive round-trip test suite. 63 tests, all PASSED as of 2026-06-23.

```
python3 vendors/hantek/2d82/scripts/hantek_test_protocol.py
python3 vendors/hantek/2d82/scripts/hantek_test_protocol.py --no-init      # skip init_scope
python3 vendors/hantek/2d82/scripts/hantek_test_protocol.py --skip-capture # skip waveform capture
```

Log: `tmp/hantek_test_protocol.log`

### `vendors/hantek/2d82/scripts/hantek_adc_watch.py`

Cycles through volt/div, coupling, and BW limit settings every 3 seconds. Each `SEND` line in the log has an exact timestamp — listen for relay clicks and match them to the log to determine which USB command triggers which relay.

```
python3 vendors/hantek/2d82/scripts/hantek_adc_watch.py
python3 vendors/hantek/2d82/scripts/hantek_adc_watch.py --interval 5.0
python3 vendors/hantek/2d82/scripts/hantek_adc_watch.py --kill
```

Log: `tmp/hantek_adc_watch.log` (SEND lines only)

Sequence: CH0 volt/div × 11 → CH0 couple × 3 → CH0 BWlimit × 2 → timebase sweep per volt step → same for CH1. ~230 steps at 3s = ~11 minutes.
