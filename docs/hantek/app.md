# 2000-series — Windows Application

Application version: v2021-08-17. Installed from the `Setup.EXE` inside tracked
[`vendor-tools/Hantek2000_Software.zip`](../../vendor-tools/Hantek2000_Software.zip) into a Wine
prefix (see [Running under Wine](#running-under-wine-linux) below for how to rebuild it).

---

## Application Files

| File | Size | Purpose |
|------|------|---------|
| `Scope.exe` | 3.8 MB | Main GUI |
| `HTHardDll.dll` | 92 KB | Hardware/USB protocol — see [usb-protocol.md](../hardware/usb-protocol.md) |
| `HTLANDll.dll` | 64 KB | LAN/WiFi transport (higher-level API, different protocol from `HTHardDll`) |
| `HTDisplayDll.dll` | 88 KB | Display rendering |
| `HTSoftDll.dll` | 84 KB | Software processing |
| `HTNTLVDll.dll` | 84 KB | NTL/V decode |
| `HTDecodeDll.dll` | 36 KB | Protocol/signal decode |
| `HTETSDll.dll` | 44 KB | ETS (equivalent-time sampling) |
| `HTFFTDll.dll` | 52 KB | FFT |
| `MeasDll.dll` | 52 KB | Auto-measurements |
| `STDFU.dll` | 71 KB | ST DFU firmware update |
| `STDFUPRT.dll` | 28 KB | ST DFU port layer |

---

## Running under Wine (Linux)

Tested 2026-06-22 with Wine-Staging 11.0.

The prefix is a **build artifact — it is not kept on disk.** It was 1.6 GB, almost all of it
stock `drive_c/windows`, so it was deleted 2026-07-28 (see `tools/scripts/clean_machine_debris.py`).
Rebuild it from the installer when you next need the app:

```bash
export WINEPREFIX=./tmp/wine-hantek

# One-time setup
winetricks -q mfc42                      # fixes missing MFC42u.DLL crash
unzip -j vendors/hantek/2d82/vendor-tools/Hantek2000_Software.zip Setup.EXE -d tmp/
wine tmp/Setup.EXE                       # installs to C:\Program Files (x86)\Hantek2xx2

# Restore the saved app/bridge config (LAN settings, registry) over the fresh prefix
tar xzf debris/wine-hantek-config/wine-hantek-config-20260728-145833.tar.gz -C "$WINEPREFIX"

# Launch
wine "C:\\PROG~5P2\\HANT~1ID\\Scope.exe"
```

Restoring the tarball is optional but saves redoing the LAN settings by hand — it carries
`AppData/Roaming/HTK-DSO/lan.set` (the `127.0.0.1:8899` bridge config below), `Default.set`,
`lan.set` and the prefix's `user.reg`/`system.reg`.

**Connecting to the USB bridge:**  
In Scope.exe: *Device LAN Setting* → IP `127.0.0.1`, Port `8899` → **Connect** (not *Search*).  
Start `vendors/hantek/2d82/scripts/hantek_usb_tcp_bridge.py` first; log at `tmp/hantek_bridge.log`.

---

## Screenshots (`Scope.exe` under Wine)

Captured 2026-07-14 while driving the app over the TCP/USB bridge. All show the **DSO tab**
(top-right tab bar: DSO · DDS · DMM) in various connection / trigger states — the right-hand panel
carries Horizontal (Time/DIV, Format), Vertical (CH1/CH2, V/DIV, coupling, probe ratio), Trigger
(Mode/Sweep/Source/Slope), Move-Waveform and Recorder controls. The `Connect:` dot at top-right is
orange while negotiating, grey when idle, and the status bar reads `Connected` / `Trig'D` once live.

| File | State |
|------|-------|
| [pcapp-dso-01.png](screens/pcapp-dso-01.png) | DSO tab, connecting |
| [pcapp-dso-02-connected.png](screens/pcapp-dso-02-connected.png) | Connected (orange Connect dot) |
| [pcapp-dso-03.png](screens/pcapp-dso-03.png) | Idle (grey Connect dot) |
| [pcapp-dso-04.png](screens/pcapp-dso-04.png) | DSO tab |
| [pcapp-dso-05-ch-onoff.png](screens/pcapp-dso-05-ch-onoff.png) | Channel ON/OFF toggled |
| [pcapp-dso-06-trigd.png](screens/pcapp-dso-06-trigd.png) | Triggered (`Trig'D`) |
| [pcapp-dso-07.png](screens/pcapp-dso-07.png) … [pcapp-dso-09.png](screens/pcapp-dso-09.png) | Further trigger/connection states |
| [pcapp-dso-10-latest.png](screens/pcapp-dso-10-latest.png) | Latest captured frame |

These are the **PC host application** UI, not the device's on-board LCD. For the on-board LCD, the
Renode harness (`code/renode/hantek_emu.py render`) captures the real firmware's boot menu; walking
the firmware through its own mode screens under Renode is not yet wired up (keypad injection past the
boot menu is unresolved — see `vendors/hantek/2d82/docs/hardware/renode-emulator.md`).

---

## DLL Export List (`HTHardDll.dll`)

For the USB packet each export generates see [usb-protocol.md](../hardware/usb-protocol.md).

### Device lifecycle
`dsoHTDeviceConnect`, `_dsoHTConnectType@12`, `_dsoHTSetNetinfo@12`, `_dsoGetDevPath@8`,
`dsoDeviceName`, `dsoDeviceSN`, `dsoGetSTMID`, `_dsoGetArmVersion@8`, `_dsoGetFPGAVersion@8`,
`_dsoIsAutomotive@4`, `dsoInitHard`, `dsoWorkType`, `_dsoClearBuffer@4`

### Scope control
`dsoHTReadAllSet`, `dsoHTScopeAutoSet`, `dsoHTSetRunStop`, `dsoHTSetTimeDiv`, `dsoHTSetYTFormat`,
`dsoHTSetTriggerSource`, `dsoHTSetTriggerSlope`, `dsoHTSetTriggerSweep`, `dsoHTSetTriggerVPos`,
`dsoHTSetTriggerHPos`, `dsoHTSetCHOnOff`, `dsoHTSetCHVolt`, `dsoHTSetCHPos`, `dsoHTSetCHCouple`,
`dsoHTSetCHProbe`, `dsoHTSetCHBWLimit`, `dsoGetSampleRate`

### Data acquisition
`dsoHTGetRealData`, `dsoHTGetSourceData`, `dsoHTGetGetMeasure`, `dsoHTGetMeasureResult`,
`dsoHTSetMeasureItem`

### DMM
`dsoGetDMMData`, `dsoSetDMMType`

### DDS / AWG
`ddsSDKSetOnOff`, `ddsSDKWaveType`, `ddsSDKFre`, `ddsSDKAmp`, `ddsSDKOffset`,
`ddsSDKSquareDuty`, `ddsSDKRampDuty`, `ddsSDKTrapDuty`, `ddsSDKDownload`,
`_ddsSDKExist@4`, `_ddsSDKSetBurstNum@8`, `_ddsSDKSetOptions@8`, `_ddsSDKSetWavePhase@8`

### Calibration / factory
`dsoWriteDeviceCali`, `_dsoGetDeviceCali@12`, `dsoHTScopeZeroCali`, `_dsoZeroCali@12`,
`_dsoFactorySetup@4`, `_dsoWriteBanner@12`, `dsoSetButtonTest`, `dsoUpdateFPGA`

---

## FPGA Update

`dsoUpdateFPGA` takes a **data-buffer pointer**, not a file path — no bitstream is embedded in the
app (PE resources #250/#251 are 600×360 BMP splash screens; bitstreams are user-supplied external
files). **Correction 2026-07-13**: earlier notes describing the 2nd argument as a "file path" were
imprecise — the DLL export itself never opens a file. Its own body (traced below) only does a raw
memory copy from the pointer it's given; whichever caller wraps it (the hidden dialog's `OnBnClicked`
handler, or `HT2000Update.exe`) must be the one that reads the `.bin` file into memory first and
computes its byte length before calling in.

### `dsoUpdateFPGA` internal trace (2026-07-13)

Full byte-level static trace of the export itself (`HTHardDll.dll`, RVA `0x3250`, VA `0x10003250`,
ordinal 55/`0037`, IAT slot `0x474190` in `Scope.exe`). Traced via `objdump` disassembly only —
Ghidra headless decompilation was attempted (fresh standalone project, per task instructions) but
hung indefinitely at script execution in this sandbox regardless of approach (full-analysis import,
single-function decompile with a 45s per-function timeout, and even a decompiler-free script that
only reads `Function`/`Parameter` metadata all hung identically) — most likely the native decompile
subprocess cannot spawn in this environment. All findings below are from manual `objdump` disassembly,
with the arithmetic (see "byte-count division" below) cross-checked by brute-force emulation in
Python rather than by hand, specifically to catch stdcall-argument-order and stack-offset transposition
mistakes before they became documented "facts."

**Signature:** `WORD __stdcall dsoUpdateFPGA(a1: uint16 dev_idx, a2: void* data_ptr, a3: uint32 tagged_value)`
— confirmed 3-arg stdcall from the `ret 0xc` at the end of both the success and gate-fail paths.

**Call-site trace (`Scope.exe`, wrapper at `0x43ea40`, single call site `0x43ea63`):**
```
0x43ea40: eax = [ecx+0xdea0]          ; caller object field -- nonzero = busy/error, short-circuits to "return 1" (skip)
0x43ea46: test eax,eax / jne 0x43ea6f ; if busy: return 1 without calling dsoUpdateFPGA at all
0x43ea4a: eax = [ecx+0x3f0]           ; caller object field -- SEE NOTE BELOW, value/producer not conclusively traced
0x43ea50: edx = [esp+0x4]             ; this wrapper's own 2nd argument = data_ptr, passed straight through
0x43ea54: eax |= 0x56700000           ; **THE MAGIC TAG IS OR'd IN HERE BY THE CALLER, NOT READ FROM THE FILE**
0x43ea59: push eax                    ; -> arg3 (a3, the gated value)
0x43ea5a: ax = [ecx+0x3fc]            ; caller object field, 16-bit -> device index
0x43ea61: push edx                    ; -> arg2 (a2, data_ptr)
0x43ea62: push eax                    ; -> arg1 (a1, dev_idx -- low16 of [ecx+0x3fc], high16 is
                                       ;    leftover garbage from the previous 'or', harmless since
                                       ;    dsoUpdateFPGA masks a1 with `and eax,0xffff` before using it)
0x43ea63: call [0x474190]             ; dsoUpdateFPGA(a1, a2, a3)
```

**Magic-tag finding (answers the task's key question):** `0x56700000` is **not read from the
bitstream file** — it is an **OR'd-in sentinel the CALLER (`Scope.exe`/`HT2000Update.exe`) stamps
onto its own internal state value before the call**, purely so the callee can verify the caller
constructed the argument correctly (a defensive "did you really mean to call me" gate against a
zero-initialized/garbage 3rd argument), not a bitstream-file-format requirement. **This means a
valid Hantek FPGA `.bin` file does NOT need any particular magic header byte sequence** — the gate
is entirely on the call-argument value, invisible to the file's own bytes. `dsoUpdateFPGA`'s own
gate check is simply:
```
0x1000325d: eax = a3 & 0xfff00000
0x10003264: cmp eax, 0x56700000
0x1000326a: je 0x1000327c   ; continue only if the top 12 bits match; else return 0 (fail)
```

**NOTE on `[ecx+0x3f0]`'s producer — not conclusively resolved.** The low 20 bits of `a3`
(`a3 & 0xfffff`, i.e. `[ecx+0x3f0]`'s low 20 bits before the OR) are consumed inside `dsoUpdateFPGA`
as a **byte count** (see "byte-count division" below) — this must be the length of the bitstream data
at `data_ptr`, most plausibly obtained by the caller via `GetFileSize`/`fread` after opening the
user-selected `.bin` file, since a real MachXO2 bitstream is tens-to-hundreds of KB and the field
is clearly meant to carry that magnitude (max representable ~1 MiB, `0xfffff`). However, the *only*
write site to a struct offset `+0x3f0` found anywhere in `Scope.exe` (`0x435cd5`, inside a generic
8-field optional-property setter `0x435c90`, called from an unrelated large-object constructor at
`0x428800`) sets it to a **constant `1`**, not a variable file size — this constructor is very
likely a different class entirely (byte offset collisions across unrelated classes are expected in
a large MFC binary) and not proven to be the FPGA-update dialog's own data model. Dialog #178's own
member-read/write of this field was not found by searching for its known control IDs (`0x4ed`-`0x4f2`)
in the disassembly. **Conclusion: the byte-count producer is inferred from `dsoUpdateFPGA`'s own
consumption pattern, not directly observed being written with a real file-size value.** Flagged as
an open item below.

**Body trace, post gate-check (baseline = esp right after `sub esp,0x90` + 4 register pushes):**

```
stripped_len = a3 & 0xfffff                  ; byte count of bitstream data (0..0xFFFFF, ~1 MiB max)
total_pkts   = ceil(stripped_len / 48)       ; via a 0x2aaaaaab magic-multiply/shift idiom -- confirmed
                                                by brute-force Python emulation across 16 test values
                                                (0, 1, 10, 47, 48, 49, 96, 100, 143..145, 1000, 4096,
                                                65536, 100000, 0xFFFFF): computed value == ceil(n/48)
                                                in every case, not a hand-parsed guess.
remainder48  = stripped_len % 48

--- STEP 1: two 10-byte no-payload header packets (flag=0x00, write+readback) ---
Send: 00 0a 03 00 02 00 00 00 00 00     ; bank=3, cmd=2 (begin-config ack)
Send: 00 0a 03 00 03 00 00 00 00 00     ; bank=3, cmd=3 (ISC enable + erase)
(abort / return 0 if either 0x10002060() send call returns <=0)

Sleep(3000)                              ; KERNEL32!Sleep, confirmed via IAT slot 0x1001102c ->
                                            named import "Sleep" in the PE import table. Gives the
                                            firmware time to complete its ISC-enable/erase busy-poll
                                            sequence (fw_machxo2_isc_enable + status polling +
                                            fw_machxo2_init_address, per firmware TSV).

--- STEP 2: page-program loop, i = 0 .. total_pkts-1 ---
each iteration builds a fresh 64-byte packet:
  [0]      = 0x00
  [1]      = 0x0a
  [2]      = 0x03                        ; bank 3
  [3]      = 0x00                        ; flag: write+readback
  [4]      = 0x04                        ; cmd 4: program page
  [5:0x35) = up to 48 bytes copied from data_ptr + i*48
             (48 bytes on every iteration except the last, which gets `remainder48` bytes if
              stripped_len is not an exact multiple of 48 -- else still a full 48)
  [0x35]   = 1 if i == total_pkts-1 else 0   ; LAST-PACKET FLAG
  [0x36:0x40) = 0x00 padding
Send packet (dev_idx, &packet)             ; abort / return 0 on send failure
data_ptr += 48                             ; always advances by 48, even on the final partial packet

On loop completion (including the zero-length-file case, total_pkts==0): return 1 (success).
```

**Cross-reference to firmware side — exact match, no discrepancies found.** Compare against
`fw_usb_cali_write_handler`'s documented sub-commands in `vendors/hantek/2d82/ghidra/ghidra_names_2d82.tsv`:

| App sends | Firmware does (`fw_usb_cali_write_handler`) | Match |
|---|---|---|
| `bank=3,cmd=2`, no payload | `[4]==2`: sets `g_fpga_prog_bc_flag=1` only | Exact |
| `bank=3,cmd=3`, no payload | `[4]==3`: sets `g_fpga_prog_begin_flag=1`, runs MachXO2 ISC unlock (refresh_rti, poll status, `isc_enable(4)`, poll, `init_address`) | Exact |
| App `Sleep(3000)` | Firmware's own internal `fw_delay_ms(50)` calls during the STEP2 unlock sequence (several, but well under 3s total) | App sleep is a generous safety margin over the firmware's own delays |
| `bank=3,cmd=4`, `data[0:0x30)`=48 bytes, `data[0x35]`=last-flag | `[4]==4`: programs **3× 0x10-byte pages** via `fw_machxo2_prog_incr(rx_pkt+5 + ch*0x10)` for `ch=0,1,2`, checks `rx_pkt[0x35]` as the **host-supplied "last packet" flag (not a page counter)** | **Exact byte-offset match**: 48 = 3×16 (three `fw_machxo2_prog_incr` calls per packet, each consuming 16 bytes starting at `rx_pkt+5`), and offset `0x35` = `5 + 48` is precisely where the app's payload ends and its own last-flag byte sits |
| Final packet (`data[0x35]=1`) | Triggers `fw_machxo2_prog_done`, `fw_fpga_sspi_isc_reset`, `fw_machxo2_isc_erase`→`refresh`, clears all 3 progress flags | Exact — the app's `i == total_pkts-1` condition is the only thing that ever sets the flag byte to 1, so the firmware's "done" sequence fires exactly once, on the true last page |

The two sides were reversed completely independently (firmware from `decompiled_named.c` +
`ghidra_names_2d82.tsv`, app from this session's fresh `objdump` trace) and agree on every field:
bank/cmd bytes, the 3-pages-per-packet chunking, and the offset-`0x35` last-packet flag semantics.
**No discrepancies found.**

**Packet-format cross-check against `usb-protocol.md`'s documented 10-byte format:** the two STEP1/
STEP2 header packets are textbook 10-byte packets (`[0]=0x00 transport tag, [1]=0x0a standard type,
[2]=bank, [3]=flag, [4]=cmd`, matching the doc exactly). The STEP2 page-program packets are **64
bytes**, not 10 — an **extended format not previously documented** for bank 3. The first 5 bytes
still follow the standard header layout (`[0][1][2][3][4]` = tag/type/bank/flag/cmd), so the
existing 10-byte header format is a strict prefix of this extended packet; only the payload region
(`[5:0x40)`, 59 bytes of which 48 are file data + 1 is the last-flag byte + 10 are unused padding)
extends past the documented format. Recommend `usb-protocol.md` note this as a second, longer
packet variant used only for bank-3 cmd-4 (FPGA page programming).

**Open items:**
- The exact producer of the byte-count value at `[caller_obj+0x3f0]` was not conclusively traced
  (see NOTE above) — would require locating dialog #178's own class methods specifically (its
  control IDs `0x4ed`-`0x4f2` were not found as literal pushes anywhere in the `Scope.exe`
  disassembly this session, suggesting they may only appear in the compiled resource template, not
  in code, or that the dialog uses `CDataExchange`/indirect addressing not caught by a literal grep).
- Whether the 3rd argument to the low-level send helper `0x10002060` is truly a byte-length or a
  packet-type/format tag consumed by the transport layer (evidence points to the latter: loop2's
  send reuses the constant `0x0a` — the STEP1 packets' own byte[1] "standard-format" tag — as this
  argument, even though the actual buffer sent is 64 bytes; a literal length interpretation would
  contradict the packet actually needing to carry 48+ payload bytes to match the firmware side).

### Hidden hotkey — CONFIRMED, but not as originally claimed (2026-07-13)

An external, unverified claim held that `Scope.exe` has a hidden **Ctrl+Alt+Shift+F** shortcut
opening a separate "FPGA Update" dialog, safer than raw ST-tool flashing. Checked by static
disassembly of `Scope.exe` (`objdump`, no Wine/live-device testing performed). Verdict: **partially
confirmed** — a hidden hotkey path to an FPGA-update dialog is real, but the exact combo and its
behaviour differ from the claim in specific ways.

**What's actually in the binary:**

- `Scope.exe` is an MFC42u app. Its accelerator table (`RT_ACCELERATOR` id 128, `.rsrc`, 31 entries)
  has **no** `CTRL+ALT+SHIFT` combo bound to `F`, and no `RegisterHotKey`/`LoadAccelerators` path
  produces the claimed shortcut. `GetKeyState` is imported and called at 12 sites, all standard
  MFC edit/list-control text-selection logic (SHIFT extend-selection, CTRL word-jump) — not a
  global hotkey.
- The real mechanism is a hand-written `WM_KEYDOWN` check inside what is very likely the main
  frame's `PreTranslateMessage` override, at `0x415d15` (function starting `0x415d00`, RVA
  relative to image base `0x400000`):
  ```
  cmp  [msg.message], 0x100      ; WM_KEYDOWN?
  mov  eax, [msg.wParam]         ; virtual-key code
  cmp  eax, 0x43 ('C') / je ...
  cmp  eax, 0x46 ('F') / jne skip
  push 0x11 ; GetKeyState(VK_CONTROL) — high bit set (down) required
  push 0x10 ; GetKeyState(VK_SHIFT)   — high bit set (down) required  [see note below]
  test [msg.lParam], 0x20000000  ; lParam bit 29 = ALT context flag (WM_KEYDOWN's own Alt bit,
                                  ; not a 3rd GetKeyState call) — must be set
  ```
  Net effective combo: **Ctrl+Alt+C** *and* **Ctrl+Alt+F** (two separate hotkeys), each
  constructing and `DoModal()`-ing the same dialog. **Not Ctrl+Alt+Shift+F** — the claimed
  extra Shift modifier was not found; the code checks CTRL (0x11) and the ALT lParam bit, and
  a `GetKeyState(0x10)` call is present in the same block but its result feeds the same
  "must be down" gate alongside CTRL — i.e. the practical combo the code accepts is
  **Ctrl+Shift+Alt** as modifiers with **either C or F** as the key (both `0x415d4d` pushing 0x11
  and `0x415d5f` pushing 0x10 must return "key down", then the Alt lParam bit is also checked) —
  so the modifier set genuinely *is* Ctrl+Shift+Alt, matching the claim's modifier combination,
  but it fires on **both C and F**, not F alone. (Re-verified against the raw bytes at
  `0x415d34`–`0x415e2c`: `cmp eax,0x43 / je 0x415d45` then `cmp eax,0x46 / jne 0x415ee3` gates
  entry into the CTRL/SHIFT/ALT check for *both* keys.)
- Both hotkeys open **dialog resource #178** — window/class title string `UpdateFPGA` (not the
  `FPGA_Update` string, which is a separate, unrelated file-open-filter string used elsewhere:
  `Bin File(*.bin)|*.bin||` at file offset `0x8d044`). Dialog #178's constructor (`0x468ee5`)
  passes dialog ID `0xb2` (178) to the `CDialog` base constructor and binds 6 controls (IDs
  1261–1266 / `0x4ed`–`0x4f2`):

  | Control ID | Class | Label / role |
  |---|---|---|
  | 1261 (`0x4ed`) | Edit | Update File Path (text box) |
  | 1262 (`0x4ee`) | Button | "…" browse button (opens `CFileDialog`, filter `Bin File(*.bin)\|*.bin\|\|`) |
  | 1263 (`0x4ef`) | Group/combo | "Operate" mode selector |
  | 1264 (`0x4f0`) | Button | (unlabeled in template dump; part of the Operate group) |
  | 1265 (`0x4f1`) | Button | **"Update"** |
  | 1266 (`0x4f2`) | Button | **"Read Flash"** |

  This dialog is **reachable only via the two hotkeys** — no menu item, toolbar button, or other
  UI path was found constructing it anywhere else in the binary (single constructor call graph:
  3 call sites, all inside the `WM_KEYDOWN` handler at `0x415d15`/`0x415ee3` region). It is a
  genuinely hidden/undocumented dialog, consistent with the claim's framing.

- **"Update" button (`OnBnClicked`, handler `0x4691c0`)**: does real work. Reads the file-path
  edit box, checks the file, then calls a generic device-dispatch helper (`0x43c380`, ~80 call
  sites across the app — not FPGA-specific) which reaches a small wrapper at `0x43ea40`. That
  wrapper is **byte-for-byte the same gated-call pattern already documented above** for
  `HT2000Update.exe`: `ptr & 0xfff00000 == 0x56700000` magic-tag check, then a direct call to the
  imported `dsoUpdateFPGA` (`HTHardDll.dll`, IAT slot `0x474190`, single call site at `0x43ea63`).
  **Conclusion: the hidden dialog's "Update" action is the same `dsoUpdateFPGA` call and the same
  magic-tagged-pointer gate already known from `HT2000Update.exe` — it is a UI wrapper, not a
  different/safer USB command sequence.** (This also means `dsoUpdateFPGA` is *not*
  updater-exclusive as previously documented — see correction in `usb-protocol.md`.)

- **"Read Flash" button (`OnBnClicked`, handler `0x422430`)**: **does nothing but close the
  dialog.** The message-map entry (`BN_CLICKED`, id `0x4f2`) points to a 5-byte stub
  (`jmp 0x46a26c`) that falls straight through to a generic MFC42u ordinal function (IAT-style
  ordinal slot value `0x800012ef` → ordinal 4847, high bit = import-by-ordinal) called from 11
  unrelated places across the binary — i.e. an ordinary, ubiquitous MFC base-class method
  (behaviourally consistent with `CDialog::EndDialog`/`OnCancel`), not anything FPGA- or
  USB-specific. **No call to `dsoUpdateFPGA`, `0x43c380`, or any `HTHardDll.dll` export appears
  anywhere in this handler's path.** The button's label promises a read-back capability the code
  does not implement — it is inert/vestigial (most likely a UI leftover from an earlier build, or
  a stub intended for a feature never wired up).

**Answering the task's key question — does a read-back/dump capability exist anywhere in
`Scope.exe`/`HTHardDll.dll`?** No. The "Read Flash" button is the single most promising lead and
it is a dead stub. No other code path in `Scope.exe`'s IAT or disassembly references any
FPGA-bitstream-read/dump function; `HTHardDll.dll`'s export list (already fully enumerated in
`usb-protocol.md`) has no such export either. **This does not unblock the earlier live-bitstream-
comparison investigation** — the hidden dialog is a dead end for that purpose.

**Not yet checked (static analysis only, no live Wine/device test performed, per task
constraints):** whether the dialog's "Operate" selector (id `0x4ef`/`0x4f0`) changes behaviour of
Update/Read Flash at runtime (e.g. selecting a "read" mode before clicking Update) — the control's
own handler (`pfn=0x469020`) was not fully traced. Low priority given Read Flash's handler is
independently confirmed inert regardless of any mode selector state.

---

## Firmware Updater (`HT2000Update.exe`)

Separate standalone binary, bundled in `Hantek2000_Software.zip` alongside `Scope.exe` but **not
present** in the previously-analyzed `win_app/Hantek2xx2/` reference copy — found 2026-07-13 while
investigating an unexplained firmware discrepancy on the physical 2D82AUTO test unit (see
`vendors/hantek/2d82/docs/hardware/bugs/` and `vendors/hantek/2d82/docs/hardware/swd-debug.md` for that investigation).

No firmware is bundled in the installer (confirmed by full extraction + byte-level STM32
vector-table scan of every file — zero hits). Instead, `HT2000Update.exe` fetches firmware **at
update time** over HTTPS from a cloud API:

```
https://api.autoupdate.lemontreex.com/v1.0/softwares/GetSoftwareInfo?softwareName=<name>
```

Three software names are referenced in the binary's strings:

| Software name | Purpose | Live response (queried 2026-07-13) |
|----------------|---------|-------------------------------------|
| `HT2XXXARM` | Base 2000-series MCU firmware | `{"Version":"2010010101","DownLoadUrl":".../20210222084100572.dfu"}` — DFU targets generic USB PID `0483:0000` (not `2D42`), 236KB, unrelated to the 2D82/2D82AUTO. Different device in the same product family. |
| `HT2XXXFPGA` | Base 2000-series FPGA bitstream | `{"Version":"0","DownLoadUrl":".../20200522114049530.bin"}` |
| `HT2XXXARMAuto` | AUTO-variant MCU firmware (the one that would apply to the 2D82AUTO) | `{}` — **empty, nothing published for this software name** |

**Domain ownership**: `lemontreex.com` resolves to Tencent Cloud (Beijing) and serves a page titled
"Hantek online shopping mall" — this is Hantek's own e-commerce/update backend under a separate
brand domain, not third-party or suspicious infrastructure. Registered via Xin Net (Chinese
registrar) since 2015.

**Conclusion on the firmware-discrepancy investigation**: this cloud endpoint is **not** the source
of the mystery firmware found on the test unit — `HT2XXXARMAuto` (the only software name that would
apply to this device) returns nothing at all, and the one real firmware file this API does serve
(`HT2XXXARM`) is for a different, unrelated USB device ID and is byte-different from both the known
`2021090901` build and the mystery SWD dump.

**Request/file-naming mechanics** (from binary strings, not yet live-tested beyond the above):
- Requires local `.dfu`/`.bin` files to be named as an all-digit version number, or falls back to
  the network fetch above ("using a local file if the computer is not convenient for networking").
- A device-version query path also exists (`GetSoftwareInfo?id=<uuid>` — a fixed UUID string was
  found in the binary, purpose/meaning not yet investigated).
- ARM/FPGA version read via `_dsoGetArmVersion@8`/`_dsoGetFPGAVersion@8` (already listed in the
  Device Lifecycle export list above) — presumably used to compare against the API's `Version`
  field to decide whether an update is needed.

**Open threads, not yet investigated:**
- What `GetSoftwareInfo?id=33b4171e-2358-4544-8eb7-3d9dca028f1f` returns and what it's for.
- Whether an AUTO-specific firmware was ever published under this software name and later removed,
  vs. never published at all.
- The full FPGA-reprogram trigger path in `HT2000Update.exe` (`dsoUpdateFPGA`, gated behind a
  magic-tagged pointer check `ptr & 0xfff00000 == 0x56700000`, reachable only via explicit user
  action in the updater's UI per a 2026-07-13 disassembly pass — not automatic/silent).

See [usb-protocol.md](../hardware/usb-protocol.md) Open Questions table (row on `ddsSDKSetOptions`)
for a related dead-USB-command finding from the same investigation.
