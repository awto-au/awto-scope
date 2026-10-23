# 2D82 — Firmware Fonts

## How it works

All font knowledge lives in `ghidra_names_2d82.tsv` as `var` entries. From there:

- `python3 vendors/hantek/2d82/scripts/gen.py` → reads TSV + `app.bin` → generates `vendors/hantek/2d82/docs/fonts/*.html` contact sheets
- `ExportAll.java` → reads TSV → emits TSV `#` doc block into `decompiled_named.c` header

The TSV is the single source of truth. Do not edit the `.c` file or the HTML — edit the TSV.

---

## Running the contact sheet generator

```bash
python3 vendors/hantek/2d82/scripts/gen.py
# opens vendors/hantek/2d82/docs/fonts/index.html — all fonts linked from there
```

To add or correct a glyph name for the Shape font, edit the `Known glyphs:` list in the
`g_font_shape` TSV entry, then re-run `gen.py`. The name appears in the contact sheet Name
column automatically.

---

## TSV entry format

Font entries use `type=var`. The comment field is free text but must contain:
- `stride 0xNN` — used by gen.py to compute glyph offsets
- `Rendered by <fn>` or `Used by/in <fn>` — shown as "Used by" in contact sheet meta

For the Shape font, glyph names are embedded in the comment as:

```
Known glyphs:\n0xNN=name, 0xNN=name, ...
```

Example TSV entry (Shape font):

```
0x0805147c	g_font_shape	var	[flash/byte[~4096]] Shape font: 8x9 ... Rendered by fw_gfx_draw_glyphs_centered2 ... Known glyphs:\n0x11=DC, 0x12=AC, 0x14=V, 0x96=Ohm(Ω), 0xdc=Hz-v2
```

---

## What appears in decompiled_named.c

TSV `#` comment lines are prepended as a block comment at the top of `decompiled_named.c`
by ExportAll. Font var entries appear there like:

```c
 * 0x0804a5a4  g_font_table      [flash/byte[5278]]  13x13 glyph table (CONFIRMED)
 *                                            26 bytes/glyph (0x1a stride).
 *                                            ~203 glyphs: 0-76=CJK, 77-97=ASCII, 98-202=mix.
 *                                            Rendered by fw_gfx_draw_char as RGB565 fg/bg pairs.
```

---

## Fonts summary

Addresses, dimensions, and glyph counts: see `g_font_table`, `g_font_32px`, `g_font_16px`, `g_font_small`, `g_font_e`, `g_font_shape`, `g_navpad_bitmap` in named.c.

| ID | Symbol | Contact sheet |
|----|--------|---------------|
| A | `g_font_table` | fontA.html |
| B | `g_font_32px` | fontB.html |
| C | `g_font_16px` | fontC.html |
| D | `g_font_small` | fontD.html |
| E | `g_font_e` | fontE.html |
| S | `g_font_shape` | shape.html |
| — | `g_navpad_bitmap` | navpad.html |
