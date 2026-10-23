#!/usr/bin/env python3
"""Renode-attachable simulation of the new FPGA design (aw2_gateware, SPI-v2).

Runs the *real* Amaranth `Aw2Gateware` in amaranth.sim and exposes its
SPI-v2 slave over a TCP socket, so a Renode process (running the STM32 firmware)
can drive it byte-for-byte and read back real responses — firmware-in-the-loop
without hardware, and without re-implementing the design behaviourally.

    Renode (STM32 SPI1)  ──socket──▶  this server  ──▶  amaranth.sim(Aw2Gateware)
         MOSI/MISO/CS                 SPI-v2 bit-bang        real gateware + capture

An internal synthetic ADC generator free-runs a periodic interleaved waveform
with a rising edge, so an armed capture actually triggers and freezes — the
attached master only needs to speak SPI-v2 (configure trigger, arm, poll status,
read wp_trig + window), exactly as the standalone aw2_gateware self-test does.

## Wire protocol (socket)

Deliberately trivial so a Renode C# peripheral is a few lines (see
ScopeSimBridge.cs). Each request is 2 bytes, each reply is 1 byte:

    [OP_CS_LOW,  0 ]  -> [0]     assert CS   (begin a frame)
    [OP_CS_HIGH, 0 ]  -> [0]     deassert CS (end a frame)
    [OP_XFER,  mosi]  -> [miso]  clock one SPI byte, full-duplex
    [OP_QUIT,    0 ]  -> [0]     stop the simulation

## Run

    python3 scope_sim_server.py --port 42001        # serve until QUIT/^C
    python3 scope_sim_server.py --port 0            # ephemeral port (prints it)

The companion test (test_scope_sim_server.py) starts this in-process and drives
it as a mock STM32 master, proving the full stack over the socket.
"""
import argparse
import os
import queue
import socket
import sys
import threading

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "rtl"))
from aw2_gateware import Aw2Gateware      # noqa: E402
from amaranth.sim import Simulator        # noqa: E402

# socket opcodes
OP_CS_LOW, OP_CS_HIGH, OP_XFER, OP_QUIT = 0x01, 0x02, 0x03, 0x04

# SPI-v2 (mirrors aw2_gateware.py) — exported for clients
CMD_READ, CMD_WRITE = 0x03, 0x02
R_STATUS, R_CTRL = 0x1C00, 0x1C01
R_WPTRIG = 0x1C04
R_TEDGE, R_TLEVEL, R_THYST = 0x1C10, 0x1C11, 0x1C12
R_POSTDLY = 0x1C18
R_MEASOFF = 0x1F04

# sub-ticks per SPI half-bit — matches the standalone aw2_gateware testbench (T=3)
_T = 3
# synthetic ADC waveform: low for the first half of the period, high for the
# second, repeating — a periodic rising edge for an armed capture to catch.
_ADC_PERIOD, _ADC_EDGE = 160, 80
_ADC_LOW, _ADC_HIGH = 60, 200
_ADC_B_MISMATCH = 6           # +6 on the B interleave slice, so interleave-cal has work to do


def _adc_sample(i):
    pos = i % _ADC_PERIOD
    base = _ADC_LOW if pos < _ADC_EDGE else _ADC_HIGH
    phase = i & 1
    return (base + (_ADC_B_MISMATCH if phase else 0)) & 0xFF, phase


class ScopeSim:
    """Owns the amaranth simulation of Aw2Gateware and a socket front-end.

    The simulation runs as a single amaranth testbench that free-runs the ADC
    generator and, whenever a request is queued by a socket handler thread,
    performs the corresponding SPI action (driving cs_n/sck/mosi, sampling
    miso). Socket threads never touch the sim context — they only exchange work
    through thread-safe queues.
    """

    def __init__(self, depth=256):
        self.dut = Aw2Gateware(depth=depth)
        self.cmd_q = queue.Queue()        # (op, arg, reply_q) from socket -> sim
        self._shutdown = False
        self.sock = None
        self.port = None
        self._sim_thread = None

    # ── amaranth testbench (runs in the sim thread) ──────────────────────────
    async def _tb(self, ctx):
        i = 0

        async def tick():                 # one clock; ADC advances every tick
            nonlocal i
            v, p = _adc_sample(i); i += 1
            ctx.set(self.dut.adc, v)
            ctx.set(self.dut.phase, p)
            await ctx.tick()

        ctx.set(self.dut.cs_n, 1)
        ctx.set(self.dut.stb, 1)
        for _ in range(4):
            await tick()

        while not self._shutdown:
            try:
                op, arg, reply = self.cmd_q.get_nowait()
            except queue.Empty:
                await tick()              # idle: keep the ADC (and capture FSM) running
                continue

            if op == OP_CS_LOW:
                ctx.set(self.dut.cs_n, 0)
                for _ in range(_T):
                    await tick()
                reply.put(0)
            elif op == OP_CS_HIGH:
                ctx.set(self.dut.cs_n, 1)
                for _ in range(_T):
                    await tick()
                reply.put(0)
            elif op == OP_XFER:
                rxv = 0
                for b in range(8):        # mode-0, MSB-first — matches aw2_gateware xfer()
                    ctx.set(self.dut.mosi, (arg >> (7 - b)) & 1)
                    for _ in range(_T):
                        await tick()
                    rxv = (rxv << 1) | ctx.get(self.dut.miso)
                    ctx.set(self.dut.sck, 1)
                    for _ in range(_T):
                        await tick()
                    ctx.set(self.dut.sck, 0)
                reply.put(rxv & 0xFF)
            elif op == OP_QUIT:
                self._shutdown = True
                reply.put(0)

    # ── socket front-end (runs in handler threads) ───────────────────────────
    def _handle(self, conn):
        with conn:
            while not self._shutdown:
                hdr = _recv_exact(conn, 2)
                if hdr is None:
                    break
                op, arg = hdr[0], hdr[1]
                reply = queue.Queue()
                self.cmd_q.put((op, arg, reply))
                try:
                    r = reply.get(timeout=30)
                except queue.Empty:
                    break
                conn.sendall(bytes([r & 0xFF]))
                if op == OP_QUIT:
                    break

    def _accept_loop(self):
        while not self._shutdown:
            try:
                conn, _ = self.sock.accept()
            except OSError:
                break
            threading.Thread(target=self._handle, args=(conn,), daemon=True).start()

    # ── lifecycle ────────────────────────────────────────────────────────────
    def bind(self, host="127.0.0.1", port=42001):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((host, port))
        self.sock.listen(4)
        self.port = self.sock.getsockname()[1]
        return self.port

    def start_background(self):
        """Bind (if needed), start accepting, and run the sim on a daemon thread."""
        if self.port is None:
            self.bind()
        threading.Thread(target=self._accept_loop, daemon=True).start()
        self._sim_thread = threading.Thread(target=self._run_sim, daemon=True)
        self._sim_thread.start()
        return self.port

    def serve_forever(self):
        if self.port is None:
            self.bind()
        threading.Thread(target=self._accept_loop, daemon=True).start()
        self._run_sim()

    def _run_sim(self):
        sim = Simulator(self.dut)
        sim.add_clock(1e-8)
        sim.add_testbench(self._tb)
        sim.run()

    def stop(self):
        self._shutdown = True
        if self.sock is not None:
            self.sock.close()


def _recv_exact(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


class SpiV2Master:
    """Socket client that speaks SPI-v2 to a ScopeSim — the STM32 side.

    Mirrors aw2_gateware.py's frame() grammar: CS-low, CMD, ADDR-hi, ADDR-lo, then
    data (write) or dummy-clocked reads with the FPGA's auto-incrementing address.
    """

    def __init__(self, host="127.0.0.1", port=42001):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))

    def _op(self, op, arg=0):
        self.sock.sendall(bytes([op, arg & 0xFF]))
        return _recv_exact(self.sock, 1)[0]

    def xfer(self, byte):
        return self._op(OP_XFER, byte)

    def write_reg(self, addr, data):
        self._op(OP_CS_LOW)
        self.xfer(CMD_WRITE); self.xfer((addr >> 8) & 0xFF); self.xfer(addr & 0xFF)
        for d in data:
            self.xfer(d)
        self._op(OP_CS_HIGH)

    def read_reg(self, addr, n=1):
        self._op(OP_CS_LOW)
        self.xfer(CMD_READ); self.xfer((addr >> 8) & 0xFF); self.xfer(addr & 0xFF)
        out = [self.xfer(0xFF) for _ in range(n)]
        self._op(OP_CS_HIGH)
        return out

    def quit(self):
        try:
            self._op(OP_QUIT)
        except (OSError, IndexError, TypeError):
            pass

    def close(self):
        self.sock.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=42001,
                    help="TCP port (0 = ephemeral, printed on stdout)")
    ap.add_argument("--depth", type=int, default=256, help="capture depth")
    args = ap.parse_args()

    sim = ScopeSim(depth=args.depth)
    port = sim.bind(args.host, args.port)
    print(f"scope_sim_server: aw2_gateware SPI-v2 on {args.host}:{port} "
          f"(depth={args.depth}) — attach Renode's ScopeSimBridge here", flush=True)
    try:
        sim.serve_forever()
    except KeyboardInterrupt:
        sim.stop()


if __name__ == "__main__":
    main()
