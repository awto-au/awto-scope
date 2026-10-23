# 2D82 — UI & Graphics Design

**Stack:** LVGL 9.x on top of Zephyr `display` API → ILI9341 320×240 RGB565

Reference for layout intent: `vendors/hantek/2d82/docs/ui-redesign-concept.md`
Reference for screen/mode inventory: `vendors/hantek/2d82/docs/reimplementation-plan.md §Screen tree`

---

## Hard rules (graphics stack)

**None of the OEM graphics assets are used.**
- No OEM font data from `decompiled_named.c` / `vendors/hantek/2d82/docs/fonts/*.html`
- No OEM bitmaps or shape glyphs
- No OEM colour palette
- No OEM layout assumptions beyond "320×240 landscape"

The OEM font sizes and element positions in `vendors/hantek/2d82/docs/firmware/fonts.md` are useful as a
*scale reference only* — they tell you roughly what fits at what size on this panel.
The actual fonts, the actual layout, and the actual grid geometry are redesigned from
scratch based on `vendors/hantek/2d82/docs/ui-redesign-concept.md` and the Fluke 190 as the primary reference.

---

## Font strategy

### Principle
All text rendering uses standard bitmap fonts, sourced from open-licensed pixel/bitmap
font projects. LVGL's built-in font engine (`lv_font_t`) is used directly — no custom
renderer.

### Font roles and size targets

| Role | Size target | LVGL source | Notes |
|------|------------|-------------|-------|
| **DMM primary reading** | ~80px tall | Custom 7-segment stroke font or large bitmap | Must be readable at arm's length (~1m). 4 digits + decimal + unit. |
| **Status bar / labels** | 12–14px | `lv_font_montserrat_12` or `lv_font_unscii_8` | Channel label, timebase, trigger. Compact, fully ASCII. |
| **Softkey labels** | 10–12px | same as status bar | 4 labels across 320px, must fit single line |
| **Measurement readouts** | 14–16px | `lv_font_montserrat_14` or `lv_font_montserrat_16` | 4–5 concurrent measurements. Value + unit on one line. |
| **Menu items** | 14–16px | same as measurements | Navigable list items |
| **Min/Max/Avg (DMM)** | 12px | same as status bar | Three values in one row below the bar graph |
| **Trend / axis labels** | 10px | smallest available | Y-axis tick labels on scope grid |

### Phase 1 font shortlist (to evaluate)

These are all permissively licensed (MIT / OFL / public domain) and have LVGL converter
support via `lv_font_conv`:

- **[Terminus](https://terminus-font.sourceforge.net/)** — bitmap pixel font, multiple
  sizes (6×12, 8×14, 8×16, 10×20, 12×24, 14×28, 16×32). Clean, hinting-free, designed
  for small screens. Strong candidate for status bar and menu.

- **[Spleen](https://github.com/fcambus/spleen)** — bitmap font, 5×8 through 32×64.
  5×8 would fit 53 chars across 320px (no spacing). 8×16 fits 40. Strong candidate for
  smallest sizes.

- **[Unifont](https://unifoundry.com/unifont/)** — 8×16 bitmap, full Unicode. Useful if
  CJK or symbol coverage is ever needed. Heavy (large flash), use sparingly.

- **[m5x7 / m3x6](https://managore.itch.io/m5x7)** (Daniel Linssen) — pixel fonts
  optimised for small LCD. 7px cap height. Very compact, good readability.

- **LVGL built-ins** — `lv_font_montserrat_*` (8 through 48), proportional. Good
  fallback but are vector-hinted TTF, larger flash footprint than bitmap options.

### DMM reading font

The DMM primary reading needs ~80px tall digits readable at 1m. Options:

1. **Custom 7-segment stroke font** drawn via LVGL canvas/line primitives. Each segment
   is a filled rectangle. Stroke width 6–8px. This is what `vendors/hantek/2d82/docs/ui-redesign-concept.md`
   specifies and is the cleanest approach — no font file, pure vector, scales freely.
   Implement as `dmm_draw_digit(canvas, x, y, char c, lv_color_t col)`.

2. **Large bitmap font** scaled up. Terminus 16×32 doubled → 32×64 is too small.
   A purpose-made 40×80 bitmap numeral set would work but is significant effort.

**Recommendation: option 1.** The 7-segment is simple to implement (7 rectangles per
digit), looks correct on this display, and matches the Fluke 190 aesthetic exactly.
Only digits 0–9, `.`, `-`, and unit glyphs (`V`, `mV`, `A`, `mA`, `Ω`, `kΩ`, `MΩ`,
`nF`, `°C`) are needed — ~20 glyphs total.

---

## Colour palette

Derived from the Fluke 190 and standard scope conventions. All RGB565.

| Name | RGB565 | Use |
|------|--------|-----|
| `COL_BG` | `0x0000` black | Screen background |
| `COL_GRID` | `0x2124` dark grey | Waveform grid lines |
| `COL_GRID_MINOR` | `0x10A2` dimmer grey | Minor tick marks |
| `COL_CH1` | `0xFFE0` yellow | CH1 trace, CH1 label |
| `COL_CH2` | `0x001F` blue | CH2 trace, CH2 label |
| `COL_MATH` | `0xF81F` magenta | Math channel trace |
| `COL_TRIG` | `0xF800` red | Trigger level line, trigger marker |
| `COL_STATUS_BG` | `0x18C3` dark blue-grey | Status bar background |
| `COL_STATUS_FG` | `0xFFFF` white | Status bar text |
| `COL_ACTIVE_PARAM` | `0xFFE0` yellow | Highlighted active parameter box border |
| `COL_SOFTKEY_BG` | `0x2945` dark grey | Softkey bar background |
| `COL_SOFTKEY_FG` | `0xFFFF` white | Softkey label text |
| `COL_DMM_READING` | `0xFFFF` white | DMM primary digit |
| `COL_DMM_BAR_FG` | `0x07E0` green | DMM bar graph fill |
| `COL_DMM_BAR_OL` | `0xF800` red | DMM bar graph overload |
| `COL_WARN` | `0xFD20` orange | Warnings, OL indicator |
| `COL_PASS` | `0x07E0` green | PASS verdict |
| `COL_FAIL` | `0xF800` red | FAIL verdict |

---

## Waveform grid

The OEM uses a 10×8 division grid. The redesign keeps this ratio but the pixel
geometry is set by the available waveform area, not copied from the OEM.

**Phase 1 waveform area:** y=12 to y=170 → 158px tall, 320px wide.

```
8 vertical divisions:   158px / 8 = 19.75px/div  → use 19px (rounding leaves 6px spare, distribute top/bottom)
12 horizontal divisions: 320px / 12 = 26.67px/div → use 26px (4px spare, distribute left/right)
```

**Grid lines:** 1px wide, `COL_GRID`.
**Minor ticks:** at 20% intervals (every 0.2 div) on the centre cross only, 3px long, `COL_GRID_MINOR`.
**Centre cross:** horizontal at y_mid, vertical at x_trig — full lines, slightly brighter than grid.

This is standard scope grid convention (Fluke 190, Rigol, Siglent all use this).
Do not add sub-division grid lines across the full area — it clutters the display at 320px width.

---

## Screen zones (Phase 1)

```
y=0    ┌──────────────────────────────────────────────┐
       │  PLACEHOLDER BAR  12px  (Phase 2: buffer overview)   │
y=12   ├──────────────────────────────────────────────┤
       │                                              │
       │  WAVEFORM AREA   158px                       │
       │  8×12 divisions, traces, grid, trig marker   │
       │                                              │
y=170  ├──────────────────────────────────────────────┤
       │  STATUS BAR   18px                           │
       │  CH1 │ T/div │ TRIG │ CH2 │ state           │
y=188  ├──────────────────────────────────────────────┤
       │  SOFTKEY BAR  20px                           │
       │  [F1]   [F2]   [F3]   [F4]                  │
y=208  └──────────────────────────────────────────────┘
```

Total: 12 + 158 + 18 + 20 = 208px of 240px used. 32px spare — kept as bottom margin
to avoid the panel edge. In Phase 2 the placeholder bar becomes a real 12px buffer
overview strip; nothing else changes.

---

## Status bar layout (18px, y=170–188)

Single-row, monospace-style using the smallest font (10–12px).

```
| CH1:1V 10x DC | 500us/div | ^1.6V-AUTO | CH2:off | TRIG |
```

When a parameter is active (TBASE / VOLT / TRIG key pressed), that field gets a
1px yellow border box and the value flashes/highlights. The other fields stay normal.

Active TBASE example:
```
| CH1:1V 10x DC | [500us/div] | ^1.6V-AUTO | CH2:off | RUN |
                  ^--- yellow border box
```

Implementation: LVGL `lv_label` widgets in a horizontal `lv_obj` container.
Active field: draw a 1px `lv_border` in `COL_ACTIVE_PARAM` on that label's parent cell.

---

## Softkey bar (20px, y=188–208)

Four equal-width cells across 320px = 80px each. Label centred, 10–12px font.
Background `COL_SOFTKEY_BG`, text `COL_SOFTKEY_FG`.

Labels are set by the active screen. The screen_manager calls `softkey_set(n, label)`
when pushing/popping screens. An empty string hides that softkey (dims the cell).

---

## DMM screen zones

```
y=0    ┌──────────────────────────────────────────────┐
       │  HEADER  18px:  DMM  [*] DC VOLTAGE  [BACK]  │
y=18   ├──────────────────────────────────────────────┤
       │                                              │
       │  READING BOX  100px                          │
       │  7-seg digits, ~80px tall, centred           │
       │  "  3 . 2 9 5    V  "                        │
       │                                              │
y=118  ├──────────────────────────────────────────────┤
       │  BAR GRAPH  20px                             │
       │  ████████░░░░░░  33%                         │
y=138  ├──────────────────────────────────────────────┤
       │  MIN / MAX / AVG  27px                       │
       │  MIN: 3.291V   MAX: 3.310V   AVG: 3.295V    │
y=165  ├──────────────────────────────────────────────┤
       │  TREND STRIP  17px  (last 60 readings)       │
y=182  ├──────────────────────────────────────────────┤
       │  SOFTKEY BAR  20px                           │
y=202  └──────────────────────────────────────────────┘
```

**7-segment digit geometry** (80px tall, 6px stroke):

```
 _
|_|   Width per digit: ~40px.  Decimal point: 8px.  Space: 8px.  Unit: ~44px.
|_|   Total for "3.295 V": 4×40 + 8 + 8 + 44 = 228px — fits in 280px with margins.
```

Segment layout (standard 7-segment, 'a' top through 'g' middle):
```
  aaa
 f   b
 f   b
  ggg
 e   c
 e   c
  ddd
```
Each segment = filled `lv_draw_rect` call. On/off per digit code is a static lookup table.

**Bar graph:** LVGL `lv_bar` widget, range set to current DMM range, value = reading.
Colour changes to `COL_DMM_BAR_OL` (red) at >90% of range.

**Trend strip:** 60-pixel-wide `lv_canvas` with 1px columns. Each new reading shifts
left by 1px (memmove) and draws a new column at right. Updated at DMM rate (~3 Hz).

---

## LVGL configuration notes

Key `lv_conf.h` settings for this target:

```c
#define LV_COLOR_DEPTH        16      /* RGB565 */
#define LV_HOR_RES_MAX        320
#define LV_VER_RES_MAX        240
#define LV_DPI_DEF            100     /* ~100 DPI for 2.8" 320×240 */

/* Memory: STM32F103 has 64KB SRAM. LVGL draw buffer + heap must fit. */
#define LV_MEM_SIZE           (12 * 1024U)   /* 12KB LVGL heap */

/* Draw buffer: 1/10 screen = 320*24 = 7680 bytes = two 3840-byte buffers for DMA */
/* Defined in lcd.c as static arrays, passed to lv_display_set_buffers() */

/* Disable features not needed in Phase 1 */
#define LV_USE_ANIMATION      0   /* re-enable for Phase 2 transitions */
#define LV_USE_SHADOW         0
#define LV_USE_BLEND_MODES    0
#define LV_USE_OPA_SCALE      0
#define LV_USE_GPU            0   /* no GPU on F103 */

/* Fonts: only include what's selected */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
/* All others: 0 — saves flash */
```

**Draw buffer size:** 320×24 pixels × 2 bytes = 15KB for a full double buffer. Too large
for F103's 64KB SRAM alongside everything else. Use a single 320×10 = 6.4KB partial
buffer. DMA flush still works — LVGL calls `flush_cb` per strip.

If RAM pressure is a problem, drop to 320×4 (2.6KB). Slower (more flush calls per
frame) but correct. Benchmark in Renode first.

> **Implemented (issue #125):** the actual Zephyr LVGL module (`modules/lvgl/Kconfig.memory`
> in this Zephyr version, 4.4.99) doesn't expose an absolute-byte buffer size Kconfig —
> only `CONFIG_LV_Z_VDB_SIZE`, a percentage of `hor_res*ver_res`. `code/app/prj.conf`
> sets `CONFIG_LV_Z_VDB_SIZE=6` (≈4608px = 9216 bytes/buffer, closest percentage step to
> the ~8KB target) with `CONFIG_LV_Z_DOUBLE_VDB=y` (2 buffers = ~18KB total — this module
> has no static triple-buffer option). Render mode is `LV_DISPLAY_RENDER_MODE_PARTIAL`
> (matches this doc's "per-strip flush" design). Flush path is `code/a2k_board/src/lcd.c`
> `zd_write()` — DMA1 M2M (`lcd_blit_dma`) when `CONFIG_DMA && CONFIG_AW2_HW_VALIDATED`,
> polled otherwise (Renode always uses polled, per the config's design intent).

---

## LVGL task integration

`lv_task_handler()` is called from the supervisor loop, not a dedicated thread, to
avoid a separate 4KB stack. The supervisor drains its message queue then calls
`lv_task_handler()` each iteration (10ms tick).

```c
/* supervisor.c */
void supervisor_run(void)
{
    while (1) {
        /* drain message queue */
        struct hantek_msg msg;
        while (k_msgq_get(&hantek_msgq, &msg, K_NO_WAIT) == 0) {
            supervisor_dispatch(&msg);
        }
        /* LVGL tick + handler */
        lv_tick_inc(10);
        lv_task_handler();
        k_msleep(10);
    }
}
```

If LVGL rendering is ever too slow at 10ms (waveform redraw takes >10ms), move
`lv_task_handler()` to a dedicated thread at lower priority than the FPGA capture thread.
Start with the simple approach and measure.

---

## What is NOT done via LVGL

| Thing | How instead |
|-------|-------------|
| Waveform trace rendering | Direct pixel writes via `display_write()` into a framebuffer — too fast/custom for LVGL widgets |
| DMM 7-segment digits | Custom draw via `lv_draw_rect` calls on an `lv_canvas`, not an `lv_label` |
| Trend strip | `lv_canvas` with direct pixel manipulation |
| Scope grid | Direct pixel writes before LVGL composites the overlay |

The strategy: render the waveform area directly, then let LVGL composite the status bar,
softkey bar, measurement panel, and any active menus on top. The waveform canvas is an
`lv_obj` with a custom draw callback that calls the direct renderer.

---

## Scalable layout — `aw2_layout_t`

All pixel coordinates are derived at runtime from display capabilities — no hardcoded
values anywhere except inside `aw2_layout_init()`.

```c
typedef struct {
    uint16_t w, h;           /* display resolution, e.g. 320, 240 */
    uint16_t status_h;       /* ~7.5% of h */
    uint16_t softkey_h;      /* ~8% of h */
    uint16_t overview_h;     /* Phase 2 buffer strip, 0 in Phase 1 */
    uint16_t wave_y0;        /* top of waveform area */
    uint16_t wave_h;         /* h - overview_h - status_h - softkey_h */
    uint16_t hdiv;           /* w / 12 — one horizontal division */
    uint16_t vdiv;           /* wave_h / 8 — one vertical division */
} aw2_layout_t;

void aw2_layout_init(aw2_layout_t *L, const struct display_capabilities *caps);
```

Font size selection is also resolution-driven — the renderer picks the largest available
font that fits within the tile's allocated height, never a hardcoded point size.

---

## Unified channel model

All signal sources — ADC, DMM, AWG, math — are uniform channel objects. The display
engine does not know or care what type of channel it is rendering.

```c
typedef enum {
    CH_SRC_ADC,    /* FPGA burst capture — CH1, CH2 */
    CH_SRC_DMM,    /* CS7721 readings */
    CH_SRC_AWG,    /* computed from DDS config, no ADC */
    CH_SRC_MATH,   /* computed from other channels */
} ch_src_t;

typedef struct channel channel_t;
struct channel {
    ch_src_t    src;
    bool        enabled;
    lv_color_t  colour;
    const char *label;           /* "CH1", "CH2", "DMM", "AWG", "M1" … */

    /* vtable — called by tile renderer */
    void (*render_full)(channel_t *, const lv_area_t *area);   /* sole active channel */
    void (*render_tile)(channel_t *, const lv_area_t *area);   /* shared tile */

    /* current instant values, updated by each channel's data path */
    float       value_v;         /* volts (or ohms, Hz — unit in label) */
    float       freq_hz;         /* 0 if not applicable */
    const char *unit;            /* "V", "mV", "kHz", "Ω" … */
};
```

Channel list is a fixed static array of up to 8 entries (CH1, CH2, DMM, AWG, M1–M4).
No heap allocation. Only `enabled` channels are rendered.

### Layout engine

```c
/* Returns tile rects for all enabled channels, tiled left-right then top-bottom */
int layout_tiles(const aw2_layout_t *L,
                 channel_t **active, int n_active,
                 lv_area_t *rects_out);
```

Grid selection by `n_active`:
```
n=1  → 1×1  (full wave area)
n=2  → 2×1  (side by side) or 1×2 (stacked) — user preference
n=3  → 2×2  with one tile spanning full width
n=4  → 2×2
n=8  → 2×4
n=16 → 4×4
```

The renderer calls `ch->render_full()` when `n_active == 1`, otherwise `ch->render_tile()`.

---

## Channel tile layout

Every channel uses the same tile template regardless of type or size. The tile renderer
receives only a bounding rect and fills it:

```
┌──────────────────────────────────┐  ← tile top
│                                  │
│   3.295 V   1.02 kHz     CH1    │  ← large instant reading (dominant)
│                                  │
│  [trace overlay, semi-transparent, drawn on top of reading]
│                                  │
└──────────────────────────────────┘  ← tile bottom
```

- **Reading** fills the tile, font size scaled to tile height.
- **Trace** is a semi-transparent overlay drawn over the reading — temporal context
  without consuming separate vertical space. Uses `lv_canvas` with alpha blending or
  direct pixel writes with a fixed alpha step.
- **Channel label** (CH1, DMM, AWG …) small, top-right corner, always visible.

No history list. No separate trend zone. Reading + trace overlay, nothing else.

### Tile size tiers

At 16 channels on 320×240 the minimum tile is ~80×52px. Three rendering tiers,
selected by `tile_h`:

| Tier | `tile_h` | Reading font | Freq shown | Label |
|------|----------|-------------|------------|-------|
| LARGE | ≥ 100px | ~40px | yes, same line | corner |
| MEDIUM | ≥ 52px | ~20px | yes, stacked below value | corner |
| SMALL | < 52px | ~14px | no | inline prefix |

At SMALL tier: `CH1 3.30V` on one line, no freq — just enough to read from 1m.

Font is selected from available bitmap sizes to fill `tile_h * 0.4` — e.g. at
tile_h=52, target ~20px → pick Terminus 10×20 or nearest available.

---

## RAM / flash discipline

**Rule: at most one screen's LVGL objects exist in heap at any time.**

- Pushing a new screen: allocate new screen objects → render first frame → delete old screen.
- Never pre-build hidden screens. Never keep a second screen in memory "for fast switching".
- All `lv_font_t` glyph bitmaps are `const` in flash (`__attribute__((section(".rodata")))`).
  LVGL font descriptors point directly into flash — zero RAM copy of font data.
- Channel structs are static globals (always needed, not per-screen).
- DMM ring buffer (for trace overlay history) is a fixed static array in the DMM channel
  struct — size chosen to fill the trace overlay at maximum tile width (320 samples = 320B).
- AWG waveform table (1024B LUT) is computed into a static buffer and uploaded to FPGA.
  Buffer is reused for math channel scratch — never doubled up.
- LVGL draw buffer: single 320×10 partial buffer (6.4KB). Drop to 320×4 (2.6KB) if RAM
  pressure requires it. Benchmark in Renode.

---

## Open research items

- **`lv_chart` viability**: Does LVGL 9.x `lv_chart` support the trace overlay pattern
  (semi-transparent, sparse updates, drawn over text)? What is its heap footprint per
  instance? Check Zephyr's bundled LVGL version first. If `lv_chart` doesn't fit the
  model cleanly, use direct pixel writes on `lv_canvas` instead.
- **Bitmap font selection**: Evaluate Terminus 8×14/10×20, Spleen 8×16, m5x7 side by
  side in Renode at MEDIUM and SMALL tile sizes before committing.
- **Trace alpha blending**: ILI9341 via FSMC has no hardware blending. Semi-transparent
  overlay requires read-modify-write per pixel (read RGB565 from framebuffer, blend,
  write back). Measure cost at 320px wide trace — may need to drop to XOR or fixed-colour
  overlay if too slow.
- **Fluke 190 reference materials**: Collect service manual + user manual PDFs into
  `sources/fluke/` for layout reference. The on-device UI (screen layouts, grid geometry,
  DMM reading presentation) is the primary reference, not the PC software.

## Open questions (not decisions yet)

- Does the waveform grid belong inside the channel tile renderer or is it a separate
  background layer drawn before tiles? Separate layer is cleaner for the scope channels
  but means the DMM/AWG tiles render over a grid they don't use.
- 2×1 vs 1×2 split for 2 active channels — user preference setting, default TBD.
- 32px bottom margin (y=208–240) — reserved for now. Phase 2 candidate: persistent
  mini-measurement row or buffer overview strip.
