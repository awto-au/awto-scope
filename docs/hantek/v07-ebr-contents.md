# V07 EBR contents + EFB config — recovered (0x72 fence lifted)

The native (pure-Python) bitstream decoder now decodes MachXO2 command `0x72`
losslessly, so the EBR block-RAM initial contents and the EFB config-register
preload that prjtrellis silently truncated are recovered. This document reports
what those previously-BLOCKED contents are.

**Provenance / reproduce**

```
# native decode (includes .bram_init + .efb_block), then load
python3.15t scripts/trellis_unpack.py fpga/v7/FPGA_V07.bin OUT.config
cp fpga/v7/FPGA_V07.bin.config.iomap.tsv OUT.config.iomap.tsv   # IO map is decode-invariant
python3.15t load.py --label V07 --config OUT.config --board boards/aw2-2d82auto
```

Decode: `frames_read=333/333`, `crc_verified=True`, 6 EBR init blocks
(6144 raw 9-bit words), 1 EFB block. DB tables `ebr_init`, `ebr_init_blocks`,
`efb_config` populated for bitstream `V07`.

---

## 1. The six initialized EBRs are full-scale staircase RAMPS (a decimation ladder)

`.bram_init <WID>` maps to the EBR tile whose `EBR.WID` word equals `WID`. The
raw sections list the physical 9-bit words; decoded **at each block's configured
read-port width** (LSB-first packing) every block is a clean, monotonic,
full-scale staircase ramp. There is no captured/noisy data and no
high-resolution waveform anywhere — all six are deterministic ramps.

| EBR tile | WID | mode | read width | logical content | dwell | period |
|----------|-----|------|-----------|-----------------|-------|--------|
| **R6C4**  | 4 | DP8KC   | 2-bit | ramp 0→1→2→3 | 2   | 8   |
| **R6C1**  | 5 | DP8KC   | 2-bit | ramp 0→1→2→3 | 8   | 32  |
| **R6C10** | 6 | DP8KC   | 2-bit | ramp 0→1→2→3 | 32  | 128 |
| **R6C17** | 7 | PDPW8KC | 2-bit | ramp 0→1→2→3 | 128 | 512 |
| **R6C7**  | 8 | DP8KC   | 4-bit | ramp 0→…→15  | 2   | 32  |
| **R6C14** | 3 | DP8KC   | 4-bit | ramp 0→…→15  | 32  | 512 |

("dwell" = number of consecutive logical samples per level; "period" = samples
per full ramp cycle. Physically the ramp levels appear as digit-doubled bytes —
2-bit `0/1/2/3` → `0x00/0x55/0xAA/0xFF`; 4-bit `n` → `n·0x11`, i.e.
`0x00,0x11,…,0xFF` — which is exactly a narrow field replicated across the 9-bit
physical word.)

Two families, each a **geometric dwell ladder**:

- **2-bit ramps** (levels 0–3), dwell **2 → 8 → 32 → 128** (×4 each step),
  across R6C4, R6C1, R6C10, R6C17.
- **4-bit ramps** (levels 0–15), dwell **2 → 32** (×16), across R6C7, R6C14.

The dwell scales monotonically with `WID` inside each family. This is a
programmatically generated set (an HDL `initial`/mem-init generator), not
hand-authored and not captured — a canonical **linear full-scale ramp test
pattern rendered at several decimation / sample-dwell settings**.

### What this answers (previously BLOCKED)

The open question was: *are the row-6 EBRs ADC-capture buffers, AWG waveform
tables, or coefficient/LUT ROMs?* The recovered contents settle it:

- **They are working (read/write) sample buffers pre-loaded with a self-test
  ramp — NOT fixed waveform or coefficient ROMs.** Evidence: (a) content is a
  generic low-bit-depth ramp, not a computed sine/coefficient set; (b) the
  blocks are dual-port with a runtime write port, so the init is transient
  (overwritten in operation); (c) the ramp bit-depth exactly equals the
  configured read-port width (2-bit → 0–3, 4-bit → 0–15), the signature of a
  datapath/DAC linearity + timebase test, not of a stored operational signal.
- **Datapath role:** the six ramps drive known monotone full-scale codes into
  the read-out → DAC/display path at a ladder of effective rates, i.e. a
  **power-on / production self-test** of the ADC→EBR→DAC chain traced in
  `v07-fabric-datapath.md` §4, exercisable without a live analog input. In
  normal capture the ADC write port overwrites them.

### New nuance for the §4 datapath (worth a re-trace)

The recovered **port widths** refine `v07-fabric-datapath.md` §4:

- The six ramp-initialized blocks have **narrow fabric-facing read ports
  (2- or 4-bit)** — too narrow to be the primary source of a 12-bit DAC word on
  their own.
- The **seventh** row-6 EBR, **R6C20 (WID 1)**, is **PDPW8KC × 18-bit** and is
  **left zero-initialized** (no `.bram_init`). §4 already flagged R6C20 as the
  outlier (reaches no output pad, only one clocked on HPBX0100). The width
  recovery adds that R6C20 is also the **only wide (18-bit) block** — i.e. the
  one actually shaped like a real multi-bit sample/coefficient store, and the
  only one Diamond leaves uninitialized (pure runtime buffer).

So the width evidence gently inverts the emphasis: the six narrow ramp blocks
look like **auxiliary / test / control-path RAMs**, while the wide zero-init
R6C20 is the block shaped like the real data buffer. This should be reconciled
against the §4 reachability trace (which had the six reaching output pads and
R6C20 reaching none) — a genuine open item, flagged below.

### Design note: static prefill costs bitstream; an FF generator would not

EBR init is **per-block optional**: Diamond omits all-zero blocks from the
bitstream entirely. The recovery proves it — **R6C20 carries no `.bram_init`**,
while each of the six ramp blocks emits its full physical contents (1024 × 9 ≈
**1152 B**). So the self-test prefill is a deliberate size cost: **~6.8 KB**
(6 × 1152 B) of bitstream that a blank design would not carry. The content
being a ramp is irrelevant to the cost — any non-blank block emits a full block.

The same power-on self-test could instead be **generated at the ADC capture
point by a small FF counter / LFSR**, rolling the ramp into the EBR write path
(or straight down the datapath) on the fly, with no baked-in init. That trades
~6.8 KB of bitstream for a handful of FFs and a mux at the capture input — a
better tradeoff if bitstream size matters. The vendor chose the static prefill
(no extra fabric, no clocking, trivial to author). Worth noting as the
preferred alternative for any of our own derivative designs.

---

## 2. EFB config (command 0x72) — SPI block, DEFAULT preload

Exactly one 0x72 block, at bitstream offset `0x58c0`:

```
.efb_block sel 0x54 flags 0x10 len 10
data: 00 80 00 00 ff 00 00 00 00 00
```

- `sel 0x54` = the **EFB hardened SPI** peripheral (see `pluribus/docs/cmd-0x72.md`).
  **No `0x5e` (Timer/Counter) block** → the EFB timer/counter is not instantiated
  in V07.
- The payload `00 80 00 00 ff 00 00 00 00 00` is the **generic default** — byte-
  identical to the prjtrellis single-feature SPI fuzz default *and* to the other
  vendor bitstream. V07 does **not** carry a custom SPI configuration in the
  bitstream; the EFB SPI is enabled with reset/default control-register values
  (only two non-zero bytes: `0x80` and `0xff`, the reset states of two SPI
  control registers).

### What this answers about the SPI-ident / REG 0x05 path

The stalled question was whether the previously-invisible 0x72 config hid the
SPI ident (REG 0x05) behaviour. **It does not.** The EFB SPI preload is the
stock default, so:

- **The ident/response value is NOT baked into the EFB config registers.** The
  0x72 fence was not hiding a preconfigured ident. The REG 0x05 ident must be
  produced by **fabric logic (± UFM/EBR) at runtime**, not by a bitstream EFB
  register preload.
- This narrows the SPI-ident investigation: we can now **rule out** "hidden EFB
  register preload" as the reason the ident path was opaque. The EFB→fabric
  wiring (17 EFB ports, still hard-IP-only in the netlist, `v07-fabric-datapath.md`)
  remains the place to look.

---

## 3. Boot / sysCONFIG context from the full command stream

The native decoder walks the whole command stream cleanly to `ISC_PROGRAM_DONE`.
Full sequence for V07:

```
LSC_RESET_CRC
LSC_WRITE_COMP_DIC   dict=ff 48 50 28 05 03 30 90     # compressed bitstream
LSC_INIT_ADDRESS
LSC_PROG_INCR_CMP    frames=333 bpf=136               # fabric config (compressed)
CMD_0x72             efb sel=0x54 len=10 (SPI, default)   <- previously invisible
ISC_PROGRAM_USERCODE usercode=0x00000000  (CRC OK)
LSC_PROG_CNTRL0      ctrl0=0xC0000000                 # EBR-write window
6× (LSC_EBR_ADDRESS @0x1800/0x2000/0x2800/0x3000/0x3800/0x4000 + LSC_EBR_WRITE 128 frames)
LSC_PROG_CNTRL0      ctrl0=0x40000000                 # final control reg
ISC_PROGRAM_DONE
```

- **USERCODE = 0x00000000** — no firmware/version stamp in the bitstream (no
  usercode-based version tag to recover).
- **No `ISC_PROGRAM_SECURITY` command** in the config stream → read-protection /
  security is not set via this `.bin`. Security/boot fuses live in the sysCONFIG
  **feature row** (separate NV programming, not present in this fabric `.bin`),
  so re-decoding the `.bin` does **not** newly expose feature-row security bits.
- **CTRL0** ends at `0x40000000` (transiently `0xC0000000` while the six EBR
  blocks are written). Exact CTRL0 bit semantics not decoded here.
- The six `LSC_EBR_WRITE`s (128 frames each) are the six ramp blocks of §1 —
  confirming R6C20 gets no EBR-write pass (its all-zero content is not emitted).

---

## 4. Status: answered vs still open

**Now answered (were BLOCKED on 0x72):**
- EBR contents are recovered and identified: six full-scale ramp self-test
  patterns on a decimation ladder (§1); they are working buffers, not fixed
  waveform/coefficient ROMs.
- EFB config recovered: SPI-only, stock default; the SPI ident is **not** an EFB
  register preload (§2).
- Boot stream recovered: usercode = 0, no SECURITY command, compressed stream,
  CTRL0 final = 0x40000000 (§3).

**Still open (not resolved by the 0x72 recovery):**
- **Exact operational role of the six narrow ramp RAMs vs the wide R6C20**, given
  the port-width evidence pulls against §4's "these six are the DAC waveform
  buffers." Needs a datapath re-trace correlating read-port width with the DAC
  bus stitching (§1 "new nuance").
- **REG 0x05 ident logic** — now known to be fabric/runtime, but the EFB→fabric
  ident datapath is still not in the recovered netlist (hard-IP-only EFB ports).
- **PLL / clock frequencies & ENCA/ENCB phase** — depend on PLL + feature-row
  config, still not decoded (unchanged by this recovery).
- **Feature-row / sysCONFIG security & boot-mode fuses** — not in the fabric
  `.bin`; need the sysCONFIG/JEDEC or a live read.
