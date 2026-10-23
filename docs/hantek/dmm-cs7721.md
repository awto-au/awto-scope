# CS7721CN-1 DMM chip — interface reference

Chip: CS7721CN-1 (Chipsea / Wuxi China Resources Semico).
Compatible with FS9721_LP3 (Fortune Semiconductor) — same pinout, same protocol.
Datasheets: CS7721 2-page EN | FS9721_LP3 full 32-page EN

---

## Overview

- Σ-Δ ADC with embedded 8-bit MCU, 4000-count, 3¾ digit
- Own 4MHz crystal oscillator (XIN/XOUT pins 61/62) — not clocked by STM32
- Supply: 2.4–3.6V (VDD). Internal charge pump generates VGG (~2×VDD) and VDDA (~3.9V)
- Serial output only (one-way RS232 from chip to STM32)
- Function/range controlled by parallel GPIO inputs (MEA1–4, SELECT, RANGE etc.)
- MEA1–4 etc. driven via **shift register** (not direct GPIO)

---

## STM32 ↔ CS7721 connections

### Serial output — CS7721 → STM32 (one-way)

| CS7721 pin | Signal | STM32 | Confirmed |
|---|---|---|---|
| 64 TXD | RS232 serial data out | PA3 (USART2 RX, 2400 baud) — see `fw_dmm_usart_init` in named.c | [V] |
| 84 ENTX | RS232 output enable (active low) | ? — likely GND-strapped | [?] |

### Control inputs — STM32 → shift register → CS7721

MEA1–4, SELECT, RANGE, HOLD, REL, HZ/DUTY, CAP are **static parallel inputs** on the CS7721.
Logic: floating/open = 1, connected to VSS = 0.

The STM32 does NOT drive these directly. A shift register sits between:

| STM32 pin | Role |
|---|---|
| PE0 | Shift register serial data (bit-bang) — see `fw_dmm_usart_send_byte` in named.c |
| PC4 | Shift register clock — see `fw_dmm_usart_send_byte` in named.c |
| PE6 | SR1 latch/strobe — pulsed after each command byte (HOLD/REL/SELECT/RANGE) |
| PB0 | SR2 latch/strobe — pulsed after MEA/mode byte — see `fw_dmm_bit_send_clk` in named.c |

Two shift registers in series: SR1 carries CS7721 command lines, SR2 carries MEA/mode lines.
Shift register type: 74HC595 (confirmed — TSOP16 on AFE board). U600 outputs are shorted in groups: bits[5:4] and bits[3:1] always move together and are always the complement of bits[7:6]. Effectively two signals drive all 5 optos across the isolation barrier.

CS7721 control pins driven by SR outputs: MEA1–4 (pins 32–29), SELECT (23), RANGE (24), REL (25), HOLD (26). Logic: open/floating = 1, VSS = 0.

---

## MEA1–MEA4 mode truth table

Source: FS9721_LP3 §10.1. Open=1, VSS=0. MEA4=MSB, MEA1=LSB.

| MEA4 | MEA3 | MEA2 | MEA1 | Function |
|---|---|---|---|---|
| 1 | 1 | 1 | 1 | DCV / ACV — SELECT cycles DC↔AC |
| 1 | 1 | 0 | 1 | Ohm / Diode / Cont / Cap — CAP pin selects cap, SELECT cycles others |
| 1 | 0 | 1 | 0 | DCV (no mV range) |
| 1 | 0 | 1 | 1 | DCmV |
| 1 | 0 | 0 | 1 | ACV |
| 1 | 0 | 0 | 0 | 10A DC/AC |
| 1 | 1 | 1 | 0 | mA DC/AC |
| 1 | 1 | 0 | 0 | µA DC/AC |
| 0 | 1 | 1 | 1 | Capacitance |
| 0 | 1 | 0 | 1 | Hz / Duty cycle |
| 0 | 1 | 1 | 0 | ADP input (Additional ADC Positive, pin 10) |
| 0 | 0 | x | x | ADP input (various sub-modes, see §10.2) |

---

## SR byte truth table per DMM mode

Two daisy-chained 74HC595s, both fed from PE0 (data) + PC4 (clock):
- **SR1 / U9** (latch PE6): HOLD/REL/SELECT/RANGE command pins — `fw_dmm_usart_send_byte`
- **SR2 / U600** (latch PB0): MEA1–4 + SELECT via 5 optocouplers across isolation barrier — `fw_dmm_bit_send_clk`

U600/SR2 drives **trigger/pulse inputs** (RANGE, REL, HOLD, SELECT, and one more) via 5 optocouplers — NOT the static MEA1-4 lines. MEA1-4 are set by U9/SR1. SR2 has **3 independent signals** across 7 output pins (QA always 0):

| Group | Pins | Bits | Signal |
|---|---|---|---|
| A | QH + QG | 7:6 | independent |
| B | QF + QE | 5:4 | independent |
| C | QD + QC + QB | 3:2:1 | independent |

SR2 drives CS7721 trigger inputs (RANGE/REL/HOLD/SELECT/Hz-DUTY) via 5 optos — NOT static MEA lines. MEA4:3:2:1 (§10.1) is set by U9/SR1. SR2 shorted groups (code-confirmed): [QH=QG], [QF=QE], [QD=QC=QB], QA=0 always. clk1=transient pulse, clk2=latched idle. 1=opto driven=CS7721 pin LOW. SELECT pulse via `fw_dmm_pulse_bit6` (bit6 U9 ctrl_reg).

| M | Function | U9 SR1 | clk | QH | QG | QF | QE | QD | QC | QB | QA | MEA4:3:2:1 | SEL |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | DC Voltage (range) | `0x14` | clk1 | 1 | 1 | 0 | 0 | 1 | 1 | 1 | 0 | 1111 | — |
| 0 | DC Voltage (range) | `0x14` | clk2 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1111 | — |
| 1 | DC Voltage | `0x1A` | clk1 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 1111 | — |
| 1 | DC Voltage | `0x1A` | clk2 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 1111 | — |
| 2 | AC Voltage | `0x1A` | clk1 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 1111 | ×1 |
| 2 | AC Voltage | `0x1A` | clk2 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 1111 | ×1 |
| 3 | Ohms | `0x10` | — | — | — | — | — | — | — | — | — | 1101 | — |
| 4 | Ohms / Continuity | `0x1C` | — | — | — | — | — | — | — | — | — | 1101 | — |
| 5 | HOLD | `0x16` | clk1 | 1 | 1 | 0 | 0 | 1 | 1 | 1 | 0 | 1111 | — |
| 5 | HOLD | `0x16` | clk2 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 1111 | — |
| 6 | REL | `0x12` | clk1 | 1 | 1 | 0 | 0 | 1 | 1 | 1 | 0 | 1101 | — |
| 6 | REL | `0x12` | clk2 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1101 | — |
| 7 | Diode | `0x10` | — | — | — | — | — | — | — | — | — | 1101 | ×1 |
| 8 | Capacitance | `0x1C` | — | — | — | — | — | — | — | — | — | 1101 | ×1 |
| 9 | AC Current (mA) | `0x1A` | clk1 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 1110 | ×1 |
| 9 | AC Current (mA) | `0x1A` | clk2 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 1110 | ×1 |
| 10 | Init / mA range | `0x0E` | clk1 | 0 | 0 | 1 | 1 | 0 | 0 | 0 | 0 | 1110 | — |
| 10 | Init / mA range | `0x0E` | clk2 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 0 | 1110 | — |

Modes 3,4,7,8: no SR2 clk — trigger pins idle, MEA set by prior transition.

---

## Mode state tracking and SELECT safety

Mode state is held in **`g_dmm_mode_byte`** (SRAM `0x2000D381`). `fw_dmm_mode_dispatch` (`0x08029AB8`) reads this byte and calls the appropriate absolute CS7721 command function — modes are NOT set by counting SELECT pulses, they are set by explicit MEA bit patterns via the shift register.

The SELECT input on CS7721 cycles through modes sequentially (it is a pulse/toggle input per §10.1). The firmware compensates by reading back the CS7721's own RS232 frame before pulsing SELECT:

- `fw_dmm_send_cmd_low` / `fw_dmm_send_cmd_high` check `frame[0]` of the live CS7721 RX buffer (`0x2000D180`) before deciding whether to send a SELECT strobe:
  - bit 3 (AC flag): checked before entering DC current modes (cmd=1, cmd=3)
  - bit 2 (AC+ flag): checked before entering diode/cap modes (cmd=2, cmd=4)
  - If the flag shows the chip is already in the wrong sub-mode, one extra `fw_dmm_pulse_bit6()` strobe is sent to advance it

This is **positional compensation, not a safety interlock**. There is no check on input signal level, no jack-position detection, and nothing prevents the firmware commanding current mode while a voltage source is connected. If `g_dmm_mode_byte` is corrupted or the CS7721 frame is stale, the compensating pulse can land on the wrong mode — potentially switching to current measurement under voltage.

`fw_dmm_validate_gpio_triplet` validates the MEA GPIO pattern against the expected value for `g_dmm_mode_byte` and re-calls `fw_dmm_mode_dispatch` on mismatch — but this re-issues the same mode command, not a safe fallback.

---

## RS232 frame format (CS7721 → USART2/PA3)

Source: FS9721_LP3 §13.

- Direction: CS7721 → STM32 only (one-way)
- Baud: 2400, 8N1
- Frame: 14 bytes
- Each byte: upper nibble = sequence number (1–14), lower nibble = 4 LCD segment bits
- Full frame encodes complete LCD display state (all digit segments + annunciators)
- Sampling: 40kHz. ADC output: 10Hz. After auto-zero: 4Hz

---

## Power and reference pins (board verification)

| Pin | Signal | Typ value | Connect to |
|---|---|---|---|
| 48 VDD | Positive supply | 2.4–3.6V | Battery+ via regulator |
| 44 VSS | Digital GND | 0V | GND |
| 46 AGND | Analog GND | VDD/2 | Internal — decouple only |
| 58 VDDA | Regulated analog out | ~3.9V | Decouple cap only (output) |
| 56 VGG | Charge pump out | ~2×VDD | Decouple cap only (output) |
| 54 CB | Charge pump cap− | — | Cap to CA (pump flying cap) |
| 55 CA | Charge pump cap+ | — | Cap to CB |
| 61 XIN | Crystal in | 4MHz | Crystal |
| 62 XOUT | Crystal out | — | Crystal |
| 89 REFI | ADC reference input | 0.44V rel AGND | R divider from REFO |
| 90 REFO | Bandgap output | 1.25V | Feeds REFI via divider |
| 52 VB | Bias current input | — | R to VSS (sets bias) |
| 59 RLCD | LCD driver adjust | — | R to VSS |

See FS9721_LP3 §15 Diagram 6 (page 15) for the complete reference application schematic
with all component values.

---

## USB DMM status packet (host-facing, post-decode)

The RS232 LCD-segment frame above is decoded by firmware into a compact 14-byte USB status
packet (`fw_usb_dmm_status_pack`, bank `0x01` cmd `0x00` read) — see
[usb-protocol.md](usb-protocol.md) for the full USB command set and
[test-setup.md "DMM Probe Connection"](test-setup.md#dmm-probe-connection-separate-front-end-not-auto-connected)
for how to physically wire a signal into the DMM for testing (its probe jacks are a separate
front-end from the scope/AWG — not USB-switchable).

**Sign/overload byte layout NOT YET CONFIRMED**: byte[4] ("sign/overload flag" per the TSV) stayed
constant across a +1000mV and -1000mV DMM input while the LCD correctly showed the sign, ruling
out "byte[4]=1 means negative". byte[5] toggled between polarities instead — better candidate,
not fully confirmed. See `fw_dmm_mode_xlat` in `ghidra_names_2d82.tsv`.

**Magnitude accuracy confirmed**: DMM digits match AWG DC input to within ~0.2%
(e.g. 1000mV → 0999) — the sign byte is the only unresolved part.
