# prjtrellis fixes → GitHub issues (target: awtoau/prjtrellis, PUBLIC fork)

Derived from the two-agent review of `libtrellis/src/Bitstream.cpp` + our patches,
grounded in byte-decodes of the real V07/V02 bitstreams. Ranked by severity.
DB-encoding bugs (EBR.MODE F1B33/34, PULLMODE/BASE_TYPE overlap, CIB F24–F27) are
already tracked in pluribus#29 and are NOT duplicated here — these 9 are the
parser/toolchain code fixes not yet filed anywhere.

---

## #A CRITICAL — Decoder silently returns partial config on unknown command
`deserialise_chip` lenient default (Bitstream.cpp:811-822) returns `*chip` on the
first unknown opcode with only a stderr NOTE — no error, no flag. In forced mode
`chip` is always set, so it fires unconditionally and a truncated decode is
indistinguishable from a complete one. **Fix:** add `truncated`/`stop_offset`/
`stop_cmd` to `Chip`, expose via PyTrellis, make callers see it; fail-loud by
default; gate any continuation on evidence the final-frame CRC passed.

## #B CRITICAL — Command 0x72 unmodeled → EBR block-RAM init dropped
After the fabric frames, V07's stream is `0x72 → 6×LSC_EBR_WRITE (~7KB, fully
modelled) → USERCODE → CNTRL0 → DONE`; the lenient stop at 0x72 discards all of
it, so `.bram_init` (ADC/AWG EBR contents) never reaches `.config`. 0x72 is a
standard MachXO2 post-fabric command (present in V07 and V02; leading form
`72 10 54 0a 00 80 00 00`; variable length; likely Feature-Row/FEABITS).
**Fix:** characterize 0x72 length via Diamond oracle, add a consuming case so the
parser continues into the EBR/USERCODE/DONE tail.

## #C HIGH — Forced device skips all geometry validation (wrong-device silent truncation)
Forced mode builds `Chip(force_device)` and never validates against the stream;
`trellis_unpack.py` defaults `TRELLIS_DEVICE=LCMXO2-1200` (333 frames). A -2000
stream (420 frames) fills 333, desyncs, lenient-returns — wrong-geometry chip, no
error. **Fix:** assert/​warn when read frame count ≠ forced device's frame count.

## #D HIGH — pybind11 too old for free-threading → segfault under python3.14t NoGIL
Bundled pybind11 is 2.11.0.dev1 (no free-threading); import crashes in
`PyInit_pytrellis` under python3.14t. **Fix:** upgrade to pybind11 ≥2.13 +
`py::mod_gil_not_used()`. *(In progress — 2.13.6 swapped, module declared, building.)*

## #E MEDIUM — No parser tests; CI is packaging-only
libtrellis ships no `add_test`/pytest and no round-trip test. **Fix:** add a
decode-completeness + byte-exact `read→deserialise→serialise→write` regression on
golden bitstreams.

## #F MEDIUM — Unvalidated frame_count causes desync
`frame_count` from the stream is used unchecked as loop bound and CRAM index
(Bitstream.cpp:677-702); `frame_count==0` skips the trailing CRC → desync.
**Fix:** bounds-check against `num_frames`; handle the zero case.

## #G MEDIUM — assert-only over-read guard compiled out under Release
`get_byte()`/`get_command_opcode()` guard end-of-buffer with `assert` only
(:118,:129); a Release/NDEBUG build removes them → UB on short/corrupt streams.
**Fix:** replace with checks that throw `BitstreamParseError`.

## #H LOW — Unchecked `chip->` derefs before device is known
`LSC_PROG_CNTRL0/1`, `LSC_EBR_ADDRESS`, `LSC_PROG_SED_CRC` deref the `boost::optional`
`chip` without a null-check (:566,:573,:745,:805) — UB if they precede VERIFY_ID
in non-forced mode. **Fix:** null-check with a clear parse error.

## #I LOW — SPI_MODE bad value throws runtime_error, bypassing parse-error path
`SPI_MODE` on an unknown mode byte throws `runtime_error` (:791), not
`BitstreamParseError`, and isn't covered by the lenient/​diagnostic path.
**Fix:** throw `BitstreamParseError` for consistency.
