#!/usr/bin/env python3
"""aw2_gateware — front-end + capture + SPI-v2, end-to-end.

Integrates the redesign spine into one synthesizable module and drives the whole
flow over SPI-v2:

    interleaved ADC → interleave-cal (auto) → capture core (trigger+freeze)
                                              ↕ SPI-v2 register bridge
    STM32:  config trigger → arm → poll status → read wp_trig → read window

The end-to-end sim bit-bangs real SPI transactions (mode-0) to arm the scope,
feeds an interleaved ADC stream with a rising edge, polls the status register,
then reads back wp_trig and the frozen window — proving the full stack.

Amaranth, self-checked, emits aw2_gateware.v. Reuses ScopeCapture + InterleaveCal.
"""
import os
import sys
from amaranth import Elaboratable, Module, Signal, Cat, Mux
from amaranth.sim import Simulator
from amaranth.back import verilog

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from scope_capture import ScopeCapture
from interleave_cal import InterleaveCal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = os.path.join(REPO, "tmp", "aw2_gateware.log")

CMD_READ, CMD_WRITE = 0x03, 0x02
P_CMD, P_AHI, P_ALO, P_DATA = range(4)
# register map (SPI-v2, see fpga-spi-v2.md)
R_STATUS, R_CTRL   = 0x1C00, 0x1C01
R_WPTRIG           = 0x1C04
R_TEDGE, R_TLEVEL, R_THYST = 0x1C10, 0x1C11, 0x1C12
R_POSTDLY          = 0x1C18
R_MEASOFF          = 0x1F04


class Aw2Gateware(Elaboratable):
    def __init__(self, depth=256):
        self.depth = depth
        self.aw = (depth - 1).bit_length()
        # interleaved ADC front-end
        self.adc   = Signal(8)
        self.phase = Signal()          # 0=slice A, 1=slice B
        self.stb   = Signal(init=1)
        # SPI-v2
        self.sck = Signal(); self.cs_n = Signal(); self.mosi = Signal(); self.miso = Signal()

    def elaborate(self, platform):
        m = Module()
        m.submodules.ical = ical = InterleaveCal()
        m.submodules.cap  = cap  = ScopeCapture(depth=self.depth)

        # front-end: interleave-corrected sample feeds the capture core
        m.d.comb += [ical.adc.eq(self.adc), ical.phase.eq(self.phase), ical.stb.eq(self.stb)]
        corr8 = Signal(8)
        m.d.comb += corr8.eq(cap.adc)     # placeholder to size; real assign below
        # clamp corrected (signed sw+2) to 8-bit unsigned for the ring
        cval = ical.corrected
        m.d.comb += cap.adc.eq(Mux(cval < 0, 0, Mux(cval > 255, 255, cval)))
        m.d.comb += cap.sample_stb.eq(self.stb)

        # ── config registers (driven by SPI writes) ─────────────────────
        r_edge  = Signal(2)
        r_level = Signal(8, init=128)
        r_hyst  = Signal(8, init=4)
        r_post  = Signal(16, init=30)
        arm_pulse = Signal()
        m.d.comb += [cap.trig_edge.eq(r_edge), cap.trig_level.eq(r_level),
                     cap.trig_hyst.eq(r_hyst), cap.post_delay.eq(r_post),
                     cap.arm.eq(arm_pulse)]

        # ── SPI-v2 slave, bridged to the capture register map ───────────
        def sync2(sig):
            a, b = Signal(), Signal()
            m.d.sync += [a.eq(sig), b.eq(a)]
            return b
        sck = sync2(self.sck); cs = sync2(self.cs_n); mosi = sync2(self.mosi)
        sck_d = Signal(); m.d.sync += sck_d.eq(sck)
        rise = sck & ~sck_d
        cs_act = ~cs

        bitcnt = Signal(3); rx = Signal(8); phase = Signal(2)
        cmd = Signal(8); addr = Signal(16)
        recv = Cat(mosi, rx[:7])

        # read data multiplexer over the flat address space
        rdata = Signal(8)
        with m.If(addr < 0x1000):
            m.d.comb += cap.rd_addr.eq(addr[:self.aw]), rdata.eq(cap.rd_data)
        with m.Elif(addr == R_STATUS):
            m.d.comb += rdata.eq(Cat(cap.armed, cap.triggered, cap.frozen))
        with m.Elif(addr == R_WPTRIG):
            m.d.comb += rdata.eq(cap.wp_trig)
        with m.Elif(addr == R_MEASOFF):
            m.d.comb += rdata.eq(ical.meas_offset[:8])
        with m.Else():
            m.d.comb += rdata.eq(0xA5)

        out_byte = Signal(8)
        with m.If(phase == P_DATA):
            m.d.comb += out_byte.eq(rdata)
        sel = Signal(3); m.d.comb += sel.eq(7 - bitcnt)
        m.d.comb += self.miso.eq(out_byte.bit_select(sel, 1))

        m.d.sync += arm_pulse.eq(0)          # default: no arm this cycle
        with m.If(~cs_act):
            m.d.sync += [bitcnt.eq(0), phase.eq(P_CMD)]
        with m.Elif(rise):
            m.d.sync += [rx.eq(recv), bitcnt.eq(bitcnt + 1)]
            with m.If(bitcnt == 7):
                m.d.sync += bitcnt.eq(0)
                with m.Switch(phase):
                    with m.Case(P_CMD):
                        m.d.sync += [cmd.eq(recv), phase.eq(P_AHI)]
                    with m.Case(P_AHI):
                        m.d.sync += [addr[8:].eq(recv), phase.eq(P_ALO)]
                    with m.Case(P_ALO):
                        m.d.sync += [addr[:8].eq(recv), phase.eq(P_DATA)]
                    with m.Case(P_DATA):
                        with m.If(cmd == CMD_WRITE):
                            with m.Switch(addr):
                                with m.Case(R_CTRL):    m.d.sync += arm_pulse.eq(1)
                                with m.Case(R_TEDGE):   m.d.sync += r_edge.eq(recv)
                                with m.Case(R_TLEVEL):  m.d.sync += r_level.eq(recv)
                                with m.Case(R_THYST):   m.d.sync += r_hyst.eq(recv)
                                with m.Case(R_POSTDLY): m.d.sync += r_post.eq(recv)
                        m.d.sync += addr.eq(addr + 1)
        return m


# ─────────────────────────────────────────────────────────────────────────
def simulate():
    lines = []
    def out(*a):
        s = " ".join(str(x) for x in a); print(s); lines.append(s)

    DEPTH = 256
    dut = Aw2Gateware(depth=DEPTH)
    T = 3

    async def xfer(ctx, byte_out):
        rxv = 0
        for i in range(8):
            ctx.set(dut.mosi, (byte_out >> (7 - i)) & 1)
            for _ in range(T): await ctx.tick()
            rxv = (rxv << 1) | ctx.get(dut.miso)
            ctx.set(dut.sck, 1)
            for _ in range(T): await ctx.tick()
            ctx.set(dut.sck, 0)
        return rxv

    async def frame(ctx, cmd, ah, al, data=(), reads=0):
        ctx.set(dut.cs_n, 0)
        for _ in range(T): await ctx.tick()
        await xfer(ctx, cmd); await xfer(ctx, ah); await xfer(ctx, al)
        for d in data: await xfer(ctx, d)
        got = [await xfer(ctx, 0xFF) for _ in range(reads)]
        ctx.set(dut.cs_n, 1)
        for _ in range(T): await ctx.tick()
        return got

    EDGE_AT = 80
    def slice_at(i):
        base = 60 if i < EDGE_AT else 200
        phase = i & 1
        return (base + (6 if phase else 0)), phase   # B slice +6 mismatch

    res = {}
    async def tb(ctx):
        ctx.set(dut.cs_n, 1); ctx.set(dut.adc, 60)
        for _ in range(4): await ctx.tick()
        # configure trigger over SPI: level=128, hyst=8, edge=rising, post=30
        await frame(ctx, CMD_WRITE, 0x1C, 0x11, data=[128])   # level
        await frame(ctx, CMD_WRITE, 0x1C, 0x12, data=[8])     # hyst
        await frame(ctx, CMD_WRITE, 0x1C, 0x10, data=[0])     # edge=rising
        await frame(ctx, CMD_WRITE, 0x1C, 0x18, data=[30])    # post_delay
        # arm over SPI
        await frame(ctx, CMD_WRITE, 0x1C, 0x01, data=[1])
        # feed the interleaved ADC stream through the edge
        for i in range(160):
            v, p = slice_at(i)
            ctx.set(dut.adc, v); ctx.set(dut.phase, p)
            await ctx.tick()
        # poll STATUS over SPI until frozen (bit2)
        status = 0
        for _ in range(4):
            status = (await frame(ctx, CMD_READ, 0x1C, 0x00, reads=1))[0]
            if status & 0b100: break
        res["status"] = status
        # read wp_trig + a window around it over SPI
        res["wp"] = (await frame(ctx, CMD_READ, 0x1C, 0x04, reads=1))[0]
        wp = res["wp"]
        res["win"] = await frame(ctx, CMD_READ, (wp - 3) >> 8, (wp - 3) & 0xFF, reads=8)

    s = Simulator(dut); s.add_clock(1e-8); s.add_testbench(tb); s.run()

    out("═"*70); out(" aw2_gateware — end-to-end over SPI-v2"); out("═"*70)
    st = res.get("status", 0); wp = res.get("wp"); win = res.get("win", [])
    ok_frozen = bool(st & 0b100)
    ok_wp     = wp is not None and abs(wp - EDGE_AT) <= 3
    ok_win    = len(win) == 8 and win[0] < 128 and win[-1] > 128
    out(f"  SPI-configured trigger, armed over SPI")
    out(f"  STATUS after capture : 0b{st:03b} (armed/trig/frozen)  {'PASS' if ok_frozen else 'FAIL'}")
    out(f"  wp_trig via SPI      : {wp} (≈ edge {EDGE_AT})          {'PASS' if ok_wp else 'FAIL'}")
    out(f"  window via SPI burst : {win}")
    out(f"    (pre-low → post-high, interleave-corrected)          {'PASS' if ok_win else 'FAIL'}")

    vpath = os.path.join(os.path.dirname(__file__), "aw2_gateware.v")
    v = verilog.convert(Aw2Gateware(depth=DEPTH), name="aw2_gateware", ports=[
        dut.adc, dut.phase, dut.stb, dut.sck, dut.cs_n, dut.mosi, dut.miso])
    with open(vpath, "w") as fh: fh.write(v)
    out(""); out(f"── emitted {os.path.relpath(vpath, REPO)} ({v.count(chr(10))} lines) ──")

    allok = ok_frozen and ok_wp and ok_win
    out(""); out(f"OVERALL: {'PASS' if allok else 'FAIL'}")
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    with open(LOG, "w") as fh: fh.write("\n".join(lines) + "\n")
    return allok


if __name__ == "__main__":
    raise SystemExit(0 if simulate() else 1)
