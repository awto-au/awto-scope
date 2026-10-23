# 2D82AUTO Comprehensive Test Setup

## Hardware Setup (AWG → CH1 Loopback)

1. **Physical connections:**
   - AWG output → CH1 input
   - Both on the same ground reference
   - Use short, shielded cable (minimizes noise)

2. **Scope configuration:**
   - CH0 (or CH1) set to trigger on rising edge at mid-level
   - V/div appropriate for AWG amplitude (start with 500mV for ±2.5V AWG)
   - Timebase: 10µs/div (adjustable per waveform)

## DMM Probe Connection (separate front-end, NOT auto-connected)

**The DMM has its own physical probe jacks (COM + V/Ω/mA), electrically
separate from the scope CH1/CH2 BNC inputs.** Selecting a DMM mode over USB
(`fw_usb_mode_switch`) only switches which measurement circuit is active
internally — it does **not** connect the DMM input to the scope channels or
to the AWG output. Confirmed 2026-07-14: with nothing physically plugged into
the DMM probe jacks, DC-V and AC-V both read overload/near-zero regardless of
what the scope or AWG are doing, because the DMM input is genuinely floating.

**To measure a scope/AWG signal on the DMM you must physically jumper a wire**
from the signal source (e.g. the AWG output BNC, or a CH1/CH2 probe tip) to
the DMM's V/Ω/mA jack, **and** the DMM's COM jack to the same ground reference
used by the scope/AWG. There is no USB-controllable internal path between the
DMM front-end and the scope/AWG front-end — this is a hardware-level firmware
design (each subsystem has its own CS7721/ADC signal chain), not a
configuration you can enable in software.

## Software Setup

### Option A: TCP Bridge (Recommended for Wine testing)

```bash
# Terminal 1: Start bridge (from awto-2000/)
cd .
/usr/bin/python3 vendors/hantek/2d82/scripts/hantek_usb_tcp_bridge.py --host 0.0.0.0 --port 8899 --log tmp/hantek_bridge.log

# Terminal 2: Run test
cd .
/usr/bin/python3 vendors/hantek/2d82/scripts/hantek_comprehensive_test.py --log tmp/test_output.log
```

### Option B: Direct USB (Linux only)

```bash
# Single command
/usr/bin/python3 vendors/hantek/2d82/scripts/hantek_comprehensive_test.py --use-usb --log tmp/test_output.log
```

## Test Sequence

The script exercises:

1. **Device init** — FW version, serial, HW version
2. **Scope parameters** — V/div, timebase, trigger source/edge/level
3. **AWG control** — Waveform select, frequency, amplitude
4. **Capture** — Start/stop, read waveform data
5. **Loop test** — 5 iterations of capture with readback validation
   - Detects idle pattern (`00 00 ff ff ...` when no trigger)
   - Validates data arrives when AWG triggers scope

## Expected Output

```
[CONNECT] TCP: 127.0.0.1:8899
[OK] TCP connected

[TEST] Query device info
  TX: 55 0a 03 00 0a 00 00 00 00 00
  RX: 7e 09 01 09 09 21 00 00 00 00
  FW Version: 2021090901

[TEST] Set CH0 V/div to index 5
  TX: 55 0a 00 01 00 05 00 00 00 00

[TEST] Start capture (RunStop)
  TX: 55 0a 00 01 0c 01 00 00 00 00

[TEST] Read waveform: start=0, count=64
  TX: 55 0a 00 00 16 00 00 40 00 00
  RX: 12 00 00 00 ff ff 00 00 ff ff 00 00 ...
  Waveform data (first 32 bytes): 00 00 ff ff 00 00 ff ff 00 00 ff ff ...
    → Idle pattern (no trigger yet)

[COMPLETE] All tests passed
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "Device not found" (USB) | Check device plugged in: `lsusb \| grep 0483:2d42` |
| "Connection refused" (TCP) | Start bridge first; check `ss -tnp \| grep 8899` |
| "Idle pattern" always | AWG not connected to CH1, or frequency too low for timebase |
| Data corrupted | Check shielded cable, reduce AWG amplitude, add delay in loop |

## Next Steps

After loopback test validates:
1. Implement native Linux capture tool using this protocol
2. Add FFT analysis of captured waveforms
3. Test DMM mode (voltage/frequency measurements)
4. Validate all 58 DLL exports map to USB commands
