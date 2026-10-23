#!/usr/bin/env python3
"""Prove the Renode-attachable aw2_gateware sim over its socket — firmware-in-the-loop.

Starts scope_sim_server.ScopeSim in-process, then acts as the STM32 master would:
configure the trigger over SPI-v2, arm, poll STATUS until the capture freezes,
read wp_trig, and read the frozen window around it — asserting the design
actually captured the synthetic edge. This is the same end-to-end path as the
standalone aw2_gateware self-test, but driven entirely through the socket the Renode
bridge speaks, so a green run means Renode can attach and get real behaviour.

    python3 test_scope_sim_server.py        # exit 0 = PASS, 1 = FAIL
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scope_sim_server import (            # noqa: E402
    ScopeSim, SpiV2Master,
    R_STATUS, R_CTRL, R_WPTRIG, R_TEDGE, R_TLEVEL, R_THYST, R_POSTDLY,
)

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # fpga/hantek/
LOG = os.path.join(REPO, "tmp", "scope_sim_server.log")
DEPTH = 256


def run():
    lines = []
    def out(*a):
        s = " ".join(str(x) for x in a); print(s); lines.append(s)

    sim = ScopeSim(depth=DEPTH)
    port = sim.start_background()
    m = SpiV2Master(port=port)

    # configure the trigger over SPI-v2: level=128, hyst=8, edge=rising, post=30
    m.write_reg(R_TLEVEL, [128])
    m.write_reg(R_THYST, [8])
    m.write_reg(R_TEDGE, [0])
    m.write_reg(R_POSTDLY, [30])
    # arm; the server's ADC generator free-runs a periodic edge for it to catch
    m.write_reg(R_CTRL, [1])

    # poll STATUS until frozen (bit2). Each read advances the sim well past one
    # waveform period, so the armed capture triggers and freezes within a few.
    status = 0
    for _ in range(16):
        status = m.read_reg(R_STATUS, 1)[0]
        if status & 0b100:
            break
    wp = m.read_reg(R_WPTRIG, 1)[0]
    start = (wp - 3) % DEPTH
    win = m.read_reg(start, 8)

    m.quit(); m.close(); sim.stop()

    ok_frozen = bool(status & 0b100)
    ok_edge   = len(win) == 8 and min(win) < 128 and max(win) > 128

    out("═" * 70)
    out(" aw2_gateware SPI-v2 sim over socket — Renode-attachable, firmware-in-the-loop")
    out("═" * 70)
    out(f"  server port          : {port}")
    out(f"  STATUS after capture : 0b{status:03b} (armed/trig/frozen)   "
        f"{'PASS' if ok_frozen else 'FAIL'}")
    out(f"  wp_trig via SPI      : {wp}")
    out(f"  window via SPI burst : {win}")
    out(f"    (edge captured: has a <128 and a >128 sample)          "
        f"{'PASS' if ok_edge else 'FAIL'}")

    allok = ok_frozen and ok_edge
    out(""); out(f"OVERALL: {'PASS' if allok else 'FAIL'}")
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    with open(LOG, "w") as fh:
        fh.write("\n".join(lines) + "\n")
    return allok


if __name__ == "__main__":
    raise SystemExit(0 if run() else 1)
