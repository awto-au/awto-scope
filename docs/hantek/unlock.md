# 2000-series models and feature unlock

For hardware BOM, community intel, and full context see [series.md](series.md).

---

## Model matrix

| Model | BW | AWG | `g_fpga_status` bit 6 |
|-------|-----|-----|------------------------|
| 2C42 | 40 MHz | No | 0 |
| 2D42 | 40 MHz | Yes | 1 |
| 2C72 | 70 MHz | No | 0 |
| 2D72 | 70 MHz | Yes | 1 |
| 2D82AUTO | 80 MHz | Yes | 1 |

`g_fpga_status` bit 6 is read live from the FPGA — reflects hardware presence of the AWG DAC.

Model string, RESOLVED 2026-07-15 (was: "model digit mechanism for other variants is unanalysed"): the "MODEL:" row is built by `fw_fpga_status_screen` (`decompiled_named.c:27625-27639`) as two independently-sourced halves.

- **Prefix ("2D"/"2C") — genuine runtime hardware detection.** The function issues a live SPI read (`fw_fpga_read_status()`, `[0xa0][0x00][0xa0]`) immediately before testing bit6 of the fresh FPGA status byte: bit6 set → `"2D"` (AWG board present), bit6 clear → `"2C"` (absent). Same bit6 test as the AWG gate below — this is hardware presence detection, not a firmware SKU lock.
- **Suffix ("42"/"72"/"82") — unconditional compile-time constant, no runtime selection exists.** The `"82"` `fw_strcat` call has no guarding condition; it fires on every invocation. Confirmed by direct raw-byte comparison against the `2d72-20210909` firmware image: this 2D82 image's literal-pool cluster (flash `0x080170BC`-`C8`) contains only `"2C\0"`/`"2D\0"`/`"82\0"`, while the 2D72 image has its own separate, larger cluster (flash `0x08015C90`-`CE4`) containing `"2C\0"`/`"2D\0"`/`"70\0"`/`"72\0"`/`"40\0"`/`"42\0"`/`"XX\0"`. Each model variant ships a **separately-compiled firmware image** with only its own suffix baked in — there is no cross-model runtime suffix-selection code in the 2D82 image to find.

---

## AWG

Gate is `g_fpga_status` bit 6, read live from the FPGA over SPI each time the AWG menu is entered (`decompiled_named.c:38249`). No stored model byte is involved.

The community-documented "model byte flip" mod does not affect this gate. AWG is only available if the FPGA reports the hardware as present (bit 6 = 1). The 3PD5651E DAC + EL5166/LMH6702 amp + Gen-Out BNC must be populated.

---

## Which USB commands were suspected of being gated per-model

The shared firmware handles all of these commands identically regardless of model label:

| USB cmd | Name | What it does in firmware | Model gate? |
|---------|------|--------------------------|-------------|
| `0x0e` | TimeDiv | Raw store to `g_usb_cs7721_buf + 6`, then `fw_apply_capture_mode()` — no clamp, no model check (`decompiled_named.c:49542`) | **None** |
| `0x00`/`0x06` | ChanSel / VoltDiv | Raw store to param table, `fw_scope_limit_set()` reads a flag byte, not a model comparator | **None** |
| `0x02`/`0x08` | CH1/CH2 bandwidth limit | Writes to param table, `fw_apply_ch1/ch2_bandwidth_limit()` sets FPGA REG bit2 (20 MHz analog filter) — no per-model check | **None** |
| AWG menu entry | AWG present check | `fw_awg_present_check()` reads `g_fpga_status` bit 6 (
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   live FPGA GPIO) | **Hardware: DAC populated or not** |

**No USB command sent from the PC has a per-model software limit in firmware.** The firmware does not know which model it is — it has no stored model byte it branches on for capability gating.

## Bandwidth

BW is set by the **analog hardware** — the frontend opamps and ADC path. The firmware has no software minimum divisor gate: `fw_usb_bulk_rx` cmd 0x0e (TimeDiv) stores the raw timebase step with no clamp, and `fw_apply_timebase_div` reads the divisor table without any model comparison.

The model digit (`"82"` at flash `0x080170c4`, displayed by `fw_fpga_status_screen`) **names** the hardware BW. It is not enforced by firmware — it is a label.

The community `-40`→`-70` hex-edit hack changed the displayed model string only. It never affected the divisor table or the FPGA sample rate. The analog frontend hardware did not change, so actual BW did not change. The display and the hardware BW were never coupled in the firmware — the hardware is the gate.

### The non-AUTO build DOES have runtime suffix selection — and it confirms the above (GH#181)

2D82AUTO ships a single hardcoded `"82"`. The **non-AUTO 2D72 build is different**: it carries
seven strings — `2C` `2D` `70` `72` `40` `42` `XX` — and genuinely selects between them at
runtime. Traced in `2d72-20210909` `[fw]`:

```c
str    = g_model_string;                       // SRAM 0x2000c69c
iVar2  = fw_strchr(str, '-');                  // find the dash
memcpy(auStack_68, iVar2 + 1, 2);              // the TWO chars after it

// prefix — genuine hardware detection, same as 2D82AUTO
fw_strcat(out, g_awg_present_flag == 0 ? "2C" : "2D");

// suffix — derived purely from the stored TEXT
if      (fw_strcmp(auStack_68, "70") == 0) fw_strcat(out, "72");
else if (fw_strcmp(auStack_68, "40") == 0) fw_strcat(out, "42");
else                                       fw_strcat(out, "XX");
```

Read carefully, this **strengthens** the conclusion above rather than complicating it:

- `"70"` and `"40"` are **comparison operands**, not outputs. The outputs are `"72"`, `"42"`, `"XX"`.
- The input is the **stored model string in SRAM**, reached via `strchr(str,'-')`. Nothing here
  reads the FPGA, the ADC, the divisor table, or any calibration field. The suffix is a pure
  **string transform on text the unit already stores**.
- **CORRECTED 2026-07-31 — an earlier revision of this section was WRONG.** It claimed editing
  `-40` → `-70` *in flash* would change the displayed model. **A user reported no change, and the
  user is right.** The input `g_model_string` lives in **SRAM** (`0x2000c69c`) and is written **at
  runtime by USB command 9**:

  ```c
  else if (*(char *)(rxbuf + 4) == '\t') {           // command 9
    fw_memcpy_fast(g_model_string, rxbuf + 5, 0x20);  // the HOST writes it
  }
  ```

  read back by that same command. The 2D72 **app** image contains zero occurrences of `2D72`,
  `2D42`, `2C72`, `2C42` — which is what led to the (wrong) conclusion that no flash source
  existed at all.

- **SUPERSEDED IN TURN — there IS a flash source, in the per-unit CAL PAGE.** USB command 9 is an
  *additional* writer of that SRAM buffer, not the only one. The chain closes exactly:

  ```
  fw_cali_volt_data_dump (0x08014A60)
    fw_flash_memcpy(src=0x08059800, len=0xEC, dst=0x2000C5E4)

  serial at flash 0x080598B8 = cal-page offset +0xB8   (inside the 0xEC block)
  0x2000C5E4 + 0xB8          = 0x2000C69C              = g_model_string_rx
  ```

  The 2D72 full-chip dump holds `"CN19010072001598-70"` at `0x080598B8`, surrounded by IEEE-754
  cal words. So editing the ASCII suffix **in the cal page** WOULD change the displayed model —
  that is exactly the community hex-edit (dump via ST-Link, `-40` → `-70`, reflash), and it
  matches their reports. The user's "no change" observation was about editing the **app image**,
  where the string genuinely is absent. Both are true; I had conflated them.

  Full detail:
  [`calibration.md`](hardware-calibration.md#the-voltage-cal-block-also-carries-the-per-unit-serial--and-that-sets-the-model-name).
  The lesson stands: trace the whole chain before claiming an effect — including where the input
  comes from.
- The `"XX"` fallback is the tell: an unrecognised model string yields `2DXX` instead of failing.
  A firmware that gated capability on bandwidth would not shrug at not knowing the bandwidth.

The prefix is the one part that *is* hardware-derived — `g_awg_present_flag` (`0x2000cd13`),
matching 2D82AUTO's FPGA-status bit6 AWG detection.

> Note on structure: these seven strings are **not a lookup table**. They are 4-byte-aligned
> ASCII immediates in a function's **literal pool**, interleaved with SRAM pointers
> (`0x2000cd0d` at `0x08015c8c`, `0x2000c934` at `0x08015c94`). That is why none of them has its
> own literal-pool pointer — a table-shaped reading of `0x08015c90..0x08015cac` would be wrong.

### FPGA clock is not a bandwidth gate (confirmed from V07 bitstream)

All 2D42/2D72/2D82/2C42/2C72 models use the **identical FPGA bitstream**. The FPGA PLL
(EHXPLLJ R0C0, 25 MHz crystal) is fixed at synthesis time (full breakdown in
[model.md](model.md#clock-architecture-confirmed-from-v07-bitstream)):

- CLKOP = 250 MHz, CLKOS = 125 MHz, CLKOS2 = 62.5 MHz
- No CLKDIV primitive in the bitstream
- ADC_ENCA/ENCB driven from the 125 MHz PLL spine — **not runtime-switchable**

There is no clock MUX, no bitstream reload, and no per-model FPGA configuration.
The `fw_apply_ch1/ch2_bandwidth_limit()` functions control a **20 MHz analogue filter** in the
front-end (bit 2 of FPGA REG 0x00/0x01) — separate from and independent of the model BW rating.

Summary: the 40/70/80 MHz specs are analogue bandwidth of the input signal path, determined
entirely by component selection on the PCB.

### The bootloader carries NO per-model difference — direct evidence `[fw]`

A natural theory (and one worth testing) is that the vendor's Windows calibration tool
writes a **per-model bootloader**, letting shipped firmware updates stay generic. The
mechanism half of that is real — official DFU updates write only `0x08005000` upward and
never touch the 20 KB bootloader, and DFU alt-setting `a` (the low 12 KB) is read-only,
so DFU *cannot* touch it. But the conclusion does not follow, and the evidence is now
direct.

`vendors/hantek/2d72/firmware/arm1-20210108/HantekHTX2021010801.bin` is a **combined
bootloader+app image** recovered from a 2019 Hantek-moderator post — what the vendor's
own "program arm1 with a TTL serial port tool" procedure flashes, using the ST Flash
Loader to write the whole chip over UART. Compared byte-for-byte against our 2D82
bootloader pulled off real hardware by SWD:

| Region | Result |
|---|---|
| `0x0000`–`0x342F` (13,360 bytes of code) | **IDENTICAL** |
| `0x3430`–`0x4FFF` (unused tail) | `0x00` pad vs `0xFF` erased flash — the only difference |

A generic Hantek2000-family recovery image and a specific 2D82 unit run **the same
bootloader binary**. If the bootloader encoded the model, these two would differ in code.
They do not.

Combined with everything else ruled out — the divisor table is byte-identical `.data`,
option bytes/OTP are never read, there is no ADC strap, and the BW-limit bit defaults off
with no model test — the only per-model store found anywhere is the **serial string in
the per-unit calibration page** (see
[`calibration.md`](hardware-calibration.md#the-voltage-cal-block-also-carries-the-per-unit-serial--and-that-sets-the-model-name)),
and that feeds a display label.

### Calibration flash as a hidden limiter — ruled out

**Hypothesis:** Factory production could write a per-model capability field into the 0x2f8
MachXO2 cal block that firmware reads on boot but never overwrites.

**Confirmed false.** Full audit of `fw_fpga_write_config` and `fw_fpga_config_restore` shows:

`fw_fpga_write_config` always calls `fw_memset_word(buf, 0x2f8, 0)` first — zeroing the entire
buffer — before filling only three named sub-regions (sel=0/1/2). Any factory-written bytes in
gaps would be destroyed on the first cal save. The 24-byte gap at `+0x1e8..+0x1f7` specifically
is zeroed on every write and silently discarded on every read.

The 0x2f8 block has **no field that is simultaneously (a) readable by firmware and (b) never
overwritten by normal firmware operation**. See [calibration.md](firmware-calibration.md) §1c
for the code-level proof.

---

### Remaining potential investigations

Everything below is genuinely open — no firmware or PC app code has been found that implements
a software capability gate. The 40/70/80 MHz split is analogue hardware. However, these angles
have not been fully closed:

1. **Factory flash slot at `0x0805c000`** (16 bytes, STM32 internal flash)
   Read by `0x08014ac4` in the 2D72 firmware → SRAM `0x2000cb9c`. Format unknown. On the known
   consumer SWD dump this region is 0xFF (erased). A factory-programmed 2D42 unit might have
   non-0xFF data here that the firmware acts on. The read function and its callers have not been
   fully traced. See [calibration.md](firmware-calibration.md) §Factory flash slot.

2. **AFE board hardware differences**
   The 2D42/2D72 distinction may be purely the analogue frontend board (opamp bandwidth, input
   network). No firmware or software change is needed if the board itself limits the signal path.
   A PCB diff between a 2D42 and 2D72 AFE board would settle this.

3. **`g_dmm_mode_index` callers** (SRAM `0x2000D3DC`, always=1 on 2D82AUTO)
   Set from ARM Cortex-M3 PIDR silicon ID (fixed at 0x477 → index 1). But what does index 1
   *gate* downstream? The callers of `g_dmm_mode_index` beyond the info screen have not been
   fully traced. If it gates DMM ranges or scope params for index 0 vs 1+, it could be model-
   discriminating on other hardware.

**Tracking:** GH issue #145.

**Public-web evidence sweep** (2026-07-31, `scripts/hunt_hantek_images.py`): [image-hunt.md](image-hunt.md) — 14 sourced findings, the `-40`→`-70` serial-suffix mechanism, the DFU `06*002Ka` read-only-bootloader descriptor, and the untried leads (eediscuss attachments; `pablogventura/decompilador`'s `HTHardDll.dll` cal-write exports).

---

## 2D82AUTO

112 extra functions — automotive preset UI (scope view presets, not protocol decoders). Identical hardware to 2D72.
