# DMM Driver Conversion Rules

This document captures standing rules for the DMM driver conversion work.
These are project rules, not one-off task instructions.

## Scope

- Applies to: `code/a2k_board/include/a2k_board/dmm.h`, `code/a2k_board/src/dmm.c`, and immediate DMM consumers in `code/app`.
- Focus is the onboard DMM path (CS7721 frame transport + decode + typed result API).

## Rules

1. No legacy API shims
- Do not add compatibility wrappers for old scalar-read APIs.
- The typed reading API is the only public read path.

2. Typed output first
- DMM reads must return a structured result suitable for:
  - logging,
  - UI rendering,
  - programmatic validity checks.

3. UART is transport, not text protocol
- USART2 carries CS7721 framed nibble packets.
- Do not redesign this as line-oriented text UART.

4. Protocol-faithful decode, clean implementation
- Keep decode behavior anchored to verified reverse-engineered protocol facts.
- Do not copy decompiler-shaped code directly into the driver API surface.

5. Keep permanent rules separate from one-off tasks
- Transient requests (for a specific test or temporary migration) are not policy.
- Only add here what should remain true for future changes.

## Non-Rules (Examples)

- A one-time bring-up instruction.
- A temporary test harness workaround.
- A request tied to a single debug session.

## References

- `vendors/hantek/2d82/docs/hardware/dmm-cs7721.md`
- `code/a2k_board/include/a2k_board/dmm.h`
- `code/a2k_board/src/dmm.c`
