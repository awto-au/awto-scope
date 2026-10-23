# USB fuzzing hang: cmd 0x16 garbage packet precedes total USB wedge + infinite `fw_calibrate` loop

**Status:** reproduced once, root cause partially confirmed by static analysis, one open question
remains (see "What we could NOT confirm" below). Device was left in the hung state for this
analysis — **not power-cycled, not sent any further USB traffic.**

**UPDATE 2026-07-14 (GH#180):** the central open question below (how does `key_code==4` fire
without a physical press?) has been answered — see GH#180's comment thread and
`ghidra_names_2d82.tsv`. Verdict: it cannot, in software; no firmware path exists that writes
the key-code cell other than the debounced physical GPIO keypad-matrix scan. That investigation
also found and corrected two real errors this doc's reasoning was built on: (1) there is no
rotary encoder/knob on this hardware at all — `fw_encoder_step_read` (referenced throughout
this doc) has been renamed `fw_keypad_code_read`; "encoder" was never accurate, the UI is a
plain keypad with left/right arrow keys. (2) The "keypad-ADC-mux" framing referenced below is
also wrong — there is no ADC-based keypad; it's a plain GPIO row/column matrix scan. Read this
doc's remaining historical narrative with those two corrections in mind — line-by-line text
below is left as originally written for the historical record, not retroactively edited.

## Summary

During `vendors/hantek/2d82/scripts/hantek_usb_fuzz.py` (random bank/flag/cmd/data USB packets against the live
2D82AUTO), iteration #118 sent a `cmd 0x16` (waveform-data-transfer request) with garbage
start/count fields. Five iterations later (#123) every subsequent USB write to the device began
failing with `[Errno 110] Operation timed out`, and stayed that way for the remaining ~300
iterations logged (device still enumerated on the USB bus throughout). Independently, the
firmware's debug UART was found looping forever printing a calibration binary-search debug line,
which traces to `fw_calibrate()` being stuck in its very first convergence step with a
rail-saturated ADC reading.

**This writeup treats these as two distinct findings tied together by one shared root cause: the
firmware's single-threaded main loop got stuck inside `fw_calibrate()`, which is why USB commands
stopped being serviced at all.** The cmd 0x16 garbage packet itself is very likely an unrelated
coincidence for this specific hang — see "Is the cmd 0x16 packet the trigger?" below.

## The trigger packet

```
550a00011697970bca8c
```

Decoded: `55 0a 00 01` header (magic, len=10, bank=0x00, flag=0x01=read/dispatch), `cmd=0x16`
(waveform data transfer, handled by `fw_usb_scope_readback` per
`vendors/hantek/2d82/ghidra/ghidra_names_2d82.tsv:928`), data bytes `97 97 0b ca 8c`.

`fw_usb_scope_readback`'s cmd 0x16 branch (`decompiled_named.c:53033-53084`) computes:

```c
ch_idx = *(short *)(rx_buf + 7) + *(short *)(rx_buf + 5);   // 16-bit wrapping add
```

With this packet: `*(short*)(rx+5)` = bytes `97 97` LE = `0x9797` = -26729 (signed);
`*(short*)(rx+7)` = bytes `0b ca` LE = `0xca0b` = -13813 (signed). Sum wraps to
`ch_idx = 0x61a2` = 24994 — a huge, nonsensical "transfer end offset" (legitimate values are
≤ `0x4b0` = 1200, the waveform buffer length used elsewhere in the firmware, e.g. in
`fw_calibrate`'s own `fw_scope_wave_process` calls).

## What actually happened on the wire (from `tmp/hantek_usb_fuzz.log`)

```
--- [118/500] bank=0x00 flag=0x01 cmd=0x16 ---
OUT 550a00011697970bca8c
 IN 00000000...00 (64B)                          <- all-zero 64-byte reply
--- [119/500] bank=0x00 flag=0x00 cmd=0x1f ---
 IN (timeout)
--- [120/500], [121/500]: normal replies (bank 0x02 cmd 0x08)
--- [122/500] bank=0x00 flag=0x00 cmd=0x17 ---
 IN (timeout)
--- [123/500] bank=0x02 flag=0x01 cmd=0x07 ---
 ERR write: [Errno 110] Operation timed out          <- FIRST write failure
```

From iteration #123 onward, **every** packet (306 of the remaining ~306 logged attempts, through
iteration #429 where the log ends) fails identically on the write itself, not just the read —
the device stopped accepting bulk OUT data at all. `lsusb -d 0483:2d42` still showed the device
enumerated throughout (confirmed separately by the user). No further traffic was sent after the
log ends; the device was left hung.

## Tracing the cmd 0x16 handler (confirmed, no blocking code found here)

Full re-read of `fw_usb_scope_readback`'s cmd 0x16 branch and its callee
`fw_scope_read_captured_samples` (`decompiled_named.c:22263-22311`, address `0x80128f0`):

- If `*g_usb_rx_buf_ptr` (the "bytes sent so far" cursor, aliased `wave_cursor`) is 0 (fresh
  transfer), the firmware calls `fw_scope_read_captured_samples(ch_idx)`. That function is
  **non-blocking** — it polls the FPGA capture-done bit exactly once and returns -1 immediately if
  not ready, in which case the firmware sends a well-formed all-0xff/0 "no data" 12-byte marker.
  This path handles garbage input safely.
- The **all-zero 64-byte reply actually observed** at iteration #118 means the *other* branch ran:
  the FPGA reported capture-done, so the firmware copied up to `min(ch_idx - cursor, 0x40)` = 64
  bytes of (zero-valued, plausible for a grounded front end) sample data and advanced the cursor
  to `0x40`.
- Completion (`cursor` reset to 0, `fw_scope_apply_all(1)` + `fw_set_acq_run_flag(1)`
  re-enabling the scope) only fires when `ch_idx <= cursor`. With `ch_idx = 24994` and only 64
  bytes advanced per packet, that condition can never be met from further 0x16 packets alone at
  this rate, and the fuzzer moved on to other commands immediately — so the waveform-transfer
  state machine is left "stuck mid-transfer" (cursor nonzero, scope acquisition not re-armed).
- **No loop, wait, or blocking call exists anywhere in this branch** — every code path returns
  immediately after `fw_usb_ep_init()`. This state-machine desync, by itself, does not explain a
  full USB-write-timeout wedge; it would just mean the waveform transfer never "completes" and the
  scope stays in a paused/held acquisition state.

**Conclusion on this part: the garbage cmd 0x16 packet leaves the firmware's waveform-transfer
state machine confused (cursor stuck, scope not re-armed), but does not itself contain any
infinite loop that would explain the total, permanent USB wedge observed from iteration #123
onward.**

## The other symptom: `fw_calibrate()` stuck in an infinite binary search

The UART log (`tmp/tio_session.log`) is filled, from some point on, with:

```
************ch_mode = 0, ch = 0,volt = 9,level = 100,adc_value = 65534,avg = -12
```

(the log has no timestamps; it is a raw `tio` capture, so this cannot be time-correlated against
the fuzz log directly). This is `ptr_cali_fmt_search` (`ghidra_names_2d82.tsv:3117`), printed once
per iteration of the innermost binary-search loop in `fw_calibrate()`
(`decompiled_named.c:28161-28404`, address `0x8018c18`). Decoding the loop variables against the
printed values: `local_ac`(pass)=0 (AC path), `uVar6`(ch)=0 (CH1), `local_a8`(step)=9 — i.e. this
is the **very first iteration** of the whole calibration routine, immediately stuck.
`adc_value=65534` = `0xFFFE`, essentially ADC-full-scale — consistent with a channel input that is
railed/floating/disconnected rather than near 0V as the (uncommunicated) calibration procedure
expects.

**Escape condition, confirmed by full re-read of `decompiled_named.c:28301-28378`:** the *only*
way out of this loop is `fw_encoder_step_read() == 3` (line 28336-28342), which aborts via
`fw_cali_setup_scope()` + `return`. There is genuinely **no timeout, iteration cap, or other
host-triggerable abort** — confirmed by reading the full nested `while(true)` loop body; the
convergence check (`fw_abs(local_b0 - iVar8) <= threshold`) is the only other way to advance to
the next step, and it will never fire while `adc_value` stays pinned at `0xFFFE`.

`fw_encoder_step_read()` returning 3 corresponds to the physical **F3 key** (per the already
hardware-confirmed key layout documented on `fw_scope_frame_tick`, `decompiled_named.c:45731-45741`:
"3=F3(save_custom1)" in the general key map, reused here as an "abort calibration" input). This is
a genuine physical-button-only escape — **no USB command can send this abort.**

### How did `fw_calibrate()` start firing without a physical action?

Traced `fw_calibrate`'s one and only call site: `fw_recall_default_settings`
(`decompiled_named.c:38151-38154`), which fires `fw_calibrate()` unconditionally whenever
`fw_scope_param_get_cur() == 4` (menu cursor on the "Calibrate" item) — with **no check for a
fresh keypress at that specific branch**, unlike sibling branches in the same function.

`fw_recall_default_settings` itself is called from exactly one place:
`fw_scope_frame_tick(key_code)` at `decompiled_named.c:45866`, gated on `key_code == 4` (physical
F4). `fw_scope_frame_tick` is called only from `fw_app_main`'s single superloop
(`decompiled_named.c:46802`), where the `key_code`/`bVar3` value is produced **exclusively** by
`fw_encoder_step_read()` (scope/AWG mode) or `fw_dmm_byte_read()` (DMM mode) each loop iteration —
i.e. always a genuine fresh keypad/encoder scan result, never a value replayed from USB. **A fresh
physical F4 press is required to reach this dispatcher at all.**

However, *whether* that F4 press actually triggers `fw_calibrate()` depends entirely on whatever
`fw_scope_param_get_cur()` already equals at that moment — and that value can be **stale, left
over from earlier navigation**, or — critically — **restored via USB** without any fresh physical
menu navigation. See next section.

## Is the menu-cursor variable (`ptr_scope_param_cur`) USB-writable?

**Answer: partially — it is USB-*preservable*, not USB-*settable to an arbitrary value*.**

Searched all 53 call sites of `fw_scope_param_set_cur` in `decompiled_named.c`. All but two are in
physical key-dispatch functions (`fw_scope_frame_tick`, `fw_ch_menu_key_dispatch`, etc.), only
reachable via a real keypress. The two USB-reachable call sites are both inside
`fw_usb_cali_write_handler` (bank 0x03, `cmd[3]=0`, already documented in the TSV):

- `decompiled_named.c:53769`, byte[4]==0,byte[5]==0 ("scope mode select"):
  `cur = fw_scope_param_get_cur1b()` — **restores** a previously-saved value into `cur`.
- The companion save is at `decompiled_named.c:53783`, byte[4]==0,byte[5]==1 ("DMM mode select"):
  `cur1b = fw_scope_param_get_cur()` — **saves** the current `cur` into `cur1b`.
- byte[5]==2 ("AWG mode select") sets `cur` to a fixed constant (9, 0xb, or 10, chosen from packet
  data) — never 4.

`vendors/hantek/2d82/scripts/hantek_test_protocol.py`'s `test_mode_switch()` (run earlier in the same session per the
task context) sends exactly this sequence: Scope→DMM→AWG→Scope, via
`_pkt(0x03, 0x00, 0x00, [mode])`. This round-trip **preserves** whatever `cur` was before it ran
(saves on the way to DMM, restores on the way back to Scope) — **it cannot set `cur` to an
arbitrary value not already present**. No code path was found anywhere that writes USB packet data
directly into `ptr_scope_param_cur`.

**Conclusion:** `cur == 4` reaching the calibrate-fire check is consistent with the menu cursor
having been left on the "Calibrate" item by earlier **physical** navigation (or another firmware
path), and then being faithfully round-tripped/preserved by the bank-3 USB mode-switch commands
that `hantek_test_protocol.py` sent earlier in the session — **not** with the fuzzer's cmd 0x16
garbage packet, and not with any direct USB write of an arbitrary cursor value. This matches
hypothesis (b) from the original investigation brief: the fuzzer's cmd 0x16 packet is most likely
an **unrelated coincidence** for this specific hang.

## Is this one bug or two?

**Two separate, only loosely-related findings:**

1. **cmd 0x16 garbage-input state desync** (`fw_usb_scope_readback`, waveform-transfer cursor vs.
   `ch_idx` never converging): a real firmware robustness bug — the handler does not validate that
   the computed `ch_idx` is within a sane range before using it as a transfer-completion target.
   Confirmed via static analysis to leave the waveform-transfer state machine "stuck mid-transfer"
   (scope acquisition not re-armed). **Does not, by itself, explain the total USB wedge** — no
   blocking code was found in this path.

2. **`fw_calibrate()` infinite binary-search loop with no host-triggerable abort**: a genuine
   design gap — calibration can only be aborted by a physical F3 press, and the reachability gate
   for entering `fw_calibrate()` in the first place (`cur==4` check with no fresh-press
   confirmation at that call site) is looser than the "user explicitly chose Calibrate right now"
   model implied by the old docs. **This is the more likely explanation for the observed total USB
   wedge**: while the main superloop is captive inside `fw_calibrate`'s inner loop, it never reaches
   the `fw_usb_cmd_parser()` poll call at all (confirmed: `fw_app_main`'s only USB-polling call
   sites are in the outer superloop, not inside `fw_calibrate`), so the USB peripheral's OUT
   endpoint backs up and the host's write() eventually times out. This matches the *sustained,
   total* nature of the observed wedge (306 consecutive write failures with zero recovery in the
   log) far better than a per-transfer state-machine confusion would.

**What we could NOT confirm:** whether the fuzz run's cmd 0x16 packet at #118 is what caused
`cur` to be 4 at the moment F4 was (physically or via replay) triggered, versus the cursor having
already been at 4 from the `hantek_test_protocol.py` run alone, prior to the fuzzer starting.
Also unconfirmed: whether a **physical F4 press** actually occurred around iteration #118-123 (the
task states nobody touched the physical UI during the *fuzz* run specifically, which — combined
with the "USB can only preserve, not set, cur=4" finding above — leaves open the question of
**how `fw_scope_frame_tick` was reached with key_code==4 at all** if no physical button was
pressed during the fuzz run. Two possibilities remain, neither fully closed by static analysis
alone: (a) a stray F4 press did occur but wasn't noticed/logged, or (b) there is a still-untraced
path that synthesizes a `key_code==4` event without a physical press — no such path was found in
this pass, but the full keypad-scan/ADC-mux code that populates `g_dat_1768`
(SRAM `0x2000D35C`) was not traced end-to-end (see `fw_encoder_step_read` TSV entry). This should
be flagged for a follow-up hardware-first RE pass on the keypad ADC scan path if the "no physical
input" premise needs to be fully closed out.

## Remote/USB-only triggerability assessment

- The **cmd 0x16 state desync** (finding 1) is 100% USB-triggerable with a single malformed
  packet, no device state prerequisites, no physical access needed. Low-severity on its own (no
  crash, no lockup, just a stuck waveform-transfer state and a paused acquisition), but should be
  fixed by clamping `ch_idx` to the actual buffer/sample-count range before use.
- The **`fw_calibrate` infinite loop** (finding 2) requires the menu cursor to already be on the
  Calibrate item (`cur==4`) — reachable via prior physical navigation, or via the USB bank-3
  mode-switch preserve/restore mechanism *if* `cur` was already 4 — plus (per current evidence) a
  fresh physical F4 press. **It is not confirmed to be triggerable by USB traffic alone with no
  physical input at any point**, though the "how did F4 fire during the fuzz run" question above
  is not fully closed.

## Recovery

**Not tested — hardware was left untouched per instructions.** Based on static analysis:

- The firmware never returns from `fw_calibrate()`'s inner loop except via the F3 abort or
  convergence (which cannot happen with a railed ADC reading) — a **physical F3 keypress** should,
  per the traced code, cause it to call `fw_cali_setup_scope()` and return normally, restoring the
  scope and presumably letting the main loop resume polling USB. This is the first thing to try
  before assuming a power cycle is needed, since it is non-destructive and directly supported by
  the traced abort path.
- If F3 does not recover it (e.g. if the true cause turns out to be something else not captured by
  this analysis), a power cycle should recover the device, since nothing in the traced paths writes
  a "stuck" flag to flash — `fw_calibrate`'s state is all in RAM/FPGA registers and is
  reinitialized by `fw_app_main` on boot. This is an assumption based on the code paths read, not a
  tested fact.
- Do not send further USB traffic to the device while it is in this state — the OUT endpoint
  appears to be backed up already; more writes will just add to the timeout pile without changing
  firmware behavior, since (per this analysis) the firmware isn't polling USB at all right now.

## Suggested firmware-side fix (for upstream/vendor awareness, not actionable by us)

1. Clamp `ch_idx` in the cmd 0x16 handler to a sane maximum (e.g. the known waveform buffer length,
   `0x4b0`) before using it as the transfer-completion target.
2. Give `fw_calibrate`'s binary-search loop an iteration cap or timeout independent of the
   convergence check, so a railed/disconnected input can't hang the main loop forever.
3. Consider gating `fw_calibrate` entry on a genuinely fresh menu-select action (e.g. an edge on
   the "just navigated to this item" transition) rather than the current level-triggered
   `cur==4` check, to close the "stale cursor + USB preserve/restore" reachability gap documented
   above.

## Addendum (2026-07-13): second independent reproduction, rules out cmd 0x16 as a required trigger

During a follow-up "Phase 1" systematic walk (`vendors/hantek/2d82/scripts/hantek_phase1_walk.py`, exercising ONLY
documented-valid, in-range commands per `vendors/hantek/2d82/docs/hardware/usb-protocol.md` — no garbage/out-of-range
packets of any kind), the exact same `fw_calibrate()` stuck-binary-search UART pattern
(`ptr_cali_fmt_search`, "ch_mode=0,ch=1,volt=4,level=-100,adc_value=0,avg=..." repeating) recurred,
confirmed via direct read of `tmp/tio_session.log`: the ADC value converged normally through several
steps (`32767→16383→8191→...→0`) then got stuck oscillating at `adc_value=0` for over 1000
consecutive prints, with the device's USB OUT endpoint going completely unresponsive (write-level
timeouts, not just read-level) — identical total-wedge signature to the original report.

**This occurrence had no cmd 0x16 traffic anywhere in the preceding session.** The command
immediately preceding the hang's onset was a `ZeroCali` (bank 0x00, cmd 0x17, flag=0x00) packet —
but static analysis (see `ghidra_names_2d82.tsv` `fw_usb_bulk_rx` entry) confirms this exact packet
is a **firmware no-op** (no branch matches cmd 0x15/0x16/0x17 in `fw_usb_bulk_rx`'s write-side
dispatch chain), so it cannot be the mechanism. The UART log shows the actual onset landed right
after a normal, successfully-completing `AutoSet` (cmd 0x13 → `fw_scope_autoset()`, confirmed by its
own "ch1 ON ch2 ON!!!!!!!!" completion print immediately preceding the calibration-search prints).
`fw_scope_autoset()` was traced and confirmed to call only the bounded (4-round-capped)
`fw_cali_voltscale_auto` — it has no call path into `fw_calibrate()`.

**Conclusion: the cmd-0x16-garbage packet from the original report was very likely never the actual
trigger for either hang.** Both known occurrences are consistent with `fw_calibrate()` firing
whenever the main loop reaches `fw_scope_frame_tick` with `key_code==4` while
`ptr_scope_param_cur==4` — a state that can arise from stale/leftover menu-cursor position (see
`ptr_scope_param_cur` TSV entry) combined with an as-yet-untraced path that synthesizes or
coincides with a `key_code==4` event. **No physical keypress was performed during either the
original fuzz run or this Phase-1 walk**, per the operators in both cases — the mechanism producing
`key_code==4` without physical input remains the single most important open question blocking full
closure of this bug, more so than either specific preceding USB packet. A third, much larger (11,000+
line) calibration-search run was also found earlier in the same UART capture, spanning a period
before the Phase-1 session's own traffic began — confirming this is not a rare edge case: the
firmware appears to enter `fw_calibrate()` fairly often during ordinary bench/test use of this unit
today, not only under adversarial fuzzing.

**Recommendation for future sessions:** treat `fw_calibrate()` hangs as an ambient risk during ANY
extended USB test session on this unit (not specifically tied to cmd 0x16), and budget for physical
F3-press recovery or SWD reset as a routine possibility, independent of which USB commands were sent.

## Addendum (2026-07-14): third reproduction, confirmed on the OFFICIAL 2021090901 firmware

All prior occurrences documented above were captured while this bench unit was running the
mystery/unofficial firmware build (`2d82-20260713-swd`) -- see `vendors/hantek/2d82/docs/hardware/series.md` and the
SWD investigation earlier in this session for the full story on that build's discovery. The unit
was restored to the confirmed-official `HTX2D82-2021090901.dfu` release via SWD flash
(`vendors/hantek/2d82/scripts/dev.py`) on 2026-07-14, and the same error-flood pattern was observed on the debug UART
during live `vendors/hantek/2d82/scripts/hantek_signal_chain_test.py` / `vendors/hantek/2d82/scripts/hantek_awg_dmm_scope_xcheck.py`
testing shortly after: a long repeating run of `"error data!!!!!!!!!!!!!!!"` and
`"bad end address!!!!!!!!!!!!!"` lines, ending abruptly (`"end end end!!!!!!!!!!!!!!"` observed at
the point of manual interruption).

**This is a new, distinct debug-print signature from the `ptr_cali_fmt_search` binary-search
prints documented above** -- not yet cross-referenced to a specific TSV-named string/function
(follow-up needed: grep `decompiled_named.c`/`ghidra_names_2d82.tsv` for `"error data"` and
`"bad end address"` literal strings, likely `ptr_usb_fmt_error_data`-style entries per this
session's naming convention, to identify which function actually emits these and whether it's the
same `fw_calibrate()` stuck-state or a different failure mode with a similar "wedge" outcome).

**Confirms the bug is genuinely build-independent** -- it is not an artifact specific to the
mystery firmware, and reproduces on the confirmed-official vendor release under ordinary live USB
testing (no fuzzing, no out-of-range packets; this occurred during a structured, in-protocol AWG
+ DMM + scope test sequence). Device recovered on its own without intervention this time (USB
re-enumerated, UART returned to silence) -- unlike the two occurrences above, which required
active recovery (F3 press / SWD reset). Whether self-recovery vs. requiring intervention depends
on which specific stuck state is reached is not yet understood; don't assume self-recovery is
reliable, still budget for manual recovery as the addendum above recommends.

## References

- Trigger packet capture: `tmp/hantek_usb_fuzz.log` (iterations #115-#429)
- UART capture: `tmp/tio_session.log`
- Firmware: `vendors/hantek/2d82/firmware/2d82-20210818/analysis/decompiled_named.c`
  - `fw_usb_scope_readback` (cmd 0x16): lines 52860-53090 approx. (`0x8030f0c`)
  - `fw_scope_read_captured_samples`: lines 22263-22311 (`0x80128f0`)
  - `fw_calibrate`: lines 28161-28404 (`0x8018c18`)
  - `fw_recall_default_settings`: lines 37978-38270+ (`0x8021c44`)
  - `fw_scope_frame_tick`: lines 45744+ (`0x802b364`)
  - `fw_app_main`: lines 46299+ (`0x802bd78`)
  - `fw_encoder_step_read`: lines 37022-37049 (`0x8020db8`)
- TSV entries updated: `ptr_scope_param_cur`, `fw_recall_default_settings`,
  `fw_encoder_step_read`, `fw_usb_scope_readback` (cmd 0x16 comment block), calibration
  comment block ("PATH A" entry correction) — see `vendors/hantek/2d82/ghidra/ghidra_names_2d82.tsv`.
