# 2D82 — Clean-room Reimplementation Plan

**Target:** STM32 (same family — F4 or F7 upgrade TBD) · Zephyr RTOS · LVGL 9.x  
**Display:** 2.8" ILI9341, 320×240 landscape (same physical panel as original)  
**Input:** 4×5 key matrix (20 keys) + 4 F-softkeys beside LCD  
**Peripherals:** SDIO µSD · USB-C (DFU + CDC) · FPGA (via SPI/FSMC) · CS7721 DMM (bit-bang USART) · AWG DAC (SPI) · ILI9341 (FSMC)

**Reference docs:**
- `vendors/hantek/2d82/docs/reimplementation-guidance.md` — concise cross-subsystem rules and hardware guidance notes
- `vendors/hantek/2d82/docs/ui-redesign-concept.md` — screen layouts, key assignments, design principles
- `vendors/hantek/2d82/firmware/2d82-20210818/analysis/decompiled_named.c` — original firmware (read-only reference)
- `vendors/hantek/2d82/docs/app/app.md` — PC app / DLL API (USB protocol reference)

---

## Approach

This is a **clean-room reimplementation**:

- Read `decompiled_named.c` to understand *what* the device does (behaviour, parameter ranges, limits, calibration structure). Do not copy code structure or algorithms verbatim.
- The original firmware is a bare-metal STM32F103 monolith. The new firmware is a Zephyr + LVGL layered application with a proper hardware abstraction layer.
- Phase 1 goal: **feature parity** — same screens, same key behaviour, same USB protocol. The UI *design* follows `ui-redesign-concept.md`, not the original.
- Phase 2+: new features (histogram, buffer overview, Rogowski probes, FIND auto-setup, user-accessible calibration).

---

## Layer model

```
┌────────────────────────────────────────────────────────────────────┐
│  Application layer (app/)                                          │
│  ┌───────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │  Screen tree  │  │ Instrument   │  │  USB host    │           │
│  │  (LVGL views) │  │ state machine│  │  protocol    │           │
│  └───────┬───────┘  └──────┬───────┘  └──────┬───────┘           │
│          │                 │                  │                   │
│  ┌───────▼─────────────────▼──────────────────▼───────┐           │
│  │             Instrument API (instrument/)            │           │
│  │  scope_api · dmm_api · awg_api · cal_api           │           │
│  └───────────────────────┬────────────────────────────┘           │
│                          │                                        │
│  ┌───────────────────────▼────────────────────────────┐           │
│  │             Hardware drivers (drivers/)             │           │
│  │  fpga · dmm_cs7721 · awg_dac · lcd_ili9341         │           │
│  │  sdio_fs · usb · keypad                            │           │
│  └───────────────────────────────────────────────────┘           │
└────────────────────────────────────────────────────────────────────┘
```

---

## Phase 1 scope

**Deliver:** Every screen listed in §Screen tree below, navigable via all 20+4 keys, connected to real hardware (FPGA scope capture, DMM, AWG). USB protocol compatible with existing PC app. SDIO waveform save/recall.

**Explicitly NOT in Phase 1:**
- Histogram (Explorer mode)  
- Buffer overview strip  
- FIND auto-setup  
- Persist/density mode  
- Probe library  
- Rogowski coil handling  
- Automotive tests  
- User-accessible calibration modes A–D  

These are Phase 2+. The screen tree stubs them with "Coming soon" placeholders so navigation is not broken.

---

## Screen tree

Screens are LVGL `lv_obj_t` root panels, managed by a central `screen_manager`. Only one screen is active (displayed) at a time; the manager push/pop stack handles BACK correctly.

```
ROOT
├── scope/                        ← Phase 1 core
│   ├── waveform_view             ← Main scope view (waveform + status bar + softkeys)
│   ├── channel_menu[2]           ← CH1/CH2 settings (coupling, probe ratio, invert, BWL)
│   ├── trigger_menu              ← Trigger: source, slope, level, sweep mode
│   ├── timebase_menu             ← T/div + horizontal pos
│   ├── measurement_panel         ← Overlay: up to 5 simultaneous measurements
│   ├── cursor_overlay            ← Time/voltage cursors (ΔT, ΔV, 1/ΔT)
│   ├── math_channel_menu         ← Math: CH1+CH2, CH1−CH2, FFT
│   ├── fft_view                  ← Full-screen FFT (or split with waveform)
│   ├── xy_mode_view              ← X-Y (Lissajous) mode
│   ├── memory_menu               ← Save/recall waveform (2 slots in internal flash; unlimited on SDIO)
│   └── [explorer_view]           ← Phase 2: histogram + FIND
│
├── dmm/                          ← Phase 1 core
│   ├── dmm_view                  ← Giant reading + bar + MIN/MAX/AVG + trend strip
│   └── dmm_function_menu         ← Cycle: DC-V, AC-V, Res, Cap, Cont, Diode, Cur, Temp
│
├── awg/                          ← Phase 1 core
│   ├── awg_view                  ← Type + Freq + Amp + Offset + waveform preview
│   └── awg_waveform_menu         ← Sine, Square, Triangle, Trapezoid, Ramp, ARB1-4
│
├── automotive/                   ← Phase 1: stub; Phase 2: full 89-test library
│   └── [auto_test_view]
│
├── cal/                          ← Phase 1: factory defaults only; Phase 2: modes A-D
│   ├── [cal_zero_view]           ← Mode A: short-circuit offset cal
│   ├── [cal_gain_view]           ← Mode B: battery gain cal
│   ├── [cal_probe_comp_view]     ← Mode C: probe compensation
│   └── [cal_awg_xref_view]       ← Mode D: AWG↔scope cross-cal
│
├── util/
│   ├── util_menu                 ← Backlight, language, default setup, firmware info
│   ├── settings_save_menu        ← Save/load named settings (SDIO)
│   └── firmware_info_view        ← Version, serial, FPGA ver, DMM chip
│
└── [probe_browser]               ← Phase 2: probe library (PicoScope model)
```

`[bracketed]` = Phase 2 stub in Phase 1.

---

## Instrument API

The instrument API is the clean-room boundary. It is defined by the behaviour observed in the firmware, not by its internal data structures.

### `scope_api.h`

```c
// Channel parameters
typedef enum { COUPLE_DC, COUPLE_AC, COUPLE_GND } ch_coupling_t;
typedef enum { PROBE_1X, PROBE_10X, PROBE_100X, PROBE_1000X } ch_probe_t;

// V/div steps (mV units): 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000
// (from fw_ch_param tables in named.c — 10 steps per channel)
typedef struct {
    bool     enabled;
    ch_coupling_t coupling;
    ch_probe_t    probe;
    int32_t  vdiv_mv;       // millivolts/div, one of the 10 legal values
    int32_t  vpos_pct;      // vertical position, −100..+100 (percent of screen)
    bool     bw_limit;      // 20 MHz BW limit
    bool     invert;
} ch_config_t;

// Trigger
typedef enum { TRIG_SRC_CH1, TRIG_SRC_CH2, TRIG_SRC_EXT } trig_src_t;
typedef enum { TRIG_SLOPE_RISE, TRIG_SLOPE_FALL, TRIG_SLOPE_EITHER } trig_slope_t;
typedef enum { TRIG_SWEEP_AUTO, TRIG_SWEEP_NORMAL, TRIG_SWEEP_SINGLE } trig_sweep_t;
typedef enum { TRIG_TYPE_EDGE, TRIG_TYPE_VIDEO } trig_type_t;

typedef struct {
    trig_src_t   source;
    trig_slope_t slope;
    trig_sweep_t sweep;
    trig_type_t  type;
    int32_t      level_mv;   // trigger level in millivolts (probe-corrected)
    int32_t      hpos_pct;   // horizontal position (trigger point) −100..+100
} trig_config_t;

// Timebase
// T/div steps (ns): 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000,
//                   10000, 20000, 50000, 100000 (100ms), 200000, 500000, 1000000 (1s)
// (from fw_apply_timebase_freq / fw_timebase_lookup in named.c)
typedef struct {
    uint32_t tdiv_ns;     // one of the legal ns/div values above
    bool     roll_mode;   // roll mode active (only at slowest timebases)
} timebase_config_t;

// Acquisition state
typedef enum {
    ACQ_RUNNING,
    ACQ_TRIGGERED,  // single-shot: waiting for trigger
    ACQ_STOPPED,
    ACQ_COMPLETE,   // single-shot: capture done
} acq_state_t;

void scope_ch_set(int ch, const ch_config_t *cfg);
void scope_ch_get(int ch, ch_config_t *cfg);
void scope_trig_set(const trig_config_t *cfg);
void scope_trig_get(trig_config_t *cfg);
void scope_timebase_set(const timebase_config_t *cfg);
void scope_timebase_get(timebase_config_t *cfg);
void scope_run(void);
void scope_stop(void);
void scope_single(void);
acq_state_t scope_state(void);
// Returns raw ADC samples (8-bit offset-binary: 0x80=0V, 0x00=-FS, 0xFF=+FS) for one channel.
// Buffer is 6144 bytes per channel. Dual-interleaved in fast mode: even=CH1, odd=CH2.
// Caller maps to mV using: mv = (int8_t)(sample - 0x80) * vdiv_mv * 8 / 128 + vpos_mv + cali_offset
int  scope_get_samples(int ch, uint8_t *buf, size_t *count);

// Math channel
typedef enum { MATH_ADD, MATH_SUB, MATH_FFT, MATH_MUL } math_op_t;
void scope_math_set(math_op_t op, bool enabled);

// Measurements (derived from waveform buffer, not from FPGA hardware)
typedef enum {
    MEAS_FREQ, MEAS_PERIOD, MEAS_VMAX, MEAS_VMIN, MEAS_VPP,
    MEAS_VRMS, MEAS_VMEAN, MEAS_DUTY_POS, MEAS_DUTY_NEG,
    MEAS_RISE_TIME, MEAS_FALL_TIME, MEAS_WIDTH_POS, MEAS_WIDTH_NEG,
    MEAS_COUNT,
} meas_type_t;
// Computes measurement from last captured buffer. Returns NAN on invalid.
float scope_measure(int ch, meas_type_t type);

// Waveform save/recall
// Internal flash: 2 slots (2048 bytes per channel, flash @ 0x08066800 / 0x08067000)
// SDIO: unlimited named files under /sd/awto2000/wave_NAME.bin
int  scope_waveform_save(int slot);   // 0–1 for flash; returns 0 on success
int  scope_waveform_load(int slot);
int  scope_waveform_save_sdio(const char *name);
int  scope_waveform_load_sdio(const char *name);
```

### `dmm_api.h`

```c
// DMM chip: CS7721CN-1 (3.5 digit, ~2000 count)
// Communication: bit-bang USART via USART1 on STM32 (fw_dmm_usart_* in named.c)
typedef enum {
    DMM_DC_V, DMM_AC_V,
    DMM_RES,               // auto-range: ohm → kΩ → MΩ
    DMM_CAP,               // nF (capacitance)
    DMM_CONT,              // continuity + buzzer
    DMM_DIODE,
    DMM_DC_A, DMM_AC_A,    // current (requires external shunt or clamp via CH1)
    DMM_TEMP,              // thermistor input if fitted
} dmm_mode_t;

typedef struct {
    float    value;        // calibrated reading in SI units
    char     unit[8];      // "V", "mV", "kΩ", "MΩ", "nF", "A", "mA", "°C"
    bool     overload;     // OL flag from CS7721
    bool     autorange;
    uint8_t  range;        // 0 = auto (current selected range 0-2)
    float    min, max, avg; // rolling min/max/avg (reset on mode change)
    // trend: last 60 readings ring buffer for trend strip
    float    trend[60];
    uint8_t  trend_head;
} dmm_reading_t;

void dmm_set_mode(dmm_mode_t mode);
dmm_mode_t dmm_get_mode(void);
// Non-blocking — returns last decoded reading. CS7721 updates at ~3 Hz.
void dmm_get_reading(dmm_reading_t *out);
void dmm_reset_minmaxavg(void);
```

### `awg_api.h`

```c
// AWG chip: 3PD5651E, 12-bit DAC, frequency range 5–25 MHz
// (from named.c fw_awg_param_struct_init: waveform type 7 initial, defaults at DAT_0801b9d0)
typedef enum {
    AWG_SINE, AWG_SQUARE, AWG_TRIANGLE, AWG_RAMP_UP, AWG_RAMP_DOWN,
    AWG_TRAPEZOID, AWG_DC, AWG_ARB1, AWG_ARB2, AWG_ARB3, AWG_ARB4,
} awg_wave_t;

typedef struct {
    awg_wave_t wave;
    float      freq_hz;      // 1 Hz – 25 MHz (chip limit)
    float      amp_vpp;      // 0 – 3.0 Vpp (DAC + output amp range)
    float      offset_v;     // DC offset −1.5 to +1.5 V
    float      duty_pct;     // square/trapezoid: 0–100%
    bool       output_en;
} awg_config_t;

void awg_set(const awg_config_t *cfg);
void awg_get(awg_config_t *cfg);
void awg_output_enable(bool en);
// ARB waveform: 4096 samples, 12-bit unsigned (0-4095 = 0..Vpp)
int  awg_arb_upload(int slot, const uint16_t *samples, size_t count);
```

### `cal_api.h`

```c
// Calibration table — confirmed structure from firmware analysis of g_cali_table / g_scope_cali_block
// Flash @ 0x0803fc00 (original) — use equivalent page on new target
// 6 frequency bands × 2 channels × 10 V/div steps × 4 bytes = 480 bytes + 2-byte header + padding
//
// Band indices (0–5):
//   0 = DC_1x    (DC-coupled, 1× relay position)
//   1 = DC_10x   (DC-coupled, 10× relay position)
//   2 = AC_low   (AC-coupled, low frequency band)
//   3 = AC_mid   (AC-coupled, mid frequency band)
//   4 = AC_hi1   (AC-coupled, high band 1)
//   5 = AC_hi2   (AC-coupled, high band 2)
//
// Each entry is an int32 gain coefficient (signed fixed-point, applied as DAC offset to FPGA REG 0x0a/0x0c)
#define CAL_BANDS    6
#define CAL_CHANNELS 2
#define CAL_STEPS   10

typedef struct {
    uint8_t  valid;                                  // 0x01 = valid
    uint8_t  dirty;                                  // set before flash write
    int32_t  gain[CAL_BANDS][CAL_CHANNELS][CAL_STEPS];
} cal_table_t;

int  cal_load(void);   // load from flash, returns 0 on success
int  cal_save(void);   // write to flash (erase + program)
void cal_reset_defaults(void);
// Apply: returns calibrated mV for raw 8-bit offset-binary ADC sample
float cal_apply(int band, int ch, int step, uint8_t raw_adc);
// Helper: select band index from coupling + relay position + frequency estimate
int  cal_band_select(ch_coupling_t coupling, ch_probe_t probe);
```

---

## Key subsystems

### 1. FPGA driver (`drivers/fpga/`)

The FPGA is the acquisition engine (MachXO2-1200 + AD9288 dual 8-bit ADC). The STM32 communicates via **SPI1** (PA5=SCK, PA6=MISO, PA7=MOSI, PA4=CS). SPI commands confirmed from firmware:
- `0xa8` = register write (cmd + reg + data)
- `0xc8` = control write
- `0xa0` = register read
- `0xa4 / 0x01` = burst sample read (double-preamble, 6144 bytes per channel)

Key FPGA register map (extracted from `fw_apply_*` functions in named.c):

| Reg | R/W | Name | Notes |
|-----|-----|------|-------|
| 0x00 | W | CH1 frontend config | bits[7:5]=RANGE, bit[4]=coupling, bit[3]=relay_coil, bit[2]=BWL |
| 0x01 | W | CH2 frontend config | same layout as 0x00 |
| 0x02 | W | Trigger mode + coupling | bits[4:3]=trig_type, bit[0]=AC_coupling |
| 0x03 | W | Trigger hysteresis | 3 bytes: lo/center/hi ADC offset-binary |
| 0x06 | W | CH1 trigger level | uint16 LE, 0x8000=0V |
| 0x08 | W | CH2 trigger level | uint16 LE |
| 0x0a | W | CH1 vertical offset DAC | uint16 LE, calibration-applied |
| 0x0c | W | CH2 vertical offset DAC | uint16 LE |
| 0x0e | W | Timebase mode flags | bit[1]=FAST_DUAL, bit[0]=FAST_RATE, bits[5:4]=CH_SELECT |
| 0x0f | W | Timebase divisor | uint32 LE, FPGA_CLK/divisor = sample rate |
| 0x17 | W | Trigger slope | bits[1:0]: 0x01=rising, 0x00=falling |
| 0x18 | W | AWG DDS frequency word | uint32 LE, Hz = word × 25e6 / 2³² |
| 0x1c | W | AWG DC offset | uint16 LE, 0x8000=0V |
| 0x1d | W | AWG amplitude | uint16 LE |
| 0x1e | W | AWG duty/symmetry | uint16 LE (negated) |
| 0x50 | W | AWG waveform table | burst: 1024 × uint8 LUT |
| 0x00 | R | Acquisition status | bit[1]=capture_running |

**Relay double-write protocol** for CH range changes (latching relay): write config byte, delay (time TBD from hardware characterisation), write config byte with bits 0+3 cleared (de-energise). Original firmware uses 50 ms between writes.

Key operations identified from named.c:

- `fw_fpga_check_capture_done` — poll capture complete flag
- `fw_acq_arm_set` — arm/disarm acquisition
- `fw_apply_timebase_freq` / `fw_apply_timebase_freq2` — configure sample rate registers
- `fw_apply_trigger_edge` / `fw_apply_trigger_level_raw` — configure trigger
- `fw_ch_coupling_set` — CH coupling (relays/mux on frontend board, controlled via GPIO)
- `fw_ch2_display_params_set` — CH2 vertical parameters
- `fw_calc_waveform_mem_segments` — segment the 64kbit FPGA SRAM buffer across channels
- Buffer readout: DMA transfer from FPGA to STM32 SRAM (`fw_dma_blit` / `fw_dma_wait_done`)

Implement as a Zephyr device driver with a clean `fpga_driver.h` API. The register map needs to be extracted by cross-referencing `fw_apply_timebase_freq` + `fw_fsmc_timing_write` + the FPGA bitstream's known register layout (from pluribus reverse engineering).

### 2. LCD driver (`drivers/lcd/`)

ILI9341 via FSMC with 16-bit parallel bus. Named.c shows:

- `fw_lcd_init_seq` — ILI9341 init command sequence (extract commands verbatim — these are vendor-specified, not copyright)
- `fw_lcd_set_window_words` — set CASET/PASET for rectangular blit
- `fw_dma_blit` / `fw_dma_blit2` — DMA pixel burst to LCD
- `fw_backlight_pwm_set` — TIM1 PWM on PA8 (or equivalent on new STM32)

Use LVGL's `lv_display_t` with a DMA flush callback.

### 3. Keypad driver (`drivers/keypad/`)

4×5 matrix. DFU pin note: PB12 is column input shared with DFU sense at boot (original design). In new firmware, use dedicated GPIO with debounce via `fw_gpio_multi_debounce` pattern (44-sample debounce observed in named.c).

Expose as Zephyr `INPUT_EV_KEY` events. The application input handler maps raw key codes to `app_key_t` enum per `ui-redesign-concept.md` key assignment table.

### 4. DMM driver (`drivers/dmm_cs7721/`)

CS7721CN-1 uses a bit-banged serial protocol (not standard USART framing). From named.c:

- `fw_dmm_usart_init` — configures USART1 for bit-bang clocking
- `fw_dmm_byte_read` — read one byte from CS7721
- `fw_dmm_usart_send_byte` / `fw_dmm_bit_send_clk` — write byte + clock
- `fw_dmm_mode_xlat` — translate mode enum to CS7721 command byte
- `fw_dmm_pulse_bit6` / `fw_dmm_pulse_bit7` — specific bit-pattern sequences for range/mode

Update rate: ~3 Hz. Run in a dedicated Zephyr thread, publish readings to a k_msgq.

### 5. AWG driver (`drivers/awg_3pd5651e/`)

3PD5651E 12-bit DAC via SPI. From named.c:

- `fw_awg_param_defaults` — default params (waveform type 7, confirmed init)
- `fw_awg_draw_shape` — shape rendering for each of ~7 waveform icons
- `fw_usb_awg_write` — USB command handler that programs the AWG chip (use this to understand the SPI protocol)
- `fw_awg_meas_scale_*` — float scale helpers for amplitude/offset

### 6. USB protocol (`subsys/usb_protocol/`)

Compatible with existing PC app (`HTHardDll.dll` API). From `vendors/hantek/2d82/docs/app/app.md` the DLL exports map to USB bulk commands. Key handlers in named.c:

- `fw_usb_dmm_status_pack` — serialises DMM reading into USB packet
- `fw_usb_awg_write` — receives AWG config from PC
- `fw_usb_cali_write_handler` — receives calibration write from PC
- `fw_usb_ep0_*` — control endpoint (device lifecycle, DFU)

Phase 1: implement the same command set so `Scope.exe` running under Wine can connect and display waveforms. Use Zephyr USB device API.

### 7. Settings persistence (`subsys/settings_store/`)

From named.c:

- `fw_settings_save_custom1` — saves ~120 bytes of scope/DMM/AWG params to flash
- `fw_recall_default_setup` — loads factory defaults
- Flash page used: near 0x08051000 (shared with factory cal — separate pages in new layout)

Use Zephyr NVS (Non-Volatile Storage) subsystem. Store:
- `scope/ch[0,1]` — channel configs
- `scope/trig` — trigger config
- `scope/timebase` — timebase
- `dmm/mode` — last DMM mode
- `awg/config` — last AWG config
- `cal/table` — calibration table
- `display/backlight` — backlight level

SDIO µSD: FAT32 filesystem via Zephyr FS + `ff` (FatFs). Mount point `/sd`. Waveform slots: `/sd/awto2000/wave_N.bin` (N=0–5). Named settings: `/sd/awto2000/settings_NAME.bin`.

---

## Screen state machine

Each screen implements three callbacks:

```c
typedef struct screen_ops {
    void (*enter)(void *ctx);          // called on screen push
    void (*exit)(void *ctx);           // called on screen pop
    bool (*key)(void *ctx, app_key_t key, bool held);  // returns true if consumed
} screen_ops_t;
```

The `screen_manager` owns a stack of `(screen_ops_t *, ctx *)` pairs. `BACK` key pops; entering a sub-screen pushes.

### Waveform view key map (Phase 1)

| Key | Tap | Hold |
|-----|-----|------|
| CH1 | Toggle CH1 on/off | Push `channel_menu[0]` |
| CH2 | Toggle CH2 on/off | Push `channel_menu[1]` |
| MODE | Cycle: Waveform→XY→FFT→Waveform | — |
| FIND | Stub (Phase 2) | — |
| ^ | Adjust active param (VOLT/TBASE/TRIG) up | — |
| DMM | Push `dmm_view` | Push `dmm_function_menu` |
| AWG | Push `awg_view` | Push `awg_waveform_menu` |
| SNAP | Save waveform slot prompt | Recall menu |
| ← | Pan left (zoom mode) / decrease coarse | — |
| OK | Confirm active param / enter submenu | — |
| → | Pan right (zoom mode) / increase coarse | — |
| BACK | Exit zoom mode / return | — |
| v | Adjust active param down | — |
| ZOOM | Toggle zoom mode | — |
| MEAS | Toggle measurement panel overlay | Hold: push `measurement_setup` |
| RUN | Run/Stop toggle | — |
| TBASE | Activate TBASE parameter (arrows adjust) | — |
| VOLT | Activate VOLT parameter (arrows adjust) | — |
| TRIG | Activate TRIG level (arrows adjust) | — |
| SINGL | Single acquisition | — |
| F1-F4 | Context-dependent softkey actions | — |

### Active parameter pattern

TBASE / VOLT / TRIG each set `active_param` in waveform view state. While active:
- ^ / v = step the parameter (coarse)
- ← / → = step fine
- Second press of the same key OR `BACK` = deactivate
- The status bar highlights the active parameter with a border box (see `ui-redesign-concept.md`)

---

## Waveform view layout (Phase 1)

320×240 landscape. No buffer overview strip in Phase 1 (stub area at top, 12px, filled solid).

```
y=0    ┌──────────────────────────────────────────────────────────────┐
       │  [buffer overview — Phase 2 placeholder, 12px black bar]     │
y=12   ├──────────────────────────────────────────────────────────────┤
       │                                                              │
       │  WAVEFORM AREA  (8 divisions vertical, 12 divisions horiz)  │
       │  Grid: 10×8 divs with minor ticks at 20% intervals          │
       │  CH1 trace: yellow  CH2 trace: blue  Math: purple           │
       │  Trigger level: horizontal dashed line (colour = trig src)  │
       │  Trigger point: vertical tick at top (post-trigger marker)   │
       │                                                              │
y=170  ├──────────────────────────────────────────────────────────────┤
       │  STATUS BAR (18px)                                           │
       │  CH1:1V 10x DC │ 500us/div │ ^1.6V-AUTO │ CH2:off │ TRIG│  │
y=188  ├──────────────────────────────────────────────────────────────┤
       │  SOFTKEY BAR (20px) [F1] [F2] [F3] [F4]                     │
y=208  └──────────────────────────────────────────────────────────────┘
```

Waveform area: 158px tall = 8 divs × ~20px/div.  
Horizontal: 320px = 12 divs × ~26.7px/div (matches scope grid standard).

**Waveform rendering:**
- Pixel-column renderer (same approach as `fw_scope_waveform_render_a/b/c` in named.c): for each x-pixel column, find min/max ADC sample in that column's time slice, draw a vertical line segment. Handles 1:1 and compressed display.
- Interpolation (`fw_gfx_waveform_col_interp` pattern): fill gap between adjacent columns when signal moves more than 1 pixel vertically.
- XY mode: CH1 → x-axis, CH2 → y-axis, scatter plot (`fw_xy_mode_render` pattern).

---

## DMM view layout (Phase 1)

Full Fluke-190-style layout per `ui-redesign-concept.md §DMM mode`.

```
y=0    ┌──────────────────────────────────────────────────────────────┐
       │  DMM  [*] DC VOLTAGE                        [BACK to SCOPE]  │
y=18   ├──────────────────────────────────────────────────────────────┤
       │                                                              │
       │                                                              │
       │  ┌──────────────────────────────────────────────────────┐   │
       │  │          3 . 2 9 5    V                              │   │  ← 80px tall 7-seg font
       │  └──────────────────────────────────────────────────────┘   │
y=118  │  ████████████████████████████░░░░░░░░░░░░░░░░░░░  33%      │  ← bar graph
y=138  │  MIN: 3.291 V        MAX: 3.310 V        AVG: 3.295 V      │
y=165  │  TREND: _______________________________________________      │  ← 60-sample strip
y=182  ├──────────────────────────────────────────────────────────────┤
       │  [F1:DC-V]  [F2:AC-V]  [F3:RES/CAP]  [F4:DIODE/CONT/…]    │
y=202  └──────────────────────────────────────────────────────────────┘
```

The 7-segment digit font is drawn via LVGL custom draw callbacks at 6px stroke width, 80px tall. The exact segment geometry follows the existing `fw_gfx_draw_glyph_32px` / `fw_gfx_draw_glyph_16px` patterns scaled up — do not copy pixel data; implement a standard 7-segment layout.

---

## AWG view layout (Phase 1)

Per `ui-redesign-concept.md §AWG mode`.

```
y=0    ┌──────────────────────────────────────────────────────────────┐
       │  AWG  OUTPUT: [*] ON                           [F4: ON/OFF]  │
y=18   ├──────────────────────────────────────────────────────────────┤
       │  TYPE           FREQ             AMP              OFFSET     │
       │  > SINE       > 1.000 kHz <   > 2.000 Vpp <   > 0.000 V <  │
       │   SQUARE        [TBASE adj]     [VOLT adj]       [TRIG adj]  │
       │   TRIANGLE                                                   │
       │   TRAPEZOID                                                  │
       │   ARB 1-4                                                    │
y=80   ├──────────────────────────────────────────────────────────────┤
       │  PREVIEW (live CH1 if AWG output looped back)                │
       │  ┌──────────────────────────────────────────────────────┐   │
       │  │   ~~~    ~~~    ~~~    ~~~    ~~~    ~~~    ~~~       │   │
       │  └──────────────────────────────────────────────────────┘   │
y=180  ├──────────────────────────────────────────────────────────────┤
       │  [F1:TYPE ^/v]  [F2:FREQ ^/v]  [F3:AMP/OFS ^/v]  [F4:OUT] │
y=200  └──────────────────────────────────────────────────────────────┘
```

TBASE / VOLT / TRIG keys repurposed in AWG mode (frequency / amplitude / offset adjustment).

---

## Calibration data (Phase 1: load-only)

Phase 1 loads the factory calibration page from flash at startup and applies it via `cal_apply()`. No user-facing calibration UI in Phase 1. The cal table format matches the original firmware's `CaliParam` layout (derived from `fw_cali_voltscale_auto` and `fw_cali_voltscale_reset` in named.c) so the factory-calibrated device boots with correct readings.

---

## Build structure

```
app/
├── CMakeLists.txt
├── Kconfig
├── prj.conf                 ← Zephyr + LVGL config
├── boards/                  ← board-specific overlays
│   └── awto2000.overlay
├── src/
│   ├── main.c
│   ├── screen_manager.c/.h
│   ├── input_handler.c/.h   ← maps keypad events to app_key_t
│   ├── screens/
│   │   ├── waveform_view.c/.h
│   │   ├── channel_menu.c/.h
│   │   ├── trigger_menu.c/.h
│   │   ├── timebase_menu.c/.h
│   │   ├── measurement_panel.c/.h
│   │   ├── cursor_overlay.c/.h
│   │   ├── math_menu.c/.h
│   │   ├── fft_view.c/.h
│   │   ├── xy_mode_view.c/.h
│   │   ├── memory_menu.c/.h
│   │   ├── dmm_view.c/.h
│   │   ├── dmm_function_menu.c/.h
│   │   ├── awg_view.c/.h
│   │   ├── awg_waveform_menu.c/.h
│   │   └── util_menu.c/.h
│   ├── instrument/
│   │   ├── scope_api.c/.h
│   │   ├── dmm_api.c/.h
│   │   ├── awg_api.c/.h
│   │   └── cal_api.c/.h
│   └── subsys/
│       ├── usb_protocol.c/.h
│       └── settings_store.c/.h
└── drivers/
    ├── fpga/
    │   ├── fpga_driver.c/.h
    │   └── fpga_regs.h        ← register map (from hardware analysis)
    ├── lcd_ili9341/
    │   └── ili9341.c/.h
    ├── keypad/
    │   └── keypad.c/.h
    ├── dmm_cs7721/
    │   └── cs7721.c/.h
    └── awg_3pd5651e/
        └── awg_3pd5651e.c/.h
```

---

## Implementation order (Phase 1)

1. **BSP bring-up** — clock, GPIO, FSMC, DMA, SPI. LCD driver + LVGL flush callback. Keypad driver. Verify: show a static screen.

2. **Instrument drivers** — FPGA register map, DMM CS7721 bit-bang, AWG SPI protocol. Verify: capture a waveform, read DMM, output AWG sine.

3. **Instrument API** — `scope_api`, `dmm_api`, `awg_api`, `cal_api`. Load factory cal from flash.

4. **Settings store** — NVS + SDIO FatFS. Save/restore all configs.

5. **Screen manager + input handler** — push/pop stack, key routing.

6. **Core screens** — waveform_view (rendering + status bar + softkeys), channel_menu, trigger_menu, timebase_menu. This is the largest single task.

7. **DMM view** — 7-seg font, bar graph, MIN/MAX/AVG, trend strip.

8. **AWG view** — parameter display, waveform type selector, waveform preview.

9. **Measurement panel + cursor overlay** — computed measurements on buffer.

10. **Memory save/recall** — 6 flash slots + SDIO export.

11. **USB protocol** — compatible with HTHardDll.dll so Scope.exe connects.

12. **Math/FFT/XY** — math channel rendering, FFT (use CMSIS-DSP), XY mode.

---

## Phase 2+ (after feature parity)

Per `ui-redesign-concept.md`:

- **Explorer mode** — amplitude histogram, FIND smart auto-setup algorithm
- **Buffer overview strip** — always-visible 12px compressed capture view
- **Persist/density mode** — eye diagram, pixel density accumulation
- **Split mode** — waveform + histogram side-by-side
- **Probe library** — PicoScope-model browser, Rogowski coil handling, ground loop warning
- **Automotive test library** — 89 tests with reference waveforms and PASS/FAIL
- **User-accessible calibration** — guided modes A–D (zero, gain, probe comp, AWG x-ref)
- **FIND algorithm** — 3-sweep histogram → V/div + T/div recommendation
- **DMM trend record** — longer strip chart (scroll through last N readings)

---

## Confirmed hardware findings (from firmware analysis)

This section records concrete facts extracted from `decompiled_named.c` that must be respected in the reimplementation. All are confirmed (not inferred) unless noted.

### SOC / MCU
- **STM32F103** (72 MHz PLL from 8 MHz crystal, confirmed from `fw_set_sysclock_72mhz`)
- New target: STM32F4 or STM32H7 (faster, more RAM, same Zephyr support)

### FPGA
- **Lattice MachXO2-1200** (from pluribus RE work)
- ADC: **AD9288** dual-channel 8-bit, 250 MSPS max
- Sample format: **8-bit offset-binary** (0x80 = 0V, 0x00 = −FS, 0xFF = +FS)
- Buffer depth: **6144 bytes per channel** (48kbit — fits in MachXO2-1200 embedded BRAM)
- SPI interface: PA4=CS, PA5=SCK, PA6=MISO, PA7=MOSI (SPI1)
- Read command: `0xa4` + `0x01` preamble then 6144-byte burst

### DMM
- Chip: **CS7721CN-1** (Σ-Δ ADC, 3.5 digit, ~2000 count)
- Interface: USART2 at 2400 baud 8N1 (PA3=RX) for receive; TX via two GPIO shift registers:
  - SR1: PE6 latch, PE0 data, PC4 clock → HOLD / REL / SELECT / RANGE bits
  - SR2: PB0 latch → MEA1–4 mode selection
- Update rate: ~3 Hz
- Modes confirmed in firmware: DC-V, AC-V, Res, Cap, Cont, Diode, Freq (Temperature not implemented)
- Hold mode: `g_dmm_hold_flag` toggled by HOLD key (0x11)

### AWG
- Chip: **3PD5651E** (12-bit DAC, 25 MHz reference, 32-bit DDS)
- Waveform table: 1024-entry uint8 LUT uploaded via SPI burst (cmd 0x50)
- Waveform types: DC/rectangle, Sine, Triangle, Sawtooth (+ inferred: Noise)
- Frequency: 1 Hz – ~12.5 MHz (DDS word = freq × 2³² / 25e6)
- Amplitude register: FPGA REG 0x1d (uint16), range 0–2000 mV (0xffff = full scale)
- Offset register: FPGA REG 0x1c (uint16), 0x8000 = 0V, ±1000 mV range
- Duty/symmetry: FPGA REG 0x1e (uint16, negated; 0x0000 = 50%)
- Output enable: PE5 GPIO HIGH = AWG active

### LCD
- ILI9341, 320×240, RGB565 (16-bit colour)
- Interface: FSMC 16-bit parallel (PD11=RS/DC, PE7–PE15 data bus)
- DMA-accelerated: 2×200-pixel column buffer, split DMA transfers
- Backlight: TIM1 PWM on PA8 (TIM1_CH1)
- Buzzer: TIM8 CH2 on PC7, 3030 Hz tone at PSC=719

### Keypad
- 4×5 matrix: GPIOC rows, GPIOB columns (20 keys)
- PB12 shared with DFU trigger at boot (pull LOW to enter DFU)
- Debounce: main-loop scan with counter state machine

### Flash memory layout (original STM32F103)
```
0x08000000  Bootloader (20 KB, DFU 1.1 — VID 0x0483 PID 0x2d42)
0x08005000  Application (stage-2, VTOR relocated here)
0x08049014  g_automotive_menu (89 entries × 60 bytes = ~5340 bytes)
0x08053c20  USB string: "HT2xxLxx" (UTF-16LE)
0x08053d24  USB string: "Hantek"
0x08053fe4  Model string "Hantek"
0x08066800  CH1 waveform slot 0 (2048 B raw + 2048 B display)
0x08067000  CH2 waveform slot 0
0x0803fc00  Calibration block (0x2f8 B): [valid][dirty][6 bands × 2ch × 10steps × 4B]
0x0803fe00  Scope config snapshot (0x1f8 B): timebase, V/div, coupling, trigger
```

### V/div FPGA encoding (10 steps, confirmed from `fw_apply_ch_frontend`)

| Index | V/div | FPGA byte (DC, no BWL) | Relay (bit3) |
|-------|-------|------------------------|--------------|
| 0 | 10 mV | 0x81 | 0 (1× path) |
| 1 | 20 mV | 0xc1 | 0 |
| 2 | 50 mV | 0x21 | 0 |
| 3 | 100 mV | 0x01 | 0 |
| 4 | 200 mV | 0x61 | 0 |
| 5 | 500 mV | 0x88 | 1 (10× path) |
| 6 | 1 V | 0xc8 | 1 |
| 7 | 2 V | 0x48 | 1 |
| 8 | 5 V | 0x08 | 1 |
| 9 | 10 V | 0x68 | 1 |

Relay latch protocol: write config byte → 50 ms delay → write (config & 0xf6) → 50 ms delay.

### Trigger modes (FPGA REG 0x02 bits[4:3])
- 0x00 = Edge (rising/falling, confirmed)
- 0x08 = Pulse-width (inferred)
- 0x10 = Video/sync (inferred)
- 0x18 = Dual A&B AND logic (inferred, not in normal UI)

Trigger level: FPGA REG 0x06 (CH1) / 0x08 (CH2), uint16 offset-binary (0x8000 = 0V).
Hysteresis: FPGA REG 0x03, 3 bytes {lo, center, hi} = center ± 10 ADC counts.
Sweep mode: implemented in STM32 acquisition state machine (not a hardware register).

### Measurements (31 measurement codes, 0x01–0x1f)
All computed in software from the 6144-byte sample buffer. Key codes:
- 0x01 = Vpp, 0x0f = Vrms, 0x10 = Vmax, 0x11 = Vmin, 0x12 = Vmean
- 0x15 = Rise time, 0x16 = Fall time
- 0x19 = Frequency, 0x1a = Period, 0x1b = Duty+, 0x1c = Width+, 0x1d = Width−
- 0x1e = Phase (CH1 vs CH2)
- 0x02–0x0e = CH1/CH2 ratio variants

No FFT in original firmware — strictly time-domain. FFT is a new feature for Phase 1 of reimplementation (CMSIS-DSP).

### Automotive tests
- ~89 test entries in flash table at 0x08049014, 60-byte stride
- Each entry: preset scope config (timebase, V/div, trigger) + category strings
- String table supports CJK (5 bitmap fonts in firmware)
- No automatic PASS/FAIL — user interprets waveform manually
- Categories: Ignition, Distributor, Spark, Air Flow, Camshaft, Crankshaft, Lambda, Throttle, ABS, Coolant, Knock, MAP, Engine
