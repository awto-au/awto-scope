# aw2_gateware co-simulation — Renode-attachable FPGA sim

Run the **real** new FPGA gateware (`fpga/hantek/rtl/aw2_gateware.py`, the Amaranth
`Aw2Gateware` with the SPI-v2 slave) as a live simulation that a Renode process —
the STM32 firmware — attaches to over a socket. The firmware drives SPI, the
gateware responds; firmware-in-the-loop with no hardware and no behavioural
re-implementation of the design.

```
 Renode (STM32 SPI1 @ 0x40013000)                scope_sim_server.py
   firmware ── DR write/read ──▶ ScopeSimBridge.cs ──socket──▶ ScopeSim ──▶ amaranth.sim(Aw2Gateware)
             ◀── MISO ─────────                    2B req/1B resp        SPI-v2 + capture core
   PA4 (CS) ─────────────────────▶ (frames tx)                          synthetic ADC edge generator
```

Two things make this the *real* design rather than a stub: the SPI-v2 bytes are
bit-banged into the actual synthesised `Aw2Gateware` (front-end → interleave-cal →
capture core → SPI-v2 register bridge), and an internal ADC generator free-runs a
periodic interleaved waveform with a rising edge so an armed capture genuinely
triggers and freezes.

> Contrast with `code/renode/FpgaSPI.cs`, which models the **OEM** FPGA's protocol
> (`0xa8/0xc8/0xa0/0xa4/0x50`) behaviourally. `Aw2Gateware` speaks **SPI-v2**
> (`0x02` write / `0x03` read, 16-bit auto-incrementing address) — a different
> protocol — so this bridge is separate and holds no protocol state.

## Files

| file | what it is | tested |
|---|---|---|
| `scope_sim_server.py` | socket server running `Aw2Gateware` in `amaranth.sim`; `ScopeSim` + `SpiV2Master` | ✅ |
| `test_scope_sim_server.py` | mock STM32 master over the socket: configure → arm → poll → read window | ✅ green |
| `ScopeSimBridge.cs` | Renode peripheral: SPI1 MMIO + PA4 CS → socket | provided; verify in Renode |
| `scope_sim.repl` | Renode overlay wiring the bridge + CS | provided |

## Socket protocol

Two-byte request, one-byte reply — trivial so the C# bridge is a thin pipe:

| request | reply | meaning |
|---|---|---|
| `[0x01, 0]` | `[0]` | assert CS (begin frame) |
| `[0x02, 0]` | `[0]` | deassert CS (end frame) |
| `[0x03, mosi]` | `[miso]` | clock one SPI byte, full-duplex |
| `[0x04, 0]` | `[0]` | stop the simulation |

## Run it

**Standalone proof (no Renode) — this is the tested path:**

```
python3 fpga/sim/test_scope_sim_server.py
# → STATUS 0b110 (triggered+frozen), window [60,60,200,200,...], OVERALL: PASS
```

**With Renode:**

1. Start the gateware sim server (leave it running):
   ```
   python3 fpga/sim/scope_sim_server.py --port 42001
   ```
2. In your Renode script, load the bridge assembly and the overlay, then boot the
   STM32 machine as usual:
   ```
   i @fpga/sim/ScopeSimBridge.cs           # or: include the compiled assembly
   # ... load your base STM32F103 platform + app.bin ...
   machine LoadPlatformDescription @fpga/sim/scope_sim.repl
   start
   ```
   The firmware's SPI1 traffic now hits the live `Aw2Gateware`.

### CS framing

The SPI-v2 slave resets its command phase on CS high, so the bridge must see CS.
The firmware toggles CS on **GPIOA PA4**; `scope_sim.repl` routes that edge into
the bridge (`gpioPortA: 4 -> scopeSim@0`). If your base platform names the port
differently, adjust that line. (`FpgaSPI.cs` sidesteps CS by decoding the OEM
grammar; SPI-v2's variable-length reads make grammar-only framing fragile, so we
use the real CS pin instead.)

## Status

The Python side (server + gateware + capture) is **fully tested end-to-end** by
`test_scope_sim_server.py`. `ScopeSimBridge.cs` + `scope_sim.repl` are the Renode
integration and are provided ready to wire; the C# side has not been run inside
Renode here, so verify the SPI1/PA4 wiring against your machine on first boot.

Related: awto-2000 #190 (co-sim design note), pluribus #49 (hard-IP models — the
path to co-simulating the *recovered* netlist instead of the redesign).
