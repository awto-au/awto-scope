#!/usr/bin/env python3
"""ADC bit-toggle BIST — does every ADC bit actually toggle?

A built-in self-test in the self-cal family: over a measurement window, OR every
sample together and AND every sample together. A bit that toggled was both 1
(shows in OR) and 0 (shows as 0 in AND); a bit that never toggled is stuck.

    toggled = ored & ~anded          (bit seen both high and low)
    stuck   = ~toggled = ~ored | anded

Catches stuck ADC data lines, dead interleave slices, and broken bit-order — the
kind of hardware fault the OEM never checks. Cheap: two 8-bit registers.
Pairs with noise_check (pk2pk=0 ⇒ dead channel) and interleave_cal.

Amaranth, self-checked (healthy vs stuck bit), emits selftest.v.
"""
import os
import random
from amaranth import Elaboratable, Module, Signal

random.seed(4)
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = os.path.join(REPO, "tmp", "selftest.log")


class BitToggleCheck(Elaboratable):
    def __init__(self, sw=8, win_log2=8):
        self.sw = sw; self.win = 1 << win_log2; self.wl = win_log2
        self.adc = Signal(sw); self.stb = Signal(init=1); self.start = Signal()
        self.stuck_mask = Signal(sw)     # bits that never toggled (1 = stuck)
        self.all_ok     = Signal()       # every bit toggled
        self.valid      = Signal()

    def elaborate(self, platform):
        m = Module()
        ored  = Signal(self.sw)
        anded = Signal(self.sw, init=(1 << self.sw) - 1)
        cnt   = Signal(self.wl + 1)
        run   = Signal()
        with m.If(self.start):
            m.d.sync += [run.eq(1), ored.eq(0), anded.eq((1 << self.sw) - 1),
                         cnt.eq(0), self.valid.eq(0)]
        with m.Elif(run & self.stb):
            new_or  = ored | self.adc
            new_and = anded & self.adc
            m.d.sync += [ored.eq(new_or), anded.eq(new_and), cnt.eq(cnt + 1)]
            with m.If(cnt == self.win - 1):
                stuck = ~new_or | new_and
                m.d.sync += [self.stuck_mask.eq(stuck), self.all_ok.eq(stuck == 0),
                             self.valid.eq(1), run.eq(0)]
        return m


def simulate():
    lines = []
    def out(*a):
        s = " ".join(str(x) for x in a); print(s); lines.append(s)

    def run_case(mask_stuck=None, stuck_val=0):
        dut = BitToggleCheck(win_log2=8)
        res = {}
        async def tb(ctx):
            ctx.set(dut.start, 1); await ctx.tick(); ctx.set(dut.start, 0)
            for _ in range(300):
                v = random.randint(0, 255)
                if mask_stuck is not None:            # force some bits stuck
                    v = (v & ~mask_stuck) | (stuck_val & mask_stuck)
                ctx.set(dut.adc, v); await ctx.tick()
                if ctx.get(dut.valid):
                    res["mask"] = ctx.get(dut.stuck_mask); res["ok"] = ctx.get(dut.all_ok)
                    break
        s = Simulator(dut); s.add_clock(1e-8); s.add_testbench(tb); s.run()
        return res

    from amaranth.sim import Simulator          # local import so the helper closes over it
    globals()["Simulator"] = Simulator

    out("═"*68); out(" ADC BIT-TOGGLE BIST — self-check"); out("═"*68)
    healthy = run_case()
    stuck_b3 = run_case(mask_stuck=0b0000_1000, stuck_val=0b0000_1000)   # bit3 stuck high
    stuck_b0 = run_case(mask_stuck=0b0000_0001, stuck_val=0)             # bit0 stuck low
    out(f"  healthy ADC        : stuck_mask=0b{healthy['mask']:08b}  all_ok={healthy['ok']}")
    out(f"  bit3 stuck-high    : stuck_mask=0b{stuck_b3['mask']:08b}  all_ok={stuck_b3['ok']}")
    out(f"  bit0 stuck-low     : stuck_mask=0b{stuck_b0['mask']:08b}  all_ok={stuck_b0['ok']}")
    ok = (healthy["ok"] == 1 and stuck_b3["mask"] == 0b0000_1000
          and stuck_b0["mask"] == 0b0000_0001)
    out(f"  → healthy passes, stuck bits pinpointed  {'PASS' if ok else 'FAIL'}")

    from amaranth.back import verilog
    vpath = os.path.join(os.path.dirname(__file__), "selftest.v")
    d2 = BitToggleCheck()
    v = verilog.convert(d2, name="selftest", ports=[
        d2.adc, d2.stb, d2.start, d2.stuck_mask, d2.all_ok, d2.valid])
    with open(vpath, "w") as fh: fh.write(v)
    out(""); out(f"── emitted {os.path.relpath(vpath, REPO)} ({v.count(chr(10))} lines) ──")
    out(""); out(f"OVERALL: {'PASS' if ok else 'FAIL'}")
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    with open(LOG, "w") as fh: fh.write("\n".join(lines) + "\n")
    return ok


if __name__ == "__main__":
    raise SystemExit(0 if simulate() else 1)
