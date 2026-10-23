## Fuzz-corpus + Diamond-3.14 confirmation of items 1–4 (data-backed, report-only)

Ran the full `diamond-fuzz` corpus (EBR width/mode sweep + the new 352-config
`re_iostd` BASE_TYPE×PULLMODE×DRIVE sweep) and cross-checked against V07. All
four fixable items reproduce with concrete bit addresses. Findings are now
mechanised as a regression suite (`scripts/re_assertions.py`, 8/8 confirmed),
so any future prjtrellis update can be re-checked in one run.

**Item 1 — EBR.MODE wrong bit address → CONFIRMED, decoded.**
Active mode is `F1B33 + F1B34`, **not** the `F1B35` in `EBR1/bits.db`. Verified
across corpus `dp8kc_x1/x2/x4/x9`, `pdpw8kc_x18`, `fifo8kb_x18` — every mode
sets F1B33+F1B34, none sets F1B35. V07's real EBRs carry the matching unknowns
(F1B33 ×3, F1B34 ×4). DP8KC and PDPW8KC set an *identical* bit set
`{F1B33, F1B34}` — no distinguishing bit exists, so the DB genuinely cannot
tell them apart (needs a new differentiator bit, as noted).

**Item 2 — `F1B32` undocumented 3rd mode bit → CONFIRMED.**
V07 emits `F1B32` ×4 alongside the mode bits, in combinations no current enum
matches. Present in the corpus too. Confirms the item-2 hypothesis; still needs
a targeted re-fuzz to enumerate which DATA_WIDTH/port-asymmetry sets it.

**Item 3 — CIB F24–F27 not fuzzed → CONFIRMED (9 unknown bits in V07).**
V07 has 9 distinct unknown bits in frames F24–F27 (e.g. F26B30/39/46/47). A
*single* high-speed pad does **not** trigger these frames — verified: an
isolated output pad decodes to 0 F24–F27 unknowns. The config only appears with
several bonded-adjacent pads carrying high-speed IO together, which matches the
"fuzz adjacent pads" fix. (A `re_edgehs_*` family sweeping N-adjacent-pad buses
at MIPI/SSTL/HSTL/LVDS is now generating this data.)

**Item 4 — IO BASE_TYPE/PULLMODE overlap → CONFIRMED, decoded (the silent-wrong-answer class).**
The full single-ended `re_iostd` sweep shows **60/96 LVCMOS/LVTTL output configs
mis-decode**, and the wrong answer is systematic: `SSTL25_I` is the universal
mis-decode target (42×), with `PCI33` next (12×). Differential `*D` rails are
unaffected (they legitimately map to MLVDS25E/SSTL25D_I). This is worse than a
visible `fan=0` because the decode *looks* valid — a drive-4 LVTTL33 output
silently reads back as SSTL25_I. (Tracked board-side in awto-2000#11.)

**Provenance / boundary:** all of the above is static-bitstream + Diamond-oracle
data — no live-device reads. Report-only; no upstream prjtrellis push without
sign-off (per #19). The `re_assertions.py` suite is the durable check-suite the
items 1/3/4 fixes should extend.
