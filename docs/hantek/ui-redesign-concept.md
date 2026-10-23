# 2D82 — UI Redesign Concept

**Hardware:** 2.8" ILI9341 LCD (240x320 native, rotated -> **320x240 landscape effective**),
5x4 key matrix (20 keys), F1-F4 softkeys alongside LCD, connectors: CH1/CH2 BNC + EXT BNC
+ DMM banana jacks (V/ohm and COM) + Gen Out BNC + USB-C.

---

## What the 2D82AUTO most closely resembles

The nearest commercial reference is the **Fluke 190-102 ScopeMeter** -- both are:
- Handheld 4-in-1: scope + DMM + signal source + guided automotive tests
- Portrait display with keys alongside, used landscape
- Battery powered, rugged handheld

Differences (the gap to close in UX):

| Feature | Fluke 190-102 | 2D82AUTO |
|---------|---------------|----------|
| Display | 3.5" color, 320x240 | 2.8" color, 320x240 |
| Bandwidth | 100 MHz | 80 MHz |
| Sample rate | 1.25 GS/s | 250 MS/s |
| DMM resolution | 5000 counts (4.5 digit) | ~1999 counts (3.5 digit, CS7721) |
| Keys | 5 softkeys + mode selector | 4 softkeys + 16 function keys |
| Cal output | BNC on top edge | None (use AWG output) |
| Price | ~$2200 | ~$300 |

### Detailed spec comparison

| Spec | Fluke 190-102 | 2D82AUTO | Notes |
|------|---------------|----------|-------|
| Bandwidth | 100 MHz | 80 MHz | Fluke +25% |
| Sample rate | 1.25 GS/s peak (625 MS/s/ch) | 250 MS/s | Fluke 5x faster |
| Buffer depth | 10,000 samples | **6,000 samples** (FPGA 64kbit RAM limit) | Fluke 1.7x deeper |
| Display | 3.5" color, 320x240 | 2.8" color, 320x240 | Same resolution, Fluke larger |
| DMM | 5,000 count, 0.025% accuracy | **~2,000 count** (CS7721CN-1, 3.5 digit) | Fluke 2.5x better resolution |
| **Input isolation** | **600V CAT III / 300V CAT IV -- floating** | **None -- common ground, no CAT rating** | **Critical safety difference** |
| IP rating | IP51 (dust + drip proof) | None | Fluke sealed |
| AWG built-in | **No** | **Yes** (3PD5651E, 12-bit, 5-25 MHz) | 2D82AUTO wins |
| Automotive tests | None | **89-test library** built-in | 2D82AUTO wins |
| Trigger types | Edge, Video, N-Cycle, Ext | Edge only (from firmware) | Fluke richer |
| Math | +, -, x, FFT | +, -, FFT | Fluke has xB |
| Waveform save | 9 memories + PC via USB | 6 positions in STM32 flash | Fluke more flexible |
| Battery | Proprietary pack, ~4 hrs | USB-C rechargeable | 2D82AUTO more practical |
| Weight | 820g | ~350g (estimated) | 2D82AUTO lighter |
| PC software | FlukeView (paid) | None standard (DFU only) | Fluke connects to PC |
| Price | ~$2,200-2,400 USD | ~$300 USD | **2D82AUTO 7x cheaper** |

**The critical difference -- isolation:**
The Fluke 190's floating isolated inputs mean CH1 and CH2 can each reference their own ground.
This matters enormously in automotive/industrial work:
- Engine block and chassis grounds can differ by several volts
- CAN bus common-mode voltage may not match your scope ground
- Measuring a floating-ground signal without isolation creates a ground loop that injects noise
  into both the measurement AND the system under test
- Without isolation, connecting CH1 to one ground and CH2 to another can damage the instrument
- The device has NO isolation. Both channels share the chassis ground (USB-C / BNC outer).
  This is the single most important difference in actual field use.

**What the Fluke 190 does that we should copy exactly:**
- DMM mode: the reading fills ~60% of the screen height in giant 7-segment numerals
- MIN / MAX / AVG are always live -- never hidden in a menu
- A thin "trend bar" runs across the bottom of the DMM view (last 60 readings compressed)
- The trigger level is shown as a horizontal line on a mini-waveform alongside the main reading
- Mode switching is one button (SCOPE <-> METER <-> RECORD)
- Guided tests: screen shows what to connect, what a good signal looks like, PASS/FAIL verdict

**What the Fluke 190 gets wrong that we can fix:**
- Its AUTO setup is still slow and often lands on a bad timebase
- It has no amplitude histogram (you still need to know your signal before AUTO works)
- Its buffer overview is not always visible -- you have to explicitly enter "Recorder" mode
- Calibration is a factory procedure, not user-accessible

---

## Current interface (what we're replacing)

```
+------------------------------------+------------------+
|  CH1   1V   500us/div      CH2off  |  K1  K2  K3  K4  |
|                                    |                   |
|   ------+      +-----               |  K5  K6  K7  K8  |
|         +------+                   |                   |
|                 T^/v                 |  K9 K10 K11 K12   |
| - - - - - - - - - - - - -        |                   |
|                                    | K13 K14 K15 K16   |
| [F1:CH1] [F2:CH2][F3:TRIG][F4:...] |                   |
+------------------------------------+ K17 K18 K19 K20   |
                                      +------------------+
```

Problems:
- Must already know V/div + T/div before any waveform is legible
- No buffer overview -- no way to know what was captured vs what is displayed
- Trigger fires silently -- no "did it actually trigger?" feedback
- All parameters buried 2-4 key presses deep in softkey menus
- DMM reading is small -- competes for space with the scope grid
- Probe type affects all readings but is buried in Channel settings
- No calibration accessible to user at all (factory-only)

---

## What other scopes do well (reference survey)

### Fluke 190 ScopeMeter (primary reference for this design)
- **Giant DMM reading**: fills 60% of screen height, legible across a room
- **MIN/MAX/AVG**: always live in meter mode, no activation needed
- **Trend graph** (Record mode): 60-second strip chart of the DMM reading
- **Guided tests**: annotated with "connect to X", reference waveform overlaid
- **Isolated inputs**: 600V CAT III -- input protection is part of the brand promise
- The Fluke UI is the benchmark for a handheld instrument; every decision here is measured against it

### Rigol DHO900 (2023)
- Persistent acquisition history bar at top of screen
- Optional amplitude histogram on right edge of waveform area
- Still assumes the user knows T/div and trigger mode before starting

### Siglent SDS2000X Plus
- Live waveform histogram alongside the trace
- "Search & Navigate": marks all trigger events in the buffer, step through them
- Closest competitor to the histogram-first approach in this design

### PicoScope 6 (PC -- best UX for buffer navigation)
- ALWAYS shows compressed full-buffer overview at top with zoom box visible
- The user cannot get lost -- they always know where in time they are looking
- Buffer navigator: scroll through time WITHOUT changing acquisition settings
- This is the one idea we most want on the 2D82AUTO

### Keysight U1600 series (handheld competitor)
- "IntelliScale": auto-selects V/div and timebase from a histogram sweep
- Guided test mode with pass/fail vs reference waveform
- 4 quickmenu buttons map the 4 most-recently-changed parameters

### Key convergent pattern
Every modern high-end scope has: **compressed buffer overview strip always visible + amplitude
histogram available without menu navigation**. The 2D82AUTO has neither.

---

## Core design principles

1. **Show what the device knows, always.** Buffer overview is a permanent fixture.
2. **Histogram first, waveform second.** Amplitude histogram tells you if there's a signal
   and what voltage range it spans -- before you need to look at the waveform at all.
3. **DMM = maximum legibility.** The reading must be readable from arm's length. This is the
   most important single display decision in the instrument.
4. **FIND beats AUTO.** Auto-setup locks in settings; FIND shows you the histogram and
   *recommends* settings for you to confirm. The user stays in control.
5. **Zoom != T/div.** Zoom navigates within the captured buffer. T/div sets capture resolution.
   These are clearly separated: ZOOM key navigates; TBASE key changes capture rate.
6. **Calibration is user-accessible.** Four self-guided cal modes, runnable with standard probes.
7. **Probe type flows through to all displays.** Current probe selected once -> current shown
   everywhere in A, not V, for that channel, from that moment on.

---

## Proposed button layout (renaming 20 keys)

Physical layout: keys are in 5 rows x 4 cols to the right of the LCD.
F1-F4 softkeys run alongside the bottom edge of the LCD (labeled on screen).

```
                    COL1    COL2    COL3    COL4
Row 0 (top):       [CH1]   [CH2]  [MODE]  [FIND]
Row 1:             [ ^ ]   [DMM]  [AWG ]  [SNAP]
Row 2:             [ <- ]   [ OK]  [ -> ]  [BACK]
Row 3:             [ v ]  [ZOOM]  [MEAS] [RUN[*]]
Row 4 (bottom):   [TBASE] [VOLT]  [TRIG] [SINGL]

Softkeys along bottom edge of LCD:
           [F1]        [F2]        [F3]        [F4]
```

### Key assignments

| Key | Label | Tap function | Hold function |
|-----|-------|--------------|---------------|
| K1 | **CH1** | Toggle CH1 on/off | CH1 settings (coupling, probe type, invert) |
| K2 | **CH2** | Toggle CH2 on/off | CH2 settings (coupling, probe type, invert) |
| K3 | **MODE** | Cycle display modes: Explorer->Waveform->Split->Persist | -- |
| K4 | **FIND** | Smart auto-setup (histogram first) | FIND settings (sweep range, sensitivity) |
| K5 | **^** | Navigate up / increase active parameter | -- |
| K6 | **DMM** | Switch to full-screen DMM mode | DMM function selector |
| K7 | **AWG** | Switch to full-screen AWG mode | AWG type selector |
| K8 | **SNAP** | Save current screen + waveform to internal flash | Recall saved snapshots |
| K9 | **<-** | Navigate left / pan left in zoom mode | -- |
| K10 | **OK** | Confirm selection / enter submenu | -- |
| K11 | **->** | Navigate right / pan right in zoom mode | -- |
| K12 | **BACK** | Cancel / exit submenu / return to scope from DMM/AWG | -- |
| K13 | **v** | Navigate down / decrease active parameter | -- |
| K14 | **ZOOM** | Enter zoom mode -- arrows pan/scale view within buffer | -- |
| K15 | **MEAS** | Toggle measurement panel (Freq, Vmax, Vmin, Vp-p, period) | Measurement setup |
| K16 | **RUN[*]** | Run/Stop acquisition toggle | -- |
| K17 | **TBASE** | Activate timebase adjust -- arrows change T/div directly | -- |
| K18 | **VOLT** | Activate V/div adjust -- arrows change scale directly | -- |
| K19 | **TRIG** | Activate trigger adjust -- arrows move level; OK cycles mode | -- |
| K20 | **SINGL** | Single acquisition (one capture then stop) | -- |

### Parameter quick-access (no menus needed)

Pressing TBASE, VOLT, or TRIG activates that parameter. Arrows adjust. Second press locks in.
The status strip highlights the active parameter with a box:

```
Active TBASE:   [ TBASE: >500us< ]   CH1:1V DC   TRIG:^1.6V-AUTO
Active VOLT:    TBASE:500us   [ VOLT: >1V/div< ]   TRIG:^1.6V-AUTO
Active TRIG:    TBASE:500us   CH1:1V DC   [ TRIG:>^ 1.6V< AUTO ]
                                                        ^^/v arrows adjust level
                                                        OK cycles: AUTO->NORM->SINGL->AUTO
```

<- and -> change step size (coarse / fine). Arrows otherwise are waveform navigation.

---

## Screen layouts (320x240 effective landscape)

### Layout 1: EXPLORER MODE (default on power-on and after FIND)

"Top view" -- designed for when you don't yet know what the signal is.

```
+------------------------------------------------------------------------------+  y=0
| BUFFER:  [#############.....................T#####.....................]  |  y=0-11  (12px)
|          <------------------ 10.0ms captured ------------------------------> |
+------------------------------------------------------------------------------+  y=12
|                                                                              |
|  CH1                                                      +--------------+  |
|  HISTOGRAM       |                                        | CH1   1V DC  |  |
|                  |  ####                                  | Probe: 10x   |  |  y=12-145
|     +3.3V ----   |  ##########                            |              |  |  (134px)
|                  |  ##########                            | Vmax: 3.31V  |  |
| TRIG - -1.6V-   ----------- <- trigger level             | Vmin: 0.02V  |  |
|                  |  ####                                  | Vmean: 1.67V |  |
|      0V ------   |  ##########                            | Freq: 1.02kHz|  |
|                  |  ####                                  |              |  |
|     -0.5V ----   |  ####                                  | TRIG: ARMED |  |
|                  |                                        +--------------+  |
|   <- bars = dwell time at each voltage level (horizontal bar = probability)  |
|                                                                              |
+------------------------------------------------------------------------------+  y=145
| CH1:1V 10x  TBASE:500us  TRIG:^1.6V AUTO  CH2:off    RUNNING          2.8V |  y=145-160 (15px)
+------------------------------------------------------------------------------+  y=160
|  [F1: WAVEFORM]    [F2: LOCK IN [ok]]    [F3: CH1 SETUP]    [F4: UTIL]        |  y=160-179 (20px)
+------------------------------------------------------------------------------+

Pixel budget: 12 + 133 + 15 + 20 = 180px of content + 60px spare -> use spare to grow histogram
```

**Reading the histogram -- no oscilloscope knowledge required:**

| What you see | What it means | What FIND will do |
|---|---|---|
| Two sharp clusters (0V and 3.3V) | Digital square wave | Set trig at 1.65V, T/div auto-period |
| One wide spread cluster | Noisy/analog AC signal | Set trig at mean, T/div to estimated period |
| Single narrow spike at one voltage | DC -- no AC component | Warn "DC ONLY -- no trigger period" |
| Histogram is flat (uniform) | No signal / open input | Show "NO SIGNAL DETECTED" |
| Three or more clusters | Complex/multi-level signal | Set trig at largest gap between clusters |

The dashed line shows the current trigger level. If ARMED, the trigger is set to catch a crossing.

---

### Layout 2: WAVEFORM MODE (default after FIND lock-in; also MODE key 1st press)

```
+------------------------------------------------------------------------------+  y=0
| [###..................[[*][*][*][*][*][*][*][*][*][*]].............T........................]  |  y=0-11 (12px)
|  <-- full capture 10ms ----------[your view]----------------------------->  |
+------------------------------------------------------------------------------+  y=12
|                                                                              |
|  3.3V- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -   |
|                                                                              |
|       +----+      +----+      +----+      +----+      +----+      +----+   |  y=12-170
|  1V --+    +------+    +------+    +------+    +------+    +------+    +-  |  (158px waveform)
|  ^/vT   |    |      |    |      |    |      |    |      |    |      |    |   |
|  0V --+    +------+    +------+    +------+    +------+    +------+    +-  |
|                                                                              |
|  CH2- - - - - - - - - - - - - - - - (OFF) - - - - - - - - - - - - -   |
|                                                                              |
+------------------------------------------------------------------------------+  y=170
| CH1:1V 10xDC  500us  ^1.6V-AUTO | Freq:1.02kHz  Vp-p:3.29V  T:980us  TRIG|  y=170-188 (18px)
+------------------------------------------------------------------------------+  y=188
|  [F1: EXPLORE]  [F2: SPLIT/HIST]  [F3: MEAS v]  [F4: CURSOR]               |  y=188-208 (20px)
+------------------------------------------------------------------------------+
```

**Buffer overview strip (top 12px) -- always present:**
- Full capture shown compressed horizontally
- `[[*][*][*][*]]` box = your current view window; position + width show exactly where you are in time
- `T` marker = where the trigger fired
- Dark fill = signal energy present; lighter = low signal
- **ZOOM key** + <- -> moves the box (pan buffer without re-acquiring)
- **ZOOM key** + ^ v resizes the box (zoom in/out without re-acquiring)
- You cannot get "lost in the buffer" -- this strip makes it impossible

**Trigger marker:**
- `^/vT` on the left edge of the waveform = trigger level line (horizontal dashed)
- The trigger event (where it fired) = vertical tick mark at the top of the waveform

---

### Layout 3: SPLIT MODE (MODE key 2nd press)

```
+------------------------------------------------------------------------------+  y=0
| [##................[[*][*][*][*][*]]...............T..............................]  |  y=0-11 (12px)
+--------------------------------------------+---------------------------------+  y=12
|                                            |                                 |
|   +----+     +----+     +----+            |  HISTOGRAM (live)               |
|---+    +-----+    +-----+    +--          |                                 |
|   |T   |                                  |  3.3V- | ##########             |  y=12-160
|   +----+                                  |  1.6V- - (trig level)          |  (148px)
|                                            |       | ####                   |
|   CH2- - - - - - - - - (OFF)- - -       |  0V-  | ##########             |
|                                            |                                 |
|                                            |  Updates each acquisition       |
|                                            |  <- move trig with TRIG+arrows  |
+--------------------------------------------+---------------------------------+  y=160
| CH1:1V 10xDC  500us  ^1.6V-AUTO   CH2:off   Vmax:3.31  Vmin:0.02  RUNNING  |  y=160-178
+------------------------------------------------------------------------------+  y=178
|  [F1: EXPLORE]  [F2: WAVEFORM]  [F3: FFT split]  [F4: PERSIST split]        |  y=178-200
+------------------------------------------------------------------------------+
```

Split mode is the "hunting" view -- you're looking at the waveform while the histogram shows
you whether the amplitude distribution is changing.
F3 replaces the histogram panel with a live FFT (same split layout).
F4 replaces it with a persistence/density view (accumulated pixel density).

---

### Layout 4: PERSIST / DENSITY MODE (MODE key 3rd press)

```
+------------------------------------------------------------------------------+
| PERSIST  256 acq  DECAY: INFINITE                     [F4: CLEAR]            |  y=0-14
+------------------------------------------------------------------------------+
|                           *           *                                      |
|                         *   *       *   *                                    |
|      ####################     #######     ####################               |  main area
|    ##                    *   *       *   *                    ##             |  y=14-190
|   ##                       *           *                       ##            |  (176px)
|                                                                              |
|  key: ##=frequent  #=occasional  *=rare/single hit                          |
+------------------------------------------------------------------------------+
| CH1:1V 10x  500us  256 acq  Freq:1.02kHz                                    |  y=190-208
+------------------------------------------------------------------------------+
|  [F1: EXPLORE]  [F2: WAVEFORM]  [F3: SPLIT]  [F4: CLEAR-HIST]               |  y=208-228
+------------------------------------------------------------------------------+
```

Persist mode is for eye diagrams, jitter visualisation, finding intermittent glitches.
The color intensity is the visit count per pixel -- rendered using the LCD's 16-bit palette
as a gradient from background color (zero) through cool (few hits) to warm/bright (many).

---

## DMM mode -- maximum legibility (Fluke 190 standard)

The single most important display decision: **the DMM reading must be readable from arm's
length.** On 320x240, this means the primary digit height is 80px minimum.

```
+------------------------------------------------------------------------------+  y=0
|  DMM  [*] DC VOLTAGE                                         [BACK to SCOPE]  |  y=0-18 (18px)
+------------------------------------------------------------------------------+  y=18
|                                                                              |
|                                                                              |
|                                                                              |
|                                                                              |
|    +------------------------------------------------------------------+      |
|    |                                                                  |      |  y=18-118
|    |      3 . 2 9 5    V                                              |      |  (100px reading)
|    |                                                                  |      |  7-seg style
|    +------------------------------------------------------------------+      |
|                                                                              |  y=118-138
|     ############################...............................  33%       |  (bar graph, 20px)
|                                                                              |
|     MIN:  3.291 V          MAX:  3.310 V          AVG:  3.295 V             |  y=138-165 (27px)
|                                                                              |
|     TREND: ________________________________________________________        |  y=165-182 (17px)
|     (last 60 readings, oldest left)                                          |
+------------------------------------------------------------------------------+  y=182
|  [F1: DC-V]  [F2: AC-V]  [F3: RES/CAP]  [F4: > DIODE/CONT/IDV/TEMP]       |  y=182-202 (20px)
+------------------------------------------------------------------------------+

Reading box breakdown (100px tall, ~280px wide):
  Sign:        20px wide x 80px tall  ("+"/"-"/"")
  Digits:      40px wide x 80px tall  (x4 = 160px)
  Decimal pt:  10px wide
  Space:       10px wide
  Unit:        40px wide x 60px tall  ("V"/"mV"/"A"/"mA"/"kohm"/"Mohm"/"nF")
Total = 240px -- centred in 320px with 40px each side margin

The digit font is a 5-segment stroke font at 6px stroke width -- optimised for this size.
At 80px tall the primary digits are readable at ~1m distance (verified against Fluke 190 at
same pixel density).
```

**All DMM functions:**

| F-key | Mode | Range |
|-------|------|-------|
| F1 | DC Voltage | Auto: 200mV / 2V / 20V (CS7721 range limits) |
| F2 | AC Voltage | Same ranges, RMS |
| F3 (cycle) | Resistance -> Continuity -> Capacitance | Auto |
| F4 (cycle) | Diode -> Current DC -> Current AC -> Temperature | -- |

Hold BACK in DMM mode -> scope. Hold DMM key -> DMM function selector (full menu).

**MIN/MAX/AVG are always live.** They reset only when: (a) function changes, (b) user presses
OK while AVG is highlighted (long-press OK = manual reset).

**TREND strip** (bottom row, 17px):
- Last 60 readings shown as 1-pixel-wide bars scaled to the current range
- Gives a glance at drift, ripple, instability without entering "RECORD" mode
- Exactly what the Fluke 190's trend bar does

---

## Probe library -- PicoScope model

### Design model: how PicoScope handles probes

PicoScope 6/7's probe selector is the right model. When you click a channel:
- A full browser opens showing all known probes organized by type
- Each probe has: product name, type icon, sensitivity, max rating, one-line description
- Selecting a probe applies all scaling automatically -- the channel label shows the probe name
- "Add user probe" lets you enter any probe you own using its datasheet parameters
- Recent probes appear at the top for fast repeat use
- The probe name propagates to all measurements, trigger labels, and saved data

We adapt this for 20 physical keys and a 320x240 display. Hold CH1 or CH2 -> full-screen probe browser.

---

### Probe browser layout (full screen, activated by hold CH1 or CH2)

```
+------------------------------------------------------------------------------+
|  CH1 PROBE LIBRARY          ^v scroll   OK select   F4 add custom            |  y=0-14
+------------------------------------------------------------------------------+
|  RECENT                                                                      |  y=14-24
|  > CC-65              current clamp   100mV/A   65A max   <- last used        |
|    10x Passive        voltage         10:1      500V max                     |
+------------------------------------------------------------------------------+
|  CURRENT -- AC/DC CLAMP PROBES                                                |  y=48
|    CC-65              current clamp   100mV/A   65A max                      |
|    CC-650             current clamp   10mV/A    650A max                     |
|    Generic 100mV/A    current clamp   100mV/A   user-rated                  |
|    Generic 10mV/A     current clamp   10mV/A    user-rated                  |
|    Custom clamp...      -> opens custom entry                                  |
+------------------------------------------------------------------------------+
|  CURRENT -- ROGOWSKI COIL (AC only, requires integrator box)                  |  y=102
|    PEM CWT Ultra/1    Rogowski        1mV/A     3000A pk                     |
|    PEM CWT Ultra/B    Rogowski        100mV/A   300A pk                      |
|    PEM CWT Mini       Rogowski        10mV/A    1500A pk                     |
|    Generic Rogowski   Rogowski        custom    custom                       |
|    Custom Rogowski...   -> opens custom entry                                  |
+------------------------------------------------------------------------------+
|  VOLTAGE -- PASSIVE                                                           |  y=148
|    1x passive         voltage         1:1       CAT-II rated                 |
|    10x passive        voltage         10:1      500V max                     |
|    100x passive       voltage         100:1     1kV max                      |
|    1000x HV probe     voltage         1000:1    40kV max                     |
+------------------------------------------------------------------------------+
|  VOLTAGE -- DIFFERENTIAL                                                      |  y=180
|    Diff 10x (generic) differential    10:1      +/-700V diff                  |
|    Diff 100x (generic)differential    100:1     +/-1400V diff                 |
|    Custom diff...       -> opens custom entry                                  |
+------------------------------------------------------------------------------+
|  [F1: ^ page]   [F2: v page]   [F3: CONNECT GUIDE]   [F4: ADD CUSTOM]      |  y=218-238
+------------------------------------------------------------------------------+

Highlighted row (>) = cursor. OK applies the probe to the channel and closes.
F3 = opens the connection guide for the currently highlighted probe (does not apply it yet).
```

---

### Connection guide (F3 on any probe)

One screen that shows the physical connection, notes, and any auto-config it will apply.
Example for a Rogowski coil:

```
+------------------------------------------------------------------------------+
|  CONNECT: PEM CWT Ultra/1 Rogowski Coil              [F4: APPLY & CLOSE]    |  y=0-14
+------------------------------------------------------------------------------+
|                                                                              |
|   WHAT IT IS:                                                                |
|   A flexible loop that threads around the conductor under test.              |
|   AC current only -- it measures rate-of-change (di/dt), not DC.            |
|   An integrator box converts the coil output to a current reading in A.     |
|                                                                              |
|   WIRING:                                                                    |
|                                                                              |
|    +-------------+        +----------------------+                          |
|    |  Rogowski   |        |   Integrator box      |                          |
|    |  coil loop  +--BNC-->|IN  PWR     OUT BNC   +------> CH1               |
|    |  (flex)     |        |                       |                          |
|    +-------------+        +----------------------+                          |
|                                                                              |
|   STEPS:                                                                     |
|   1. Power up integrator (battery or USB -- check your model)                |
|   2. Leave coil in air (not around any conductor). Wait 30s for warmup.     |
|   3. Trim DC OFFSET pot on integrator until reading is near zero.           |
|   4. Thread coil around the conductor (direction matters -- mark on coil).   |
|   5. Connect integrator OUT -> CH1 BNC.                                      |
|                                                                              |
|  [!] AUTO-CONFIG WILL APPLY:                                                   |
|     Coupling: AC  (DC offset in integrator drifts -- AC coupling removes it) |
|     Unit: A/div  (sensitivity: 1mV/A = 1A/div at 1mV/div ADC setting)      |
|     Scale: 50A/div suggested starting point                                 |
|                                                                              |
|  [F2: OVERRIDE -- keep DC coupling]    [F3: BACK TO LIST]   [F4: APPLY]     |
+------------------------------------------------------------------------------+
```

Connection guides exist for every probe type in the library.
For clamp probes the guide shows the jaw-around-conductor diagram.
For differential probes it shows the +/- lead placement.

---

### Add custom probe (F4 in browser, or "Custom..." entry)

```
+------------------------------------------------------------------------------+
|  ADD CUSTOM PROBE                                                             |  y=0-14
+------------------------------------------------------------------------------+
|                                                                              |
|  Name:           [ MY CWT MINI_____________ ]  (label on channel in status) |
|                                                                              |
|  Type:           > CURRENT (A)  <                                            |
|                    VOLTAGE (V)                                               |
|                    CURRENT Rogowski (A, AC only -- auto-applies AC coupling)  |
|                    DIFFERENTIAL VOLTAGE (V)                                  |
|                    OTHER (custom unit)                                       |
|                                                                              |
|  Sensitivity:    [ 10.0  ] mV  per  [ A   ]                                 |
|                  (how many millivolts from the probe output = 1 unit)        |
|                  Examples:  100mV/A for CC-65,  10mV/A for CC-650,          |
|                             1mV/A for PEM CWT/1 (via integrator)            |
|                                                                              |
|  DC Offset:      [ 0.000 ] V  at zero input  (null shift, usually 0.000)    |
|                                                                              |
|  Max rating:     [ 1500  ] A  (displayed as warning if exceeded)            |
|                                                                              |
|  Notes:          [ AC only - zero integrator before use___ ]                |
|                  (shown in connection guide and on probe-select screen)      |
|                                                                              |
|  [F2: TEST -- apply to CH1 now]   [F3: SAVE to library]   [F4: CANCEL]      |
+------------------------------------------------------------------------------+

Navigation: ^v move between fields. <--> adjust numeric values (coarse/fine).
OK on a text field opens character entry (arrows scroll through chars A-Z 0-9 space - / .)
```

**Sensitivity field is the key field** -- it is the number from the probe's datasheet:
```
  Probe datasheet says:           Enter as:
  "100 mV/A output"           ->   100.0 mV per A
  "10 mV per amp"             ->    10.0 mV per A
  "0.1 V/A"                   ->   100.0 mV per A   (same thing)
  "1 V/kA" (large Rogowski)   ->     1.0 mV per A   (1V/kA = 1mV/A)
  "50:1 voltage attenuation"  ->  (use Voltage type, ratio 50:1)
```

---

### Rogowski coil -- specific handling

Rogowski coils are fundamentally different from clamp-on CT probes:

| Property | CT clamp (CC-65 etc.) | Rogowski coil (CWT etc.) |
|----------|----------------------|--------------------------|
| Conductor closure | Must clamp fully around | Flexible, threads through tight spaces |
| DC current | Yes | **No -- AC only** |
| Output at zero current | Near-zero | Drifts (integrator DC offset) |
| Integrator needed | No | Yes (usually external box) |
| Typical max current | 65-1000A | 300A-3000A (flexible = higher range) |
| Typical sensitivity | 100mV/A or 10mV/A | 1mV/A to 100mV/A |
| Use case | General AC/DC | Motor drives, inverters, welding, large AC |

**When Rogowski type is selected, the device automatically:**
1. Sets channel coupling to AC (integrator DC offset drift makes DC coupling useless)
2. Prompts to zero the integrator: "WITH COIL IN AIR -- zero the offset pot until reading < 0.1A"
3. Shows the A/div label (not V/div)
4. Adds a "[!] AC only" annotation to the status strip for this channel
5. If the DC component of the reading exceeds 5% of full scale: warns "DC DRIFT DETECTED -- re-zero integrator or reduce DC coupling"

**Zero-calibration for Rogowski (built into the probe selection flow):**
```
  ROGOWSKI ZERO-CAL
  -----------------
  Leave coil in open air, not near any conductor.
  Watch the reading below -- adjust integrator DC OFFSET trim pot
  until it reads less than 0.1 A.

  Current reading: ################....  +2.3 A  <- adjust pot until near zero

  [  --1A  ] [  --0.1A ] current reading    [  +0.1A ] [  +1A   ]
              adjust pot <-                        -> adjust pot

  When stable < 0.1A: [F2: ZERO ACCEPTED -- proceed]   [F4: CANCEL]
```

This is the most common mistake with Rogowski coils -- forgetting to zero before measuring.
Making it mandatory (the probe won't be marked "ready" until zeroed) prevents the error.

---

### Probe name in status strip

After probe selection, the channel label in the status strip changes from the generic type
to the probe's short name. This is how PicoScope does it:

```
Old style (type only):
  CH1: 1V  10x  DC  500us  ^1.6V-AUTO

New style (probe named):
  CH1: 50A/div  CC-65  DC  500us  ^8.2A-AUTO
  CH1: 200A/div MY-CWT AC[!] 500us  ^0.0A-AUTO    <- Rogowski, AC coupling, zero needed
  CH1: 100V/div 10x    DC  500us  ^24.0V-AUTO
  CH1: 1.5kV/div HV-1k DC  500us  ^500V-AUTO
```

The probe name persists across power cycles (stored in the same flash table as coupling + V/div).
When the device powers on with a probe previously configured, it shows the probe name
immediately -- no need to re-select.

---

### [!] Isolation warning

Since the device has NO isolated inputs (unlike Fluke 190), the probe library warns when a
differential probe or Rogowski is used in a way that assumes isolation:

```
  [!] GROUND LOOP WARNING
  ---------------------
  CH1 is set to: Diff 10x probe
  CH2 is set to: CC-65 current clamp

  Both channels share the same chassis ground.
  If you connect CH1 and CH2 to circuits with DIFFERENT grounds,
  current will flow through the scope ground path -- this can:
    - Disturb the circuit under test
    - Damage the scope
    - Create a shock hazard on exposed BNC outer collars

  For isolated measurements, use a differential probe on one channel
  only, or use a battery-powered scope without USB connected.

  [OK: I understand -- proceed]   [F3: BACK -- reconfigure]
```

This warning appears at connection guide view and when multiple channels are active with
different probe types that imply different reference grounds.

---

## FIND -- smart auto-setup

**Problem with current AUTO:**
AUTO picks a timebase that gives "some" display, but if V/div is wrong the waveform clips
or disappears. It has no knowledge of the signal before trying to display it.

**FIND algorithm:**

```
Step 1: Quick sweep (3 acquisitions at 5ms/div, 500us/div, 50us/div)
         -> build combined amplitude histogram from all three captures
         -> 256-bin histogram from ADC-min to ADC-max across all sweeps

Step 2: Analyse histogram
         If histogram is flat (RMS < 5% of range): -> "NO SIGNAL" display
         Find histogram peaks (bins with >5% of total samples)
         Find histogram gaps (bins with <1% of total samples between peaks)

Step 3: Set V/div
         V/div = (max_peak - min_peak) x 1.5 / 8  (fits signal in 6.7 of 8 divisions)
         Offset = midpoint of range centred in grid

Step 4: Set trigger level
         If 2 peaks: trigger = midpoint of the gap between them (ideal for digital)
         If 1 peak: trigger = peak_center (DC -- warn that trigger may not be stable)
         If 3+ peaks: trigger = widest gap (multi-level signal)

Step 5: Estimate frequency
         Count zero-crossings at trigger level across the longest sweep
         If >2 crossings: set T/div so ~3 periods fit on screen
         If 0-2 crossings: keep coarsest timebase, warn "low frequency or aperiodic"

Step 6: Show EXPLORER view
         Display histogram with recommended settings overlaid as annotations
         Show verdict string: "SIGNAL FOUND: ~3.3V digital, ~1kHz, rising trigger at 1.6V"
         F2: LOCK IN -> apply settings -> switch to WAVEFORM mode
         F3: ADJUST -> apply settings but stay in EXPLORER for tuning
```

FIND is never silent. It always shows what it found (or didn't find) before changing anything.
The user confirms with LOCK IN. This is the "OneKey Measure" pattern from Keysight U1600,
extended with the histogram step.

---

## AWG mode -- full screen

```
+------------------------------------------------------------------------------+
|  AWG  OUTPUT: [*] ON                                         [F4: ON/OFF]     |  y=0-18
+------------------------------------------------------------------------------+
|                                                                              |
|  TYPE    FREQ              AMPLITUDE         OFFSET                         |
|  > SINE  > 1.000 kHz <   > 2.000 Vpp <   > 0.000 V <                     |  y=18-48
|   SQUARE                                                                    |
|   TRIANGLE    [TBASE key adjusts]  [VOLT key adjusts]  [TRIG adjusts]      |
|   TRAPEZOID                                                                 |
|   ARB 1-4                                                                   |
|                                                                              |
|  PREVIEW -- CH1 live if AWG output is probed back to CH1:                    |  y=80-180
|  +--------------------------------------------------------------------+      |
|  |    ~~~       ~~~       ~~~      ~~~      ~~~      ~~~      ~~~     |      |
|  +--------------------------------------------------------------------+      |
|                                                                              |
|  CAL TIP: for calibration, probe Gen Out -> CH1 and short Gen Out -> CH2      |
+------------------------------------------------------------------------------+
|  [F1: TYPE ^/v]  [F2: FREQ ^/v]  [F3: AMP/OFFSET ^/v]  [F4: AWG OUTPUT]         |
+------------------------------------------------------------------------------+
```

The TBASE / VOLT / TRIG direct-access keys are repurposed in AWG mode:
- TBASE adjusts frequency (^ = x10 step, v = /10; <- -> = fine tune)
- VOLT adjusts amplitude (^v = coarse, <- -> = fine)
- TRIG adjusts DC offset (^v = move offset up/down)

---

## Calibration system -- 4 modes (user-accessible)

Access via: UTIL (F4 in most modes) -> CALIBRATION -> choose mode.
All four modes are step-by-step guided (Keysight U1600 / Fluke 190 style).

---

### CAL MODE A -- Zero offset calibration ("short inputs")

**Purpose:** Remove DC offset errors on CH1, CH2, and DMM.
Stores correction table: `CaliParam.volt_cali_data[ch][range][0]` (zero-point per range).

**Physical setup:**
```
Connect all of these together with a short wire or clip:
  CH1 BNC centre pin --+
  CH2 BNC centre pin --+-- short wire -- GND (chassis / BNC outer / DMM COM)
  DMM V/ohm input -------+
```

**Screen during cal:**
```
+------------------------------------------------------------------------------+
|  CAL A -- ZERO OFFSET                                           Step 1 of 3  |
+------------------------------------------------------------------------------+
|                                                                              |
|  SHORT all inputs together:                                                  |
|                                                                              |
|     CH1 -----------------------------+                                      |
|     CH2 -----------------------------+---- GND                              |
|     DMM V/ohm -------------------------+                                      |
|                                                                              |
|  Use the BNC short-circuit cap or clip CH1 probe tip to CH2 probe tip        |
|  and both to the scope chassis ground.                                       |
|                                                                              |
|  When connected: press [F2: MEASURE] to capture the offset on each input.   |
|                                                                              |
|                              [F2: MEASURE]   [F4: CANCEL]                   |
+------------------------------------------------------------------------------+
|  Measuring 512 samples per channel...                                          |
|                                                                              |
|  CH1 offset: +2.3mV  (range 1V)    -> STORE: -2.3mV correction              |
|  CH2 offset: -1.1mV  (range 1V)    -> STORE: +1.1mV correction              |
|  CH1-CH2 match: 3.4mV difference   -> NOTE: channel matching offset          |
|  DMM offset: +0.8mV                -> STORE: -0.8mV correction              |
|                                                                              |
|  All within spec (< +/-5mV at 1V range).                                      |
|                  [F1: REPEAT]   [F2: SAVE & NEXT]   [F4: CANCEL]            |
+------------------------------------------------------------------------------+
```

After SAVE & NEXT -> Step 2: repeat at each V/div range (repeating with attenuators switched).
The firmware already has `CaliParam.volt_cali_data[ch][range][point]` -- this fills `point=0`.

---

### CAL MODE B -- Gain + noise check ("connect a battery")

**Purpose:** Verify gain accuracy on CH1, CH2, DMM; measure ADC noise floor;
cross-check scope reading vs DMM reading.

The battery is the ideal calibration source because:
- Stable DC -- no AC to confuse the histogram
- Known approximate voltage (~1.5V fresh AA, ~9V PP3/6F22)
- Low impedance -- not affected by probe loading
- No external power needed
- The small noise you see on a battery in DC-coupled scope IS the ADC noise floor

**Physical setup:**
```
  Battery (+) ---- CH1 BNC centre ---- CH2 BNC centre ---- DMM V/ohm
  Battery (-) ---- scope/DMM GND (BNC outer / COM)

  Recommended: 1.5V AA or 9V block battery
  Do NOT use a mains-powered supply -- too much noise and hum
```

**What the device measures:**
```
+------------------------------------------------------------------------------+
|  CAL B -- GAIN + NOISE                                          Step 1 of 2  |
+------------------------------------------------------------------------------+
|  Connect a fresh battery (1.5V or 9V) across CH1, CH2, and DMM as shown.    |
|                                                                              |
|  Battery (+) ---- CH1 BNC --- CH2 BNC --- DMM V/ohm                         |
|  Battery (-) ---- GND (BNC outer / COM)                                     |
|                                                                              |
|                  [F2: BATTERY CONNECTED -- START CAL]  [F4: CANCEL]          |
+------------------------------------------------------------------------------+
|  MEASURING...                                                                  |
|                                                                              |
|  Battery reading (DMM):          1.512 V   (this is the reference)          |
|  CH1 reading (scope):            1.509 V   error: -0.3%   [ok] PASS (<1%)     |
|  CH2 reading (scope):            1.511 V   error: -0.1%   [ok] PASS           |
|  CH1 vs CH2 match:               2.0mV difference        [ok] PASS (<5mV)     |
|                                                                              |
|  ADC NOISE FLOOR (CH1, DC-coupled, 1V range):                               |
|    RMS noise:   0.82mV    peak-to-peak: 3.1mV                               |
|    Dither:      GOOD -- noise is smooth (not quantisation steps)   [ok] PASS    |
|    Noise level: GOOD -- within spec (< 2mV RMS at 1V range)       [ok] PASS    |
|                                                                              |
|  [F1: SHOW NOISE WAVEFORM]  [F2: SAVE RESULTS]  [F3: NEXT RANGE]           |
+------------------------------------------------------------------------------+
```

**Dither explanation (shown if user presses F1 on SHOW NOISE WAVEFORM):**
```
  ABOUT NOISE / DITHER:
  The small noise visible on a flat DC source is EXPECTED and GOOD.
  It means the ADC is "dithering" -- the noise smears the quantisation
  steps, improving effective resolution below 1 LSB.
  Too little noise = quantisation staircase visible on slow signals.
  Too much noise = poor SNR.
  Your noise: 0.82mV RMS = 0.82 LSB at 1V range -> IDEAL dither level.
```

**Gain error correction:**
If CH1 shows systematic gain error (e.g. consistently 0.8% low across all ranges),
the device stores a per-channel gain trim factor in `CaliParam.volt_cali_data[ch][range][1]`.

---

### CAL MODE C -- Probe compensation (internal square wave)

**Purpose:** Verify probe tip compensation capacitor trimmer is correctly adjusted.
Uses the AWG output as the calibration signal (500Hz-2kHz square wave).

```
+------------------------------------------------------------------------------+
|  CAL C -- PROBE COMPENSATION                                                  |
+------------------------------------------------------------------------------+
|                                                                              |
|  Connect CH1 probe TIP to GEN OUT connector.                                 |
|  Connect CH1 probe GROUND CLIP to scope GND (BNC outer collar).              |
|                                                                              |
|  The AWG will output a 1kHz square wave.                                    |
|  Watch the display -- adjust the probe's TRIMMER SCREW until the waveform   |
|  shows flat top and bottom corners (like the CORRECT example).               |
|                                                                              |
|  UNDER-compensated   CORRECT           OVER-compensated                     |
|  (rounded corners)   (sharp corners)   (overshoot)                          |
|         +--+             +--+               ++                             |
|        /    \            |  |               ||                             |
|  -----+      +--   ------+  +---   ------+  +-----                        |
|         Adjust   ->   trimmer   <-   Adjust                                   |
|                                                                              |
|  [F2: ZOOM IN on corner]   [F3: CHANNEL 2 ->]   [F4: DONE]                  |
|                                                                              |
|  Live scope display (mini, top of panel):                                   |
|  [         +---+     +---+     +---+     +---+        ]                   |
|  [---------+   +-----+   +-----+   +-----+   +--------]                   |
+------------------------------------------------------------------------------+
```

The "Live scope display" section is a mini real-time waveform using the existing scope capture
pipeline, drawn as a 60px tall strip inside the cal screen. The main explanation text stays.
No mode switch needed -- cal screen shows the result inline.

---

### CAL MODE D -- AWG-to-scope cross-calibration (internal reference)

**Purpose:** Use the AWG as a known-voltage reference to cross-calibrate CH1 and CH2.
This does not require any external equipment -- the device calibrates itself.

The AWG DAC is 12-bit. If we know the AWG is outputting X.XXX V (from its own DAC code),
and CH1 reads Y.YYY V, then gain_error = Y/X - 1. We can store a correction factor for CH1.

**Physical setup:**
```
  Gen Out BNC ---- CH1 BNC (direct BNC-to-BNC coupler, or short coax)
  Gen Out BNC ---- CH2 BNC (BNC T-piece or two separate connects, one at a time)
  Note: set both channels to 1x probe for this cal
```

**Steps:**
1. AWG outputs 1.000V DC (DAC code for 1V, corrected for AWG gain from prev factory cal)
2. Measure CH1 -> store correction
3. AWG outputs 2.500V DC -> measure CH1 again -> verify linearity
4. AWG outputs 1.000V DC to CH2 -> measure CH2 -> store correction
5. AWG outputs 0.000V DC (GND) -> check all-zero reading on both

This mode is the equivalent of using a precision voltage standard, but using the AWG instead.
It is only as accurate as the AWG DAC, which is +/-0.5% typical -- still better than the
unadjusted scope gain error of +/-2-3%.

---

## Automotive / guided test mode

The existing 89-entry automotive test library in firmware is accessed via:
UTIL -> VEHICLE DIAG (current path)

In the redesign it stays on the same path, but the display is improved:

```
+------------------------------------------------------------------------------+
|  VEHICLE DIAG > SENSORS > Crankshaft Sensor (Inductive)                    |
+------------------------------------------------------------------------------+
|  CONNECT:                          REFERENCE WAVEFORM:                      |
|  CH1+ -> CKP signal wire            +------------------------------+         |
|  CH1- -> sensor ground              |   ___     ___     ___        |         |
|  CH2  -> (not used)                 |  /   \   /   \   /   \       |         |
|                                    | /     \_/     \_/     \___   |         |
|  Expected:                         +------------------------------+         |
|  - AC voltage, ~0.5-2Vpp           LIVE WAVEFORM:                           |
|  - Varies with RPM                 +------------------------------+         |
|  - Sync pulses for missing tooth   |  (live CH1 input here)       |         |
|    (typically 36-1 or 60-2)        |                              |         |
|                                    +------------------------------+         |
+------------------------------------------------------------------------------+
|  [F1: PREV TEST]  [F2: NEXT TEST]  [F3: AUTO MATCH]  [F4: SCOPE VIEW]      |
+------------------------------------------------------------------------------+
```

F3: AUTO MATCH runs FIND on the live signal, then compares the amplitude and frequency range
to the reference bounds for this test -> PASS / WARN / FAIL verdict.

---

## Mental model map -- what changes for the user

**Old mental model (4 invisible abstractions):**
```
  "I need to know" -> V/div -> T/div -> trigger level -> trigger mode -> waveform
                                ^ user must guess all four before seeing anything ^
```

**New mental model (everything visible, one step at a time):**
```
  Press FIND
     v
  Histogram shows: is there a signal? what voltage range? (no prior knowledge needed)
     v
  Lock In: settings auto-set from histogram
     v
  Waveform view: buffer overview strip always shows "where you are" in the capture
     v
  If unstable: switch to SPLIT -- histogram + waveform live simultaneously
     v
  If hunting a glitch: PERSIST mode -- all captures overlaid as density
```

---

---

## Two-channel waveform view

When both CH1 and CH2 are active the display has two sub-modes toggled by F2.

### OVERLAY mode (default for two-channel)

Both channels on the same voltage grid. CH1 = yellow, CH2 = cyan (16-bit colour).
Each channel has its own ground marker (>) on the left axis showing its position relative
to the grid.

```
+------------------------------------------------------------------------------+
| [###........[[*][*][*][*][*][*][*]]................T.................................]  |  buffer strip
+------------------------------------------------------------------------------+
|                                                                              |
| 4V - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  |
| CH1>                                                                        |
| 3V ---------------------------------------------------------------------   |
| >   +----+    +----+    +----+    +----+    +----+    +----+     (CH1 yel) |
| 2V -+    +----+    +----+    +----+    +----+    +----+    +--           |
|     +----+    +----+    +----+    +----+    +----+    +----+             |
| 1V - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  |
| CH2>--------------/\--------------/\--------------/\--           (CH2 cyn) |
| 0V -------------/    \----------/    \----------/    \--                   |
|           -----/        \--------      \--------        \---               |
|                                                                              |
+------------------------------------------------------------------------------+
| CH1:1V CC65*DC  CH2:500mV 10x*DC  500us/div  ^CH1 2.5V AUTO  >RUN        |
+------------------------------------------------------------------------------+
|  [F1: EXPLORE]  [F2: SPLIT 2CH v]  [F3: MEAS]  [F4: CURSOR]               |
+------------------------------------------------------------------------------+

CH1> = ground reference position marker for CH1 on the left axis
CH2> = ground reference position marker for CH2
Both markers are movable with CH1/CH2 key + ^v
```

### SPLIT 2CH mode (F2 press -- two independent half-height grids)

Better when the two channels have very different V/div (e.g. CAN-H and CAN-L, or
voltage + current from a current clamp).

```
+------------------------------------------------------------------------------+
| [buffer overview strip -- both channels shown in their own colour]            |
+------------------------------------------------------------------------------+
| CH1 -- 1V/div  CC-65  DC                                          (80px)     |
| 3.3V- - -                                                                   |
|        +--+  +--+  +--+  +--+  +--+  +--+  +--+                          |
|  1V ---+  +--+  +--+  +--+  +--+  +--+  +--+  +--                        |
|  >     +--+  +--+  +--+  +--+  +--+  +--+  +--+                          |
|  0V- - -                                                                    |
+------------------------------------------------------------------------------+
| CH2 -- 500mV/div  10x  DC                                         (80px)    |
|  1V- - - /\- - - - - -/\- - - - - -/\- - - - - -                       |
|          /  \          /  \          /  \                                   |
|  0V-----/    \--------/    \--------/    \--------------                   |
|  >  --/        \----/        \----/        \----                           |
| -1V- - -                                                                   |
+------------------------------------------------------------------------------+
| CH1:1V CC65*DC  CH2:500mV 10x*DC  500us  ^CH1 2.5V AUTO  >RUN            |
+------------------------------------------------------------------------------+
|  [F1: EXPLORE]  [F2: OVERLAY ^]  [F3: MEAS]  [F4: CURSOR]                 |
+------------------------------------------------------------------------------+

Split 2CH is also the most natural view for differential signals:
  CAN bus: CH1 = CAN-H (probe + to CAN-H, - to CAN-L)
           CH2 = CAN-L (probe + to CAN-L, - to CAN-H)
  The split view shows both wires independently; math CH1-CH2 shows the
  differential signal as a third trace when MATH is active.
```

---

## Measurement panel (MEAS key)

MEAS key toggles the measurement panel. It overlays the right ~110px of the screen.
The waveform display narrows slightly; the panel shows 6 active measurements.

```
+------------------------------------------------------------------------------+
| [buffer overview strip]                                                      |
+------------------------------------------------------------------------------+
|                              | MEASUREMENTS         [F3: EDIT] [F4: CLOSE] |
|  3.3V- - - - - - - - - -  |                                              |
|                              | CH1  FREQ    1.020kHz                        |
|      +--+   +--+   +--+    | CH1  PERIOD  980.4us                         |
| 1V --+  +---+  +---+  +-   | CH1  DUTY+    49.0%                          |
|      +--+   +--+   +--+    | CH1  Vp-p     3.29V                          |
|                              | CH1  Vmax     3.31V                          |
|  CH2- - - - (off)- - - -  | CH1  Vmin     0.02V                          |
|                              |                                              |
+------------------------------+                                              |
| CH1:1V CC65*DC  500us  AUTO | ^v to select   OK to change   F3 to edit   |
+------------------------------+----------------------------------------------+
|  [F1: EXPLORE]  [F2: SPLIT/HIST]  [F3: EDIT MEAS]  [F4: CLOSE MEAS]       |
+------------------------------------------------------------------------------+
```

### Available measurements (EDIT panel -- F3)

```
+------------------------------------------------------------------------------+
|  EDIT MEASUREMENTS -- select up to 6 active                    [F4: DONE]    |
+------------------------------------------------------------------------------+
|  SLOT  SOURCE  MEASUREMENT         CURRENT VALUE                            |
|  --------------------------------------------------------------------------  |
|  1 >   CH1     Frequency            1.020 kHz                               |
|  2     CH1     Period               980.4 us                                |
|  3     CH1     Duty cycle +          49.0 %                                 |
|  4     CH1     Vp-p                   3.29 V                                |
|  5     CH1     Vmax                   3.31 V                                |
|  6     CH1     Vmin                   0.02 V                                |
|  --------------------------------------------------------------------------  |
|  AVAILABLE (v^ scroll, OK add):                                              |
|    Vmean         Vrms           Rise time      Fall time                     |
|    Width+        Width-         Overshoot      Pre-shoot                    |
|    Phase CH1->2   Delay CH1->2   CH2 versions of all above                   |
|                                                                              |
|  OK on slot: change source/type.  OK on available list: add to empty slot.  |
|  <- on active slot: remove from panel.                                       |
|  [F1: CLEAR ALL]  [F2: CH1 DEFAULTS]  [F3: CH2 DEFAULTS]  [F4: DONE]       |
+------------------------------------------------------------------------------+
```

All measurements run continuously on the current captured buffer. They update each acquisition.
Rise/fall time threshold = 10%-90% by default; settable in UTIL.

---

## Cursor mode (F4: CURSOR in waveform modes)

Two cursor types: time cursors (T1, T2 = vertical lines) and voltage cursors (V1, V2 = horizontal).
Both types can be active simultaneously.

```
+------------------------------------------------------------------------------+
| [buffer overview -- T1 and T2 markers shown in overview strip]                |
+------------------------------------------------------------------------------+
|                    |                            |                            |
|  3.3V- - - - - - -|- - - - - - - - - - - - -|- - - - - - - - - - -   |
|  - - - - - V1 - - - - - - - - - - - - - - - - - - - - - - - - - - -  |
|              +----+ |  +----+     +----+     +----+ |  +----+     +--     |
|  1V ---------+    +-|--+    +-----+    +-----+    +-|--+    +-----+       |
|  - - - - - V2- - - - - - - - - - - - - - - - - - - - - - - - - - -  |
|              +----+ |  +----+     +----+     +----+ |  +----+              |
|                     T1                               T2                     |
|  +----------------------------------------------------------------------+   |
|  | T1=490us  T2=1470us  DT=980us  f=1/DT=1.020kHz                     |   |
|  | V1=3.29V  V2=0.02V  DV=3.27V                                        |   |
|  | V@T1(CH1)=3.29V    V@T2(CH1)=3.29V                                  |   |
|  +----------------------------------------------------------------------+   |
+------------------------------------------------------------------------------+
|  [F1: MOVE T1 <-->]  [F2: MOVE T2 <-->]  [F3: MOVE V1^/v / V2^/v]  [F4: EXIT]   |
+------------------------------------------------------------------------------+
```

**Cursor controls:**
- F1: select T1 as active -- <- -> moves T1 (fine); ZOOM+<- -> moves coarse
- F2: select T2 as active -- same
- F3: cycles V1/V2 as active -- ^v moves the selected V cursor
- Active cursor = solid line; inactive = dashed
- OK while F1 active: T1 snaps to nearest waveform peak/edge ("snap to feature")
- BACK: exit cursor mode, return to previous mode

**Cursor readout (always shown while cursors active):**
```
  Time cursors:   T1=490us   T2=1470us   DT=980.0us   f=1.020kHz
  Voltage cursors: V1=3.29V   V2=0.02V    DV=3.27V
  At-cursor:       V@T1=3.29V  V@T2=3.29V   (voltage of waveform at each time cursor)
```

If MEAS panel is also open, cursors override the cursor-measurement slots.

---

## Record / Trend mode (MODE key 4th press -- after Persist)

Captures one measurement value per sample interval and plots a strip chart.
Equivalent to the Fluke 190's RECORD mode. Ideal for: monitoring battery voltage
over time, watching a temperature sensor output drift, capturing a slow-changing signal.

```
+------------------------------------------------------------------------------+
|  RECORD  Measuring: CH1 Vmax    Interval: 1s    Duration: 5m 00s            |  header
+------------------------------------------------------------------------------+
|                                                                              |
|  3.50V - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  |
|                                                                              |
|         _________________________________________________________        |
|  3.30V -############################################################- -  |
|                                                                              |
|  3.10V - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  |
|         <- 300 seconds ago                                          NOW ->    |
|                                                                              |
|  +----------------------------------------------------------------------+   |
|  | CURRENT: 3.29V   MIN: 3.21V   MAX: 3.34V   MEAN: 3.29V   n=182     |   |
|  +----------------------------------------------------------------------+   |
+------------------------------------------------------------------------------+
|  [F1: WHAT ^/v]   [F2: INTERVAL ^/v]   [F3: CLEAR]   [F4: WAVEFORM VIEW]      |
+------------------------------------------------------------------------------+
```

**F1: WHAT to record (cycles):**
CH1 Vmax -> CH1 Vmin -> CH1 Vmean -> CH1 Vp-p -> CH1 Freq -> CH1 Vrms ->
CH2 same options -> DMM reading -> back to CH1 Vmax

**F2: INTERVAL (cycles):**
100ms -> 500ms -> 1s -> 5s -> 10s -> 30s -> 60s

**Chart behaviour:**
- X axis always shows the last N samples, scrolling as new samples arrive
- Y axis auto-scales to fit MIN-MAX with 10% headroom
- MIN and MAX horizontal gridlines are always shown
- F3: CLEAR erases all stored samples and restarts
- F4: WAVEFORM VIEW opens a live waveform view of the current acquisition alongside the chart
- SNAP saves the current chart image (compressed strip) to one of the 6 flash slots

**Record mode also captures DMM readings:**
If "DMM reading" is selected as the source, the device runs the CS7721 DMM and plots
the multimeter reading -- effectively a datalogger. This is exactly the Fluke 190's
RECORD-Meter mode.

---

## XY mode

Activated when XY is selected in firmware (from the existing Display Mode parameter).
CH1 = horizontal (X), CH2 = vertical (Y). Shows the phase relationship between two signals.

```
+------------------------------------------------------------------------------+
|  XY MODE     CH1->X  CH2->Y     Timebase: N/A                                 |
+------------------------------------------------------------------------------+
|                                                                              |
|     +5V                                                                     |
|      |                   *  *  *                                            |
|      |               *          *                                           |
|      |            *                *                                        |
|      |           *                  *                                       |
|   0V-+----------*--------------------*---------------------- CH1 (X)       |
|      |           *                  *                                       |
|      |            *                *                                        |
|      |               *          *                                           |
|      |                   *  *  *                                            |
|     -5V                                                                     |
|                  -5V        0V        +5V                                   |
|  Circle = same frequency, 90deg phase. Ellipse = phase between 0deg and 90deg.   |
+------------------------------------------------------------------------------+
|  CH1:1V 10x  CH2:1V 10x  (no trigger in XY mode)                           |
+------------------------------------------------------------------------------+
|  [F1: EXPLORE]  [F2: WAVEFORM]  [F3: PERSIST XY]  [F4: LISSAJOUS GUIDE]   |
+------------------------------------------------------------------------------+
```

F4: LISSAJOUS GUIDE shows a quick reference chart of common Lissajous figures and what
ratio and phase they represent -- helps users who don't memorise the patterns.

---

## Roll mode (slow signals -- auto-activates at >=100ms/div)

When timebase >=100ms/div the firmware supports roll/scan mode. The display becomes a
continuous strip chart of the incoming signal -- like a paper chart recorder.

```
+------------------------------------------------------------------------------+
|  ROLL   500ms/div   CH1 1V/div   RUNNING ->->->                                |
+------------------------------------------------------------------------------+
|                                                                              |
|  3V - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -   |
|                                                                              |
|         +--+          +------------------------------------------------+ NOW|
|  1V ----+  +----------+                                                |   |
|  >      +--+          |                                                |   |
|  0V - - - - - - - - - | - - - - - - - - - - - - - - - - - - - - -  |
|                        |                                                |   |
|                        +------------------------------------------------+   |
|         <- older data                                         newest -> NOW   |
|                                                                              |
+------------------------------------------------------------------------------+
|  CH1:1V 10x*DC   ROLL 500ms/div   no trigger (free-running)   > ROLL       |
+------------------------------------------------------------------------------+
|  [F1: TBASE ^/v]  [F2: FREEZE]  [F3: MEAS]  [F4: NORMAL TRIGGER]            |
+------------------------------------------------------------------------------+
```

Roll mode notes:
- Buffer overview strip is suppressed in Roll (there is no finite capture window)
- Trigger is disabled -- data scrolls continuously left
- F2: FREEZE pauses the roll (equivalent to STOP) so you can examine the trace
- F4: NORMAL TRIGGER exits Roll and re-enables triggering at this timebase
- SINGL in Roll: captures one screen-width and freezes (equivalent to single-shot at slow speed)

---

## Math mode (F3 in 2-channel waveform)

Math adds a third trace to the waveform display. Activated via the F3 menu in Waveform mode.

```
+------------------------------------------------------------------------------+
| [buffer overview strip -- math trace shown in magenta in overview]           |
+------------------------------------------------------------------------------+
|                                                                              |
| CH1(Y)  +--+  +--+  +--+  +--+  +--+                        (yellow)     |
|  3V ----+  +--+  +--+  +--+  +--+  +--                                   |
|         +--+  +--+  +--+  +--+  +--+                                     |
| CH2(C)  -----------------/\-----------------/\--              (cyan)       |
|                          /  \               /  \                            |
| MATH(M) +--+  +--+  +--+    +--+  +--+  ++    +--  CH1-CH2  (magenta)  |
|  0V ----+  +--+  +--+          +--+  +--+                                |
|         +--+  +--+  +----------+  +--+  +----------                      |
|                                                                              |
|  MATH: CH1 - CH2    Scale: 1V/div   Position: mid-grid                      |
+------------------------------------------------------------------------------+
|  CH1:1V*DC  CH2:1V*DC  500us  ^CH1 AUTO  MATH: CH1-CH2 1V/div   > RUN    |
+------------------------------------------------------------------------------+
|  [F1: MATH TYPE ^/v]   [F2: MATH SCALE ^/v]   [F3: MATH POS ^/v]   [F4: OFF]   |
+------------------------------------------------------------------------------+
```

**Math types (F1 cycles):**
- CH1 + CH2 -- useful for common-mode rejection check
- CH1 - CH2 -- differential signal (CAN, LIN, RS-485)
- CH2 - CH1 -- same, inverted
- FFT(CH1) -- spectrum of CH1 (see FFT section)
- FFT(CH2) -- spectrum of CH2

### FFT display

FFT replaces the math waveform with a frequency-domain view. Split display with time
domain on top and FFT on bottom.

```
+------------------------------------------------------------------------------+
| [buffer overview strip]                                                      |
+------------------------------------------------------------------------------+
|                                                                              |
|      +--+   +--+   +--+   +--+   (time domain, top 80px)                  |
| 1V --+  +---+  +---+  +---+  +--                                          |
|      +--+   +--+   +--+   +--+                                            |
+------------------------------------------------------------------------------+
|  dBV |                                                                      |
|   0  |  ####  fundamental @ 1.020kHz                                        |
| -20  |  ####  - - - - - - - - - - - - - - - - - - - - - -             |
| -40  |    ##  third harmonic @ 3.060kHz                                     |
| -60  |    - - - - - - - - - - - - - - - - - - - - - - - -             |
|      +----------------------------------------------------------------      |
|       0Hz             5kHz            10kHz           15kHz                 |
|  Peak: 1.020kHz  -2.1dBV  (3.29Vpp ~ 1.17Vrms = -2.1dBV)                 |
+------------------------------------------------------------------------------+
|  [F1: WINDOW ^/v]   [F2: FREQ SPAN ^/v]   [F3: dBV/Vrms ^/v]   [F4: CLOSE FFT] |
+------------------------------------------------------------------------------+
```

Window types (F1): Rectangle | Hanning | Hamming | Blackman | Flat-top
The recommended default is Hanning (best for general signals).
Frequency span auto-scales to Nyquist/2 (= sample rate/4 for real FFT).

---

## Status strip -- complete design

The status strip is the 15px band at the bottom of every scope view. Always present.
It is the instrument's "instrument panel" -- a complete state snapshot at a glance.

```
Layout (320px x 15px):

+-----------------------------------------------------------------------------+
| C1:50A CC65*DC  C2:-  |  500us  |  ^C1 8.2A AUTO  |  >RUN  [bat]75%  USB   |
+-----------------------------------------------------------------------------+
  <--- channel info ->  < tbase >  <--- trigger ---->  <--- system ------>

Column widths:  channel info = 100px  tbase = 50px  trigger = 100px  system = 70px
```

**Channel info (leftmost 100px):**
```
  CH1 on:    C1:1V 10x*DC     (scale, probe short-name, coupling)
  CH1 off:   C1:-             (dash = off)
  Current:   C1:50A CC65*DC   (unit changes to A, probe name replaces x)
  Rogowski:  C1:200A CWT*AC[!]  ([!] = AC only warning always visible)
  CH2 same but abbreviated:   C2:1V*DC  or  C2:-
```

**Timebase (50px):**
```
  500us     (current T/div, no label -- context is always "per division")
  ROLL 1s   (in roll mode: word ROLL + interval)
  XY        (in XY mode: just "XY")
```

**Trigger (100px):**
```
  ^C1 1.6V AUTO    (^=rising edge, source, level, mode)
  vC2 0.5A NORM    (v=falling edge, C2=CH2, current probe = shows A)
  ^C1 AUTO WAIT    (WAIT = trigger armed but hasn't fired yet)
  ^C1 AUTO TRIG'd  (TRIG'd = trigger fired this acquisition)
  --- FREE ---     (no trigger -- roll/free-run mode)
```

**System (rightmost 70px):**
```
  >RUN   [bat]75%    (running, battery 75%)
  [*]STOP  [bat]75%    (stopped)
  [*]STOP  [bat]20% !  (battery warning -- ! added below 20%)
  >RUN   [!]USB     (USB connected, charging -- scope may run noisier)
  SINGL  [bat]75%    (single-shot armed)
  SINGL[ok] [bat]75%    (single-shot captured -- checkmark)
```

**Active parameter override** (TBASE/VOLT/TRIG key pressed):
The relevant strip section highlights (invert colours) while the parameter is being adjusted:
```
  C1:1V 10x*DC  C2:-  | [>500us<] |  ^C1 1.6V AUTO  | >RUN  [bat]75%
                              ^^ highlighted while TBASE is active
```

---

## Boot sequence

### Power-on flow

```
Power button pressed
        |
        v
[device logo, 0.5s]
        |
        v
Self-test (50ms):
  - ADC init check
  - FPGA version read (log to debug)
  - DMM comms check (CS7721 ping)
  - Battery level read
        |
        +-- Battery < 5%? -> "BATTERY TOO LOW -- connect USB to charge" -> off
        |
        v
Restore saved state from flash:
  - Last channel settings (V/div, coupling, probe)
  - Last timebase and trigger
  - Last display mode (Explorer/Waveform/etc.)
  - Last probe library recently-used list
        |
        +-- First power-on ever (no saved state)?
        |      |
        |      +-- Run FIND automatically on CH1
        |          Show Explorer view with result
        |
        +-- Saved state restored?
               |
               +-- Show last mode with last settings
                   Show 2-second banner: "SETTINGS RESTORED"
                   If CH1 or CH2 has a current/Rogowski probe configured:
                     Show "[!] PROBE: CC-65 on CH1 -- re-zero before use?"
                     F2: YES re-runs the probe zero-cal flow
                     F4: NO proceeds immediately
```

### First-use experience (no saved state)

```
+------------------------------------------------------------------------------+
|  WELCOME TO HANTEK 2D82AUTO                                                  |
|                                                                              |
|  This instrument combines:                                                   |
|    SCOPE  -- 2-channel oscilloscope, 80 MHz                                  |
|    METER  -- digital multimeter (DMM key)                                    |
|    AWG    -- waveform generator (AWG key)                                    |
|    AUTO   -- automotive guided tests (UTIL -> Vehicle Diag)                   |
|                                                                              |
|  START: connect a probe to CH1 and press FIND.                              |
|  FIND will detect the signal and set everything up automatically.            |
|                                                                              |
|  [F4: FIND NOW (recommended)]      [F3: SKIP -- go to scope directly]        |
+------------------------------------------------------------------------------+
```

---

## Error states and warnings

### Input overrange

When the signal exceeds the current V/div scale, the waveform clips at the top or bottom.
The channel label in the status strip flashes with `OVR`:

```
  Status strip while CH1 is clipping:   C1:1V 10x*DC [OVR!]  C2:-  | 500us ...

  On the waveform, the clipped portion is shown in a different colour (red) so
  it's visually distinct from data that's actually in range.

  An automatic suggestion appears below the status strip for 3 seconds:
  "CH1 overrange -- press VOLT ^ to increase scale"
```

### No trigger (trigger timeout)

If the trigger mode is NORM or SINGL and no trigger event fires within 3 seconds:

```
  Status strip:   ... | ^C1 1.6V NORM WAIT! | ...

  After 5 seconds without trigger, a yellow banner appears:
  +------------------------------------------------------------------------+
  | [!] TRIGGER TIMEOUT -- no trigger event in 5 seconds                      |
  | - Check trigger level (TRIG key) -- is it inside the signal range?      |
  | - Check trigger source (the histogram shows where the signal is)       |
  | [F1: SHOW HISTOGRAM]  [F2: TRY AUTO MODE]  [F3: DISMISS]              |
  +------------------------------------------------------------------------+
```

F1: SHOW HISTOGRAM switches momentarily to Explorer mode so the user can see
where the signal is vs where the trigger level is set.
F2: switches trigger to AUTO mode (which acquires even without a trigger event).

### No signal detected

When FIND runs and the histogram is flat:
```
  +------------------------------------------------------------------------+
  | NO SIGNAL DETECTED on CH1                                              |
  |                                                                        |
  | - Probe connected? Check BNC is fully clicked in.                      |
  | - Probe ground clip attached?                                          |
  | - Signal present? Use DMM (DMM key) to check for DC voltage first.    |
  | - CH1 coupling: currently DC. If signal is AC-only, try: VOLT -> AC   |
  |                                                                        |
  |  [F1: TRY CH2]   [F2: SWITCH TO DMM]   [F3: TRY AC COUPLING]         |
  +------------------------------------------------------------------------+
```

### Battery warnings

```
  20%:  Status strip [bat] icon turns amber. No interruption.
  10%:  Banner: "[!] BATTERY LOW -- connect USB-C to charge"
   5%:  Banner repeated every 60 seconds.
   2%:  "BATTERY CRITICAL -- saving state and shutting down in 30 seconds"
         Counts down. Saves all channel settings + last waveform to flash.
         At 0s: controlled power-off.
```

### USB connected (noise warning)

The USB-C connection creates ground noise (laptop/PC switching supply).
When USB is connected and scope is running:
```
  Status strip shows: >RUN [!]USB
  A one-time warning appears:
  "USB connected -- scope ground is now tied to USB host ground.
   This may add noise to measurements.
   For lowest noise: disconnect USB after charging, or use a USB isolator."
  [F4: OK -- don't show again]
```

---

## Settings / UTIL screen

Accessed via F4: UTIL in most modes.

```
+------------------------------------------------------------------------------+
|  SETTINGS                                                     [F4: BACK]    |
+------------------------------------------------------------------------------+
|  DISPLAY                                                                     |
|    Language:         > English <  /  ZHWEN                                    |
|    Backlight:        > 7 / 10 <   (^v adjust)                              |
|    Backlight timeout:> 60s <       30s / 60s / 90s / 120s / Never           |
|    Theme:            > Dark <      Dark / Light                              |
|                                                                              |
|  POWER                                                                       |
|    Auto shutdown:    > 10 min <    5m / 10m / 20m / 30m / Never            |
|    USB charge mode:  > Charge+run <  Charge+run / Charge only               |
|                                                                              |
|  SOUND                                                                       |
|    Key click:        > Off <       Off / Soft / Loud                         |
|    Trigger beep:     > Off <       Off / On  (beeps on each trigger event)  |
|    Warning beep:     > On <        On / Off                                  |
|                                                                              |
|  MEASUREMENTS                                                                |
|    Rise/fall threshold: > 10%/90% <   10/90 / 20/80                        |
|    Freq gate time:      > 1 cycle <   1 / 3 / 10 cycles                    |
|                                                                              |
|  CALIBRATION                                                                 |
|    > CAL MODE A -- zero offset (short inputs)                                |
|      CAL MODE B -- gain + noise (connect battery)                            |
|      CAL MODE C -- probe compensation                                        |
|      CAL MODE D -- AWG self-calibration                                      |
|      View cal table (read-only)                                              |
|      Reset to factory cal                                                    |
|                                                                              |
|  SYSTEM                                                                      |
|    Firmware version:  HTX2D82-2021090901                                    |
|    FPGA version:      V04                                                    |
|    Serial:            --                                                      |
|    Default settings:  [RESET ALL TO DEFAULTS]                               |
|    DFU update mode:   [ENTER DFU]  (hold F1 + power = same)                |
|                                                                              |
|  [F1: ^ page]   [F2: v page]   [F3: SELECT / CHANGE]   [F4: BACK]         |
+------------------------------------------------------------------------------+
```

---

## Save / Recall (SNAP key)

The device has 6 waveform save slots in STM32 flash (from firmware analysis).

**SNAP tap** -- saves immediately to the next empty slot (or overwrites oldest if all full):
```
  Saves:  current screen image (compressed)
          channel settings at time of save
          captured waveform buffer (raw ADC samples)
          probe configuration for each channel
          timestamp (seconds since first power-on -- no RTC, so relative time only)
  Status strip briefly shows: "SAVED to slot 3 [ok]"
```

**SNAP hold** -- opens the recall browser:
```
+------------------------------------------------------------------------------+
|  SAVED WAVEFORMS                                           [F4: BACK]       |
+------------------------------------------------------------------------------+
|                                                                              |
|  SLOT 1  #############  CH1:1V 10x  500us  TRIG AUTO    <- mini thumbnail  |
|  SLOT 2  #############  CH1:1V CC65 500us  TRIG AUTO   (3x20px)           |
|  SLOT 3  #############  CH1:5V 10x  5ms    TRIG NORM                      |
|  SLOT 4  - empty -                                                          |
|  SLOT 5  - empty -                                                          |
|  SLOT 6  - empty -                                                          |
|                                                                              |
|  ^v to select a slot                                                        |
|                                                                              |
|  [F1: LOAD -- restore this waveform]  [F2: DELETE]  [F3: COMPARE with live] |
+------------------------------------------------------------------------------+
```

**F3: COMPARE** loads the saved waveform as a reference trace (grey) overlaid on the live
waveform -- useful for comparing the current signal to a known-good capture.
This is the equivalent of the Fluke 190's "REF" waveform overlay.

---

## Mode cycle summary (MODE key)

```
  Explorer -> Waveform -> Split/Hist -> Persist -> Record -> [repeat]

  Shortcut rules (always work regardless of mode):
    FIND   -> always goes to Explorer with fresh auto-setup
    DMM    -> always goes to full-screen DMM (any mode)
    AWG    -> always goes to full-screen AWG (any mode)
    BACK from DMM/AWG -> returns to last scope mode
    RUN[*]   -> always toggles acquisition
    CH1/CH2 tap -> always toggles that channel on/off
    CH1/CH2 hold -> always opens channel settings panel
    SNAP   -> always saves (tap) or recalls (hold)
```

---

## Implementation priority

1. **DMM giant-font display** -- most visible improvement, CS7721 output is already read
2. **Buffer overview strip** -- always present in Waveform mode; solves "I'm lost"
3. **Amplitude histogram** -- Explorer mode; enables signal discovery without waveform
4. **FIND algorithm** -- smarter auto-setup using histogram; removes the knowledge gate
5. **Status strip** -- complete design above; always accurate; foundation for everything else
6. **Probe library + Rogowski handling** -- unit conversion + connection guides
7. **Measurement panel** -- MEAS key overlay, 6 live measurements, edit screen
8. **Two-channel view** -- overlay + split-2ch, ground markers on left axis
9. **Cursor mode** -- T1/T2 time + V1/V2 voltage cursors + readout box
10. **CAL mode A** (zero offset) -- fills existing `CaliParam` table
11. **CAL mode B** (battery/gain) -- gain trim + noise floor report
12. **CAL mode C** (probe comp) -- AWG as signal source, visual guide
13. **CAL mode D** (AWG cross-cal) -- self-cal, highest accuracy
14. **Quick-access parameter mode** (TBASE/VOLT/TRIG + arrows) -- removes menu diving
15. **Error states + warnings** -- overrange, trigger timeout, no signal, battery
16. **Record/Trend mode** -- strip chart over time (Fluke-style)
17. **Math + FFT** -- differential trace + spectrum
18. **Persistence mode** -- density display, eye diagrams
19. **Roll mode** -- existing firmware, improved display
20. **XY mode** -- existing firmware, add Lissajous guide
21. **Save/Recall** -- SNAP + compare-with-reference overlay
22. **Boot sequence** -- first-use wizard + restore flow
23. **Settings screen** -- UTIL complete
24. **Automotive guided test improvements** -- PASS/FAIL overlay on existing library
