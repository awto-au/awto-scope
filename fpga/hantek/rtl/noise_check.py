#!/usr/bin/env python3
"""Noise-check / dither cal mode — probe to ground, measure the noise floor.

A self-cal mode: with the input grounded (0 V = mid-scale code), measure the
front-end/ADC noise statistically and report it to the host:

  dc_offset  mean − mid-scale  → the residual offset at 0 V (offset-null target)
  noise_est  mean |x[i]−x[i-1]| → noise RMS proxy (≈1.13·σ for white noise),
             adder-only (no square, no mean-subtract) → cheap on MachXO2
  pk2pk      max − min over the window → peak noise / stuck-bit check
  dither_ok  noise_est ≥ threshold → averaging/high-res mode WILL gain ENOB;
             if 0, the input is too quiet and dither must be ADDED for the
             averager to resolve sub-LSB (quantization needs ≥~1 LSB of noise)

So the same block answers three cal questions: what's the offset at ground,
what's the noise floor, and is there enough dither for the high-res mode.

Amaranth, self-checked (noisy vs quiet), emits noise_check.v.
"""
import os
import math
import random
from amaranth import Elaboratable, Module, Signal, signed, Mux
from amaranth.sim import Simulator
from amaranth.back import verilog

random.seed(3)
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = os.path.join(REPO, "tmp", "noise_check.log")
MID = 128


class NoiseCheck(Elaboratable):
    def __init__(self, sw=8, win_log2=8, dither_thresh=1):
        self.sw = sw; self.win = 1 << win_log2; self.wl = win_log2
        self.thresh = dither_thresh
        self.adc = Signal(sw); self.stb = Signal(init=1)
        self.start = Signal()                    # begin a measurement window
        # results (readable over SPI-v2)
        self.dc_offset = Signal(signed(sw + 1))
        self.noise_est = Signal(sw)
        self.pk2pk     = Signal(sw)
        self.dither_ok = Signal()
        self.valid     = Signal()

    def elaborate(self, platform):
        m = Module()
        accw = self.sw + self.wl + 1
        s   = Signal(accw)                        # sum(x)
        sd  = Signal(accw)                        # sum|x[i]-x[i-1]|
        cnt = Signal(self.wl + 1)
        prev = Signal(self.sw)
        mn = Signal(self.sw, init=(1 << self.sw) - 1); mx = Signal(self.sw)
        run = Signal()

        d = Signal(signed(self.sw + 1))
        m.d.comb += d.eq(self.adc - prev)
        absd = Signal(self.sw + 1)
        m.d.comb += absd.eq(Mux(d < 0, -d, d))

        with m.If(self.start):
            m.d.sync += [run.eq(1), s.eq(0), sd.eq(0), cnt.eq(0),
                         mn.eq((1 << self.sw) - 1), mx.eq(0),
                         prev.eq(self.adc), self.valid.eq(0)]
        with m.Elif(run & self.stb):
            m.d.sync += [s.eq(s + self.adc), sd.eq(sd + absd),
                         cnt.eq(cnt + 1), prev.eq(self.adc)]
            with m.If(self.adc < mn): m.d.sync += mn.eq(self.adc)
            with m.If(self.adc > mx): m.d.sync += mx.eq(self.adc)
            with m.If(cnt == self.win - 1):
                ne = (sd + absd) >> self.wl
                m.d.sync += [
                    self.dc_offset.eq((s >> self.wl) - MID),
                    self.noise_est.eq(ne),
                    self.pk2pk.eq(mx - mn),
                    self.dither_ok.eq(ne >= self.thresh),
                    self.valid.eq(1), run.eq(0)]
        return m


# ─────────────────────────────────────────────────────────────────────────
def simulate():
    lines = []
    def out(*a):
        s = " ".join(str(x) for x in a); print(s); lines.append(s)

    def run_case(sigma, offset):
        dut = NoiseCheck(win_log2=8, dither_thresh=1)
        res = {}
        async def tb(ctx):
            ctx.set(dut.start, 1); await ctx.tick(); ctx.set(dut.start, 0)
            for _ in range(300):
                v = max(0, min(255, int(round(MID + offset + random.gauss(0, sigma)))))
                ctx.set(dut.adc, v); await ctx.tick()
                if ctx.get(dut.valid):
                    res["off"] = ctx.get(dut.dc_offset); res["ne"] = ctx.get(dut.noise_est)
                    res["pp"] = ctx.get(dut.pk2pk); res["dith"] = ctx.get(dut.dither_ok)
                    break
        s = Simulator(dut); s.add_clock(1e-8); s.add_testbench(tb); s.run()
        return res

    out("═"*72); out(" NOISE-CHECK CAL MODE — probe-to-ground self-check"); out("═"*72)
    out(f"  {'input':<26}{'dc_off':>8}{'noise_est':>11}{'~sigma':>8}{'pk2pk':>7}{'dither?':>9}")
    out("  " + "-"*68)
    cases = [("noisy (sigma=3, off=+2)", 3.0, 2),
             ("noisy (sigma=6, off=0)",  6.0, 0),
             ("quiet (sigma=0.3, off=-1)", 0.3, -1)]
    r = {}
    for name, sig, off in cases:
        c = run_case(sig, off); r[name] = c
        sigma_est = c["ne"] * 0.886       # mean|Δ| ≈ 1.128σ → σ ≈ 0.886·mean|Δ|
        out(f"  {name:<26}{c['off']:>8}{c['ne']:>11}{sigma_est:>8.1f}{c['pp']:>7}"
            f"{'YES' if c['dith'] else 'no (add dither)':>9}")

    out("")
    ok = (r[cases[0][0]]["dith"] == 1 and r[cases[1][0]]["dith"] == 1
          and r[cases[2][0]]["dith"] == 0
          and abs(r[cases[0][0]]["off"] - 2) <= 1)
    out(f"  noisy inputs → dither OK (high-res averaging gains ENOB); quiet → flag")
    out(f"  dc_offset tracks the injected ground offset")
    out(f"  → {'PASS' if ok else 'FAIL'}")

    vpath = os.path.join(os.path.dirname(__file__), "noise_check.v")
    d2 = NoiseCheck()
    v = verilog.convert(d2, name="noise_check", ports=[
        d2.adc, d2.stb, d2.start, d2.dc_offset, d2.noise_est, d2.pk2pk,
        d2.dither_ok, d2.valid])
    with open(vpath, "w") as fh: fh.write(v)
    out(""); out(f"── emitted {os.path.relpath(vpath, REPO)} ({v.count(chr(10))} lines) ──")
    out(""); out(f"OVERALL: {'PASS' if ok else 'FAIL'}")
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    with open(LOG, "w") as fh: fh.write("\n".join(lines) + "\n")
    return ok


if __name__ == "__main__":
    raise SystemExit(0 if simulate() else 1)
