#!/usr/bin/env python3
"""Phase-1 capture core — the spine of the all-in-one scope redesign.

Circular capture buffer + Schmitt-hysteresis trigger + pre/post-trigger freeze,
the Red-Pitaya "capture fast, freeze, read slow" scheme. Everything else in the
redesign (averager, min/max, compression, AWG, SPI-v2) bolts onto this.

Register-mapped control (the SPI-v2 slave in explore/spi_debug.py bridges these):
  arm → poll status (triggered/frozen) → read wp_trig → read the frozen ring.

Amaranth, self-checked, emits scope_capture.v. This is the general test case
for the open-source flow (Amaranth → yosys → nextpnr-machxo2).
"""
import os
from amaranth import Elaboratable, Module, Signal
from amaranth.lib.memory import Memory
from amaranth.sim import Simulator
from amaranth.back import verilog

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # fpga/
LOG = os.path.join(REPO, "tmp", "scope_capture.log")

EDGE_RISING, EDGE_FALLING, EDGE_ANY = 0, 1, 2


class ScopeCapture(Elaboratable):
    def __init__(self, depth=256, sw=8):
        self.depth = depth
        self.aw = (depth - 1).bit_length()
        self.sw = sw
        # ADC sample stream (one merged channel for the MVP; DDR 2ch is next)
        self.adc        = Signal(sw)
        self.sample_stb = Signal(init=1)      # 1 = new sample this cycle (decimation gates it later)
        # control registers
        self.arm        = Signal()
        self.trig_level = Signal(sw)
        self.trig_hyst  = Signal(sw)
        self.trig_edge  = Signal(2)
        self.post_delay = Signal(16)
        # status registers
        self.armed      = Signal()
        self.triggered  = Signal()
        self.frozen     = Signal()
        self.wp_cur     = Signal(self.aw)
        self.wp_trig    = Signal(self.aw)
        # frozen-buffer readback port
        self.rd_addr    = Signal(self.aw)
        self.rd_data    = Signal(sw)

    def elaborate(self, platform):
        m = Module()
        m.submodules.mem = mem = Memory(shape=self.sw, depth=self.depth, init=[])
        wrp = mem.write_port()
        rdp = mem.read_port(domain="comb")
        m.d.comb += [rdp.addr.eq(self.rd_addr), self.rd_data.eq(rdp.data)]

        wr = Signal(self.aw)
        m.d.comb += self.wp_cur.eq(wr)

        # ── Schmitt trigger with hysteresis + edge select ──────────────────
        sch = Signal()          # in-band comparator state
        sch_p = Signal()        # previous, for edge detect
        hi = Signal(self.sw + 1)
        lo = Signal(self.sw + 2)
        m.d.comb += [hi.eq(self.trig_level + self.trig_hyst),
                     lo.eq(self.trig_level - self.trig_hyst)]
        with m.If(self.sample_stb):
            m.d.sync += sch_p.eq(sch)
            with m.If(self.adc >= hi):
                m.d.sync += sch.eq(1)
            with m.Elif(self.adc <= lo):
                m.d.sync += sch.eq(0)
        rising  = sch & ~sch_p
        falling = ~sch & sch_p
        ev = Signal()
        with m.Switch(self.trig_edge):
            with m.Case(EDGE_RISING):  m.d.comb += ev.eq(rising)
            with m.Case(EDGE_FALLING): m.d.comb += ev.eq(falling)
            with m.Default():          m.d.comb += ev.eq(rising | falling)

        # ── capture write (circular) ───────────────────────────────────────
        cap = Signal()
        m.d.comb += [wrp.en.eq(cap & self.sample_stb),
                     wrp.addr.eq(wr), wrp.data.eq(self.adc)]
        post = Signal(16)

        with m.FSM():
            with m.State("IDLE"):
                m.d.comb += self.frozen.eq(1)
                with m.If(self.arm):
                    m.d.sync += wr.eq(0)
                    m.next = "ARMED"
            with m.State("ARMED"):
                m.d.comb += [self.armed.eq(1), cap.eq(1)]
                with m.If(self.sample_stb):
                    m.d.sync += wr.eq(wr + 1)          # wraps → circular
                    with m.If(ev):
                        m.d.sync += [self.wp_trig.eq(wr), post.eq(self.post_delay)]
                        m.next = "POST"
            with m.State("POST"):
                m.d.comb += [self.armed.eq(1), self.triggered.eq(1), cap.eq(1)]
                with m.If(self.sample_stb):
                    m.d.sync += wr.eq(wr + 1)
                    with m.If(post == 0):
                        m.next = "FROZEN"
                    with m.Else():
                        m.d.sync += post.eq(post - 1)
            with m.State("FROZEN"):
                m.d.comb += [self.triggered.eq(1), self.frozen.eq(1)]
                with m.If(self.arm):
                    m.d.sync += wr.eq(0)
                    m.next = "ARMED"
        return m


# ─────────────────────────────────────────────────────────────────────────
def simulate():
    lines = []
    def out(*a):
        s = " ".join(str(x) for x in a); print(s); lines.append(s)

    DEPTH = 256
    dut = ScopeCapture(depth=DEPTH)

    # ADC pattern: 80 low samples (pre-trigger), a rising edge through the
    # threshold at sample 80, then high. Trigger on the rising edge.
    EDGE_AT = 80
    def adc_at(i):
        return 60 if i < EDGE_AT else 200

    trig_fire = {"i": None}
    froze     = {"i": None}
    wp_trig   = {"v": None}
    async def tb(ctx):
        ctx.set(dut.trig_level, 128); ctx.set(dut.trig_hyst, 8)
        ctx.set(dut.trig_edge, EDGE_RISING); ctx.set(dut.post_delay, 30)
        ctx.set(dut.adc, adc_at(0))
        # arm
        ctx.set(dut.arm, 1); await ctx.tick(); ctx.set(dut.arm, 0)
        prev_trig = 0; prev_frozen = 1
        for i in range(1, 160):
            ctx.set(dut.adc, adc_at(i))
            await ctx.tick()
            t = ctx.get(dut.triggered); f = ctx.get(dut.frozen)
            if t and not prev_trig and trig_fire["i"] is None:
                trig_fire["i"] = i; wp_trig["v"] = ctx.get(dut.wp_trig)
            if f and not prev_frozen and froze["i"] is None and trig_fire["i"] is not None:
                froze["i"] = i
            prev_trig, prev_frozen = t, f
        # read back the frozen ring around wp_trig
        readback = []
        wt = wp_trig["v"] or 0
        for a in range(wt - 4, wt + 4):
            ctx.set(dut.rd_addr, a % DEPTH); await ctx.tick(); await ctx.tick()
            readback.append(ctx.get(dut.rd_data))
        wp_trig["rb"] = readback

    s = Simulator(dut); s.add_clock(1e-8); s.add_testbench(tb); s.run()

    out("═"*70); out(" PHASE-1 CAPTURE CORE — self-check"); out("═"*70)
    tf = trig_fire["i"]; fz = froze["i"]; wt = wp_trig["v"]; rb = wp_trig.get("rb", [])
    # trigger must fire ~1-2 cycles after the edge at EDGE_AT (schmitt+edge latency)
    ok_trig = tf is not None and EDGE_AT <= tf <= EDGE_AT + 3
    ok_wp   = wt is not None and abs(wt - EDGE_AT) <= 3
    ok_froz = fz is not None and tf is not None and (fz - tf) >= 28   # ~post_delay
    # readback: samples before wp_trig low (~60), at/after high (~200)
    ok_rb   = len(rb) == 8 and rb[0] < 128 and rb[-1] > 128
    out(f"  trigger fired at sample {tf} (edge at {EDGE_AT})     {'PASS' if ok_trig else 'FAIL'}")
    out(f"  wp_trig = {wt} (≈ edge {EDGE_AT})                    {'PASS' if ok_wp else 'FAIL'}")
    out(f"  froze {fz-tf if (fz and tf) else '?'} samples after trigger (post_delay 30)  {'PASS' if ok_froz else 'FAIL'}")
    out(f"  ring readback around wp_trig: {rb}")
    out(f"    (pre-trigger low → post-trigger high)              {'PASS' if ok_rb else 'FAIL'}")

    vpath = os.path.join(os.path.dirname(__file__), "scope_capture.v")
    d2 = ScopeCapture(depth=DEPTH)
    v = verilog.convert(d2, name="scope_capture", ports=[
        d2.adc, d2.sample_stb, d2.arm, d2.trig_level, d2.trig_hyst, d2.trig_edge,
        d2.post_delay, d2.armed, d2.triggered, d2.frozen, d2.wp_cur, d2.wp_trig,
        d2.rd_addr, d2.rd_data])
    with open(vpath, "w") as fh: fh.write(v)
    out(""); out(f"── emitted {os.path.relpath(vpath, REPO)} ({v.count(chr(10))} lines) ──")

    allok = ok_trig and ok_wp and ok_froz and ok_rb
    out(""); out(f"OVERALL: {'PASS' if allok else 'FAIL'}")
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    with open(LOG, "w") as fh: fh.write("\n".join(lines) + "\n")
    return allok


if __name__ == "__main__":
    raise SystemExit(0 if simulate() else 1)
