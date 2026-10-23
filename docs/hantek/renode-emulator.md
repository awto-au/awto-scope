# Renode emulation -- 2D82AUTO

State, usage, known issues, and what works. Updated 2026-06-22.

---

## What it does

Runs the vendor firmware (`flash_08000000.bin` stripped to the app at 0x08005000) inside
Renode with a Python peripheral modelling the ILI9341 LCD over FSMC. The emulator:

- Boots to the main scope UI screen and renders the LCD frame to a PNG
- Accepts key presses via the key-matrix hook (no real GPIO scan needed)
- Exposes an FPGA SPI register trace (hooks on SPI functions, logs decoded commands)
- Has a live native GTK window mode (C# `ILI9341FSMC` peripheral, from-source Renode build)

The main control script is `hantek_emu.py` -- one file, no shell scripts.

---

## Renode build

**One build only:** the from-source build at `<mirror>/renode/renode`.

There is no portable build in use. If the path doesn't exist, rebuild:
```
python3 tools/scripts/build_renode.py
```

This also sets up the `openssl-legacy.cnf` workaround (Fedora SHA1 crypto policy fix -- the
from-source build uses SHA1 in its cert chain; Fedora's DEFAULT policy rejects it). The cnf
is written automatically to `.run/openssl-legacy.cnf` on first use.

**Do not pin a version.** Just build from the latest `renode-infrastructure` + latest .NET.

---

## Quick start

All commands run from:
```
cd ./vendors/hantek/2d82/reference/dso2000/scripts/renode
```

| Command | What it does |
|---------|-------------|
| `python3 hantek_emu.py render` | Boot, render LCD to PNG, open in IDE |
| `python3 hantek_emu.py gui` | Open native GTK live window (from-source build) |
| `python3 hantek_emu.py start` | Start persistent session (monitor socket on :3140) |
| `python3 hantek_emu.py stop` | Clean shutdown (kills by PID + port check) |
| `python3 hantek_emu.py --kill` | Force-kill all Renode/btn_panel.py zombies |
| `python3 hantek_emu.py status` | Show running state |
| `python3 hantek_emu.py cmd 'MONITOR_CMD'` | Send monitor command to running session |
| `python3 hantek_emu.py key 5` | Press key code 5 (inject via hook -- see Key Matrix) |
| `python3 hantek_emu.py btn pb5` | Tap GPIO button (GPIO pin direct press) |
| `python3 hantek_emu.py uart` | Print USART1 debug output captured in renode.log |
| `python3 hantek_emu.py dmm [byte]` | Inject CS7721 DMM byte into USART1 RX |
| `python3 hantek_emu.py gain_map` | Step through all 10 V/div gains, log cfg_bytes |
| `python3 hantek_emu.py fpga_trace` | Hook all FPGA SPI functions, log decoded commands |

### Rendered output

- `lcd_render.png` -- raw framebuffer (240x320, top-left origin, 90deg CW from physical)
- `lcd_render_upright.png` -- rotated 90deg CCW, matches what you see on the physical device

---

## Machine platform

The machine is assembled at runtime by `write_machine()` / `write_csharp_machine()` in
`hantek_emu.py`. It writes a combined `.repl` to `.run/machine.repl` (or
`.run/csharp_machine.repl` for the C# path). Sources:

| File | Role |
|------|------|
| `stm32f103_live.repl` | Stock STM32F103 platform (UART/GPIO/timers/etc.) |
| `rcc_fix.repl` | Python RCC model (see below -- critical for boot) |
| `flash_fix.repl` | Python FLASH controller model (see below -- stops log spam crash) |
| `usart1_capture.repl` | **NOT in use** -- USART1 is a real STM32_UART, use `CreateLogAnalyzer` |
| `buttons.repl` | GPIO button hooks for key injection |
| `fpga_spi_stub.repl` | SPI1 stub for FPGA transaction logging (optional, swap in) |
| `smart_lcd.repl` | Python FSMC peripheral (ILI9341 state machine in-peripheral) |
| `ILI9341FSMC.cs` | C# FSMC peripheral with native GTK display (gui mode only) |

The platform is built by:
1. Taking the stock `stm32f103.repl` from the from-source install
2. Stripping unsupported lines -- they cause **silent, total platform load failure** (all
   peripherals fail to register; monitor shows only `sysbus`; no error is ever logged):
   - `i2c*: I2C.*` lines -- F4_I2C type absent from this build
   - `EventInterrupt/ErrorInterrupt -> nvic` lines -- IRQ type mismatch
   - `TxDMARequest -> ...` / `RxDMARequest -> ...` on the CPU block -- `CortexM` in this
     build has no `TxDMARequest`/`RxDMARequest` GPIO properties
3. Stripping the stock `fsmcBank1: Memory.MappedMemory` definition
4. Appending the Python or C# LCD peripheral as `fsmcBank1`

**Why the stripping?** Renode's `LoadPlatformDescription` fails on any unknown property --
but silently (no error in renode.log, no exception shown). The machine is created but has
no peripherals. Always check `peripherals` in the monitor after loading; if only `sysbus`
appears, there is a property/type mismatch in the repl. Building one flat pre-stripped
platform avoids this entirely.

---

## RCC fix -- why the firmware needs it

The firmware's `fw_set_sysclock_72mhz` (0x08052344) loops waiting for:
1. `RCC_CR.HSERDY` (bit 17) -- HSE oscillator ready
2. `RCC_CR.PLLRDY` (bit 25) -- PLL locked
3. `RCC_CFGR.SWS` (bits 3:2) == 0b10 -- clock switched to PLL

The stock SVD-stub RCC returns all-zeros for reads, so all three waits spin forever.

`rcc_fix.repl` is a Python peripheral at 0x40021000 that:
- Tracks CR.HSEON, CR.HSION, CR.PLLON write state
- On RCC_CR read: returns `HSIRDY` and `HSERDY` and `PLLRDY` as copies of their enable bits
- On RCC_CFGR read: mirrors `SW` (bits 1:0) into `SWS` (bits 3:2)

This lets all three waits complete in one polling cycle.

---

## Flash fix -- why the firmware needs it

The firmware calls `fw_flash_erase_and_write` in a tight loop when the settings-dirty flag
(`DAT_0802c754`) is non-zero. Without a Flash model, the STM32F103 SVD stub returns
`FLASH_SR = 0` on every read (not-busy, correct semantics), so the loop exits immediately --
but Renode logs every single read/write as WARNING (unimplemented register). This generates
thousands of log lines per second and eventually crashes Renode (OOM or log overflow).

`flash_fix.repl` is a Python peripheral at 0x40022000 that:
- FLASH_SR (0x0C): returns 0x20 (EOP=1, BSY=0 -- operation complete, not busy)
- FLASH_CR (0x10): returns 0x80 (LOCK=1 -- reset state, unlock not modelled)
- All other registers: return 0, accept writes silently

This suppresses all log spam without changing firmware behaviour.

---

## SysTick fix -- the real story

**The problem:** `fw_delay_us` (0x0800cce4) and `fw_delay_us_raw` (0x0800ccb2) poll
`SYST_CSR.COUNTFLAG` in a tight loop. This works on real hardware. In Renode it can hang.

**Root cause (confirmed from NVIC.cs source):**
- Renode virtual time advances in discrete quanta (default ~100us virtual time per quantum).
- `SYST_CSR` reads clear `COUNTFLAG` but do **not** call `cpu.SyncTime()`.
- `SYST_CVR` reads DO call `cpu.SyncTime()`, triggering a time-sync.
- `LimitReached` (which sets `CountFlag = true`) fires at quantum boundaries.
- If firmware polls only `SYST_CSR` (never `SYST_CVR`), the CPU burns its entire quantum
  in the poll loop. Virtual time only advances at the quantum boundary when the throttle
  sleep fires. With `AdvanceImmediately = false` (default), the throttle sleep blocks the
  CPU host thread -- and SysTick cannot advance while the thread is blocked.

**The fix:** `emulation SetGlobalAdvanceImmediately true` in the resc.
- Source: `MonitorTimeExtensions.cs` -> `SetAdvanceImmediatelyRecursively(MasterTimeSource, true)`
- This removes the host-side throttle sleep from all time sinks (CPU + peripherals).
- Without the throttle, the CPU thread runs at full host speed, quanta complete instantly,
  `LimitReached` fires, `CountFlag` gets set, the poll exits.
- This is the documented fix in Renode's own test suite (see `S32K3XX_STM.robot`).

**DMA2 hooks still needed:** DMA2 CCR1/CNDTR1 return 0 from SVD (not modelled) -- the
poll loops exit immediately but generate thousands of WARNING log lines/second. Hook them
to return without touching the registers. These are NOT a timing issue -- DMA2 genuinely
has no model.

**Hooks installed at startup:**

| Hook address | Function | Why hooked |
|---|---|---|
| `0x0800525c` | `fw_dma_wait` | Polls DMA2 CNDTR1 -- not modelled, generates log spam |
| `0x08005248` | `fw_dma_wait_done` | Same -- DMA done-check variant |
| `0x080052aa` | `fw_dma_blit` | DMA blit trigger + poll |
| `0x08005296` | `fw_dma_blit2` | Second blit path (large fills) |

`fw_delay_us` and `fw_delay_us_raw` are **not hooked** -- they work correctly now that
`SetGlobalAdvanceImmediately true` is set. Removing those hooks was the point.

All hooks: `cpu.PC = cpu.GetRegister(14).RawValue` (return to LR immediately).

---

## USART1 debug output

USART1 (115200 baud, `fw_usart1_send_byte` @ 0x0802e650) is a real `STM32_UART` in the
Renode machine model. The USART1 send function checks `USART1_SR.TXE` (bit 7) before each
byte -- the real model sets TXE after each write, so transmission completes normally.

To capture output, the start resc installs a log analyzer:
```
usart1 CreateLogAnalyzer
```
Output appears in `.run/renode.log` as `[INFO] usart1: char: 0xXX`.
View with: `python3 hantek_emu.py uart`

Note: `usart1_capture.repl` (Python peripheral at 0x40013800) must NOT be loaded --
it conflicts with the existing `STM32_UART` and causes an address overlap that halts the CPU
immediately ("PC does not lay in memory or PC and SP are equal to zero").

---

## DMM chip (CS7721CN-1) fake input

The CS7721CN-1 is an ICL7106-class dual-slope ADC used for the multimeter function.

**Output direction (into firmware):**
CS7721 sends conversion result bytes at **2400 baud** on **USART1 RX (PA10)**.

**CORRECTED 2026-07-14 (GH#180):** the previous version of this doc claimed a "USART1 IRQ
handler" stores the received byte into `DAT_0802bd4c` (SRAM) and uses it as an "encoder/step
value, same path as the rotary encoder" -- both wrong. This device has **no rotary
encoder/knob at all** (confirmed: no ENCA/ENCB phase pair, no TIM encoder-mode config; the UI
is a plain GPIO keypad matrix with left/right arrow keys, see `gpio-pinout.md` and
`ghidra_names_2d82.tsv`'s `fw_keypad_scan_matrix`/`fw_keypad_code_read` entries). USART1's IRQ
(vector 37) is confirmed an unpatched default-stub -- polled, not interrupt-driven, so there
is no "USART1 IRQ handler". `DAT_0802bd4c` (`ptr_dmm_rx_byte` -> `g_dmm_rx_byte`) is written
exclusively by the keypad-matrix scan in `fw_tim4_irq_handler`, unrelated to USART1/CS7721.
How the firmware actually consumes CS7721 bytes received on USART1 was not re-traced as part
of this correction -- open question, see GH#180.

**Command direction (from firmware to CS7721):**
The firmware commands the CS7721 via **bit-bang GPIO** -- NOT via USART1:
- PB0 = data bit
- PB4 = clock

These are invisible to the Renode USART model; no special handling needed.

Command bytes (shadow register at `DAT_0802cf4c` in SRAM):
| Command | Byte |
|---------|------|
| init    | 0x0E |
| hold    | 0x16 |
| range   | 0x14 |
| rel     | 0x12 |
| mode_high | 0x1A |

**Injecting fake DMM bytes in emulation:**
`STM32_UART.WriteChar(byte)` enqueues a byte into the UART receive FIFO and sets RXNE,
triggering the USART1 IRQ handler exactly as if the CS7721 sent it.

```bash
python3 hantek_emu.py dmm 0x1E    # inject byte 0x1E into USART1 RX
python3 hantek_emu.py dmm         # inject default byte (0x0A)
```

**Monitor equivalent:**
```
mach set "h"
usart1 WriteChar 0x1E
```

Note: `mach set "h"` is required on every new monitor connection -- the context is not
persistent across TCP sessions. `hantek_emu.py cmd` does this automatically.

---

## LCD peripheral (Python, `render` mode)

`smart_lcd.repl` implements the ILI9341 FSMC state machine in Python:

```
fsmcBank1 @ 0x60000000 (CMD register -- A16=0)
fsmcBank1 @ 0x60020000 (DATA register -- A16=1, bit 17 of byte addr)
```

State machine:
- CMD write -> captures command byte, resets argument counter `an`
- `0x2C` (RAMWR): sets current pixel position to (xs, ys)
- `0x2A` (CASET): 4 data bytes -> xs, xe
- `0x2B` (PASET): 4 data bytes -> ys, ye
- DATA write under `0x2C`: writes 16-bit RGB565 to framebuffer, advances x, wraps at xe, then y
- Every 0x7FFF pixels: flushes framebuffer to `.run/framebuffer.bin`
- Special sentinel `0xDEAD` to CMD address: forces immediate flush (used by `render` to
  trigger a dump after boot)
- ID read: returns 0x9341 (ILI9341 chip ID) on data reads

The framebuffer is 240x320x2 bytes (RGB565 LE). `render_png()` decodes it to a PIL Image.

---

## Key matrix injection

The keypad is a scanned 5x4 matrix (20 keys). Columns (inputs, pulled-up): **PB6, PB5, PC6, PB12**. Rows (driven low): **PD13, PD12, PC3, PB14, PB13**.

Each (row, col) maps to a bit in the key-scan result word:

| Row (driven) | PB6 | PB5 | PC6 | PB12 |
|---|---|---|---|---|
| row0 (PD13) | 0x1 | 0x2 | 0x4 | 0x8 |
| row1 (PD12) | 0x10 | 0x20 | 0x40 | 0x80 |
| row2 (PC3)  | 0x100 | 0x200 | 0x400 | 0x800 |
| row3 (PB14) | 0x1000 | 0x2000 | 0x4000 | 0x8000 |
| row4 (PB13) | 0x10000 | 0x20000 | 0x40000 | 0x80000 |

**You cannot press keys with plain GPIO button toggles** -- the row is only driven for
microseconds during the scan cycle. A button held on a column pin reads the wrong row.

**How injection works:** A self-removing hook at `0x08030556` (the return of the key-scan
function) forces `r0` to the desired key code for one scan, then removes itself:
```python
hantek_emu.py key 5   # press code 5 once
```

Key codes 1-20 correspond to matrix positions; the mapping to labelled front-panel buttons
still needs verification against physical hardware (see issue #18, #14).

To press multiple keys in sequence:
```
python3 hantek_emu.py cmd 'emulation RunFor "0.5"'   # run a moment between keys
python3 hantek_emu.py key 5
python3 hantek_emu.py key 8
```

---

## FPGA SPI trace

`fpga_spi_stub.repl` has a stub (currently `SPI.SPILoopback`) -- the full Python decode
peripheral is described in its header but not yet wired to a log file in the .repl itself.
`hantek_emu.py` implements a function-hook-based trace instead:

`_FPGA_HOOKS` (in `hantek_emu.py`) lists all `fw_fpga_*` entry points with argument formats.
`python3 hantek_emu.py fpga_trace` installs hooks on all of them and logs decoded calls
to `.run/fpga_spi.log`.

This is more useful than a byte-level SPI tap because it captures the decoded function call
(e.g. "fw_fpga_ch1_frontend(cfg_byte=0x2a)") rather than raw SPI bytes.

---

## Gain map

`python3 hantek_emu.py gain_map` steps through all 10 V/div gain settings (CH1) by:
1. Poking the V/div index byte directly into the scope_params struct (0x2000cdac + 0x42)
2. Running 50ms emulated to let the firmware's periodic update pick it up
3. Reading back the captured `cfg_byte` from a scratch SRAM cell (0x20000010)
4. Writing a summary to `.run/gain_map.txt`

cfg_byte bit layout: see `fw_fpga_ch1_frontend` in named.c.

Output is also printed to stdout. Use this to verify any AFE relay theory against emulation.

---

## What works (validated 2026-06-22)

| Feature | Status |
|---------|--------|
| `render` (PNG capture) | [V] Works -- full UI screen rendered |
| `gui` (native GTK window) | [V] Works -- ILI9341FSMC.cs compiles, live display runs |
| `start/stop/status/--kill` | [V] Works -- clean process management, zombie detection via /proc |
| `fpga_trace` | [V] Works -- 28 FPGA transactions captured at boot |
| `key` injection | [V] Works |
| `btn` GPIO press | [V] Works |
| USART1 capture | [V] Works -- `CreateLogAnalyzer` captures TX bytes to renode.log |
| `dmm` fake input | [V] Works -- `usart1 WriteChar` injects bytes into USART1 RX FIFO |
| SysTick delays | [V] Fixed -- `SetGlobalAdvanceImmediately true` (no hooks needed) |
| Flash write loop | [V] Fixed -- `flash_fix.repl` suppresses log spam |
| `gain_map` | [!] Partial -- hook fires and captures real cfg_byte, but V/div poke doesn't trigger re-send |

## Observability toolkit (2026-07-18)

All work by hooking firmware functions and/or injecting into SRAM cells the firmware already polls
— no extra peripherals modelled. Output goes to `code/renode/captures/` (gitignored, regenerable).

| Command | What it does |
|---------|--------------|
| `key <n>` | **Keypad injection (WORKS).** Hooks `fw_keypad_code_read` (0x08020db8) entry and forces its return from inject cell 0x2000dffc, bypassing the debounce that defeated SRAM pokes. Verified: key 7 -> DMM (ui_mode 3->1). Note: keys are context-dependent (key 12=BACK reaches scope/ui_mode 0; from DMM the keypad is the DMM softkey handler). |
| `cali [secs]` | Enters DMM mode + triggers the real `fw_calibrate` (key 12 -> scope, cursor=4, key 4). Logs USART1 cal telemetry (ZeroCaliFlag, the volt_cali_data table) + CS7721 DMM pin traffic (SR1/SR2 command bytes) + FPGA/DAC writes -> `cali_trace.log`. |
| `uilog [secs] [keys]` | **Symbolic UI trace.** Hooks the text/glyph draw calls, dereferences the C-strings, logs `fn(x,y) "text"` merged with the UART telemetry -> `ui_trace.log`. Fast bulk `sysbus ReadBytes` readback. |
| `pinmon [secs]` | Watches the still-unresolved FPGA regs + GPIO (tb_div, read_ident, trig_mode/band, PB15/probe-comp) — logs live args + calling PC -> `pinmon.log`. Halt-and-log. |
| `usb <bank> <flag> <cmd> [data]` | **Fake USB port.** Writes a 10-byte packet to g_usb_cmd_buffer @0x2000d3cd + raises ready flag @0x2000d3cb=0xff; the firmware's own `fw_usb_cmd_parser` consumes it. Verified: `usb 0 1 0x13` (AutoSet) consumed + processed. Banks 0=SCOPE 1=DMM 2=AWG 3=CALI. |
| `fpgamon [secs]` | Hooks the FPGA SPI port (cs_low/spi_send_n/spi_recv_n/read_status) + the JTAG/ISC MachXO2 programming path (isc_write_cfg/reset/prog_rti/send_page/prog_done) -> `fpgamon.log`. Captured 11k runtime SPI accesses (2+1+1+status poll pattern). |

**Cal convergence caveat:** the FPGA/ADC returns zeros, so `fw_calibrate`'s binary search satisfies
the zero-target trivially and does not run the full sweep (only ~1 DAC write). To exercise the full
search + the ISC programming persist path, the FPGA stub needs a DAC->ADC transfer model.

## Known blockers / not yet working

| Issue | Detail |
|-------|---------|
| DMA2 Channel 1 not modelled | LCD DMA transfers are no-ops (hooks); visually OK but timing wrong |
| SPI1 FPGA response not modelled | `fpga_spi_stub.repl` is a SPILoopback stub; FPGA reads return zeros. Firmware proceeds (no boot-halt on ident mismatch). |
| Settings loop in GUI mode | Firmware loops on `fw_ui_draw_poweroff_dialog` because `DAT_0802c754` (settings-dirty flag) reads as 0x01 from uninitialized SRAM. No fix yet -- needs SRAM preload of valid settings struct. |
| Key matrix physical->code map incomplete | Key codes 1-20 are known; physical front-panel button -> code mapping needs HW verification |

---

## File map

```
vendors/hantek/2d82/reference/dso2000/scripts/renode/
+-- hantek_emu.py          ONE control script (render/gui/start/stop/key/btn/uart/dmm/gain_map/fpga_trace)
+-- app.bin                Vendor firmware stripped to app (0x08005000, 320KB)
+-- rcc_fix.repl           Python RCC model -- REQUIRED for boot
+-- flash_fix.repl         Python FLASH controller -- suppresses SVD log spam
+-- buttons.repl           GPIO button hooks
+-- stm32f103_live.repl    Stock STM32F103 platform description
+-- smart_lcd.repl         Python FSMC/ILI9341 peripheral (render mode)
+-- ILI9341FSMC.cs         C# FSMC display peripheral (gui mode -- needs from-source Renode)
+-- fpga_spi_stub.repl     SPI1 stub (loopback -- not yet a full decode peripheral)
    (build from source: tools/scripts/build_renode.py -- also sets up the openssl workaround)
+-- decode_pins.py         Decodes GPIO CRL/CRH register dumps to pin function table
+-- map_keys.py            Key matrix analysis helpers
+-- try_keys.py            Interactive key-press sequencer
+-- lcd_render.png         Last render output (raw orientation)
+-- lcd_render_upright.png Last render output (display orientation, 90deg CCW rotated)
+-- .run/                  Runtime scratch (generated, gitignored)
    +-- machine.repl       Generated combined platform (Python peripheral path)
    +-- csharp_machine.repl Generated combined platform (C# path)
    +-- load.resc          Generated Renode startup script
    +-- framebuffer.bin    Live ILI9341 framebuffer (240x320x2 RGB565)
    +-- renode.log         Renode stdout/stderr from last session
```

---

## What was painful (so you don't repeat it)

**The clock-switch spin.** Without `rcc_fix.repl`, the firmware hangs at 0x080523ee waiting
for SWS==PLL. Nothing in the firmware reaches GPIO init. The fix is the RCC Python model.
Symptom: emulation runs forever with no GPIO activity, no LCD output.

**The `using`-override silent failure.** Renode's `using` keyword is supposed to let you
override a single peripheral. It un-registers the original but does NOT register a replacement
when the replacement type is missing. The result is a silently empty bus region -- reads return
0, writes go nowhere, no error. The machine appears to load fine but the LCD peripheral is
gone. Fix: build one flat `.repl` with the replacement inline.

**Silent platform load failure (the strip list).** Three categories of property/type mismatch
cause `LoadPlatformDescription` to abort with NO visible error -- not in stderr, not in the
Renode log at INFO level. The only symptom is `peripherals` showing only `sysbus`. Check
`.run/renode.log` for `[ERROR]` lines if you suspect this. Properties stripped from
`stm32f103.repl`:

1. `i2c*: I2C.STM32F4_I2C` -- type absent from this build
2. `EventInterrupt/ErrorInterrupt -> nvic` directives -- IRQ signal type mismatch
3. `TxDMARequest -> ...` / `RxDMARequest -> ...` on the CPU block -- `CortexM` in this build
   has no `TxDMARequest`/`RxDMARequest` GPIO properties

If you update the from-source Renode build and the machine stops loading, run the repl through
the monitor manually to find the new offending line:
```bash
python3 hantek_emu.py cmd 'machine LoadPlatformDescription @.run/machine.repl'
```
The monitor will print the exact E13 error and the line number.

**IronPython hook API is not plain Python.** Renode hooks run in IronPython with a very specific scope. Things that look like Python but aren't:

1. `self.Machine['sysbus']` -> **wrong** (no `self`, no bracket access). Use `machine.SystemBus`.
2. `self.DebugLog(msg)` -> **wrong**. Use `cpu.Log(LogLevel.Warning, msg)` -- writes to `renode.log`.
3. `self.GetRegisterUnsafe(r) & 0xff` -> **crash** -- returns a C# `RegisterValue` struct, not an int. Use `cpu.GetRegister(r).RawValue & 0xff`.
4. `cpu.SetRegisterUnsafe(0, n)` where `n` is a Python int -> **wrong type**. Use `cpu.SetRegisterUnsafe(0, System.UInt32(n))`.
5. `self.Machine['sysbus.cpu'].RemoveHooksAt(addr)` -> **wrong**. Use `cpu.RemoveHooksAt(addr)`.
6. `GetRegisterUnsafe` is deprecated -> use `GetRegister` (note: no `Unsafe` suffix).

Hook scope variables available: `machine`, `cpu`, `self` (the peripheral -- only useful for Python peripherals, not CPU hooks), `sysbus`, `LogLevel`.

Symptom of wrong API: Renode crashes with `IronPython.Runtime.Exceptions.TypeErrorException` and `core dumped`. It doesn't print a useful error to stdout -- check `.run/renode.log`.

**Python peripheral address conflicts.** If you define a Python peripheral at an address already occupied by a modelled peripheral (e.g. USART1 at 0x40013800 is a real `STM32_UART` in the machine), Renode accepts the load but corrupts the machine. Symptom: CPU halts immediately with "PC does not lay in memory or PC and SP are equal to zero". Always check `machine.repl` for existing peripheral coverage before adding Python peripherals.

**Port collision / orphaned processes.** Early sessions left Renode listening on :3140 after
crash. New sessions fail to bind. `hantek_emu.py` handles this: it reads the PID file, kills
the old process, then checks the port via `/proc/net/tcp` (via `ss`), kills whatever holds
it, and waits for the port to be free. Do not bypass this by using `renode` directly.

**The Flash log overflow crash.** The firmware saves settings to flash in a tight loop (settings-dirty flag is 0x01 from uninitialized SRAM). The STM32F103 SVD has no FLASH model -- every read/write generates a WARNING. Even though each iteration exits immediately (BSY=0), Renode generates thousands of WARNING lines per second and eventually crashes. Fix: `flash_fix.repl` (Python peripheral at 0x40022000).

**The DMA2 log overflow.** Same pattern as Flash: DMA2 CCR1/CNDTR1 return 0 from SVD (loops exit immediately, correct behaviour) but generate WARNING spam. Fix: hook all four DMA2 entry points to return immediately (`cpu.PC = LR`).

**SysTick COUNTFLAG deadlock.** Firmware polls `SYST_CSR.COUNTFLAG` without polling `SYST_CVR`. Reading `SYST_CSR` does NOT call `SyncTime()` in Renode, so virtual time doesn't advance during the spin. With `AdvanceImmediately = false` (default), the CPU throttle sleep fires and blocks the host thread -- SysTick cannot fire while the thread is blocked. Fix: `emulation SetGlobalAdvanceImmediately true` in the resc (before `start`).

**The canapple path.** The original `live.resc` was written against `<workspace>/canapple/`
(the old repo name). That path is dead. Never use `live.resc` directly -- use `hantek_emu.py`.
